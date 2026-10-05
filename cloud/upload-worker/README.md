# panvk-upload

Minimal Cloudflare Worker providing presigned upload URLs, download endpoints, and a D1-backed upload metadata log for `panvk` diagnostic logs and crash reports (`panprobe`, `panplay`).

## What It Does

- **D1 Upload Metadata Log**: Stores diagnostic records and mirror links (Catbox, Gofile, R2) with daily ingestion caps, per-IP rate limiting, and upsert deduplication keyed on SHA-256.
- **Admin Dashboard & Export**: Provides `/admin` web UI, authenticated JSON query endpoint, and formula-safe CSV export (`/admin/uploads.csv`).
- **Optional Direct R2 Storage**: Generates short-lived presigned R2 PUT URLs so client apps can upload diagnostic ZIP archives directly to Cloudflare R2 without bundling storage credentials.
- **Local Development Support**: Supports local D1 SQLite database and direct upload mode (`DEV_DIRECT=1`) for testing without Cloudflare credentials.

---

## API Endpoints

### 1. `POST /record`
Record or update diagnostic upload metadata in Cloudflare D1.

- **Request Body (JSON, max 4096 bytes):**
  ```json
  {
    "app": "panprobe",
    "version": "1.0.0",
    "version_code": 42,
    "sha256": "4a5b6c... (64 hex characters)",
    "size": 1048576,
    "catbox_url": "https://files.catbox.moe/abc123.zip",
    "gofile_url": "https://gofile.io/d/xyz789",
    "r2_url": "https://<worker-origin>/f/2026-10-05/<uuid>",
    "device_model": "Pixel 8",
    "soc": "Tensor G3",
    "gpu_model": "Mali-G715",
    "gpu_id": "0x1234",
    "arch": "arm64-v8a",
    "driver_name": "Mali",
    "driver_version": "v1.3",
    "driver_so_sha256": "0123... (64 hex characters)",
    "android_version": "14",
    "game": "quake",
    "exit_code": 0,
    "verified_a": false,
    "verified_b": false,
    "extra_json": { "custom": "data" }
  }
  ```

#### Field Schema
- `app` (required): `"panprobe"` or `"panplay"`.
- `sha256` (required): 64-character lowercase hex SHA-256 digest of the archive.
- `version` (optional): String up to 32 characters.
- `version_code` (optional): Integer from `0` to `2^31`.
- `size` (optional): Integer byte size between `1` and `209715200` (200 MB).
- `catbox_url` (optional): HTTPS URL up to 256 chars; hostname must be exactly `files.catbox.moe`.
- `gofile_url` (optional): HTTPS URL up to 256 chars; hostname must be `gofile.io` or `*.gofile.io`.
- `r2_url` (optional): URL up to 256 chars whose origin matches the worker request origin and pathname matches `^/f/\d{4}-\d{2}-\d{2}/<uuid>$`.
- `device_model`, `soc`, `gpu_model`, `gpu_id`, `arch`, `driver_name`, `driver_version`, `android_version`, `game` (optional): Strings up to 128 characters, no control characters.
- `driver_so_sha256` (optional): 64-character lowercase hex string.
- `exit_code` (optional): Integer in `-2^31..2^31`.
- `verified_a`, `verified_b` (optional): Booleans.
- `extra_json` (optional): Object or string whose JSON string length is `<= 2048`.
- Unknown top-level keys are rejected with `400 Bad Request`. `null` values are treated as absent.
- **Upsert Semantics**: On conflicting `sha256`, nullable metadata columns are merged via `COALESCE(excluded.col, uploads.col)`, `verified_a`/`verified_b` are merged with `MAX(uploads.verified_x, excluded.verified_x)`, while `created_at` and `client_ip_hash` remain unchanged.
- **Response**: `204 No Content` on success.
- **Status Codes**: `400` (validation failure), `413` (body > 4096 bytes), `429` (per-IP rate limit or `daily cap reached`), `503` (D1 database not configured).

