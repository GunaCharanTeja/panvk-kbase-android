import { AwsClient } from "aws4fetch";

const ALLOWED_APPS = new Set(["panprobe", "panplay"]);
const SHA256_REGEX = /^[0-9a-f]{64}$/;
const DATE_REGEX = /^\d{4}-\d{2}-\d{2}$/;
const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const R2_PATH_REGEX = /^\/f\/\d{4}-\d{2}-\d{2}\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const BLOB_PATH_REGEX = /^\/blob\/[0-9a-f]{64}$/;
const SINCE_REGEX = /^\d{4}-\d{2}-\d{2}(T[\d:]{8}Z)?$/;
const MAX_UPLOAD_SIZE = 200 * 1024 * 1024; // 200 MB
const MAX_BLOB_SIZE = 25 * 1024 * 1024;
const devKeys = new Map();

const UPLOAD_COLUMNS = [
  "id",
  "created_at",
  "app",
  "version",
  "version_code",
  "sha256",
  "size",
  "catbox_url",
  "gofile_url",
  "r2_url",
  "device_model",
  "soc",
  "gpu_model",
  "gpu_id",
  "arch",
  "driver_name",
  "driver_version",
  "driver_so_sha256",
  "android_version",
  "game",
  "exit_code",
  "verified_a",
  "verified_b",
  "client_ip_hash",
  "extra_json",
];

const ALLOWED_RECORD_KEYS = new Set([
  "app",
  "version",
  "version_code",
  "sha256",
  "size",
  "catbox_url",
  "gofile_url",
  "r2_url",
  "device_model",
  "soc",
  "gpu_model",
  "gpu_id",
  "arch",
  "driver_name",
  "driver_version",
  "driver_so_sha256",
  "android_version",
  "game",
  "exit_code",
  "verified_a",
  "verified_b",
  "extra_json",
]);

const METADATA_STRINGS = [
  "device_model",
  "soc",
  "gpu_model",
  "gpu_id",
  "arch",
  "driver_name",
  "driver_version",
  "android_version",
  "game",
];

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

async function authenticateAdmin(request, env) {
  if (!env.ADMIN_TOKEN) {
    return false;
  }
  const authHeader = request.headers.get("authorization");
  if (!authHeader || !authHeader.startsWith("Bearer ")) {
    return false;
  }
  const token = authHeader.slice(7);
  if (!token) {
    return false;
  }

  const enc = new TextEncoder();
  const tokenHash = await crypto.subtle.digest("SHA-256", enc.encode(token));
  const expectedHash = await crypto.subtle.digest(
    "SHA-256",
    enc.encode(env.ADMIN_TOKEN)
  );

  const a = new Uint8Array(tokenHash);
  const b = new Uint8Array(expectedHash);

  if (crypto.subtle && typeof crypto.subtle.timingSafeEqual === "function") {
    try {
      return crypto.subtle.timingSafeEqual(a, b);
    } catch {
      // Fall through to manual loop if timingSafeEqual fails
    }
  }

  let diff = a.length ^ b.length;
  for (let i = 0; i < a.length; i++) {
    diff |= a[i] ^ b[i];
  }
  return diff === 0;
}

function unauthorizedResponse() {
  return new Response(JSON.stringify({ error: "unauthorized" }), {
    status: 401,
    headers: {
      "content-type": "application/json",
      "cache-control": "no-store",
      "WWW-Authenticate": "Bearer",
    },
  });
}

function parseAdminFilters(url) {
  const params = url.searchParams;
  let limit = 100;
  let app = null;
  let since = null;

  for (const key of params.keys()) {
    if (key !== "limit" && key !== "app" && key !== "since") {
      return { error: `Unknown parameter: ${key}` };
    }
  }

  if (params.has("limit")) {
    const rawLimit = params.get("limit");
    if (rawLimit !== "") {
      if (!/^\d+$/.test(rawLimit)) {
        return { error: "Invalid limit" };
      }
      limit = parseInt(rawLimit, 10);
      if (limit < 1 || limit > 1000) {
        return { error: "Limit must be between 1 and 1000" };
      }
    }
  }

  if (params.has("app")) {
    const rawApp = params.get("app");
    if (rawApp !== "") {
      if (rawApp !== "panprobe" && rawApp !== "panplay") {
        return { error: "Invalid app" };
      }
      app = rawApp;
    }
  }

  if (params.has("since")) {
    const rawSince = params.get("since");
    if (rawSince !== "") {
      if (!SINCE_REGEX.test(rawSince)) {
        return { error: "Invalid since" };
      }
      since = rawSince;
    }
  }

  return { limit, app, since };
}

