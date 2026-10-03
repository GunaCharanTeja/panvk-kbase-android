# Normal DX11 blocked; completion-only hypothesis not demonstrated

**Resolved by runtime source fix:** [runtime-fix/README.md](runtime-fix/README.md).
Normal ARM64EC DX8/9/10/11 CLI and actual launcher captures now orange without
readback/extra sync. Same-source unpatched DX11 control remains black. Historical
failure evidence below retained; driver unchanged.

Follow-up: [SAMPLED-CLEAR.md](SAMPLED-CLEAR.md). Native sampled-clear baseline
and forced copy both orange. Actual installed DXVK API trace emits **zero orange
Vulkan clears** normally versus eight with readback. Producer materialization
is missing above driver entry; not a confirmed driver synchronization fault.

Device `192.168.1.34:40501`, unchanged beta.8 ICD, Proton 11.0-2-arm64ec,
system32 DXVK 3.1.1-arm64ec. Identical executable/DLL/driver hashes in every
bundle. No injected layers. Eight frames, then 12-second hold. Captures start
only after successful frame 7, finish while process remains alive.

| Mode | Launch UTC 2026-10-02 | Client pixels | Result |
| --- | --- | --- | --- |
| default | 16:47:16.941565 | 76800 black | normal FAIL |
| Flush only | 16:47:41.130727 | 76800 black | FAIL |
| EVENT End/Flush/GetData | 16:48:04.693298 | 76800 black | FAIL despite eight completed events |
| staging CopyResource/Map | 16:48:27.934875 | 76800 orange | diagnostic PASS only |
| default repeat | 16:48:54.527706 | 76800 black | normal FAIL reproduced |

Every Present returns S_OK; exit 0 is API success, not visual acceptance.
Readback measures 614400 orange source pixels across eight frames. No source
pixel measurement exists for default/Flush/EVENT: source remains unknown.
Raw P6 client/frame/root images, Android `screenshot.png`, timestamps, command
transcripts, identities and manifests are retained in each mode directory.

## Read-only source discrimination

`work/mesa-wsi/src/vulkan/wsi/wsi_common.c`:
- `wsi_common_queue_present`, 2485–2512: gathers supplied present wait
  semaphores, TRANSFER destination stage.
- Submission before platform present consumes those waits and signals the
  per-image fence; 2892–2895 explicitly waits that fence when `wsi->sw`.
- 2901 invokes platform `queue_present` only afterwards.

`work/mesa-wsi/src/vulkan/wsi/wsi_common_x11.c`:
`x11_present_to_x11`, 2115–2116 selects `x11_present_to_x11_sw` for software
without MIT-SHM. That path uses CPU image data with `xcb_put_image`.
Patch 083 loads X11/XCB dynamically; patch 084 advances software present IDs
after PutImage and corrects timeout enum. Neither removes the common fence wait.

Upstream DXVK tag v3.1.1 `src/dxvk/dxvk_presenter.cpp`, 205–227 supplies
`waitSemaphoreCount=1`, `pWaitSemaphores=&currSync.present` to
`vkQueuePresentKHR`. 874–875 enables present wait from feature/capability;
1337–1340 uses `vkWaitForPresentKHR` when presentWait2 is unavailable.
Installed runtime logs show `presentWait: 1`, `presentWait2: 0`.
Source: https://github.com/doitsujin/dxvk/blob/v3.1.1/src/dxvk/dxvk_presenter.cpp
This is upstream source proof, not a captured installed DLL Vulkan call trace.

**No exact missing-semaphore/completion fault proved.** Existing common WSI
code already waits; EVENT completion without CPU image copy stays black.
Readback changes resource operations/layout/cache behavior as well as timing.
Calling this a proven WSI semaphore bug would overstate evidence. Remaining
boundary: DXVK source clear/materialization versus presenter destination;
requires a non-mutating Vulkan command trace or driver-owner investigation.
No driver edits/builds, no production CPU-copy workaround, no app config fix.

## Verify / reproduce

From repository root:

```sh
python3 apps/panvk-launcher/tests/results/final-discrimination/check.py
sh apps/panvk-launcher/tests/results/final-discrimination/reproduce.sh
```

Reproduction overwrites only these evidence bundles. Test env:
`DXSMOKE_STAGING=0` normal; `DXSMOKE_SYNC=flush|event` opt-in diagnostics;
`DXSMOKE_STAGING=1` opt-in readback. Path-only wrappers no longer force staging.
Historical all-DX/UI orange bundles were readback-enabled diagnostic results;
they do not establish normal-frame acceptance. i686 SAME_VA mapping remains
a separate blocker. Next acceptance gate: unmodified normal DX11 frame orange,
then normal all-DX/UI captures, without staging or diagnostic sync.
