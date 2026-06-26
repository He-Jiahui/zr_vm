# AOT 12-S7S ZRP Metadata Byte Trim Delta

## Scope

- Slice: 12-S7S, under 12-S7 trim warnings and size statistics.
- Goal: opt-in AOT C code stripping reports before/after/removed byte deltas for embedded zrp metadata size groups.
- Metrics: `code_stripping.zrpMetadataBytes*`, `code_stripping.zrpMetadataTokenRecordBytes*`, `code_stripping.zrpMetadataDefinitionTableBytes*`, and `code_stripping.zrpMetadataPoolBytes*`.
- Non-goals: actual reachability-pruned metadata rewriting, default-min metadata policy, full trim analyzer, attribute/annotation suppression, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test"`
- Result before implementation: `zr_vm_aot_c_code_stripping_test` ran 4 tests and failed 1 test with `Expected Non-NULL`.
- Failure reason: generated C reported `aot_size.zrpMetadata*` totals and section groups, but did not report code-stripping before/after/removed metadata byte markers.

## GREEN

- Production: `backend_aot_c_emitter.c` now collects zrp metadata byte stats into a reusable struct.
- Production: the generated C header emits code-stripping zrp metadata byte deltas before final `aot_size.*` markers.
- Production: current before and after values are equal because the writer still embeds the same zrp metadata blob; removed is reported as `0` until reachability-pruned metadata rewriting lands.
- Test: `test_aot_c_code_stripping.c` now asserts before/after/removed markers for total zrp metadata bytes, token records, definition tables, and pools.

## Validation

- WSL gcc:
  direct `zr_vm_aot_c_code_stripping_test`: 4/0 passed.
  CTest filter `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- WSL clang:
  direct `zr_vm_aot_c_code_stripping_test`: 4/0 passed.
  same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- Windows MSVC Debug:
  direct `zr_vm_aot_c_code_stripping_test.exe`: 4/0 passed.
  same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test.exe`: 19/0 passed.

## Notes

- The normal WSL clang build entry is still blocked by unrelated dirty `tests/CMakeLists.txt` entries for missing non-AOT test sources. Focused clang validation used existing fast targets after rebuilding the parser shared library.
- 12-S7 remains open for actual metadata sweep/pruning, full trim analyzer behavior, attribute/annotation suppression, default-min metadata policy, and release symbol stripping.
