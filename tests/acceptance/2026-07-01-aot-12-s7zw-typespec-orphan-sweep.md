# AOT 12-S7ZW / 11-S7 TypeSpec Orphan Sweep

- Completion time: 2026-07-01 15:30:06 +08:00
- Status: completed support sub-slice for token-record-rooted TypeSpec row and signature-pool pruning.

## Scope

This slice extends emitted `.zrp` metadata pruning so TypeSpec rows are no longer raw-copied after token-record
pruning. A TypeSpec row is retained only when a matching retained `TYPE_SPEC` token record survives the existing
token-record remap. Retained TypeSpec row signature offsets are rewritten through the compacted signature blob pool,
and orphan TypeSpec rows drop together with their signature blob payload.

This does not close TypeSpec RID compaction, cross-module export-token publication/rewrite, complete metadata
sweep/pruning, complete trim analyzer, annotation-driven warning policy, or runtime ABI drift deopt coverage.

## RED

- `tests/parser/test_aot_c_zrp_metadata_typespec_pruning.c` built a `.zrp` metadata blob with a TypeSpec row whose
  `TYPE_SPEC` token record referenced a removed MethodDef.
- The expected pruned result kept two token records and one MethodDef, dropped the orphan TypeSpec row, reduced
  `signatureBlobPool.byteLength` to 0, and produced a 488-byte blob.
- The old pruner raw-copied the TypeSpec section and signature payload, so the focused Windows test failed with
  `Expected 488 Was 517`.

## GREEN

- `backend_aot_c_zrp_metadata_type_spec.{h,c}` owns retained TypeSpec row detection, counting, and copying.
- `backend_aot_c_zrp_metadata_prune.c` builds the TypeSpec section from the retained count and uses the new helper
  instead of raw section copying.
- `backend_aot_c_zrp_metadata_signature.c` collects TypeSpec signature blobs only from retained TypeSpec rows.
- Source contracts cover the new TypeSpec helper boundary from pruning and signature-pool collection.

## Validation

- Windows MSVC Debug direct runs passed:
  - `zr_vm_aot_c_zrp_metadata_typespec_pruning_test.exe` 1/0
  - `zr_vm_aot_c_zrp_metadata_pruning_test.exe` 5/0
  - `zr_vm_aot_c_zrp_metadata_pool_pruning_test.exe` 5/0
  - `zr_vm_aot_c_source_contracts_test.exe` 24/0
  - `zr_vm_aot_c_code_stripping_test.exe` 10/0
  - `zr_vm_aot_c_zrp_metadata_size_deltas_test.exe` 2/0
  - `zr_vm_aot_c_zrp_metadata_export_token_remap_test.exe` 2/0
- WSL GCC and WSL Clang built the same focused targets and passed the same direct test set.
- Focused CTest selection passed 6/6 on Windows MSVC Debug, WSL GCC, and WSL Clang.
- `git diff --check` returned exit code 0; only existing line-ending conversion warnings were printed.
