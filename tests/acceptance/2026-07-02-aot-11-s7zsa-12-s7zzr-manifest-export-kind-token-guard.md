# 11-S7ZSA / 12-S7ZZR Manifest Export Kind/Token Guard

Date: 2026-07-02 09:38 +08:00

Status: completed support slice. Full 11-S7/12-S7 remain open: persistent export manifest/table file publication,
complete metadata sweep/pruning, full trim analyzer, annotation policy, and broader ABI drift/deopt closure still need
later work.

Completed:

- `backend_aot_c_zrp_manifest_export_table_build()` now rejects manifest export declarations whose token binding does
  not match the export kind.
- Type exports may carry only a `TYPE_DEF` binding; method and field exports may carry only a `MEMBER_DEF` binding.
- The generated manifest export table still accepts unbound declarations and the existing remapped method/field member
  token path used by provider exports.

RED:

- WSL GCC `zr_vm_aot_c_zrp_metadata_export_token_remap_test` failed 10/1:
  `test_aot_c_zrp_metadata_manifest_export_table_rejects_kind_token_mismatch` expected a type export with a member token
  to be rejected, but the old table builder accepted it.

GREEN:

- WSL GCC direct `zr_vm_aot_c_zrp_metadata_export_token_remap_test`: 10/0.
- WSL Clang direct `zr_vm_aot_c_zrp_metadata_export_token_remap_test`: 10/0.
- Windows MSVC Debug direct `zr_vm_aot_c_zrp_metadata_export_token_remap_test`: 10/0.
- WSL GCC direct `zr_vm_aot_c_source_contracts_test`: 24/0.
- WSL GCC/Clang provider shared-library smoke: 1/0 each.

Notes:

- This slice closes the manifest export table builder's kind/token shape guard only. It does not add a persistent
  `.zrp` manifest export section and does not close cross-module provider ABI drift/deopt, complete metadata
  sweep/pruning, or the full trim analyzer.
