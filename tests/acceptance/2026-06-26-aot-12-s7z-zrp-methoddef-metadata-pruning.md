# 2026-06-26 AOT 12-S7Z zrp MethodDef metadata pruning

## Scope

- Opt-in AOT C code stripping now prepares an emitted embedded zrp metadata blob after reachability filtering.
- The first concrete sweep prunes MethodDef rows whose `functionIndex` no longer appears in the stripped AOT function table.
- The emitted blob, descriptor length, `aot_size.embeddedModuleBytes`, and zrp size/delta markers all use the pruned blob.
- The pruning path is deliberately conservative and keeps the original blob when dependent member metadata is present.

## RED

- Extended `zr_vm_aot_c_code_stripping_test` with a zrp fixture containing one retained and one removable MethodDef row.
- Required after-size markers, descriptor embedded length, and MethodDef section bytes to reflect one removed `SZrZrpMetadataMethodDefRow`.
- First focused WSL gcc run failed 1/5 because before/after zrp metadata sizes were still identical.

## GREEN

- Added `backend_aot_c_zrp_metadata_prune.{h,c}` to prepare/release an emitted metadata blob.
- Added blob-based zrp size sampling so after-stats can read the pruned blob.
- Threaded the prepared blob through AOT C embedded-byte-array emission and module descriptor length.
- Rebuilt section offsets while compacting MethodDef rows and adjusting TypeDef method ranges for the safe subset.

## Validation

- WSL gcc: code stripping 5/0; source contracts 21/0; frame setup 1/0; typed scalar 1/0; shared-library smoke 8/0; focused CTest 2/2.
- WSL clang: same direct set and CTest 2/2. Existing generated generic-conversion `-Wlogical-not-parentheses` warning remains unrelated.
- Windows MSVC Debug: code stripping 5/0; source contracts 21/0; frame setup 1/0; typed scalar 0 failures/1 ignored; shared-library smoke 0 failures/8 ignored; focused CTest 2/2.

## Notes

- This is MethodDef-row pruning only.
- Token records, generic params, MethodSpec, pools, cross-table token remapping, annotation-driven promotion, and dump/diff tooling remain later work.
