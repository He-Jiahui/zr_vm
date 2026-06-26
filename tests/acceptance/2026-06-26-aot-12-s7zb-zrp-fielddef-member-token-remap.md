# 2026-06-26 AOT 12-S7ZB zrp FieldDef member-token remap

## Scope

- Extends 12-S7ZA zrp MethodDef/token-record pruning to blobs that contain FieldDef rows.
- Keeps FieldDef rows, but rewrites their shared `MEMBER_DEF` tokens after MethodDef rows are removed.
- Remaps token-record fields that point at retained FieldDefs to the new compact member-token RID.
- Keeps GenericParam, GenericParamConstraint, and MethodSpec rows guarded until their owner/method-token cascades are rewritten.

## RED

- Replaced the FieldDef guard fixture in `zr_vm_aot_c_zrp_metadata_pruning_test` with a direct zrp fixture containing:
  - one TypeDef token record,
  - one retained MethodDef token record,
  - one removed MethodDef token record,
  - one FieldDef token record,
  - one FieldDef signature token record,
  - one TypeDef row,
  - two MethodDef rows,
  - one FieldDef row.
- Required code stripping to return an owned pruned blob with one MethodDef row removed, one token record removed, the FieldDef row retained, and the FieldDef token remapped from `MEMBER_DEF` RID 3 to RID 2.
- First focused WSL gcc run failed 1/2 at `TEST_ASSERT_NOT_NULL(prunedMetadata.ownedBlob)` because the previous conservative FieldDef guard kept the original blob.

## GREEN

- Removed FieldDef rows from the MethodDef-pruning refusal set.
- Added FieldDef lookup and shared `MEMBER_DEF` remapping so retained MethodDefs map first and FieldDefs follow the retained MethodDef count.
- Updated token-record counting/copying to remap MethodDef and FieldDef member-token references through the same path.
- Added FieldDef row copying that preserves row payload and rewrites the row token to the compact shared member-token RID.
- Updated source contracts to lock the FieldDef-aware remap/copy implementation path.

## Validation

- RED observed on WSL gcc: `zr_vm_aot_c_zrp_metadata_pruning_test` failed 1/2 with expected non-null owned blob.
- WSL gcc: zrp pruning 2/0; code stripping 5/0; source contracts 21/0; frame setup 1/0; typed scalar 1/0; shared-library smoke 8/0; focused CTest 3/3.
- WSL clang: same direct set and focused CTest 3/3. Existing generated generic-conversion `-Wlogical-not-parentheses` warning remains unrelated.
- Windows MSVC Debug: zrp pruning 2/0; code stripping 5/0; source contracts 21/0; frame setup 1/0; typed scalar 0 failures/1 ignored; shared-library smoke 0 failures/8 ignored; focused CTest 3/3.

## Notes

- This closes FieldDef shared `MEMBER_DEF` token remapping for the current MethodDef-pruning path.
- GenericParam/MethodSpec cascades, pool compaction, annotation-driven metadata promotion, export token handling, and dump/diff tooling remain later work.
