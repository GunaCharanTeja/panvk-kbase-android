CREATE TABLE IF NOT EXISTS uploads(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
  app TEXT,
  version TEXT,
  version_code INTEGER,
  sha256 TEXT NOT NULL UNIQUE,
  size INTEGER,
  catbox_url TEXT,
  gofile_url TEXT,
  r2_url TEXT,
  device_model TEXT,
  soc TEXT,
  gpu_model TEXT,
  gpu_id TEXT,
  arch TEXT,
  driver_name TEXT,
  driver_version TEXT,
  driver_so_sha256 TEXT,
  android_version TEXT,
  game TEXT,
  exit_code INTEGER,
  verified_a INTEGER NOT NULL DEFAULT 0,
  verified_b INTEGER NOT NULL DEFAULT 0,
  client_ip_hash TEXT,
  extra_json TEXT
);

CREATE INDEX IF NOT EXISTS idx_uploads_created_at ON uploads(created_at);
CREATE INDEX IF NOT EXISTS idx_uploads_app ON uploads(app);
