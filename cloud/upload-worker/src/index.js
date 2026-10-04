import { AwsClient } from "aws4fetch";

const ALLOWED_APPS = new Set(["panprobe", "panplay"]);
const SHA256_REGEX = /^[0-9a-f]{64}$/;
const DATE_REGEX = /^\d{4}-\d{2}-\d{2}$/;
const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const MAX_UPLOAD_SIZE = 200 * 1024 * 1024; // 200 MB
const devKeys = new Map();

function jsonResponse(data, status = 200, extraHeaders = {}) {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      "content-type": "application/json",
      ...extraHeaders,
    },
  });
}

function hexToBase64(hex) {
  if (typeof Buffer !== "undefined") {
    return Buffer.from(hex, "hex").toString("base64");
  }
  const bytes = new Uint8Array(
    hex.match(/.{1,2}/g).map((byte) => parseInt(byte, 16))
  );
  let binary = "";
  for (let i = 0; i < bytes.length; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary);
}

export default {
  async fetch(request, env) {
    try {
      const url = new URL(request.url);
      const { pathname, origin } = url;

      // POST /upload-url
      if (pathname === "/upload-url" && request.method === "POST") {
        // Rate limiting
        // Local dev (DEV_DIRECT on 127.0.0.1) skips the limiter so smoke tests can loop.
        const devLocal = env.DEV_DIRECT === "1" &&
          (url.hostname === "127.0.0.1" || url.hostname === "localhost");
        if (env.UPLOAD_LIMIT && !devLocal) {
          const clientIp =
            request.headers.get("cf-connecting-ip") || "unknown";
          const { success } = await env.UPLOAD_LIMIT.limit({ key: clientIp });
          if (!success) {
            return jsonResponse({ error: "Rate limit exceeded" }, 429);
          }
        }

        const contentLengthHeader = request.headers.get("content-length");
        if (!contentLengthHeader) {
          return jsonResponse({ error: "Missing Content-Length" }, 413);
        }
        const reqContentLength = parseInt(contentLengthHeader, 10);
        if (
          isNaN(reqContentLength) ||
          reqContentLength < 0 ||
          reqContentLength > 4096
        ) {
          return jsonResponse({ error: "Payload too large" }, 413);
        }

        let body;
        try {
          const rawText = (await request.text()).slice(0, 4096);
          body = JSON.parse(rawText);
        } catch {
          return jsonResponse({ error: "Invalid JSON body" }, 400);
        }

        const { app, version, size, sha256 } = body || {};

        if (!app || !ALLOWED_APPS.has(app)) {
          return jsonResponse(
            { error: "Invalid app. Must be 'panprobe' or 'panplay'" },
            400
          );
        }

        if (
          typeof version !== "string" ||
          version.length === 0 ||
          version.length > 32
        ) {
          return jsonResponse(
            { error: "Invalid version. Must be string up to 32 characters" },
            400
          );
        }

        if (
          typeof size !== "number" ||
          !Number.isInteger(size) ||
          size < 1 ||
          size > MAX_UPLOAD_SIZE
        ) {
          return jsonResponse(
            {
              error: `Invalid size. Must be an integer between 1 and ${MAX_UPLOAD_SIZE}`,
            },
            400
          );
        }

        if (typeof sha256 !== "string" || !SHA256_REGEX.test(sha256)) {
          return jsonResponse(
            {
              error:
                "Invalid sha256. Must be a 64-character lowercase hex string",
            },
            400
          );
        }

        const id = crypto.randomUUID();
        const date = new Date().toISOString().slice(0, 10);
        const key = `uploads/${date}/${id}.zip`;
        const downloadUrl = `${origin}/f/${date}/${id}`;

        // Local development direct upload mode
        if (env.DEV_DIRECT === "1") {
          devKeys.set(id, {
            size,
            sha256,
            expires: Date.now() + 900 * 1000,
          });
          const uploadUrl = `${origin}/dev-put/${date}/${id}`;
          const headers = {
            "content-type": "application/zip",
            "content-length": String(size),
          };
          return jsonResponse({
            uploadUrl,
            method: "PUT",
            headers,
            downloadUrl,
            expiresIn: 900,
          });
        }

        // Normal mode: presigned PUT via R2 S3-compatible API
        if (
          !env.R2_ACCESS_KEY_ID ||
          !env.R2_SECRET_ACCESS_KEY ||
          !env.ACCOUNT_ID
        ) {
          return jsonResponse({ error: "not configured" }, 503);
        }

        const bucketName = env.BUCKET_NAME || "panvk-logs";
        const aws = new AwsClient({
          accessKeyId: env.R2_ACCESS_KEY_ID,
          secretAccessKey: env.R2_SECRET_ACCESS_KEY,
          service: "s3",
          region: "auto",
        });

        const uploadHeaders = {
          "x-amz-checksum-sha256": hexToBase64(sha256),
          "content-type": "application/zip",
          "content-length": String(size),
        };

        const r2Url = `https://${env.ACCOUNT_ID}.r2.cloudflarestorage.com/${bucketName}/${key}?X-Amz-Expires=900`;
        const signed = await aws.sign(r2Url, {
          method: "PUT",
          headers: uploadHeaders,
          aws: { signQuery: true, allHeaders: true },
        });

        return jsonResponse({
          uploadUrl: signed.url.toString(),
          method: "PUT",
          headers: uploadHeaders,
          downloadUrl,
          expiresIn: 900,
        });
      }

      // PUT /dev-put/<date>/<id> (only available when DEV_DIRECT === "1")
      const devPutMatch = pathname.match(/^\/dev-put\/([^/]+)\/([^/]+)$/);
      if (devPutMatch) {
        const isLocal =
          url.hostname === "127.0.0.1" || url.hostname === "localhost";
        if (request.method !== "PUT" || env.DEV_DIRECT !== "1" || !isLocal) {
          return jsonResponse({ error: "Not found" }, 404);
        }

        const [, date, id] = devPutMatch;
        if (!DATE_REGEX.test(date) || !UUID_REGEX.test(id)) {
          return jsonResponse({ error: "Not found" }, 404);
        }

        const devEntry = devKeys.get(id);
        if (!devEntry || Date.now() > devEntry.expires) {
          devKeys.delete(id);
          return jsonResponse({ error: "Forbidden" }, 403);
        }

        const contentLengthHeader = request.headers.get("content-length");
        const contentLength = contentLengthHeader
          ? parseInt(contentLengthHeader, 10)
          : NaN;
        if (isNaN(contentLength) || contentLength !== devEntry.size) {
          return jsonResponse({ error: "Content-Length mismatch" }, 400);
        }

        const bodyBuffer = await request.arrayBuffer();
        if (bodyBuffer.byteLength !== devEntry.size) {
          return jsonResponse({ error: "Payload size mismatch" }, 400);
        }

        const hashBuffer = await crypto.subtle.digest("SHA-256", bodyBuffer);
        const hashArray = Array.from(new Uint8Array(hashBuffer));
        const bodySha256 = hashArray
          .map((b) => b.toString(16).padStart(2, "0"))
          .join("");

        if (bodySha256 !== devEntry.sha256) {
          return jsonResponse({ error: "SHA-256 mismatch" }, 400);
        }

        devKeys.delete(id);

        const key = `uploads/${date}/${id}.zip`;
        const result = await env.BUCKET.put(key, bodyBuffer, {
          httpMetadata: { contentType: "application/zip" },
        });

        const size =
          result?.size ?? bodyBuffer.byteLength;
        return jsonResponse({ ok: true, size }, 200);
      }

      // GET /f/<yyyy-mm-dd>/<uuid>
      const getFileMatch = pathname.match(/^\/f\/([^/]+)\/([^/]+)$/);
      if (getFileMatch) {
        if (request.method !== "GET") {
          return jsonResponse({ error: "Not found" }, 404);
        }

        const [, date, id] = getFileMatch;
        if (!DATE_REGEX.test(date) || !UUID_REGEX.test(id)) {
          return jsonResponse({ error: "Not found" }, 404);
        }

        const key = `uploads/${date}/${id}.zip`;
        const object = await env.BUCKET.get(key);
        if (!object) {
          return jsonResponse({ error: "File not found" }, 404);
        }

        const headers = new Headers();
        object.writeHttpMetadata?.(headers);
        headers.set("content-type", "application/zip");
        headers.set(
          "content-disposition",
          `attachment; filename="panvk-${id}.zip"`
        );
        headers.set("cache-control", "private");
        if (object.httpEtag) {
          headers.set("etag", object.httpEtag);
        }
        if (object.size !== undefined) {
          headers.set("content-length", object.size.toString());
        }

        return new Response(object.body, {
          status: 200,
          headers,
        });
      }

      // Everything else 404
      return jsonResponse({ error: "Not found" }, 404);
    } catch (err) {
      return jsonResponse(
        { error: err.message || "Internal server error" },
        500
      );
    }
  },
};
