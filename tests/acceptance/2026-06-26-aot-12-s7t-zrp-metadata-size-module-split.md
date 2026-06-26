# AOT 12-S7T ZRP Metadata Size Module Split

## Scope

- Slice: 12-S7T, under 12-S7 trim warnings and size statistics.
- Goal: keep zrp metadata size and trim-delta accounting out of the main AOT C emitter before adding actual metadata sweep/pruning.
- Non-goals: changing the existing zrp metadata byte values, rewriting the embedded metadata blob, default-min metadata policy, full trim analyzer behavior, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- Result before implementation: `zr_vm_aot_c_source_contracts_test` ran 20 tests and failed 1 test with `Expected Non-NULL`.
- Failure reason: the source contract required `backend_aot_c_zrp_metadata_size.{h,c}` to own zrp metadata size/delta accounting, but that module did not exist yet.

## GREEN

- Production: added `backend_aot_c_zrp_metadata_size.{h,c}`.
- Production: moved `SZrAotZrpMetadataSizeStats`, zrp metadata header sampling, final `aot_size.zrpMetadata*` marker writing, and `code_stripping.zrpMetadata*Before/After/Removed` writing out of `backend_aot_c_emitter.c`.
- Production: `backend_aot_c_emitter.c` now includes the new narrow header and keeps only orchestration calls.
- Size: `backend_aot_c_emitter.c` is now 763 lines; `backend_aot_c_zrp_metadata_size.c` is 116 lines.

## Validation

- WSL gcc:
  direct `zr_vm_aot_c_code_stripping_test`: 4/0 passed.
  CTest filter `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 20/0 passed.
- WSL clang:
  after reconfiguring `build-wsl-clang` so the new globbed source entered the build, direct `zr_vm_aot_c_code_stripping_test`: 4/0 passed.
  same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 20/0 passed.
- Windows MSVC Debug:
  VsDevCmd import reported `VSCMD_VER=17.14.34`.
  direct `zr_vm_aot_c_code_stripping_test.exe`: 4/0 passed.
  same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test.exe`: 20/0 passed.

## Notes

- This is a support slice. The generated zrp metadata byte markers remain the 12-S7S behavior.
- 12-S7 remains open for actual metadata sweep/pruning, full trim analyzer behavior, attribute/annotation suppression, default-min metadata policy, and release symbol stripping.
