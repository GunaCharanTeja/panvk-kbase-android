# 093 Tessellation state emission and conditional rendering

Patch: `patches/csf-v11/093-do-not-predicate-tessellation-state-emission-on-conditional-rendering.patch`.

## Bug (from 087)

`gpu_prerast_tess` wrapped the whole draw, including the emission of draw state, in a GPU `cs_if(pred)` for conditional rendering. The dirty flags are cleared when the command buffer is recorded, so a false predicate skipped the state writes but not the clearing. The next normal draw then ran with stale FS, depth and query state.

## Fix

Only the launches are predicated (compute side already is). The state emission on the vertex/tiler queue is no longer inside the `cs_if`. The two-line hunk removes the wrapper.

## Test

`tests/dxvk/vulkan/tess-conditional-regression` (skipped conditional tess draw, then a normal draw) passes with the fix. It also passes on the old ICD, so it does not discriminate between the two. Full CTS list (`tmp/g615-gap/cts-all.txt`, 36144 cases): 17067 pass, 0 fail, 19077 NotSupported, same as the baseline with 092.

## Scope

Applies to v10/v11 and the other versions that build this code (the loop is in the shared csf draw file); no new feature bit.
