# 2026-06-26 AOT 12-S7ZA zrp token-record MethodDef pruning

## Scope

- Extends 12-S7Z zrp MethodDef-row pruning so the emitted blob also compacts token records that reference MethodDef rows.
- Retained MethodDef rows are rewritten to compact `MEMBER_DEF` RIDs, and retained token-record fields that point at those methods are remapped to the same compact tokens.
- Token records that reference removed MethodDef rows are dropped with the removed method row.
- The pruning path stays conservative: blobs with FieldDef rows, GenericParam rows, GenericParamConstraint rows, or MethodSpec rows keep the original blob until those dependent token spaces are rewritten together.

## RED

- Added `zr_vm_aot_c_zrp_metadata_pruning_test` with a direct zrp fixture containing:
  - one TypeDef token record,
  - one retained MethodDef token record,
  - one removed MethodDef token record,
  - one TypeDef row,
  - two MethodDef rows.
- Required code stripping to return an owned pruned blob with one MethodDef row removed, one token record removed, retained MethodDef tokens remapped to compact `MEMBER_DEF` RID 1, and matching section byte counts.
- First focused WSL gcc run failed 1/1 at `TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob)` because the old guard refused all token-record `MEMBER_DEF` references and kept the original blob.

## GREEN

- Replaced the all-or-nothing token-record guard with MethodDef-only token remapping.
- Added token-record counting/copying during zrp blob rebuild.
- Rewrote retained MethodDef row tokens to compact `MEMBER_DEF` RIDs.
- Added a guard test proving FieldDef-present blobs keep the original metadata because method and field definitions share the `MEMBER_DEF` token space.
- Updated source contracts to lock the new remap/copy implementation path.
- Updated the Windows test target so the internal pruning module is compiled into the direct unit test when the parser is built as a DLL.

## Validation

- RED observed on WSL gcc: `zr_vm_aot_c_zrp_metadata_pruning_test` failed 1/1 with expected non-null owned blob.
- WSL gcc: zrp pruning 2/0; code stripping 5/0; source contracts 21/0; frame setup 1/0; typed scalar 1/0; shared-library smoke 8/0; focused CTest 3/3.
- WSL clang: same direct set and focused CTest 3/3. Existing generated generic-conversion `-Wlogical-not-parentheses` warning remains unrelated.
- Windows MSVC Debug: zrp pruning 2/0; code stripping 5/0; source contracts 21/0; frame setup 1/0; typed scalar 0 failures/1 ignored; shared-library smoke 0 failures/8 ignored; focused CTest 3/3.

## Notes

- This is still a conservative MethodDef-only token-record rewrite.
- FieldDef token remapping, GenericParam/MethodSpec cascades, pool compaction, annotation-driven metadata promotion, and dump/diff tooling remain later work.
