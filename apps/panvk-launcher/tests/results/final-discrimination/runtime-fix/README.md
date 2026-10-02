# Normal ARM64EC DX8/9/10/11 fixed in DXVK, not driver

One-line runtime source fix in upstream DXVK v3.1.1:

```cpp
Rc<DxvkCommandList> DxvkContext::beginExternalRendering() {
  endCurrentPass(false);
  endCurrentCommands();
  beginCurrentCommands();
  return m_cmd;
}
```

Ending (not suspending) the current pass materializes deferred clears and
restores shader-readable image layouts before the external presenter reads.
The fix adds no CPU copies/readback, host GPU wait, feature spoof or driver
modifications. Existing software X11 presentation still copies pixels.

| Runtime / launch | DX8 | DX9 | DX10 | DX11 |
| --- | --- | --- | --- | --- |
| Patched normal CLI | 76800 orange | 76800 orange | 76800 orange | 76800 orange |
| Patched actual launcher Run | 76800 orange | 76800 orange | 76800 orange | 76800 orange |
| Same-source unpatched DX11 control | — | — | — | 76800 black |

All client images 320x240, full pixel counts. Actual UI path-only wrappers hold
12 seconds; staging unset/default off. DX11 uses eight frames, no extra Flush
or EVENT. App no longer auto-injects fbread; CLI layers disabled. Screenshot,
raw P6 images, logs, timestamps, identities and manifests in each bundle.
Committed manifests retain both client P6 captures and screenshots; redundant
8 MB full-desktop P6 copies remain local, excluded from the committed subset.
Normal patched Vulkan trace: eight orange loadOp=CLEAR operations, eight
presenter draws. Pristine and same-source control had zero orange clear ops.

## Provenance / compatibility

Catalog DXVK is **Arihany/WinlatorWCPHub**, not GameNative/dxvk. GameNative
supplies Proton. WCP profile attributes upstream Philip Rebohle; installed
version 3.1.1-arm64ec. Exact unpublished WCP source/build flags are not known.
Replacement is explicitly **upstream v3.1.1 + local fix**, not a claimed
bit-identical WCP rebuild. Source commit
`b1a1c99ab52b687cf950d62c88bc2fa316b41663`; submodule commits recorded in
`build-provenance.json`. Same-source unpatched build reproduces black, making
the one-line change causal independently of WCP build differences.

`patch_sha256` records the original build-time patch. The committed patch uses
trimmed context only (SHA256 `119cd29987c148154cf39f5972ebdff91a950ec1c0d3eec9526a33e6c8e93a2d`);
the source change is identical.

Portable LLVM-MinGW 20260922 UCRT, clang 23.1.2; Meson release, all five
DLLs ARM64EC/system32 plus i686/syswow64 built. ARM64EC runtime tested with
Proton 11.0-2-arm64ec on device `192.168.1.34:40501`; no missing imports/load
failures. i686 DLL build success is NOT an i686 game/mapping pass.

Pristine installed DLL backup:
`files/container/dxvk-pristine-before-clearfix/` (original ARM64EC DLLs at root;
original i686 DLLs in `syswow64/`). Do not use later `system32/` subfolder as
pristine: it contains the unpatched upstream control build.

Package: `/var/tmp/panvk/dxvk-clear-package/dxvk-3.1.1-clearfix.wcp`.
App Components imports local WCP using existing plumbing; fixed profile clearly
labels local runtime. Original component retained outside active contents.
No catalog URL/hash changed; no remotely published package or bundled WCP.

## Verify / rebuild

```sh
python3 apps/panvk-launcher/tests/results/final-discrimination/runtime-fix/check.py
export PATH=/var/tmp/panvk/launcher-tests/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64/bin:$PATH
sh apps/panvk-launcher/runtimes/dxvk/build.sh
```

Build script needs a fresh DXVK_WORK on rerun. Normal CLI reproduction uses
existing capture.py with `DXSMOKE_ARCH=arm64ec DXSMOKE_STAGING=0`,
`DXSMOKE_SYNC` unset. UI capture helper clicks the actual Recent Executables
Run button; coordinates must match current visible row. Independent fresh
verification passed all four normal APIs, two orange client captures each:
`fresh-verification/RESULTS.md`. Finalization also toggled the actual app DXVK
switch off/on: enabled preference restored, all ten component DLL hashes match
their prefix destinations and the recorded fixed runtime (both architectures).
See `fresh-verification/re-enable.txt`.
Clear/present smoke only, not game compatibility or zero-copy Android WSI proof.
