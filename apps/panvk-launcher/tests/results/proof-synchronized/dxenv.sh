#!/system/bin/sh
# Launcher Containers.env() replica (graphics run, DISPLAY=:0), ICD = m4wsi build.
# usage (as launcher UID): sh dxenv.sh <layer:0|1> <wine args...>
F=/data/user/0/dev.zenithblue.panvklauncher/files
I=$F/contents/imagefs/bionic
C=$F/container
W=$F/contents/Proton/11.0-2-arm64ec
LAYER=$1; shift
export WINEPREFIX=$C/.wine HOME=$C TMPDIR=$I/usr/tmp
export PATH=$W/bin:$I/usr/bin:/system/bin
export LD_LIBRARY_PATH=$I/usr/lib:/system/lib64:$W/lib
export WINEDLLOVERRIDES="mscoree,mshtml=d;d3d8,d3d9,d3d10core,d3d11,dxgi=n,b"
export WINEDEBUG=${WINEDEBUG_OVR:-fixme-all} LC_ALL=en_US.UTF-8 FONTCONFIG_PATH=$I/etc/fonts XDG_DATA_DIRS=$I/usr/share USER=xuser
export VK_ICD_FILENAMES=$F/m4wsi/icd.json VK_DRIVER_FILES=$F/m4wsi/icd.json
export MESA_LOG=file DXVK_LOG_LEVEL=info DISPLAY=:0 XKB_CONFIG_ROOT=$I/usr/share/X11/xkb
# unset -> libandroid-sysvshm strncpy NULL deref -> CreateWindowEx returns 0xC0000005
export ANDROID_SYSVSHM_SERVER=${ANDROID_SYSVSHM_SERVER:-/dev/null}
[ -f $C/.wine/drive_c/windows/system32/libwow64fex.dll ] && export HODLL=libwow64fex.dll
# Evidence harness: do not inherit explicit/injected layers or their output paths.
unset VK_INSTANCE_LAYERS VK_LAYER_PATH VK_ADD_LAYER_PATH VK_LOADER_LAYERS_ENABLE VK_LOADER_LAYERS_ALLOW PANVK_FB_PATH
export VK_LOADER_LAYERS_DISABLE='~all~'
if [ "$LAYER" = 1 ]; then
  unset VK_LOADER_LAYERS_DISABLE
  export VK_LAYER_PATH=$C/vklayers VK_INSTANCE_LAYERS=VK_LAYER_panvk_fbread PANVK_FB_PATH=$C/fb.bin
fi
[ "$LAYER" = native ] && exec "$@"
[ "$1" = env ] && exec env
exec $W/lib/wine/aarch64-unix/wine "$@"
