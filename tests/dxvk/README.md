# DXVK-only tests

This tree owns DXVK profile, Vulkan semantic, and DXVK Native validation. It
does not cover vkd3d-proton or other translation layers.

- `profile/` tests the checked-in DXVK 1.10.3, 2.7.1, and 3.1.1 requirements
  against a capability capture using `scripts/evaluate-dxvk.py`.
- `vulkan/` will hold focused Vulkan semantic tests as each DXVK blocker is
  implemented.
- `native/` will hold DXVK Native workloads after profile and CTS gates pass.

Run the current suite with:

```sh
python3 -m unittest discover -s tests/dxvk/profile -p 'test_*.py'
python3 scripts/evaluate-dxvk.py --select-version 3.1.1 --select-profile COMMON
```

Use `--select-version` and `--select-profile` together. A selected `FAIL` or
`UNKNOWN` gate exits nonzero. Optimal profiles include every named
`*_optional` capability as a requirement; their recommendation rows remain
separate from baseline gates.

Profile `PASS` is not CTS or runtime proof. Generated evidence is stored in
`validation/g615-v11-csf/dxvk/DX1-REPORT.json`, `DX1-REPORT.md`, and
`evidence-ledger.json`. Existing `tests/dxvk-vkd3d/` guards remain authoritative
historical combined-plan tests.
