# AOT 12-S7ZZ / 11-S7 ModuleRef Orphan Sweep

- Completion time: 2026-07-01 16:40:57 +08:00
- Status: completed support sub-slice for dropping orphan ModuleRef rows and compacting retained AssemblyRef tokens
  after emitted `.zrp` metadata pruning.

## Scope

This slice makes emitted `.zrp` metadata pruning treat ModuleRef rows as import-ref-rooted metadata. A ModuleRef row is
retained only when a retained `TYPE_REF` or `MEMBER_REF` token record still references its `ASSEMBLY_REF` token. Self-only
AssemblyRef/signature token records no longer keep an otherwise orphaned ModuleRef row alive, and retained AssemblyRef
tokens are republished with compacted RIDs in token records and ModuleRef rows.

This does not close cross-module export manifest/table publication rewrite, cross-module target/provider binding,
complete metadata sweep/pruning, complete trim analyzer, annotation-driven warning policy, or runtime ABI drift deopt
coverage.

## RED

- `tests/parser/test_aot_c_zrp_metadata_pool_pruning.c` added an orphan/live ModuleRef fixture with two AssemblyRef rows.
- The first AssemblyRef had only self-owned AssemblyRef/signature token records, while the second was referenced by a
  live `TYPE_REF` token record.
- The old pruner raw-copied ModuleRef rows and treated the blob as an identity result, so the focused Windows test failed
  at the non-identity expectation before the orphan row and stale AssemblyRef RID could be removed.

## GREEN

- `backend_aot_c_zrp_metadata_module_ref.{h,c}` now exposes ModuleRef retention, retained count, compacted token, and
  token-record remap helpers.
- Pruned token-record counting/copying remaps AssemblyRef fields after member-token and TypeSpec remaps, and drops token
  records that point at pruned ModuleRef rows.
- ModuleRef section copying now emits only retained rows, rewrites retained row tokens to compacted `ASSEMBLY_REF` RIDs,
  and remaps retained module name/version string-pool offsets.
- String-pool and signature-pool collection share the same ModuleRef-aware token-record retention path so orphan module
  identity strings and self-only signature records disappear from the pruned blob.
- Source contracts lock the new ModuleRef helper surface and its integration into prune, string-pool, and signature-pool
  helpers.

## Validation

- Windows MSVC Debug focused build passed for zrp pool/prune/TypeSpec/export-token/size-delta, code-stripping,
  source-contract, and frame-setup targets.
- Windows MSVC Debug direct runs passed:
  - `zr_vm_aot_c_zrp_metadata_pool_pruning_test.exe` 6/0
  - `zr_vm_aot_c_zrp_metadata_pruning_test.exe` 5/0
  - `zr_vm_aot_c_zrp_metadata_typespec_pruning_test.exe` 2/0
  - `zr_vm_aot_c_source_contracts_test.exe` 24/0
  - `zr_vm_aot_c_zrp_metadata_export_token_remap_test.exe` 3/0
  - `zr_vm_aot_c_zrp_metadata_size_deltas_test.exe` 2/0
  - `zr_vm_aot_c_code_stripping_test.exe` 10/0
  - `zr_vm_aot_c_frame_setup_contracts_test.exe` 1/0
- Windows MSVC Debug focused CTest selection passed 6/6.
- WSL GCC and WSL Clang built the focused targets. Their focused CTest selections passed 6/6, and explicit direct runs
  passed `zr_vm_aot_c_source_contracts_test` 24/0 plus `zr_vm_aot_c_frame_setup_contracts_test` 1/0.
