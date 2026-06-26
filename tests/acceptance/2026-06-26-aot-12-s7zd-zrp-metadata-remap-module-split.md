# AOT 12-S7ZD zrp metadata remap module split

## Status

- Completed: 2026-06-26 07:55:51 +08:00
- Scope: support refinement for 12-S7 / 11-S7 emitted zrp metadata pruning
- Behavior change: none intended

## Completed Items

- Added `backend_aot_c_zrp_metadata_remap.{h,c}`.
- Moved MethodDef reachability retention, shared `MEMBER_DEF` MethodDef/FieldDef token compaction, TokenRecord retention/remap, GenericParam owner remap, and TypeDef/MethodDef range compaction out of `backend_aot_c_zrp_metadata_prune.c`.
- Kept `backend_aot_c_zrp_metadata_prune.c` focused on section lookup, header rebuild, row copying, and embedded metadata ownership.
- Updated the source-contract test so prune/remap module responsibilities are checked separately.
- Updated the Windows shared-library direct unit-test target so the private remap module is compiled beside the private prune module.

## Evidence

- `backend_aot_c_zrp_metadata_prune.c`: 982 lines before split, 549 lines after split.
- `backend_aot_c_zrp_metadata_remap.c`: 434 lines.
- `backend_aot_c_zrp_metadata_remap.h`: 71 lines.

## Validation

- WSL gcc Debug (`build-wsl-gcc`): direct zrp pruning 3/0, code stripping 5/0, source contracts 21/0, frame setup 1/0, typed scalar 1/0, shared-library smoke 8/0; focused CTest `aot_c_(code_stripping|zrp_metadata_pruning|typed_scalar)$` 3/3.
- WSL clang Debug (`build-wsl-clang`): same direct and CTest set passed; existing generated generic-conversion `-Wlogical-not-parentheses` warning remains.
- Windows MSVC Debug (`build-msvc`): direct zrp pruning 3/0, code stripping 5/0, source contracts 21/0, frame setup 1/0, typed scalar 0 failures/1 ignored, shared-library smoke 0 failures/8 ignored; focused CTest 3/3.

## Remaining Work

- GenericParamConstraint cascade.
- MethodSpec method-token/signature-pool rewrite.
- Pool compaction.
- Attribute/annotation-driven metadata promotion or suppression.
- Export token handling.
- Metadata dump/diff tooling.
