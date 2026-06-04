# AOT Value SemIR Nested Inline Fields

## Scope

- Added the first direct AOT C lowering contract for layout-matched nested inline struct field load/store.
- Affected layers: archived AOT C value SemIR lowering, AOT C source-contract tests, M5 plan text, and semantic documentation.

## Baseline

- Before this slice, nested inline struct field load/store stayed on the explicit unsupported boundary introduced for resolved-but-unsupported value fields.
- Runtime interpreter tests already cover nested struct field construction, copy, and mutation through inline frame bytes, but AOT source lowering did not expose a direct nested inline field transfer path.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed on missing `backend_aot_c_value_field_layout_can_inline_struct_exec`.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_value_type_runtime.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c` constrained syntax checks

Boundary cases covered:

- Primitive POD field load/store remains on the byte-field scalar path.
- Embedded `SZrTypeValue` field load/store remains on the direct `ZrCore_Value_Copy` path.
- Nested inline struct field load copies from `frame.slotBase + baseOffset + fieldOffset` into the inline destination slot with `memmove`.
- Nested inline struct field store copies from the inline source slot into `frame.slotBase + baseOffset + fieldOffset` with `memmove`.
- Nested inline struct transfer requires matching `typeLayoutId` and byte size; mismatches stay on the explicit unsupported boundary.
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

- RED: GCC `zr_vm_aot_c_source_contracts_test` failed on missing `backend_aot_c_value_field_layout_can_inline_struct_exec`.
- PASS: GCC build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: GCC `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: GCC `backend_aot_c_value_semir.c` syntax-only check with the archived `SZrAotWriterOptions` incomplete-type stub. GCC emitted the expected incomplete-type stub warnings.
- PASS: Clang build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: Clang `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: Clang `backend_aot_c_value_semir.c` syntax-only check with the same archived `SZrAotWriterOptions` incomplete-type stub. Clang emitted the expected visibility warnings.

## Acceptance Decision

- Accepted for the narrow nested inline struct field AOT source-lowering slice.

Remaining risks:

- This slice only covers layout-matched nested field transfer where the nested field and inline source/destination slot share `typeLayoutId` and byte size.
- Whole-struct non-POD copy/drop wrappers and ownership-aware destruction remain future M6 work.
- The archived AOT include tree still needs a separate cleanup for the removed `SZrAotWriterOptions` type; constrained syntax checks use an incomplete-type stub to isolate this source file.
