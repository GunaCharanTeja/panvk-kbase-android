#!/bin/sh
# DX5 device validate: overlay tracked 018 (PANVK_DEBUG gate + 256KiB arena),
# ninja -j2, prove vkCreateDevice on default IDVS and PANVK_DEBUG=gpu_prerast,
# then run the vertical-slice matrix. Public feature bits stay false.
# Runs inside the Alpine glibc chroot. No kbase_diag/trace. ninja -j2 only.
set -eu

HERE="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
LOG="${DX5_LOG:-/tmp/dx5-validate.log}"
MESA="${DX5_MESA:-/tmp/mesa}"
BUILD="${DX5_BUILD:-/tmp/build-glibc}"
ICD="$BUILD/src/panfrost/vulkan/libvulkan_panfrost.so"
OVERLAY="$HERE/overlay"
HARNESS="$HERE/harness"
: > "$LOG"
exec >>"$LOG" 2>&1

log() { printf '%s\n' "$*"; }

abort() {
   log "BLOCKER: $*"
   log "STATUS=BLOCKED"
   exit 1
}

transport_guard() {
   # chroot cannot see adb; surface USB/kernel clues once, no poll.
   if [ -r /proc/sys/kernel/printk ]; then
      :
   fi
}

log "DX5_DEVICE_VALIDATE begin"
log "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
log "uname=$(uname -a)"
log "HERE=$HERE"

test "$(uname -m)" = aarch64 || abort "not aarch64"
test -d "$MESA/src/panfrost/vulkan" || abort "missing mesa tree $MESA"
test -f "$BUILD/build.ninja" || abort "missing build.ninja $BUILD"
test -d "$OVERLAY/src/panfrost/vulkan" || abort "missing overlay $OVERLAY"
test -f "$OVERLAY/src/panfrost/vulkan/panvk_gpu_prerast.h" || abort "overlay missing panvk_gpu_prerast.h"
test -f "$OVERLAY/src/panfrost/vulkan/panvk_vX_device.c" || abort "overlay missing panvk_vX_device.c"

export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export MALI_NO_MALI=0

log "=== overlay current 018 ==="
# Current mtime. Do not stamp 1970-01-01.
find "$OVERLAY" -type f | while IFS= read -r src; do
   rel="${src#$OVERLAY/}"
   dest="$MESA/$rel"
   mkdir -p "$(dirname "$dest")"
   cp -f "$src" "$dest"
   touch "$dest"
   log "OVERLAY $rel"
done

h="$MESA/src/panfrost/vulkan/panvk_gpu_prerast.h"
grep -q 'PANVK_GPU_PRERAST_ARENA_SIZE (256ull \* 1024)' "$h" \
   || abort "arena not 256KiB after overlay"
grep -q 'if (PANVK_DEBUG(GPU_PRERAST))' \
   "$MESA/src/panfrost/vulkan/panvk_vX_device.c" \
   || abort "arena not gated on PANVK_DEBUG after overlay"
log "OVERLAY_ARENA=$(grep PANVK_GPU_PRERAST_ARENA_SIZE "$h")"
log "OVERLAY_GATE=PANVK_DEBUG(GPU_PRERAST)"

phys="$MESA/src/panfrost/vulkan/panvk_vX_physical_device.c"
if [ -f "$phys" ]; then
   for feat in geometryShader fillModeNonSolid shaderClipDistance shaderCullDistance tessellationShader; do
      grep -q "\.${feat} = false" "$phys" || abort "unexpected $feat exposure source"
   done
   log "EXPOSURE_SOURCE=false"
fi

log "=== force rebuild changed files ==="
# Delete objects for overlayed C files so ninja cannot skip on stale .o.
# panvk_vX_*.c compiles as panvk_v11_*.c.o.
find "$BUILD" \( \
   -name '*gpu_prerast*.o' -o \
   -name '*panvk_v11_device*.o' -o \
   -name '*panvk_v11_cmd_draw*.o' -o \
   -name '*panvk_v11_shader*.o' -o \
   -name '*pan_nir_lower_vs_inputs*.o' -o \
   -name '*bifrost_compile*.o' -o \
   -name '*panvk_instance*.o' \
\) -print -delete | while IFS= read -r p; do
   log "RM_OBJ $p"
