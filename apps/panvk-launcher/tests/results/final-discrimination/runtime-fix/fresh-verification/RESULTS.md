# Fresh independent verification

Device: `192.168.1.34:40501`. Exact parent `runtime-fix/check.py` passes:
black unpatched control; orange CLI/UI DX8/9/10/11; eight materialized
Vulkan clears and eight presenter draws. Old pre-fix checker untouched.

Fresh normal CLI matrix, rebuilt smoke executable and XGetImage helper:

| API | Raw client pixels (two images) | Exit | Present observed ms | Capture ms |
| --- | --- | --- | --- | --- |
| DX8 | 76800/76800 orange | 0 | 2254.3 | 369.5 |
| DX9 | 76800/76800 orange | 0 | 2010.3 | 407.2 |
| DX10 | 76800/76800 orange | 0 | 1815.1 | 487.9 |
| DX11 | 76800/76800 orange | 0 | 2162.7 | 476.3 |

Each API directory contains commands, SHA256 manifest, DLL/driver identities,
environment, logs, screenshot, raw PPMs and monotonic/UTC timestamps.
DX11: staging=0, sync=none, readback=SKIPPED, eight successful Presents.
Explicit/injected Vulkan layers disabled; no fbread. Captures complete while
the process remains alive. Loaded prefix hashes match fixed-identity.txt.

Screenshot limitation: Android notification shade obscures the desktop in
the fresh screenshots. Raw X11 client images pass; fresh Android-visible
orange screenshot verification is NOT established. Existing parent UI
captures are separately verified by the parent checker, not fresh UI runs.

Component inspection: sole active catalogue entry is
`files/contents/DXVK/3.1.1-clearfix-arm64ec`, profile identifies upstream
`b1a1c99` plus `endCurrentPass(false)`. All five source DLL hashes match
prefix DLLs in both system32 and syswow64. `Containers.kt` selects the first
DXVK entry and copies these DLLs on enable; with the observed sole entry,
re-enable selects the fixed source, not the pristine backup.

At the independent test stage, actual UI toggle verification was incomplete:
notification shade/UI navigation prevented confirmed disable/re-enable. An unintended driver
selection during navigation was reverted by restoring the original launcher
preferences; DXVK remains enabled, original PanVK_Kbase_G615 selected.
No runtime/driver source changes, no driver builds, no commit.

Finalization subsequently confirmed actual Wine-tab DXVK off/on, preference
false/true, and all ten source/prefix DLL pairs still matching the fixed runtime
in both architectures. Evidence: `re-enable.txt`. Original driver unchanged.

Verify fresh evidence: `python3 apps/panvk-launcher/tests/results/final-discrimination/runtime-fix/fresh-verification/check.py`.