---

### 2. Admin Endpoints

All admin endpoints require `ADMIN_TOKEN` configured. The token is checked using constant-time comparison.

- **`GET /admin`**: Static single-page dashboard. Does not require authentication to load the page itself (contains no server-side data). Prompts for token (stored only in browser `sessionStorage`), allows filtering by app, since date, and limit, with inline table rendering and CSV download.
- **`GET /admin/uploads?limit=&app=&since=`**:
  - Requires `Authorization: Bearer <token>`.
  - Filters: `limit` (int 1..1000, default 100), `app` (`panprobe` or `panplay`), `since` (format `YYYY-MM-DD` or `YYYY-MM-DDTHH:MM:SSZ`).
  - Returns `200 OK` JSON `{ rows: [...] }` ordered by `id DESC`.
- **`GET /admin/uploads.csv?limit=&app=&since=`**:
  - Requires `Authorization: Bearer <token>`.
  - Returns RFC4180-compliant CSV with header row and all columns.
  - Formula injection safe: cells starting with `=`, `+`, `-`, `@`, `\t`, `\r` are prefixed with a `'`.
  - Header: `Content-Disposition: attachment; filename="panvk-uploads.csv"`.
- All admin responses return `Cache-Control: no-store`.

---

### 3. `POST /upload-url` (Optional R2 Storage)
Request a presigned R2 upload URL.
- **Request Body (JSON):**
  ```json
  {
    "app": "panprobe",
    "version": "1.0.0",
    "size": 4096,
    "sha256": "4a5b6c... (64 hex characters)"
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "uploadUrl": "https://<account_id>.r2.cloudflarestorage.com/panvk-logs/uploads/2026-10-04/<uuid>.zip?...",
    "method": "PUT",
    "headers": {
      "x-amz-checksum-sha256": "<base64-hash>",
      "content-type": "application/zip"
    },
    "downloadUrl": "https://panvk-upload.<subdomain>.workers.dev/f/2026-10-04/<uuid>",
    "expiresIn": 900
  }
  ```
- **Error Codes:**
  - `400 Bad Request`: Validation failure.
  - `429 Too Many Requests`: Rate limit exceeded.
  - `503 Service Unavailable`: Returns `{ "error": "storage not configured" }` if R2 binding is not configured.

### 4. `PUT <uploadUrl>`
Upload the ZIP payload directly to R2 (or to `/dev-put/:date/:id` in local dev mode).

### 5. `GET /f/<yyyy-mm-dd>/<uuid>`
Download the uploaded archive from R2.
- Returns `200 OK` with `Content-Type: application/zip`.
- Returns `404 Not Found` if file does not exist.
- Returns `503 Service Unavailable` (`{ "error": "storage not configured" }`) if R2 is not configured.

---

## D1 Upload Log

### Deployment Steps

1. **Log in to Cloudflare**:
   ```bash
   npx wrangler login
   ```
2. **Create D1 Database**:
   ```bash
   npx wrangler d1 create panvk-uploads
   ```
3. **Configure `wrangler.toml`**:
   Put the `database_id` output by `wrangler d1 create` into `wrangler.toml`:
   ```toml
   [[d1_databases]]
   binding = "DB"
   database_name = "panvk-uploads"
   database_id = "<your-database-id>"
   ```
4. **Initialize Database Schema**:
   Apply `schema.sql` to your remote database:
   ```bash
   npx wrangler d1 execute panvk-uploads --remote --file schema.sql
   ```
5. **Set Worker Secrets**:
   Generate an admin token and set secrets:
   ```bash
   # Generate a 32-byte hex secret
   openssl rand -hex 32

   # Store secrets on Cloudflare Worker
   npx wrangler secret put ADMIN_TOKEN
   npx wrangler secret put IP_SALT
   ```
