# AOT Value SemIR Field-Aware Copy

## Scope

- Added a generated type-layout gate for AOT inline struct `COPY_VALUE` lowering so non-POD structs no longer default to raw `memmove`.
- Affected layers: archived AOT C value SemIR lowering, AOT C source-contract tests, M5/M6 plan text, and semantic documentation.

## Baseline

- Before this slice, `backend_aot_try_write_c_value_copy_exec()` emitted `memmove` for every matching inline struct copy when destination/source slot type layout id and byte size matched.
- That was valid for POD structs but too broad for structs with embedded `SZrTypeValue`, GC, ownership, or field-copy requirements.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed on missing `zr_aot_value_exec_inline_field_copy`.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_value_type_runtime.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c` constrained syntax checks

Boundary cases covered:

- Matching inline struct copies still bypass the old generic stack-copy helper.
- Generated C resolves `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, typeLayoutId, state)` and checks the expected byte size.
- `ZrCore_TypeLayout_CanRawCopy` keeps POD struct copies on `memmove`.
- Non-POD field-copy structs emit `zr_aot_value_exec_inline_field_copy` and call `ZrCore_TypeLayout_CopyInline`.
- Copy/drop finalization and whole-call/return non-POD ownership behavior remain future work.

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

- RED: GCC `zr_vm_aot_c_source_contracts_test` failed on missing `zr_aot_value_exec_inline_field_copy`.
- PASS: GCC build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: GCC `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: GCC `backend_aot_c_value_semir.c` syntax-only check with the archived `SZrAotWriterOptions` incomplete-type stub. GCC emitted the expected incomplete-type stub warnings.
- PASS: Clang build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 5 tests.
- PASS: Clang `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: Clang `backend_aot_c_value_semir.c` syntax-only check with the same archived `SZrAotWriterOptions` incomplete-type stub. Clang emitted the expected visibility warnings.

## Acceptance Decision

- Accepted for the narrow AOT inline struct field-aware copy source-lowering slice.

Remaining risks:

- This slice updates generated `COPY_VALUE` lowering only; it does not add non-POD drop/finalization at frame teardown or exception exits.
- Direct typed call/return lowering still targets the first POD subset; non-POD call/return copy/drop needs separate M6 work.
- The archived AOT include tree still needs a separate cleanup for the removed `SZrAotWriterOptions` type; constrained syntax checks use an incomplete-type stub to isolate this source file.
