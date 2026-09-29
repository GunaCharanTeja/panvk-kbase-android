# G615 DXVK GPU-only contract

Authority: `PANVK_G615_DXVK_GPU_ONLY_REVISED_WORKER.md`
hash `a8c0a2fe63775bc1e2497d5c4e17bc366ca372ed3db10f529c517b3bddb3d9b2` (3212 lines)
Roadmap: M1 DXVK only. Do not start vkd3d, other GPUs, Wine, RC/stable.

Repo: `https://github.com/abhay-byte/panvk-kbase-android`
Branch: `feature/g615-dxvk-complete`
Integration SHA: `d0c81d7e19963fffae9164345809c530e07eb1db`
Mesa pin: `5a07217f034b3e50d8c7c7794f97a2df1742613b`
Device: Poco X6 Pro / duchamp, Mali-G615 MC6 `0xb8a31030`, PAN_ARCH 11, CSF, kbase 1.21
Serial: `Y5WWBMJVOZSK4HU8`

Labels: HARDWARE_NATIVE | GPU_LOWERED | UNSUPPORTED
Allowed CPU: compile, NIR, record, alloc, submit bookkeeping, test readback
Forbidden: CPU BC decode (incl init), CPU VS/GS/TCS/TES, CPU tess/clip, Gallium draw, llvmpipe/lavapipe/SwiftShader, CPU readback to build follow-up draws, system Mali ICD, fake bits

Section 30: implement -> internal tests (exposure off) -> identified dev candidate -> real API use -> local+CTS no false skips -> stock DXVK+Native -> release default. Never enable `textureCompressionBC` until all 16 formats + required ops PASS.

Do not patch stock DXVK. Do not label skip/unrun as PASS. Changes only in `work/mesa` are not a deliverable; export to tracked patches and prove clean apply.

Coordinator owns STATE/NEXT/DECISIONS. Workers write only assigned reports under `worklogs/g615-dxvk/results/<id>/`.