done

ninja_once() {
   ninja -j2 -C "$BUILD" src/panfrost/vulkan/libvulkan_panfrost.so
}

log "=== ninja -j2 ==="
set +e
ninja_once
nr=$?
set -e
if [ "$nr" -ne 0 ]; then
   log "NINJA first fail rc=$nr; one bounded retry after dropping overlay depfiles"
   find "$BUILD" \( \
      -name '*gpu_prerast*.d' -o \
      -name '*panvk_v11_device*.d' -o \
      -name '*panvk_v11_cmd_draw*.d' -o \
      -name '*panvk_v11_shader*.d' -o \
      -name '*pan_nir_lower_vs_inputs*.d' -o \
      -name '*bifrost_compile*.d' -o \
      -name '*panvk_instance*.d' \
   \) -delete
   set +e
   ninja_once
   nr=$?
   set -e
   [ "$nr" -eq 0 ] || abort "ninja -j2 failed after one retry rc=$nr"
   log "NINJA retry PASS"
else
   log "NINJA PASS"
fi

test -f "$ICD" || abort "ICD missing $ICD"
ICD_SHA="$(sha256sum "$ICD" | awk '{print $1}')"
ICD_SIZE="$(wc -c < "$ICD" | tr -d ' ')"
log "ICD_PATH=$ICD"
log "ICD_SIZE=$ICD_SIZE"
log "ICD_SHA256=$ICD_SHA"
file "$ICD" | grep -q 'ARM aarch64' || abort "ICD not AArch64"
strings "$ICD" | grep -q gpu_prerast && log "ICD_STRINGS=gpu_prerast" || log "ICD_STRINGS=no_gpu_prerast"

