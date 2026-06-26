# AOT 12-S7ZE zrp GenericParamConstraint cascade

## Status

- Completed: 2026-06-26 08:15:19 +08:00
- Scope: 12-S7 / 11-S7 emitted zrp metadata pruning dependency cascade
- Behavior change: GenericParamConstraint rows now participate in emitted zrp pruning when MethodDef reachability removes GenericParam owners.

## Completed Items

- Allowed emitted zrp metadata pruning to process blobs that contain `GenericParamConstraint` rows.
- Added GenericParamConstraint retention counting and row remapping to `backend_aot_c_zrp_metadata_remap.{h,c}`.
- Rebuilt the compacted GenericParamConstraint section in `backend_aot_c_zrp_metadata_prune.c`.
- Dropped constraints whose GenericParam row is removed with a trimmed MethodDef.
- Remapped retained constraint `genericParamIndex` values to compacted GenericParam indexes.
- Recomputed retained GenericParam `firstConstraintIndex` and `constraintCount` ranges after constraint compaction.
- Updated source contracts to lock the constraint copy/remap/count/range boundary.

## RED / GREEN

- RED: the new direct zrp fixture with 3 GenericParams and 4 GenericParamConstraints failed 1/4 on WSL gcc because the old guard kept the original blob and `ownedBlob` stayed null.
- GREEN: the fixture now compacts constraints from 4 to 3 rows, removes the constraint attached to the removed MethodDef-owned GenericParam, remaps the TypeDef-owned GenericParam constraint index from 2 to 1, and keeps retained GenericParam constraint ranges consistent.

## Validation

- WSL gcc Debug (`build-wsl-gcc`): direct zrp pruning 4/0, code stripping 5/0, source contracts 21/0, frame setup 1/0, typed scalar 1/0, shared-library smoke 8/0; focused CTest `aot_c_(code_stripping|zrp_metadata_pruning|typed_scalar)$` 3/3.
- WSL clang Debug (`build-wsl-clang`): same direct and CTest set passed; existing generated generic-conversion `-Wlogical-not-parentheses` warning remains.
- Windows MSVC Debug (`build-msvc`): direct zrp pruning 4/0, code stripping 5/0, source contracts 21/0, frame setup 1/0, typed scalar 0 failures/1 ignored, shared-library smoke 0 failures/8 ignored; focused CTest 3/3.

## Remaining Work

- MethodSpec method-token/signature-pool rewrite.
- Pool compaction.
- Attribute/annotation-driven metadata promotion or suppression.
- Export token handling.
- Metadata dump/diff tooling.
