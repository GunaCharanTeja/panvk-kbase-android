# panvk-upload

Minimal Cloudflare Worker providing presigned upload URLs and download endpoints for `panvk` diagnostic logs and crash reports (`panprobe`, `panplay`).

## What It Does

- Generates short-lived presigned R2 PUT URLs so client apps can upload diagnostic ZIP archives directly to Cloudflare R2 without bundling storage credentials.
- Binds uploads to a cryptographic checksum (SHA-256) and limits payloads up to 200 MB.
- Serves uploaded archives for analysis via unguessable download links.
- Supports a local development direct-upload mode (`DEV_DIRECT=1`) for testing without Cloudflare R2 credentials.

## API Endpoints

### 1. `POST /upload-url`
Request an upload URL.

- **Request Body (JSON):**
  ```json
  {
    "app": "panprobe",
    "version": "1.0.0",
    "size": 4096,
    "sha256": "4a5b6c... (64 hex characters)"
  }
  ```
  - `app`: string, must be `"panprobe"` or `"panplay"`
  - `version`: string, 1 to 32 characters
  - `size`: integer between `1` and `209715200` (200 MB)
  - `sha256`: 64 lowercase hex characters
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
  - `503 Service Unavailable`: Worker missing credentials / `ACCOUNT_ID`.

### 2. `PUT <uploadUrl>`
Upload the ZIP payload directly to R2 (or to `/dev-put/:date/:id` in local dev mode). The client must include the exact headers returned in the `POST /upload-url` response.

### 3. `GET /f/<yyyy-mm-dd>/<uuid>`
Download the uploaded archive.
- Returns `200 OK` with `Content-Type: application/zip`, `Content-Disposition: attachment; filename="panvk-<uuid>.zip"`, and `Cache-Control: private`.
- Returns `404 Not Found` if the file does not exist or parameters fail validation.

*Note: All other paths return `404 Not Found`. There is no directory listing endpoint.*

## Security Notes

- **No secrets in client applications**: Desktop/mobile apps never bundle R2 tokens or credentials. They request single-use presigned URLs on demand.
- **Presigned for 15 minutes**: Presigned PUT URLs expire strictly after 900 seconds (`expiresIn: 900`).
- **Checksum-bound**: Presigned URLs enforce `x-amz-checksum-sha256`, ensuring R2 rejects any body whose hash differs and preventing size tampering.
- **Per-IP rate limit**: Workers rate limiting binding (`UPLOAD_LIMIT`) allows 2 requests/minute per IP (approximating ~10/hour to thwart abuse while staying within Cloudflare's 10s/60s binding period options).
- **No listing endpoint**: Bucket enumeration is disabled. Objects are unlisted.
- **Unguessable UUID links**: Download URLs use cryptographically generated UUIDv4 tokens (`crypto.randomUUID()`), accessible only to those possessing the exact link.
- **Automatic 30-day lifecycle expiration**: Objects are permanently deleted from R2 after 30 days via bucket lifecycle rules.

## Deployment Steps

1. **Create Cloudflare Account**: Sign up at [cloudflare.com](https://dash.cloudflare.com/) if you do not already have an account.
2. **Enable R2**: In the Cloudflare dashboard, go to **R2** and enable the service (requires adding a payment method; free tier includes 10 GB/month storage).
3. **Install Dependencies & Login**: In your terminal within `upload-worker/`:
   ```bash
   npm install
   npx wrangler login
   ```
4. **Create R2 Bucket**:
   ```bash
   npx wrangler r2 bucket create panvk-logs
   ```
5. **Configure Bucket Lifecycle**: Auto-expire uploaded logs after 30 days:
   ```bash
   npx wrangler r2 bucket lifecycle add panvk-logs expire-30d uploads/ --expire-days 30
   ```
6. **Create R2 API Token & Configure Secrets**:
   - In Cloudflare Dashboard, go to **R2 > Manage R2 API Tokens**.
   - Create a token with **Object Read & Write** permissions scoped specifically to `panvk-logs`.
   - Store the token credentials securely as Worker secrets:
     ```bash
     npx wrangler secret put R2_ACCESS_KEY_ID
     npx wrangler secret put R2_SECRET_ACCESS_KEY
     ```
   - Copy your Cloudflare Account ID (visible in dashboard or R2 overview) and set it in `wrangler.toml`:
     ```toml
     [vars]
     ACCOUNT_ID = "<your-account-id>"
     ```
7. **Deploy the Worker**:
   ```bash
   npx wrangler deploy
   ```
8. **Configure Client Apps**:
   Copy the deployed `https://panvk-upload.<subdomain>.workers.dev` URL into `PANVK_UPLOAD_ENDPOINT` in `panprobe` and `panplay`, then rebuild both applications.

## Local Testing

You can test the Worker locally without creating an R2 API token using `DEV_DIRECT=1`:

1. Create a `.dev.vars` file in `upload-worker/`:
   ```ini
   DEV_DIRECT=1
   ```
2. Start the local Worker:
   ```bash
   npx wrangler dev
   ```
3. Run the automated smoke test suite in a separate terminal:
   ```bash
   node test/smoke.mjs
   ```