log "=== compile probes ==="
PROBE=/tmp/dx5-createdevice.c
cat > "$PROBE" <<'C'
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
typedef PFN_vkVoidFunction (*icd_gipa_fn)(VkInstance, const char *);
int main(int argc, char **argv) {
   const char *dbg = getenv("PANVK_DEBUG");
   printf("PROBE_DEBUG=%s\n", dbg ? dbg : "");
   if (argc < 2) { printf("FAIL usage\n"); return 2; }
   setvbuf(stdout, NULL, _IONBF, 0);
   void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   if (!h) { printf("FAIL dlopen %s\n", dlerror()); return 1; }
   icd_gipa_fn gipa = (icd_gipa_fn)dlsym(h, "vk_icdGetInstanceProcAddr");
   if (!gipa) { printf("FAIL gipa\n"); return 1; }
   PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
   if (!vkCreateInstance) { printf("FAIL CreateInstance pfn\n"); return 1; }
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app};
   VkInstance inst;
   VkResult r = vkCreateInstance(&ici, NULL, &inst);
   if (r != VK_SUCCESS) { printf("FAIL CreateInstance r=%d\n", r); return 1; }
   PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices =
      (PFN_vkEnumeratePhysicalDevices)gipa(inst, "vkEnumeratePhysicalDevices");
   PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties =
      (PFN_vkGetPhysicalDeviceProperties)gipa(inst, "vkGetPhysicalDeviceProperties");
   PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures =
      (PFN_vkGetPhysicalDeviceFeatures)gipa(inst, "vkGetPhysicalDeviceFeatures");
   PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties =
      (PFN_vkGetPhysicalDeviceQueueFamilyProperties)gipa(inst, "vkGetPhysicalDeviceQueueFamilyProperties");
   PFN_vkCreateDevice vkCreateDevice =
      (PFN_vkCreateDevice)gipa(inst, "vkCreateDevice");
   PFN_vkDestroyDevice vkDestroyDevice =
      (PFN_vkDestroyDevice)gipa(inst, "vkDestroyDevice");
   PFN_vkDestroyInstance vkDestroyInstance =
      (PFN_vkDestroyInstance)gipa(inst, "vkDestroyInstance");
   uint32_t n = 0;
   r = vkEnumeratePhysicalDevices(inst, &n, NULL);
   if (r != VK_SUCCESS || !n) { printf("FAIL enum n=%u r=%d\n", n, r); return 1; }
   VkPhysicalDevice *devs = malloc(n * sizeof(*devs));
   r = vkEnumeratePhysicalDevices(inst, &n, devs);
   if (r != VK_SUCCESS) { printf("FAIL enum2 r=%d\n", r); return 1; }
   VkPhysicalDevice phys = VK_NULL_HANDLE;
   VkPhysicalDeviceProperties props;
   for (uint32_t i = 0; i < n; i++) {
      vkGetPhysicalDeviceProperties(devs[i], &props);
      if (strstr(props.deviceName, "Mali")) { phys = devs[i]; break; }
   }
   free(devs);
   if (phys == VK_NULL_HANDLE) { printf("FAIL no Mali\n"); return 1; }
   vkGetPhysicalDeviceProperties(phys, &props);
   VkPhysicalDeviceFeatures feats;
   vkGetPhysicalDeviceFeatures(phys, &feats);
   printf("ICD device=%s id=0x%x api=%u.%u.%u\n", props.deviceName,
          props.deviceID, VK_API_VERSION_MAJOR(props.apiVersion),
          VK_API_VERSION_MINOR(props.apiVersion),
          VK_API_VERSION_PATCH(props.apiVersion));
   printf("EXPOSURE geometryShader=%d fillModeNonSolid=%d shaderClipDistance=%d shaderCullDistance=%d tessellationShader=%d\n",
          feats.geometryShader, feats.fillModeNonSolid, feats.shaderClipDistance,
          feats.shaderCullDistance, feats.tessellationShader);
   if (feats.geometryShader || feats.fillModeNonSolid ||
       feats.shaderClipDistance || feats.shaderCullDistance ||
       feats.tessellationShader) {
      printf("FAIL unexpected feature exposure\n");
      return 1;
   }
   uint32_t qn = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, NULL);
   VkQueueFamilyProperties *qp = malloc(qn * sizeof(*qp));
   vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qp);
   uint32_t qi = ~0u;
   for (uint32_t i = 0; i < qn; i++)
      if (qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { qi = i; break; }
   free(qp);
   if (qi == ~0u) { printf("FAIL no graphics queue\n"); return 1; }
   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = qi,
                                  .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci};
   VkDevice dev = VK_NULL_HANDLE;
   printf("CREATEDEVICE_BEGIN\n");
   r = vkCreateDevice(phys, &dci, NULL, &dev);
   printf("CREATEDEVICE_END r=%d\n", r);
   if (r != VK_SUCCESS) { printf("FAIL CreateDevice r=%d\n", r); return 1; }
   printf("CREATEDEVICE_OK\n");
   vkDestroyDevice(dev, NULL);
   vkDestroyInstance(inst, NULL);
   return 0;
}
C

CC=cc
command -v clang >/dev/null 2>&1 && CC=clang
command -v gcc >/dev/null 2>&1 && CC=gcc
log "CC=$CC"
set +e
$CC -O2 -o /tmp/dx5-createdevice "$PROBE" -ldl
cr=$?
set -e
if [ "$cr" -ne 0 ]; then
   log "PROBE compile fail rc=$cr; one retry with -I/usr/include"
   set +e
   $CC -O2 -I/usr/include -o /tmp/dx5-createdevice "$PROBE" -ldl
   cr=$?
   set -e
   [ "$cr" -eq 0 ] || abort "CreateDevice probe compile failed"
fi
log "PROBE_COMPILE=PASS"

