import assert from "node:assert";
import crypto from "node:crypto";

const BASE = process.env.BASE || "http://127.0.0.1:8787";
const ADMIN = process.env.ADMIN || "devtoken";

async function main() {
  console.log(`Running record & admin tests against: ${BASE}\n`);

  // 1. Valid insert 204
  const sha1 = crypto.randomBytes(32).toString("hex");
  {
    const res = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        version: "1.0.0",
        version_code: 42,
        sha256: sha1,
        size: 2048,
        catbox_url: "https://files.catbox.moe/test1.zip",
        device_model: "Pixel 8",
        verified_a: false,
      }),
    });
    assert.strictEqual(
      res.status,
      204,
      `Expected status 204 for valid insert, got ${res.status}`
    );
    const text = await res.text();
    assert.strictEqual(text, "", "Expected empty body for 204");
    console.log("PASS: Valid insert returned 204 with empty body");
  }

  // 2. Duplicate sha with new gofile_url and verified_a true then again verified_a false -> admin JSON shows both urls and verified_a 1
  {
    // First update: add gofile_url and verified_a true
    const resA = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        sha256: sha1,
        gofile_url: "https://gofile.io/d/abc123",
        verified_a: true,
      }),
    });
    assert.strictEqual(
      resA.status,
      204,
      `Expected status 204 for duplicate sha update, got ${resA.status}`
    );

    // Second update: verified_a false
    const resB = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        sha256: sha1,
        verified_a: false,
      }),
    });
    assert.strictEqual(
      resB.status,
      204,
      `Expected status 204 for second duplicate sha update, got ${resB.status}`
    );

    // Verify via admin JSON
    const adminRes = await fetch(`${BASE}/admin/uploads`, {
      headers: { Authorization: `Bearer ${ADMIN}` },
    });
    assert.strictEqual(
      adminRes.status,
      200,
      `Expected status 200 from /admin/uploads, got ${adminRes.status}`
    );
    const data = await adminRes.json();
    const row = (data.rows || []).find((r) => r.sha256 === sha1);
    assert.ok(row, "Expected inserted sha to be found in admin rows");
    assert.strictEqual(
      row.catbox_url,
      "https://files.catbox.moe/test1.zip",
      `Expected catbox_url preserved, got ${row.catbox_url}`
    );
    assert.strictEqual(
      row.gofile_url,
      "https://gofile.io/d/abc123",
      `Expected gofile_url merged, got ${row.gofile_url}`
    );
    assert.strictEqual(
      row.verified_a,
      1,
      `Expected verified_a to be 1, got ${row.verified_a}`
    );
    console.log(
      "PASS: Duplicate sha merged urls and retained verified_a = 1"
    );
  }

  // 3. Body > 4096 -> 413
  {
    const largeSha = crypto.randomBytes(32).toString("hex");
    const padding = "x".repeat(4000);
    const res = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        sha256: largeSha,
        extra_json: { pad: padding },
      }),
    });
    assert.strictEqual(
      res.status,
      413,
      `Expected status 413 for body > 4096 bytes, got ${res.status}`
    );
    console.log("PASS: Body > 4096 bytes rejected with 413");
  }

  // 4. catbox_url on evil host -> 400
  {
    const evilSha = crypto.randomBytes(32).toString("hex");
    const res = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        sha256: evilSha,
        catbox_url: "https://evil.catbox.moe.attacker.com/malicious.zip",
      }),
    });
    assert.strictEqual(
      res.status,
      400,
      `Expected status 400 for evil catbox_url, got ${res.status}`
    );
    console.log("PASS: catbox_url on evil host rejected with 400");
  }

  // 5. r2_url on other host -> 400
  {
    const otherSha = crypto.randomBytes(32).toString("hex");
    const res = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        sha256: otherSha,
        r2_url:
          "https://otherhost.com/f/2026-10-05/c43fa98e-4a60-449e-8c3b-741a4a159981",
      }),
    });
    assert.strictEqual(
      res.status,
      400,
      `Expected status 400 for r2_url on other host, got ${res.status}`
    );
    console.log("PASS: r2_url on other host rejected with 400");
  }

  // 6. Bad sha -> 400
  {
    const res = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        sha256: "not-a-valid-sha256-hex",
      }),
    });
    assert.strictEqual(
      res.status,
      400,
      `Expected status 400 for bad sha, got ${res.status}`
    );
    console.log("PASS: Bad sha rejected with 400");
  }

  // 7. /admin/uploads no token -> 401
  {
    const res = await fetch(`${BASE}/admin/uploads`);
    assert.strictEqual(
      res.status,
      401,
      `Expected status 401 for /admin/uploads without token, got ${res.status}`
    );
    assert.ok(
      res.headers.get("www-authenticate"),
      "Expected WWW-Authenticate header on 401"
    );
    console.log(
      "PASS: /admin/uploads without token rejected with 401 and WWW-Authenticate"
    );
  }

  // 8. Wrong token -> 401
  {
    const res = await fetch(`${BASE}/admin/uploads`, {
      headers: { Authorization: "Bearer wrong-token-12345" },
    });
    assert.strictEqual(
      res.status,
      401,
      `Expected status 401 for wrong token, got ${res.status}`
    );
    console.log("PASS: Wrong admin token rejected with 401");
  }

  // 9. Good token -> rows include sha
  {
    const res = await fetch(`${BASE}/admin/uploads`, {
      headers: { Authorization: `Bearer ${ADMIN}` },
    });
    assert.strictEqual(
      res.status,
      200,
      `Expected status 200 for good token, got ${res.status}`
    );
    const data = await res.json();
    assert.ok(Array.isArray(data.rows), "Expected rows array");
    const found = data.rows.some((r) => r.sha256 === sha1);
    assert.ok(found, "Expected rows to include inserted sha");
    console.log("PASS: Good admin token returned rows including the sha");
  }

  // 10. CSV has header line and the sha
  {
    const res = await fetch(`${BASE}/admin/uploads.csv`, {
      headers: { Authorization: `Bearer ${ADMIN}` },
    });
    assert.strictEqual(
      res.status,
      200,
      `Expected status 200 for /admin/uploads.csv, got ${res.status}`
    );
    const contentType = res.headers.get("content-type") || "";
    assert.ok(
      contentType.includes("text/csv"),
      `Expected text/csv content-type, got ${contentType}`
    );
    const csv = await res.text();
    const lines = csv.split(/\r?\n/).filter(Boolean);
    assert.ok(lines.length >= 2, "Expected at least header line and one row");
    assert.ok(
      lines[0].includes('"sha256"'),
      "Expected header line to contain sha256"
    );
    assert.ok(csv.includes(sha1), "Expected CSV content to contain the sha");
    console.log("PASS: CSV download returned header line and the sha");
  }

  // 11. /upload-url returns 503 storage not configured when R2 not bound
  {
    const testSha = crypto.randomBytes(32).toString("hex");
    const res = await fetch(`${BASE}/upload-url`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        version: "1.0.0",
        size: 1024,
        sha256: testSha,
      }),
    });
    assert.strictEqual(
      res.status,
      503,
      `Expected status 503 from /upload-url when R2 not bound, got ${res.status}`
    );
    const data = await res.json();
    assert.strictEqual(
      data.error,
      "storage not configured",
      `Expected error 'storage not configured', got '${data.error}'`
    );
    console.log(
      "PASS: /upload-url returned 503 'storage not configured' when R2 not bound"
    );
  }

  // 12. Daily cap test (if CAP_TEST=1)
  if (process.env.CAP_TEST === "1") {
    const cap = parseInt(process.env.DAILY_RECORD_CAP || "5", 10);
    console.log(`Running daily cap test with cap=${cap}...`);
    // Earlier tests already inserted rows today; fill up to the cap.
    for (let i = 0; i < cap; i++) {
      const s = crypto.randomBytes(32).toString("hex");
      const r = await fetch(`${BASE}/record`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({
          app: "panprobe",
          version: "1.0.0",
          sha256: s,
        }),
      });
      if (r.status === 429) break;
      assert.strictEqual(r.status, 204, `Expected 204/429, got ${r.status}`);
    }
    // Merge of an existing sha must still pass when capped.
    const mergeRes = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ app: "panprobe", sha256: sha1, verified_b: true }),
    });
    assert.strictEqual(mergeRes.status, 204, `Expected merge 204 past cap, got ${mergeRes.status}`);
    const overSha = crypto.randomBytes(32).toString("hex");
    const overRes = await fetch(`${BASE}/record`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        app: "panprobe",
        version: "1.0.0",
        sha256: overSha,
      }),
    });
    assert.strictEqual(
      overRes.status,
      429,
      `Expected status 429 when daily cap exceeded, got ${overRes.status}`
    );
    const overData = await overRes.json();
    assert.strictEqual(
      overData.error,
      "daily cap reached",
      `Expected error 'daily cap reached', got '${overData.error}'`
    );
    console.log("PASS: Daily cap enforced with 429 'daily cap reached'");
  } else {
    console.log("SKIP: Daily cap test (set CAP_TEST=1 to run)");
  }

  console.log("\nAll record and admin tests passed successfully.");
}

main().catch((err) => {
  console.error(`\nFAIL: ${err.message}`);
  process.exit(1);
});
