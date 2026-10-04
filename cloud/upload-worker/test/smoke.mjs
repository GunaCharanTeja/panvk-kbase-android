import assert from "node:assert";
import crypto from "node:crypto";

const BASE = process.env.BASE || "http://127.0.0.1:8787";

async function main() {
  console.log(`Running smoke tests against: ${BASE}\n`);

  const dummySha =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

  // 1. Validation: Bad app
  {
    const res = await fetch(`${BASE}/upload-url`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "invalid_app",
        version: "1.0.0",
        size: 1024,
        sha256: dummySha,
      }),
    });
    assert.strictEqual(
      res.status,
      400,
      `Expected status 400 for bad app, got ${res.status}`
    );
    console.log("PASS: Bad app rejected with 400");
  }

  // 2. Validation: Bad sha
  {
    const res = await fetch(`${BASE}/upload-url`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        version: "1.0.0",
        size: 1024,
        sha256: "not-a-valid-sha256-hex",
      }),
    });
    assert.strictEqual(
      res.status,
      400,
      `Expected status 400 for bad sha256, got ${res.status}`
    );
    console.log("PASS: Bad sha256 rejected with 400");
  }

  // 3. Validation: Size too big (> 200MB)
  {
    const res = await fetch(`${BASE}/upload-url`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        version: "1.0.0",
        size: 200 * 1024 * 1024 + 1,
        sha256: dummySha,
      }),
    });
    assert.strictEqual(
      res.status,
      400,
      `Expected status 400 for size exceeding 200MB, got ${res.status}`
    );
    console.log("PASS: Oversized payload rejected with 400");
  }

  // 4. Dev mode roundtrip: Create 4KB buffer, upload via returned PUT URL, download & verify sha256
  {
    const buffer = crypto.randomBytes(4096);
    const sha256 = crypto.createHash("sha256").update(buffer).digest("hex");

    const postRes = await fetch(`${BASE}/upload-url`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        version: "1.0.0",
        size: buffer.length,
        sha256,
      }),
    });
    assert.strictEqual(
      postRes.status,
      200,
      `Expected 200 from POST /upload-url, got ${postRes.status}`
    );

    const postData = await postRes.json();
    assert.ok(postData.uploadUrl, "Expected uploadUrl in response");
    assert.strictEqual(postData.method, "PUT", "Expected method PUT");
    assert.ok(postData.headers, "Expected headers in response");
    assert.ok(postData.downloadUrl, "Expected downloadUrl in response");
    assert.strictEqual(postData.expiresIn, 900, "Expected expiresIn 900");
    console.log("PASS: POST /upload-url returned valid uploadUrl and downloadUrl");

    // PUT body with returned headers
    const putRes = await fetch(postData.uploadUrl, {
      method: "PUT",
      headers: postData.headers,
      body: buffer,
    });
    assert.strictEqual(
      putRes.status,
      200,
      `Expected 200 from PUT upload, got ${putRes.status}`
    );
    console.log("PASS: PUT body with returned headers succeeded with 200");

    // GET downloadUrl and compare sha256
    const getRes = await fetch(postData.downloadUrl);
    assert.strictEqual(
      getRes.status,
      200,
      `Expected 200 from GET downloadUrl, got ${getRes.status}`
    );

    const downloadedBytes = Buffer.from(await getRes.arrayBuffer());
    const downloadedSha = crypto
      .createHash("sha256")
      .update(downloadedBytes)
      .digest("hex");
    assert.strictEqual(
      downloadedSha,
      sha256,
      `SHA256 mismatch: expected ${sha256}, got ${downloadedSha}`
    );
    console.log("PASS: GET downloadUrl returned matching sha256");
  }

  // 5. 404 test: Non-existent file
  {
    const randomUuid = crypto.randomUUID();
    const res = await fetch(`${BASE}/f/2026-01-01/${randomUuid}`);
    assert.strictEqual(
      res.status,
      404,
      `Expected 404 for non-existent file, got ${res.status}`
    );
    console.log("PASS: GET /f/2026-01-01/<random-uuid> returned 404");
  }

  // 6. 404 test: Root route
  {
    const res = await fetch(`${BASE}/`);
    assert.strictEqual(
      res.status,
      404,
      `Expected 404 for GET /, got ${res.status}`
    );
    console.log("PASS: GET / returned 404");
  }

  console.log("\nAll smoke tests passed successfully.");
}

main().catch((err) => {
  console.error(`\nFAIL: ${err.message}`);
  process.exit(1);
});
