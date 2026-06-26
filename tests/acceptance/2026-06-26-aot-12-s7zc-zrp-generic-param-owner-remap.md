# 2026-06-26 AOT 12-S7ZC zrp GenericParam owner remap

## Scope

- Extends emitted zrp MethodDef pruning to GenericParam rows when no GenericParamConstraint rows are present.
- Retains TypeDef-owned GenericParam rows unchanged.
- Retains GenericParam rows owned by retained MethodDefs/FieldDefs and remaps their shared `MEMBER_DEF` owner tokens.
- Drops GenericParam rows owned by removed MethodDefs.
- Recomputes TypeDef and retained MethodDef `firstGenericParamIndex` / `genericParamCount` ranges after GenericParam compaction.

## RED

- Added a direct `zr_vm_aot_c_zrp_metadata_pruning_test` fixture with:
  - one TypeDef token record,
  - one retained MethodDef token record,
  - one removed MethodDef token record,
  - one TypeDef row,
  - two MethodDef rows,
  - three GenericParam rows.
- Required pruning to delete the removed MethodDef row, delete its token record, delete its owned GenericParam row, preserve the retained MethodDef-owned GenericParam row, preserve the TypeDef-owned GenericParam row, and rebuild both owner tokens/ranges.
- First focused WSL gcc run failed 1/3 at `TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob)` because the previous GenericParam guard kept the original blob.

## GREEN

- Removed GenericParam rows from the prune refusal set while keeping GenericParamConstraint and MethodSpec rows guarded.
- Added GenericParam owner-token remapping for TypeDef and shared `MEMBER_DEF` owners.
- Added retained GenericParam counting/copying.
- Added generic-param range recomputation for TypeDef rows and retained MethodDef rows.
- Updated source contracts to lock the GenericParam remap/count/range/copy path.

## Validation

- RED observed on WSL gcc: `zr_vm_aot_c_zrp_metadata_pruning_test` failed 1/3 with expected non-null owned blob.
- WSL gcc: zrp pruning 3/0; code stripping 5/0; source contracts 21/0; frame setup 1/0; typed scalar 1/0; shared-library smoke 8/0; focused CTest 3/3.
- WSL clang: same direct set and focused CTest 3/3. Existing generated generic-conversion `-Wlogical-not-parentheses` warning remains unrelated.
- Windows MSVC Debug: zrp pruning 3/0; code stripping 5/0; source contracts 21/0; frame setup 1/0; typed scalar 0 failures/1 ignored; shared-library smoke 0 failures/8 ignored; focused CTest 3/3.

## Notes

- This closes GenericParam owner/range remapping only for rows without GenericParamConstraint dependencies.
- GenericParamConstraint cascades, MethodSpec method-token/signature-pool rewrite, pool compaction, annotation-driven metadata promotion, export token handling, and dump/diff tooling remain later work.
