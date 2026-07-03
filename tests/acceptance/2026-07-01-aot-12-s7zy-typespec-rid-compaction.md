# AOT 12-S7ZY / 11-S7 TypeSpec RID Compaction

- Completion time: 2026-07-01 16:14:53 +08:00
- Status: completed support sub-slice for compacting retained TypeSpec row tokens and TypeSpec token-record references
  after emitted `.zrp` metadata pruning.

## Scope

This slice follows the TypeSpec orphan sweep by rewriting retained TypeSpec row tokens to their compacted row RID after
earlier TypeSpec rows are removed. Token records that publish or refer to retained TypeSpec tokens now use the same
compacted token, and token-record retention/signature-pool collection share that TypeSpec-aware remap path.

This does not close cross-module export manifest/table publication rewrite, cross-module target/provider binding,
complete metadata sweep/pruning, complete trim analyzer, annotation-driven warning policy, or runtime ABI drift deopt
coverage.

## RED

- `tests/parser/test_aot_c_zrp_metadata_typespec_pruning.c` added a fixture with two TypeSpec rows where the first row
  is pruned and the second is retained.
- The focused Windows test expected retained source `TYPE_SPEC` RID2 to publish compacted RID1.
- The old helper copied the retained row token unchanged, so the test failed with `Expected 117440513 Was 117440514`.

## GREEN

- `backend_aot_c_zrp_metadata_type_spec.{h,c}` now exposes compacted TypeSpec token computation and TypeSpec token
  remap helpers.
- Pruned token-record counting/copying remaps TypeSpec references after member-token remap and drops records that point
  at pruned TypeSpec rows.
- Signature-pool collection uses the same TypeSpec-aware token-record retention path, so dropped TypeSpec token records
  no longer keep orphan signature bytes.
- Source contracts lock the TypeSpec remap helper surface.

## Validation

- Windows MSVC Debug direct runs passed:
  - `zr_vm_aot_c_zrp_metadata_typespec_pruning_test.exe` 2/0
  - `zr_vm_aot_c_zrp_metadata_pruning_test.exe` 5/0
  - `zr_vm_aot_c_zrp_metadata_pool_pruning_test.exe` 5/0
  - `zr_vm_aot_c_zrp_metadata_export_token_remap_test.exe` 3/0
  - `zr_vm_aot_c_zrp_metadata_size_deltas_test.exe` 2/0
  - `zr_vm_aot_c_code_stripping_test.exe` 10/0
  - `zr_vm_aot_c_source_contracts_test.exe` 24/0
  - `zr_vm_aot_c_frame_setup_contracts_test.exe` 1/0
- Windows MSVC Debug focused CTest selection passed 6/6.
- WSL GCC and WSL Clang built the focused targets. Their focused CTest selections passed 6/6, and explicit direct runs
  passed `zr_vm_aot_c_source_contracts_test` 24/0 plus `zr_vm_aot_c_frame_setup_contracts_test` 1/0.
