# ARM64EC DX8/9/10/11 scoped proof

Historical readback-enabled proof. Normal runtime source fix and fresh all-DX
CLI/UI captures: `../final-discrimination/runtime-fix/README.md`. No diagnostic
copy required after deferred-clear fix; driver unchanged.

Device `192.168.1.34:40501`, Proton `11.0-2-arm64ec`, system32 DXVK
`3.1.1-arm64ec`, existing beta.8 PanVK. Driver untouched.

Each API directory contains stdout/stderr, matching DLL/driver hashes, build
identity, raw P6 client/frame/root images, Android screenshot, capture timestamps,
commands and SHA-256 manifest. `reproduce.sh` uses the existing capture helper.
`check.py` independently validates hashes, source counts, X11 pixels and capture
ordering. Historical filenames `d3d11.log` apply to the API in build-identity.json.

| API | API / exit | Source orange | Client X11 orange |
| --- | --- | --- | --- |
| DX8 | PASS / 0 | 76800 / 76800 | 76800 / 76800 |
| DX9 | PASS / 0 | 76800 / 76800 | 76800 / 76800 |
| DX10 | PASS / 0 | 76800 / 76800 | 76800 / 76800 |
| DX11 FL11_0 | PASS / 0 | 614400 / 614400 (8 frames) | 76800 / 76800 |

Actual launcher UI also tested for all four: Wine tab, Executable Path, Run.
Historical path-only wrappers selected API, 12-second hold, staging enabled.
Current wrappers no longer force staging; readback is opt-in diagnostic only.
Sibling `arm64ec-ui-d3dN/` bundles contain
actual app run.log, present-observed.log, raw client images and screenshot.
UI source and client counts match the table. Exact screenshot orange counts:
DX8 28339, DX9 27151, DX10 27943, DX11 26557. Battery overlay obscures part
of the client; it is not used as source proof. App UI uses its existing fbread
layer; command-line matrix disables layers.

**Scope:** clear/readback/Present smoke only. Source readback introduces GPU
synchronization: initial runs without it returned successful Present but black
X11 images on all four APIs. These results do not prove synchronization-free
rendering, draws, real games, x86_64 games or i686 WOW64 compatibility. Existing
i686 staging/SAME_VA failures remain separate; ARM64EC is not their fix.
Earlier native/profile or paired i686 evidence is not a four-API game pass.

Final normal DX11 discrimination: `../final-discrimination/README.md`.
Normal/Flush/completed EVENT remain black; readback alone orange. No proven
missing WSI completion wait, no normal acceptance, no production workaround.
