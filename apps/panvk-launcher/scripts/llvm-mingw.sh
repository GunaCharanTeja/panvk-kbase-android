# Sourced by build scripts: fetch + verify pinned llvm-mingw (mstorsjo/llvm-mingw), put it on PATH.
LLVM_MINGW_NAME=llvm-mingw-$LLVM_MINGW_VER-ucrt-ubuntu-22.04-x86_64
LLVM_MINGW_DIR=$WORK/$LLVM_MINGW_NAME
if [ ! -x "$LLVM_MINGW_DIR/bin/arm64ec-w64-mingw32-clang" ]; then
    tarball=$WORK/$LLVM_MINGW_NAME.tar.xz
    if [ ! -f "$tarball" ]; then
        curl -fL -o "$tarball.part" \
            "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VER/$LLVM_MINGW_NAME.tar.xz"
        mv "$tarball.part" "$tarball"
    fi
    echo "$LLVM_MINGW_SHA256  $tarball" | sha256sum -c -
    tar -C "$WORK" -xJf "$tarball"
fi
PATH=$LLVM_MINGW_DIR/bin:$PATH
# Compiler temp files stay with the build (a caller's TMPDIR may vanish mid-build; /tmp is small tmpfs).
TMPDIR=$WORK/tmp
mkdir -p "$TMPDIR"
# lld stamps PE headers with the build time unless SOURCE_DATE_EPOCH is set: pin it for bit-identical DLLs.
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1}
export PATH TMPDIR SOURCE_DATE_EPOCH