function formatCsvCell(val) {
  if (val === null || val === undefined) {
    return '""';
  }
  let str = String(val);
  if (/^[\s\x00-\x1f]*[=+\-@\t\r\n]/.test(str)) {
    str = "'" + str;
  }
  return `"${str.replace(/"/g, '""')}"`;
}

function renderAdminHtml() {
  const nonce = crypto.randomUUID();
  const html = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>panvk Uploads Admin</title>
  <style>
    body { font-family: system-ui, -apple-system, sans-serif; margin: 20px; background: #fafafa; color: #333; }
    h1 { margin-top: 0; font-size: 1.5rem; }
    .controls { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 16px; align-items: center; }
    input, select, button { padding: 6px 10px; font-size: 14px; border: 1px solid #ccc; border-radius: 4px; }
    button { cursor: pointer; background: #0066cc; color: white; border: none; font-weight: 500; }
    button:hover { background: #0052a3; }
    #status { margin-bottom: 12px; font-size: 14px; min-height: 20px; }
    .error { color: #d32f2f; }
    .success { color: #2e7d32; }
    .table-container { overflow-x: auto; background: white; border: 1px solid #ddd; border-radius: 4px; }
    table { border-collapse: collapse; width: 100%; font-size: 13px; text-align: left; }
    th, td { border: 1px solid #eee; padding: 8px 10px; white-space: nowrap; }
    th { background: #f0f0f0; font-weight: 600; position: sticky; top: 0; }
    tr:nth-child(even) { background: #fdfdfd; }
    a { color: #0066cc; text-decoration: none; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <h1>panvk Uploads Admin</h1>
  <div class="controls">
    <input type="password" id="adminToken" placeholder="Admin Token" size="24">
    <select id="appSelect">
      <option value="">All Apps</option>
      <option value="panprobe">panprobe</option>
      <option value="panplay">panplay</option>
    </select>
    <input type="text" id="sinceInput" placeholder="Since (YYYY-MM-DD)" size="20">
    <input type="number" id="limitInput" value="100" min="1" max="1000" style="width: 70px;">
    <button type="button" id="loadBtn">Load</button>
    <button type="button" id="csvBtn">Download CSV</button>
  </div>
  <div id="status"></div>
  <div class="table-container">
    <table>
      <thead>
        <tr id="headerRow"></tr>
      </thead>
      <tbody id="tableBody"></tbody>
    </table>
  </div>
  <script nonce="${nonce}">
    (function() {
      var tokenInput = document.getElementById("adminToken");
      var appSelect = document.getElementById("appSelect");
      var sinceInput = document.getElementById("sinceInput");
      var limitInput = document.getElementById("limitInput");
      var loadBtn = document.getElementById("loadBtn");
      var csvBtn = document.getElementById("csvBtn");
      var statusDiv = document.getElementById("status");
      var headerRow = document.getElementById("headerRow");
      var tableBody = document.getElementById("tableBody");

      var COLUMNS = [
        "id", "created_at", "app", "version", "version_code", "sha256", "size",
        "catbox_url", "gofile_url", "r2_url", "device_model", "soc", "gpu_model",
        "gpu_id", "arch", "driver_name", "driver_version", "driver_so_sha256",
        "android_version", "game", "exit_code", "verified_a", "verified_b",
        "client_ip_hash", "extra_json"
      ];

      for (var i = 0; i < COLUMNS.length; i++) {
        var th = document.createElement("th");
        th.textContent = COLUMNS[i];
        headerRow.appendChild(th);
      }

      tokenInput.value = sessionStorage.getItem("admin_token") || "";
      tokenInput.addEventListener("input", function() {
        sessionStorage.setItem("admin_token", tokenInput.value);
      });

      function buildQuery() {
        var params = new URLSearchParams();
        if (appSelect.value) params.set("app", appSelect.value);
        if (sinceInput.value.trim()) params.set("since", sinceInput.value.trim());
        if (limitInput.value.trim()) params.set("limit", limitInput.value.trim());
        return params.toString();
      }

      function setStatus(msg, isError) {
        statusDiv.textContent = msg;
        statusDiv.className = isError ? "error" : "success";
      }

      loadBtn.addEventListener("click", async function() {
        var token = tokenInput.value.trim();
        if (!token) {
          setStatus("Please enter an admin token", true);
          return;
        }
        setStatus("Loading...", false);
        var q = buildQuery();
        var url = "/admin/uploads" + (q ? "?" + q : "");
        try {
          var res = await fetch(url, {
            headers: { "Authorization": "Bearer " + token }
          });
          if (!res.ok) {
            var err = await res.json().catch(function() { return {}; });
            setStatus("Error " + res.status + ": " + (err.error || res.statusText), true);
            return;
          }
          var data = await res.json();
          tableBody.textContent = "";
          var rows = data.rows || [];
          var origin = window.location.origin;
          for (var r = 0; r < rows.length; r++) {
            var row = rows[r];
            var tr = document.createElement("tr");
            for (var c = 0; c < COLUMNS.length; c++) {
              var col = COLUMNS[c];
              var td = document.createElement("td");
              var val = row[col];
              if (val !== null && val !== undefined) {
                var s = String(val);
                if (s.startsWith("https://") || s.startsWith(origin)) {
                  var a = document.createElement("a");
                  a.href = s;
                  a.textContent = s;
                  a.target = "_blank";
                  a.rel = "noopener noreferrer";
                  td.appendChild(a);
                } else {
                  td.textContent = s;
                }
              } else {
                td.textContent = "";
              }
              tr.appendChild(td);
            }
            tableBody.appendChild(tr);
          }
          setStatus("Loaded " + rows.length + " rows", false);
        } catch (e) {
          setStatus("Network error: " + e.message, true);
        }
      });

      csvBtn.addEventListener("click", async function() {
        var token = tokenInput.value.trim();
        if (!token) {
          setStatus("Please enter an admin token", true);
          return;
        }
        setStatus("Downloading CSV...", false);
        var q = buildQuery();
        var url = "/admin/uploads.csv" + (q ? "?" + q : "");
        try {
          var res = await fetch(url, {
            headers: { "Authorization": "Bearer " + token }
          });
          if (!res.ok) {
            setStatus("Error downloading CSV: " + res.status, true);
            return;
          }
          var blob = await res.blob();
          var objUrl = URL.createObjectURL(blob);
          var a = document.createElement("a");
          a.href = objUrl;
          a.download = "panvk-uploads.csv";
          document.body.appendChild(a);
          a.click();
          document.body.removeChild(a);
          URL.revokeObjectURL(objUrl);
          setStatus("CSV downloaded successfully", false);
        } catch (e) {
          setStatus("Download error: " + e.message, true);
        }
      });
    })();
  </script>
</body>
</html>`;

  return new Response(html, {
    status: 200,
    headers: {
      "content-type": "text/html; charset=utf-8",
      "content-security-policy":
        `default-src 'none'; script-src 'nonce-${nonce}'; style-src 'unsafe-inline'; connect-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'`,
      "x-content-type-options": "nosniff",
      "referrer-policy": "no-referrer",
      "cache-control": "no-store",
    },
  });
}

export default {
  async fetch(request, env) {
    try {
      const url = new URL(request.url);
      const { pathname, origin } = url;

      // GET /admin
      if (pathname === "/admin" && request.method === "GET") {
        return renderAdminHtml();
      }

      // GET /admin/uploads
      if (pathname === "/admin/uploads" && request.method === "GET") {
        const isAuth = await authenticateAdmin(request, env);
        if (!isAuth) {
          return unauthorizedResponse();
        }
        if (!env.DB) {
          return jsonResponse({ error: "database not configured" }, 503, {
            "cache-control": "no-store",
          });
        }

        const filterResult = parseAdminFilters(url);
        if (filterResult.error) {
          return jsonResponse({ error: filterResult.error }, 400, {
            "cache-control": "no-store",
          });
        }

        const { limit, app, since } = filterResult;
        let query = "SELECT * FROM uploads";
        const conditions = [];
        const params = [];

        if (app) {
          conditions.push("app = ?");
          params.push(app);
        }
        if (since) {
          conditions.push("created_at >= ?");
          params.push(since);
        }
        if (conditions.length > 0) {
          query += " WHERE " + conditions.join(" AND ");
        }
        query += " ORDER BY id DESC LIMIT ?";
        params.push(limit);

        const result = await env.DB.prepare(query).bind(...params).all();
        const rows = result.results || [];
        return jsonResponse({ rows }, 200, {
          "cache-control": "no-store",
        });
      }

      // GET /admin/uploads.csv
      if (pathname === "/admin/uploads.csv" && request.method === "GET") {
        const isAuth = await authenticateAdmin(request, env);
        if (!isAuth) {
          return unauthorizedResponse();
        }
        if (!env.DB) {
          return jsonResponse({ error: "database not configured" }, 503, {
            "cache-control": "no-store",
          });
        }

        const filterResult = parseAdminFilters(url);
        if (filterResult.error) {
          return jsonResponse({ error: filterResult.error }, 400, {
            "cache-control": "no-store",
          });
        }

        const { limit, app, since } = filterResult;
        let query = "SELECT * FROM uploads";
        const conditions = [];
        const params = [];

        if (app) {
          conditions.push("app = ?");
          params.push(app);
        }
        if (since) {
          conditions.push("created_at >= ?");
          params.push(since);
        }
        if (conditions.length > 0) {
          query += " WHERE " + conditions.join(" AND ");
        }
        query += " ORDER BY id DESC LIMIT ?";
        params.push(limit);

        const result = await env.DB.prepare(query).bind(...params).all();
        const rows = result.results || [];

        const headerLine = UPLOAD_COLUMNS.map((col) => `"${col}"`).join(",");
        const csvLines = [headerLine];
        for (const row of rows) {
          const rowLine = UPLOAD_COLUMNS.map((col) =>
            formatCsvCell(row[col])
          ).join(",");
          csvLines.push(rowLine);
        }
        const csvBody = csvLines.join("\r\n") + "\r\n";

        return new Response(csvBody, {
          status: 200,
          headers: {
            "content-type": "text/csv; charset=utf-8",
            "content-disposition": 'attachment; filename="panvk-uploads.csv"',
            "cache-control": "no-store",
          },
        });
      }

      // POST /record
      if (pathname === "/record" && request.method === "POST") {
        if (!env.DB) {
          return jsonResponse({ error: "database not configured" }, 503);
        }

        // Rate limiting
        const devLocal =
          env.DEV_DIRECT === "1" &&
          (url.hostname === "127.0.0.1" || url.hostname === "localhost");
        if (!devLocal) {
          if (!env.RECORD_LIMIT) {
            return jsonResponse({ error: "rate limiter not configured" }, 503);
          }
          const clientIp =
            request.headers.get("cf-connecting-ip") || "unknown";
          const { success } = await env.RECORD_LIMIT.limit({ key: clientIp });
          if (!success) {
            return jsonResponse({ error: "Rate limit exceeded" }, 429);
          }
        }

        const contentLengthHeader = request.headers.get("content-length");
        if (!contentLengthHeader) {
          return jsonResponse({ error: "Missing Content-Length" }, 413);
        }
        const reqContentLength = Number(contentLengthHeader);
        if (
          !/^\d+$/.test(contentLengthHeader) ||
          !Number.isSafeInteger(reqContentLength) ||
          reqContentLength < 0 ||
          reqContentLength > 4096
        ) {
          return jsonResponse({ error: "Payload too large" }, 413);
        }

        const bytes = new Uint8Array(4096);
        let byteLength = 0;
        const reader = request.body?.getReader();
        if (reader) {
          try {
            while (true) {
              const { value, done } = await reader.read();
              if (done) break;
              if (byteLength + value.byteLength > bytes.byteLength) {
                reader.cancel().catch(() => {});
                return jsonResponse({ error: "Payload too large" }, 413);
              }
              bytes.set(value, byteLength);
              byteLength += value.byteLength;
            }
          } finally {
            reader.releaseLock();
          }
        }

        let body;
        try {
          body = JSON.parse(
            new TextDecoder("utf-8", { fatal: true }).decode(bytes.subarray(0, byteLength))
          );
        } catch {
          return jsonResponse({ error: "Invalid JSON body" }, 400);
        }

        if (
          !body ||
          typeof body !== "object" ||
          Array.isArray(body) ||
          Object.prototype.toString.call(body) !== "[object Object]"
        ) {
          return jsonResponse({ error: "JSON must be a plain object" }, 400);
        }

        // Reject unknown top-level keys
        for (const key of Object.keys(body)) {
          if (!ALLOWED_RECORD_KEYS.has(key)) {
            return jsonResponse({ error: `Unknown field: ${key}` }, 400);
          }
        }

        // Treat null as absent
        for (const key of Object.keys(body)) {
          if (body[key] === null) {
            delete body[key];
          }
        }

        // Validate app (required)
        if (
          typeof body.app !== "string" ||
          !ALLOWED_APPS.has(body.app)
        ) {
          return jsonResponse(
            { error: "Invalid app. Must be 'panprobe' or 'panplay'" },
            400
          );
        }

        // Validate sha256 (required)
        if (
          typeof body.sha256 !== "string" ||
          !SHA256_REGEX.test(body.sha256)
        ) {
          return jsonResponse(
            { error: "Invalid sha256. Must be a 64-character lowercase hex string" },
            400
          );
        }

        // Validate version (optional)
        if (
          body.version !== undefined &&
          (typeof body.version !== "string" || body.version.length > 32)
        ) {
          return jsonResponse(
            { error: "Invalid version. Must be string up to 32 characters" },
            400
          );
        }

        // Validate version_code (optional)
        if (
          body.version_code !== undefined &&
          (typeof body.version_code !== "number" ||
            !Number.isInteger(body.version_code) ||
            body.version_code < 0 ||
            body.version_code > 2147483648)
        ) {
          return jsonResponse(
            { error: "Invalid version_code. Must be integer between 0 and 2^31" },
            400
          );
        }

        // Validate size (optional)
        if (
          body.size !== undefined &&
          (typeof body.size !== "number" ||
            !Number.isInteger(body.size) ||
            body.size < 1 ||
            body.size > MAX_UPLOAD_SIZE)
        ) {
          return jsonResponse(
            { error: `Invalid size. Must be integer between 1 and ${MAX_UPLOAD_SIZE}` },
            400
          );
        }

        // Validate driver_so_sha256 (optional)
        if (
          body.driver_so_sha256 !== undefined &&
          (typeof body.driver_so_sha256 !== "string" ||
            !SHA256_REGEX.test(body.driver_so_sha256))
        ) {
          return jsonResponse(
            { error: "Invalid driver_so_sha256. Must be 64 hex characters" },
            400
          );
        }

        // Validate metadata strings (optional)
        for (const key of METADATA_STRINGS) {
          if (body[key] !== undefined) {
            if (
              typeof body[key] !== "string" ||
              body[key].length > 128 ||
              /[\x00-\x1f\x7f]/.test(body[key])
            ) {
              return jsonResponse(
                { error: `Invalid ${key}. Must be string up to 128 chars with no control characters` },
                400
              );
            }
          }
        }

        // Validate exit_code (optional)
        if (
          body.exit_code !== undefined &&
          (typeof body.exit_code !== "number" ||
            !Number.isInteger(body.exit_code) ||
            body.exit_code < -2147483648 ||
            body.exit_code > 2147483648)
        ) {
          return jsonResponse(
            { error: "Invalid exit_code. Must be integer between -2^31 and 2^31" },
            400
          );
        }

        // Validate verified_a / verified_b (optional)
        if (
          body.verified_a !== undefined &&
          typeof body.verified_a !== "boolean"
        ) {
          return jsonResponse({ error: "Invalid verified_a. Must be boolean" }, 400);
        }
        if (
          body.verified_b !== undefined &&
          typeof body.verified_b !== "boolean"
        ) {
          return jsonResponse({ error: "Invalid verified_b. Must be boolean" }, 400);
        }

        // Validate extra_json (optional)
        let extraJsonStr = null;
        if (body.extra_json !== undefined) {
          if (typeof body.extra_json === "string") {
            if (body.extra_json.length > 2048) {
              return jsonResponse(
                { error: "extra_json string exceeds 2048 characters" },
                400
              );
            }
            extraJsonStr = body.extra_json;
          } else if (
            typeof body.extra_json === "object" &&
            body.extra_json !== null
          ) {
            try {
              extraJsonStr = JSON.stringify(body.extra_json);
            } catch {
              return jsonResponse({ error: "Invalid extra_json" }, 400);
            }
            if (extraJsonStr.length > 2048) {
              return jsonResponse(
                { error: "extra_json stringified exceeds 2048 characters" },
                400
              );
            }
          } else {
            return jsonResponse(
              { error: "Invalid extra_json. Must be object or string" },
              400
            );
          }
        }

        // Validate URLs (optional)
        if (body.catbox_url !== undefined) {
          if (
            typeof body.catbox_url !== "string" ||
            body.catbox_url.length > 256
          ) {
            return jsonResponse({ error: "Invalid catbox_url length" }, 400);
          }
          let parsed;
          try {
            parsed = new URL(body.catbox_url);
          } catch {
            return jsonResponse({ error: "Invalid catbox_url format" }, 400);
          }
          if (parsed.username || parsed.password) {
            return jsonResponse(
              { error: "catbox_url must not contain credentials" },
              400
            );
          }
          if (
            parsed.protocol !== "https:" ||
            parsed.hostname !== "files.catbox.moe"
          ) {
            return jsonResponse(
              { error: "catbox_url must be https and files.catbox.moe" },
              400
            );
          }
        }

        if (body.gofile_url !== undefined) {
          if (
            typeof body.gofile_url !== "string" ||
            body.gofile_url.length > 256
          ) {
            return jsonResponse({ error: "Invalid gofile_url length" }, 400);
          }
          let parsed;
          try {
            parsed = new URL(body.gofile_url);
          } catch {
            return jsonResponse({ error: "Invalid gofile_url format" }, 400);
          }
          if (parsed.username || parsed.password) {
            return jsonResponse(
              { error: "gofile_url must not contain credentials" },
              400
            );
          }
          if (
            parsed.protocol !== "https:" ||
            parsed.hostname !== "gofile.io"
          ) {
            return jsonResponse(
              { error: "gofile_url must be https and on gofile.io" },
              400
            );
          }
        }

        if (body.r2_url !== undefined) {
          if (typeof body.r2_url !== "string" || body.r2_url.length > 256) {
            return jsonResponse({ error: "Invalid r2_url length" }, 400);
          }
          let parsed;
          try {
            parsed = new URL(body.r2_url);
          } catch {
            return jsonResponse({ error: "Invalid r2_url format" }, 400);
          }
          if (parsed.username || parsed.password) {
            return jsonResponse(
              { error: "r2_url must not contain credentials" },
              400
            );
          }
          if (parsed.origin !== origin) {
            return jsonResponse(
              { error: "r2_url origin must match request origin" },
              400
            );
          }
          if (!R2_PATH_REGEX.test(parsed.pathname) && !BLOB_PATH_REGEX.test(parsed.pathname)) {
            return jsonResponse(
              { error: "r2_url pathname must match /f/<date>/<uuid> or /blob/<sha256>" },
              400
            );
          }
        }

        // client_ip_hash
        let clientIpHash = null;
        if (env.IP_SALT) {
          const clientIp =
            request.headers.get("cf-connecting-ip") || "";
          const ipData = new TextEncoder().encode(`${env.IP_SALT}|${clientIp}`);
          const hashBuffer = await crypto.subtle.digest("SHA-256", ipData);
          clientIpHash = Array.from(new Uint8Array(hashBuffer))
            .map((b) => b.toString(16).padStart(2, "0"))
            .join("");
        }

        // Validate the cap; enforce it atomically in the upsert below.
        const dailyCap = Number(env.DAILY_RECORD_CAP ?? "2000");
        if (!Number.isSafeInteger(dailyCap) || dailyCap < 0) {
          return jsonResponse({ error: "invalid daily cap configuration" }, 503);
        }

        // Upsert
        const verifiedA = body.verified_a ? 1 : 0;
        const verifiedB = body.verified_b ? 1 : 0;

        const result = await env.DB.prepare(`
          INSERT INTO uploads (
            app, version, version_code, sha256, size,
            catbox_url, gofile_url, r2_url,
            device_model, soc, gpu_model, gpu_id, arch,
            driver_name, driver_version, driver_so_sha256,
            android_version, game, exit_code,
            verified_a, verified_b, client_ip_hash, extra_json
          ) SELECT
            ?, ?, ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?, ?
          WHERE EXISTS(SELECT 1 FROM uploads WHERE sha256 = ?)
            OR (SELECT COUNT(*) FROM uploads WHERE created_at >= strftime('%Y-%m-%dT00:00:00Z','now')) < ?
          ON CONFLICT(sha256) DO UPDATE SET
            app = COALESCE(excluded.app, uploads.app),
            version = COALESCE(excluded.version, uploads.version),
            version_code = COALESCE(excluded.version_code, uploads.version_code),
            size = COALESCE(excluded.size, uploads.size),
            catbox_url = COALESCE(excluded.catbox_url, uploads.catbox_url),
            gofile_url = COALESCE(excluded.gofile_url, uploads.gofile_url),
            r2_url = COALESCE(excluded.r2_url, uploads.r2_url),
            device_model = COALESCE(excluded.device_model, uploads.device_model),
            soc = COALESCE(excluded.soc, uploads.soc),
            gpu_model = COALESCE(excluded.gpu_model, uploads.gpu_model),
            gpu_id = COALESCE(excluded.gpu_id, uploads.gpu_id),
            arch = COALESCE(excluded.arch, uploads.arch),
            driver_name = COALESCE(excluded.driver_name, uploads.driver_name),
            driver_version = COALESCE(excluded.driver_version, uploads.driver_version),
            driver_so_sha256 = COALESCE(excluded.driver_so_sha256, uploads.driver_so_sha256),
            android_version = COALESCE(excluded.android_version, uploads.android_version),
            game = COALESCE(excluded.game, uploads.game),
            exit_code = COALESCE(excluded.exit_code, uploads.exit_code),
            verified_a = MAX(uploads.verified_a, excluded.verified_a),
            verified_b = MAX(uploads.verified_b, excluded.verified_b),
            extra_json = COALESCE(excluded.extra_json, uploads.extra_json)
        `).bind(
          body.app !== undefined ? body.app : null,
          body.version !== undefined ? body.version : null,
          body.version_code !== undefined ? body.version_code : null,
          body.sha256,
          body.size !== undefined ? body.size : null,
          body.catbox_url !== undefined ? body.catbox_url : null,
          body.gofile_url !== undefined ? body.gofile_url : null,
          body.r2_url !== undefined ? body.r2_url : null,
          body.device_model !== undefined ? body.device_model : null,
          body.soc !== undefined ? body.soc : null,
          body.gpu_model !== undefined ? body.gpu_model : null,
          body.gpu_id !== undefined ? body.gpu_id : null,
          body.arch !== undefined ? body.arch : null,
          body.driver_name !== undefined ? body.driver_name : null,
          body.driver_version !== undefined ? body.driver_version : null,
          body.driver_so_sha256 !== undefined ? body.driver_so_sha256 : null,
          body.android_version !== undefined ? body.android_version : null,
          body.game !== undefined ? body.game : null,
          body.exit_code !== undefined ? body.exit_code : null,
          verifiedA,
          verifiedB,
          clientIpHash,
          extraJsonStr,
          body.sha256,
          dailyCap
        ).run();

        if (result.meta.changes === 0) {
          return jsonResponse({ error: "daily cap reached" }, 429);
        }

        return new Response(null, { status: 204 });
      }

      // POST /upload-url
      if (pathname === "/upload-url" && request.method === "POST") {
        // Rate limiting
        // Local dev (DEV_DIRECT on 127.0.0.1) skips the limiter so smoke tests can loop.
        const devLocal =
          env.DEV_DIRECT === "1" &&
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

        const r2Configured = env.BUCKET && (
          env.DEV_DIRECT === "1" ||
          (env.R2_ACCESS_KEY_ID && env.R2_SECRET_ACCESS_KEY && env.ACCOUNT_ID)
        );
        if (!r2Configured && env.BLOBS) {
          if (size > MAX_BLOB_SIZE) {
            return jsonResponse({ error: "too big for project storage" }, 413);
          }
          const blobUrl = `${origin}/blob/${sha256}`;
          return jsonResponse({
            uploadUrl: blobUrl,
            method: "PUT",
            headers: { "content-type": "application/zip" },
            downloadUrl: blobUrl,
            expiresIn: 900,
          });
        }

        const id = crypto.randomUUID();
        const date = new Date().toISOString().slice(0, 10);
        const key = `uploads/${date}/${id}.zip`;
        const downloadUrl = `${origin}/f/${date}/${id}`;

        // Local development direct upload mode
        if (env.DEV_DIRECT === "1") {
          if (!env.BUCKET) {
            return jsonResponse({ error: "storage not configured" }, 503);
          }
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
          !env.BUCKET ||
          !env.R2_ACCESS_KEY_ID ||
          !env.R2_SECRET_ACCESS_KEY ||
          !env.ACCOUNT_ID
        ) {
          return jsonResponse({ error: "storage not configured" }, 503);
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

      // PUT /blob/<sha256> and GET /blob/<sha256>
      if (BLOB_PATH_REGEX.test(pathname)) {
        const sha = pathname.slice("/blob/".length);
        const key = `blob/${sha}`;
        if (request.method === "PUT") {
          if (!env.BLOBS) {
            return jsonResponse({ error: "storage not configured" }, 503);
          }
          const devLocal = env.DEV_DIRECT === "1" &&
            (url.hostname === "127.0.0.1" || url.hostname === "localhost");
          if (env.UPLOAD_LIMIT && !devLocal) {
            const clientIp = request.headers.get("cf-connecting-ip") || "unknown";
            const { success } = await env.UPLOAD_LIMIT.limit({ key: clientIp });
            if (!success) {
              return jsonResponse({ error: "Rate limit exceeded" }, 429);
            }
          }

          const lengthHeader = request.headers.get("content-length");
          if (lengthHeader === null) {
            return jsonResponse({ error: "Missing Content-Length" }, 411);
          }
          const size = Number(lengthHeader);
          if (!/^\d+$/.test(lengthHeader) || !Number.isSafeInteger(size) ||
              size < 1 || size > MAX_BLOB_SIZE) {
            return jsonResponse({ error: "too big for project storage" }, 413);
          }

          const bytes = new Uint8Array(size);
          let byteLength = 0;
          const reader = request.body?.getReader();
          if (reader) {
            try {
              while (true) {
                const { value, done } = await reader.read();
                if (done) break;
                if (byteLength + value.byteLength > size) {
                  reader.cancel().catch(() => {});
                  return jsonResponse({ error: "Payload size mismatch" }, 413);
                }
                bytes.set(value, byteLength);
                byteLength += value.byteLength;
              }
            } finally {
              reader.releaseLock();
            }
          }
          if (byteLength !== size) {
            return jsonResponse({ error: "Payload size mismatch" }, 413);
          }
          const body = bytes.buffer;
          const hash = await crypto.subtle.digest("SHA-256", body);
          const bodySha = Array.from(new Uint8Array(hash))
            .map((b) => b.toString(16).padStart(2, "0")).join("");
          if (bodySha !== sha) {
            return jsonResponse({ error: "sha256 mismatch" }, 400);
          }

          const existing = await env.BLOBS.get(key, { type: "stream" });
          if (existing) {
            await existing.cancel();
            return jsonResponse({ ok: true, existing: true });
          }
          if (env.DB) {
            const cap = Number(env.DAILY_BLOB_CAP ?? "900");
            if (!Number.isSafeInteger(cap) || cap < 0) {
              return jsonResponse({ error: "invalid daily cap configuration" }, 503);
            }
            if (cap === 0) {
              return jsonResponse({ error: "daily cap reached" }, 429);
            }
            const counter = await env.DB.prepare(
              "INSERT INTO counters(day,n) VALUES(?,1) ON CONFLICT(day) DO UPDATE SET n=n+1 WHERE n < ? RETURNING n"
            ).bind(new Date().toISOString().slice(0, 10), cap).first();
            if (!counter) {
              return jsonResponse({ error: "daily cap reached" }, 429);
            }
          } else if (!devLocal) {
            return jsonResponse({ error: "database not configured" }, 503);
          }
          await env.BLOBS.put(key, body, {
            expirationTtl: 30 * 24 * 3600,
            metadata: { size, ct: "application/zip" },
          });
          return jsonResponse({ ok: true, size });
        }
        if (request.method === "GET" && env.BLOBS) {
          const body = await env.BLOBS.get(key, { type: "stream" });
          if (body) {
            return new Response(body, {
              headers: {
                "content-type": "application/zip",
                "content-disposition": `attachment; filename="panvk-${sha.slice(0, 12)}.zip"`,
                "cache-control": "private",
                "x-content-type-options": "nosniff",
              },
            });
          }
        }
        return jsonResponse({ error: "Not found" }, 404);
      }

      // PUT /dev-put/<date>/<id> (only available when DEV_DIRECT === "1")
      const devPutMatch = pathname.match(/^\/dev-put\/([^/]+)\/([^/]+)$/);
      if (devPutMatch) {
        if (!env.BUCKET) {
          return jsonResponse({ error: "storage not configured" }, 503);
        }
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

        const size = result?.size ?? bodyBuffer.byteLength;
        return jsonResponse({ ok: true, size }, 200);
      }

      // GET /f/<yyyy-mm-dd>/<uuid>
      const getFileMatch = pathname.match(/^\/f\/([^/]+)\/([^/]+)$/);
      if (getFileMatch) {
        if (request.method !== "GET") {
          return jsonResponse({ error: "Not found" }, 404);
        }
        if (!env.BUCKET) {
          return jsonResponse({ error: "storage not configured" }, 503);
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
      return jsonResponse({ error: "Internal server error" }, 500);
    }
  },
};
