# AOT 12-S7R Generated Type Layout Byte Trim Delta

## Scope

- Slice: 12-S7R, under 12-S7 trim warnings and size statistics.
- Goal: opt-in AOT C code stripping reports generated-C type-layout byte spans before and after reachability filtering, plus the removed byte delta.
- Metrics: `code_stripping.typeLayoutGeneratedBytesBefore`, `code_stripping.typeLayoutGeneratedBytesAfter`, and `code_stripping.typeLayoutGeneratedBytesRemoved`.
- Non-goals: full trim analyzer, attribute/annotation suppression, metadata sweep diff, default-min metadata policy, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test"`
- Result before implementation: `zr_vm_aot_c_code_stripping_test` ran 4 tests and failed 3 tests with `Expected Non-NULL`.
- Failure reason: generated C reported referenced inline layout counts and payload-byte deltas, but did not report generated-C layout byte before/after/removed markers.

## GREEN

- Production: `backend_aot_c_type_layouts.c` now shares the referenced inline layout emission loop between real output and byte sampling.
- Production: `backend_aot_c_type_layout_generated_bytes_referenced()` emits referenced layouts into a temporary file and sums the same byte spans used by `aot_size.typeLayoutBytesTotal`.
- Production: `backend_aot_c_emitter.c` samples generated type-layout bytes before and after code stripping and emits a non-negative removed delta.
- Test: `test_aot_c_code_stripping.c` now installs resolvable prototype type-layout cache entries on the root fixture, asserts the new markers exist, requires ordinary trim to remove generated layout bytes, and requires export/manifest roots to remove none.
- Generated evidence: ordinary trim reported `1072 -> 536`, removed `536`, and `aot_size.typeLayoutBytesTotal = 536`; export and manifest root fixtures reported `1072 -> 1072`, removed `0`.

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

- The normal WSL clang build entry tried to regenerate the whole build tree and hit unrelated dirty `tests/CMakeLists.txt` entries for two missing non-AOT test sources. Focused clang validation rebuilt the parser shared library and related test targets through existing fast targets, then ran the same binaries and CTest filter successfully.
- 12-S7 remains open for full trim analyzer behavior, attribute/annotation suppression, metadata sweep diff, default-min metadata policy, and release symbol stripping.
