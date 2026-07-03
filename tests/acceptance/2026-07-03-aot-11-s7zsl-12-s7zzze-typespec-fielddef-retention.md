# AOT 11-S7ZSL / 12-S7ZZZE TypeSpec Retained FieldDef Token-Record Guard

Date: 2026-07-03 06:53:16 +08:00
Status: Completed focused refinement

## Scope

- Refine emitted `.zrp` metadata pruning so TypeSpec retention uses retained token-record remapping with
  TypeDef/FieldDef/generic row context.
- Drop TypeSpec rows and signature blobs rooted only by token records whose `MEMBER_DEF` FieldDef owner/target rows are
  pruned.
- Keep TypeSpec rows rooted by retained FieldDef token records and compact retained `TYPE_SPEC` tokens, signature
  offsets, and signature hashes.

## RED

- Command:
  `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test"`
- Result:
  `test_aot_c_zrp_metadata_pruning_drops_typespec_rooted_only_by_pruned_field_token_record` failed with
  `Expected 765 Was 794`.
- Cause: TypeSpec retention still used the legacy member-token remapper, so a TypeSpec rooted only through a pruned
  FieldDef token record survived and carried an extra TypeSpec row/signature blob into compacted metadata.

## GREEN

- Threaded TypeDef row context through TypeSpec retained/count/copy/remap APIs.
- Switched TypeSpec retention to call `backend_aot_c_zrp_remap_retained_token_record()` so FieldDef `MEMBER_DEF`
  roots must survive retained FieldDef row checks before keeping a TypeSpec row.
- Passed the same TypeDef context through signature, module-ref, string-pool, and prune orchestration call sites.
- Updated signature `MEMBER_REF` rewrite to use the retained token-record wrapper.
- Updated source-contract needles for retained token-record usage in TypeSpec and signature pruning code.

## Verification

- WSL GCC: `zr_vm_aot_c_source_contracts_test` passed 24/0;
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` passed 10/0;
  `zr_vm_aot_c_zrp_metadata_pruning_test` passed 19/0.
- WSL clang: `zr_vm_aot_c_source_contracts_test` passed 24/0;
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` passed 10/0;
  `zr_vm_aot_c_zrp_metadata_pruning_test` passed 19/0.
- Windows MSVC Debug: `zr_vm_aot_c_source_contracts_test` passed 24/0;
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` passed 10/0;
  `zr_vm_aot_c_zrp_metadata_pruning_test` passed 19/0.

## Remaining

- Complete metadata sweep/pruning is still open beyond this focused TypeSpec retained FieldDef token-record guard.
- Full trim analyzer, annotation/promotion policy, and broader ABI drift/deopt closure remain open for later AOT 07~12
  slices.