SLICE_BIN=""
if [ -f "$HARNESS/gpu_prerast_slice.c" ]; then
   set +e
   $CC -O2 -I"$HARNESS" -o /tmp/dx5-slice "$HARNESS/gpu_prerast_slice.c" -ldl
   sr=$?
   set -e
   if [ "$sr" -ne 0 ]; then
      log "SLICE compile fail rc=$sr; one retry"
      set +e
      $CC -O2 -I"$HARNESS" -I/usr/include -o /tmp/dx5-slice "$HARNESS/gpu_prerast_slice.c" -ldl
      sr=$?
      set -e
   fi
   if [ "$sr" -eq 0 ]; then
      SLICE_BIN=/tmp/dx5-slice
      log "SLICE_COMPILE=PASS"
   else
      log "SLICE_COMPILE=FAIL"
   fi
else
   log "SLICE_COMPILE=SKIP missing harness"
fi

run_probe() {
   label="$1"
   shift
   log "=== $label ==="
   set +e
   if command -v timeout >/dev/null 2>&1; then
      timeout 45 "$@"
      rc=$?
   else
      "$@"
      rc=$?
   fi
   set -e
   log "${label}_RC=$rc"
   return "$rc"
}

unset PANVK_DEBUG || true
log "PANVK_DEBUG_IDVS="
set +e
run_probe CREATEDEVICE_IDVS /tmp/dx5-createdevice "$ICD"
idvs_rc=$?
set -e
if [ "$idvs_rc" -eq 0 ]; then
   log "CREATEDEVICE_IDVS=PASS"
else
   log "CREATEDEVICE_IDVS=FAIL rc=$idvs_rc"
fi

export PANVK_DEBUG=gpu_prerast
log "PANVK_DEBUG_PRERAST=gpu_prerast"
set +e
run_probe CREATEDEVICE_PRERAST /tmp/dx5-createdevice "$ICD"
prerast_rc=$?
set -e
if [ "$prerast_rc" -eq 0 ]; then
   log "CREATEDEVICE_PRERAST=PASS"
else
   log "CREATEDEVICE_PRERAST=FAIL rc=$prerast_rc"
fi

matrix_rc=1
if [ "$idvs_rc" -eq 0 ] && [ "$prerast_rc" -eq 0 ] && [ -n "$SLICE_BIN" ]; then
   unset PANVK_DEBUG || true
   set +e
   run_probe MATRIX_IDVS "$SLICE_BIN" "$ICD"
   mid=$?
   set -e
   export PANVK_DEBUG=gpu_prerast
   set +e
   run_probe MATRIX_PRERAST "$SLICE_BIN" "$ICD"
   mpr=$?
   set -e
   log "MATRIX_IDVS_RC=$mid"
   log "MATRIX_PRERAST_RC=$mpr"
   if [ "$mid" -eq 0 ] && [ "$mpr" -eq 0 ]; then
      matrix_rc=0
      log "MATRIX=PASS"
   else
      log "MATRIX=FAIL"
   fi
else
   log "MATRIX=NOT_RUN CreateDevice not proven on both paths or slice missing"
fi

unset PANVK_DEBUG || true
log "ICD_SHA256=$ICD_SHA"
log "CREATEDEVICE_IDVS_RC=$idvs_rc"
log "CREATEDEVICE_PRERAST_RC=$prerast_rc"
log "MATRIX_RC=$matrix_rc"
if [ "$idvs_rc" -eq 0 ] && [ "$prerast_rc" -eq 0 ] && [ "$matrix_rc" -eq 0 ]; then
   log "STATUS=COMPLETE"
   exit 0
fi
if [ "$idvs_rc" -eq 0 ] && [ "$prerast_rc" -eq 0 ]; then
   log "STATUS=PARTIAL_CREATEDEVICE_OK_MATRIX_NOT_PASS"
   exit 1
fi
log "STATUS=BLOCKED_CREATEDEVICE"
exit 1
