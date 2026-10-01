#!/bin/sh
# Build vkd3d-proton (master or $VKD3D_REF) natively for aarch64 glibc inside
# the Alpine chroot, with the d3d12 test suite enabled.
# Host: clone + pre-generate IDL headers with host widl (chroot has no widl).
# Device: build in $CHROOT/tmp/vkd3d/{src,build}; widl is a copy shim over
# the host-generated headers (widl output is target independent).
# Usage: VKD3D_WORK=/some/scratch/vkd3d scripts/vkd3d/build-vkd3d-proton.sh
set -eu

SERIAL="${ANDROID_SERIAL:-Y5WWBMJVOZSK4HU8}"
CHROOT="${VKD3D_CHROOT:-/data/local/tmp/chrootAlpine}"
W="${VKD3D_WORK:-${TMPDIR:-/tmp}/vkd3d}"
REF="${VKD3D_REF:-master}"
JOBS="${VKD3D_JOBS:-4}"
export ANDROID_SERIAL="$SERIAL"

mkdir -p "$W" && W=$(cd "$W" && pwd)
# ponytail: existing checkout is reused as-is; delete $W/src to pick a new VKD3D_REF.
if [ ! -d "$W/src/.git" ]; then
    git clone -q --recurse-submodules --shallow-submodules --depth 1 \
        --branch "$REF" https://github.com/HansKristian-Work/vkd3d-proton.git "$W/src"
fi
COMMIT=$(git -C "$W/src" rev-parse HEAD)
echo "vkd3d-proton $COMMIT"
git -C "$W/src" submodule status

rm -rf "$W/src/pregen-idl" && mkdir "$W/src/pregen-idl"
for idl in $(grep -oE "'vkd3d_[a-z0-9_]+\.idl'" "$W/src/include/meson.build" | tr -d "'"); do
    (cd "$W/src/include" && widl -h -o "$W/src/pregen-idl/${idl%.idl}.h" "$idl")
done
cat > "$W/src/pregen-idl/widl" <<'EOF'
#!/bin/sh
# widl shim: meson calls `widl -h -o OUT IN.idl`; copy host-generated header.
while [ $# -gt 0 ]; do case "$1" in -o) out=$2; shift 2;; -h) shift;; *) in=$1; shift;; esac; done
exec cp "$(dirname "$0")/$(basename "$in" .idl).h" "$out"
EOF
chmod 755 "$W/src/pregen-idl/widl"

tar czf "$W/src.tgz" -C "$W" --exclude=.git src
adb push "$W/src.tgz" /data/local/tmp/vkd3d-src.tgz >/dev/null
adb shell su -c "'rm -rf $CHROOT/tmp/vkd3d/src && mkdir -p $CHROOT/tmp/vkd3d && tar xzf /data/local/tmp/vkd3d-src.tgz -C $CHROOT/tmp/vkd3d && rm /data/local/tmp/vkd3d-src.tgz'"

adb shell su -c "'chroot $CHROOT /bin/sh -s'" <<EOF
set -eu
export PATH=/tmp/vkd3d/src/pregen-idl:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
test "\$(uname -m)" = aarch64
cd /tmp/vkd3d
[ -f build/build.ninja ] || meson setup build src --buildtype release -Denable_tests=true -Denable_extras=false > setup.log 2>&1 || { tail -20 setup.log; exit 1; }
nice ninja -j $JOBS -C build > build.log 2>&1 || { grep -E 'error|FAILED' build.log | head -20; exit 1; }
echo "$COMMIT" > build/COMMIT
sha256sum build/libs/d3d12/libvkd3d-proton-d3d12.so build/tests/d3d12
file build/tests/d3d12
EOF
echo BUILD_OK
