# AOT Value SemIR Unsupported Fields

## Scope

- Added an explicit generated AOT failure boundary for resolved value SemIR field load/store sites outside the first primitive POD executable subset.
- Affected layers: archived AOT C value SemIR lowering, AOT C source-contract tests, M5 plan text, and semantic documentation.

## Baseline

- Before this slice, `backend_aot_try_write_c_value_field_load_exec()` and `backend_aot_try_write_c_value_field_store_exec()` returned `ZR_FALSE` when a resolved inline struct field was not primitive POD or when the source/destination slot was itself inline struct storage.
- Returning `ZR_FALSE` let `backend_aot_c_function_body.c` continue to the old `ZrLibrary_AotRuntime_GetMemberSlot` / `ZrLibrary_AotRuntime_SetMemberSlot` path, which hid the fact that AOT had already proven a value-type field place but could not directly lower it.
- RED: `zr_vm_aot_c_source_contracts_test` failed on missing `zr_aot_value_unsupported_field_load`, proving the new source contract was not present before production code changed.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c` constrained syntax checks
- `tests/parser/test_value_type_runtime.c` focused interpreter guard

Boundary cases covered:

- Primitive POD field load/store remains on the executable byte-field path.
- Resolved inline struct field load/store outside primitive POD now emits a generated unsupported path.
- Nested inline-struct payloads, managed/reference `SZrTypeValue` fields, and inline struct source/destination transfers are represented by the locked `fieldLayout.isPrimitivePod`, `fieldLayout.isValueSlot`, and `fieldLayout.typeLayoutId` contract markers.
- Dynamic or unresolved member access remains outside this value SemIR boundary and can still use declared runtime contracts.

## Tooling Evidence

- GCC Debug build: `build/codex-wsl-gcc-debug`
- Clang Debug build: `build/codex-wsl-clang-debug`

Commands run:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
cmake --build build/codex-wsl-gcc-debug --target zr_vm_value_type_runtime_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test zr_vm_value_type_runtime_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
./build/codex-wsl-clang-debug/bin/zr_vm_value_type_runtime_test
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
```

## Results

- RED: GCC `zr_vm_aot_c_source_contracts_test` failed on missing `zr_aot_value_unsupported_field_load`.
- PASS: GCC build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: GCC `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: GCC `backend_aot_c_value_semir.c` syntax-only check with the archived `SZrAotWriterOptions` incomplete-type stub. The check first failed without the stub because `backend_aot_internal.h` still exposes the archived writer-options type.
- PASS: Clang build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`. The first build command timed out while still running; a follow-up incremental build completed and linked both targets successfully.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: Clang `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: Clang `backend_aot_c_value_semir.c` syntax-only check with the same archived `SZrAotWriterOptions` incomplete-type stub. Clang emitted the expected visibility warnings for the stubbed incomplete type.

## Acceptance Decision

- Accepted for the narrow AOT value SemIR unsupported-field boundary slice.

Remaining risks:

- This slice does not implement direct AOT lowering for nested struct fields or managed/reference value-slot fields; it makes those proven value-type cases fail explicitly until their lowering is designed.
- Dynamic or unresolved member access remains on existing runtime contracts.
- The archived AOT include tree still needs a separate cleanup for the removed `SZrAotWriterOptions` type; the constrained syntax checks use an incomplete-type stub to isolate this source file.
