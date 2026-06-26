# AOT 12-S7L Type Layout Payload Byte Trim Delta

## Scope

- Slice: 12-S7L, under 12-S7 trim warnings and size statistics.
- Goal: generated AOT C reports byte-level before/after/removed statistics for referenced inline type-layout payload bytes under opt-in code stripping.
- Metric: `code_stripping.typeLayoutPayloadBytes*` sums each unique referenced inline layout's canonical `frameSlotLayout.byteSize`. This complements the existing emitted-C `aot_size.typeLayoutBytes[...]` and `aot_size.typeLayoutBytesTotal` markers.
- Non-goals: pre-trim generated-C emission byte span, full metadata sweep, byte-level zrp metadata trim diff, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test"`
- Result before implementation: `zr_vm_aot_c_code_stripping_test` ran 4 tests and failed 3 tests with `Expected Non-NULL`.
- Failure reason: the generated C reported `code_stripping.typeLayoutsBefore/After/Removed`, but did not report `code_stripping.typeLayoutPayloadBytesBefore/After/Removed`.

## GREEN

- Production: `backend_aot_c_type_layouts.c` now exposes `backend_aot_c_type_layout_payload_bytes_referenced()`, using the same unique inline-layout traversal as `backend_aot_c_type_layout_count_referenced()`.
- Production: `backend_aot_c_emitter.c` samples payload bytes before and after reachability filtering and emits removed bytes as a non-negative delta.
- Test: `test_aot_c_code_stripping.c` now asserts:
  - ordinary trim fixture: before 16, after 8, removed 8
  - export root and manifest root fixtures: before 16, after 16, removed 0
- Generated evidence: `build-wsl-gcc/tests_generated/aot_c_code_stripping/generated/static_callable_trim.c` contains:
  - `code_stripping.typeLayoutPayloadBytesBefore = 16`
  - `code_stripping.typeLayoutPayloadBytesAfter = 8`
  - `code_stripping.typeLayoutPayloadBytesRemoved = 8`

## Validation

- WSL gcc:
  CTest filter `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- WSL clang:
  same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- Windows MSVC Debug:
  same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test.exe`: 19/0 passed.

## Remaining

- 12-S7 remains open for full trim analyzer behavior, complete source spans, warning suppression, pre-trim generated-C byte span attribution, metadata sweep diff, and release symbol stripping.