6. **(Optional) Configure R2 Storage**:
   R2 requires a card on file with Cloudflare. If you want R2 direct uploads:
   - Create bucket:
     ```bash
     npx wrangler r2 bucket create panvk-logs
     ```
   - Uncomment `[[r2_buckets]]` in `wrangler.toml`.
   - Set `ACCOUNT_ID` in `wrangler.toml` under `[vars]`.
   - Create an R2 API token (Object Read & Write scoped to `panvk-logs`) and store secrets:
     ```bash
     npx wrangler secret put R2_ACCESS_KEY_ID
     npx wrangler secret put R2_SECRET_ACCESS_KEY
     ```
7. **Deploy the Worker**:
   ```bash
   npx wrangler deploy
   ```
8. **Configure Client Applications**:
   Set `PANVK_UPLOAD_ENDPOINT` in both apps' build configurations (`panprobe` and `panplay`) to your deployed worker URL (`https://<worker>.<subdomain>.workers.dev`), then rebuild both applications.

> [!NOTE]
> **No token in repo/APK**: `ADMIN_TOKEN` and `IP_SALT` are secrets stored only in the Cloudflare Worker environment. Client applications (APK / desktop builds) never bundle admin tokens or storage credentials.

---

### Viewing Uploads

- **Web Dashboard**: Open `https://<worker>/admin` in your browser, paste your `ADMIN_TOKEN`, and click **Load** or **Download CSV**.
- **Cloudflare Dashboard**: Use the Cloudflare D1 console under **Workers & Pages > D1 > panvk-uploads > Explore Database**.
- **Wrangler SQL Export**:
  ```bash
  npx wrangler d1 export panvk-uploads --remote --output uploads.sql
  ```
- **CURL CSV Export**:
  ```bash
  curl -H "Authorization: Bearer $ADMIN_TOKEN" https://<worker>/admin/uploads.csv -o panvk-uploads.csv
  ```

---

### Local Development

1. **Initialize Local D1 Database**:
   ```bash
   npx wrangler d1 execute panvk-uploads --local --file schema.sql
   ```
2. **Start Local Worker**:
   ```bash
   npx wrangler dev --local --var DEV_DIRECT:1 --var ADMIN_TOKEN:devtoken
   ```
3. **Run Automated Test Suites**:
   In another terminal:
   ```bash
   # Run R2 / upload-url smoke tests
   node test/smoke.mjs

   # Run D1 record and admin test suite
   node test/record.mjs
   ```
   To test daily ingestion cap enforcement:
   ```bash
   # Start worker with a small daily cap:
   npx wrangler dev --local --var DEV_DIRECT:1 --var ADMIN_TOKEN:devtoken --var DAILY_RECORD_CAP:5

   # Run cap test
   CAP_TEST=1 DAILY_RECORD_CAP=5 node test/record.mjs
   ```

---

## Security Notes

- **No secrets in client applications**: Desktop/mobile apps never bundle R2 tokens or credentials. They request single-use presigned URLs or submit public crash reports to `/record`.
- **IP Privacy**: Raw client IPs are never stored in the database. When `IP_SALT` is configured, client IPs are hashed with SHA-256 (`hex(sha256(IP_SALT + "|" + cf-connecting-ip))`).
- **Timing-safe Authentication**: Admin token checks use SHA-256 digests compared via `crypto.subtle.timingSafeEqual` with a constant-time XOR fallback.
- **Spreadsheet Formula Injection Defused**: CSV export escapes characters `=`, `+`, `-`, `@`, `\t`, and `\r` with a leading apostrophe (`'`).
- **Strict Content-Security-Policy**: `/admin` enforces a restrictive CSP (`default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'`) and renders data solely via DOM text nodes to prevent XSS.
- **Per-IP Rate Limiting & Daily Cap**: Rate limiting bindings protect `/upload-url` and `/record`. A daily cap query enforces maximum new records per day while permitting metadata merges for existing archives.
