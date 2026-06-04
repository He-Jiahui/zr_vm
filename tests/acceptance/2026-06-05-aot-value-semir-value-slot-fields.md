# AOT Value SemIR Value-Slot Fields

## Scope

- Added the first direct AOT C lowering contract for embedded `SZrTypeValue` fields inside inline structs.
- Affected layers: archived AOT C value SemIR lowering, AOT C source-contract tests, M5 plan text, and semantic documentation.

## Baseline

- Before this slice, resolved non-primitive value SemIR field load/store sites were explicitly unsupported after the previous boundary slice.
- Runtime interpreter field access already supports embedded `SZrTypeValue` fields for string/object-like values through inline frame byte layout, but AOT source lowering did not expose a direct value-slot field copy path.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed on missing `backend_aot_c_value_field_layout_can_value_slot_exec`.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_value_type_runtime.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c` constrained syntax checks

Boundary cases covered:

- Primitive POD field load/store remains on the existing byte-field `memcpy` / `ZR_VALUE_FAST_SET` path.
- Embedded `SZrTypeValue` field load copies from inline field bytes into the scalar destination slot with `ZrCore_Value_Copy`.
- Embedded `SZrTypeValue` field store copies from the scalar source slot into inline field bytes with `ZrCore_Value_Copy`.
- Nested inline struct field transfers remain on the explicit unsupported boundary.
- Dynamic or unresolved member access remains outside this value SemIR boundary and can still use declared runtime contracts.

## Tooling Evidence

- GCC Debug build: `build/codex-wsl-gcc-debug`
- Clang Debug build: `build/codex-wsl-clang-debug`

Commands run:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_value_type_runtime_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test zr_vm_value_type_runtime_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
./build/codex-wsl-clang-debug/bin/zr_vm_value_type_runtime_test
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
```

## Results

- RED: GCC `zr_vm_aot_c_source_contracts_test` failed on missing `backend_aot_c_value_field_layout_can_value_slot_exec`.
- PASS: GCC build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: GCC `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: GCC `backend_aot_c_value_semir.c` syntax-only check with the archived `SZrAotWriterOptions` incomplete-type stub. GCC emitted the expected incomplete-type stub warnings.
- PASS: Clang build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: Clang `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: Clang `backend_aot_c_value_semir.c` syntax-only check with the same archived `SZrAotWriterOptions` incomplete-type stub. Clang emitted the expected visibility warnings.

## Acceptance Decision

- Accepted for the narrow embedded `SZrTypeValue` field AOT source-lowering slice.

Remaining risks:

- This slice covers field-level `SZrTypeValue` copy only; it does not implement whole-struct non-POD copy/drop wrappers or ownership-aware field destruction.
- Nested inline struct field transfer remains explicitly unsupported in generated AOT C until a dedicated layout-copy lowering is added.
- The archived AOT include tree still needs a separate cleanup for the removed `SZrAotWriterOptions` type; constrained syntax checks use an incomplete-type stub to isolate this source file.
