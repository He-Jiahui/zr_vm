# AOT 11-S7ZSC / 12-S7ZZT Manifest Export Target String Retention

Date: 2026-07-03 05:50:58 +08:00
Status: Completed focused refinement

## Scope

- Refine persistent `.zrp` `manifestExports` section pruning so retained export rows keep their `targetStringOffset`
  string-pool payload even when that string is not referenced by TypeDef, MethodDef, FieldDef, ModuleRef, or other row roots.
- Keep the behavior aligned with 11-S7ZSC / 12-S7ZZT section rewrite: only rows that survive type/member token remapping
  contribute target strings to the compacted string pool.

## RED

- Command: `wsl -e bash -lc "cd /mnt/e/Git/zr_vm/build-wsl-gcc && cmake --build . --target zr_vm_aot_c_zrp_metadata_pruning_test -j 2 && ./bin/zr_vm_aot_c_zrp_metadata_pruning_test"`
- Result: `test_aot_c_zrp_metadata_pruning_keeps_manifest_export_target_only_strings` failed with
  `Expected TRUE Was FALSE` at `backend_aot_c_prepare_embedded_zrp_metadata`.
- Cause: the previous string-pool remap did not collect a string referenced only by a retained manifest export target
  (`"unused"` in the fixture), so manifest export target remapping failed during compacted metadata preparation.

## GREEN

- Added `backend_aot_c_zrp_manifest_export_row_is_retained()` to reuse the manifest export remap predicate without
  mutating source rows.
- Extended `backend_aot_c_zrp_build_string_pool_remap()` to accept manifest export rows/count and add retained
  `targetStringOffset` values to the string remap.
- Updated prune orchestration to reserve capacity for manifest export target strings and pass the manifest export view
  into string-pool remap construction.
- Added source-contract needles for the manifest export retention helper and target string collection.

## Verification

- WSL GCC: `zr_vm_aot_c_zrp_metadata_pruning_test` passed 17/0; `zr_vm_aot_c_source_contracts_test` passed 24/0.
- WSL clang: `zr_vm_aot_c_zrp_metadata_pruning_test` passed 17/0; `zr_vm_aot_c_source_contracts_test` passed 24/0.
- Windows MSVC Debug: `zr_vm_aot_c_zrp_metadata_pruning_test` passed 17/0; `zr_vm_aot_c_source_contracts_test` passed 24/0.

## Remaining

- Complete metadata sweep/pruning is still open beyond this focused manifest export string root fix.
- Full trim analyzer, annotation/promotion policy, and broader ABI drift/deopt closure remain open for later AOT 07~12 slices.
