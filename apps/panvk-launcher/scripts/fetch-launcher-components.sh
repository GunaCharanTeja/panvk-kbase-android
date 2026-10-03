#!/bin/sh
# Download the prebuilt components listed with a "url" in app/bundled-components.json (rootfs, Proton)
# into $OUT and verify their pinned sha256. FEX/DXVK come from build-fex-windows.sh / build-dxvk.sh.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${OUT:-/var/tmp/panvk/components}
mkdir -p "$OUT"
python3 -c '
import json, sys
for c in json.load(open(sys.argv[1]))["components"]:
    if c.get("url"): print(c["file"], c["sha256"], c["url"])
' "$HERE/../app/bundled-components.json" | while read -r file sha url; do
    f=$OUT/$file
    if ! echo "$sha  $f" | sha256sum -c --status - 2>/dev/null; then
        curl -fL -o "$f.part" "$url"
        mv "$f.part" "$f"
        echo "$sha  $f" | sha256sum -c -
    fi
    echo "ok $file"
done
