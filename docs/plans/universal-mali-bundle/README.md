# Universal Mali planning bundle

Read `PANVK_UNIVERSAL_ROADMAP_REVIEW.md` for the source-based findings and master-roadmap insertion.
Use `PANVK_UNIVERSAL_MALI_GPU_ONLY_WORKER.md` as the detailed M3 implementation document.
`PANVK_UNIVERSAL_REFERENCE_LOCK.json` pins the requested G720 release and preserves existing donor ancestry; it is not a replacement build lock.
`START_UNIVERSAL_OPENCODE.txt` is a future execution prompt using the built-in general subagent and no reviewer.

The universal worker starts only after verified DXVK and vkd3d targets. No driver changes, hardware tests, remote commits or release promotion were performed by creating this bundle. The donor asset hash was read from GitHub metadata, not locally verified against downloaded bytes.

Suggested repository destinations: the two Markdown files under docs/plans/, the reference JSON alongside them or imported into the existing source-lock schema at implementation time. Do not overwrite the current technical workers or erase later local progress.
