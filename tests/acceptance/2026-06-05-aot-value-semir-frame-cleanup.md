# AOT Value SemIR Frame Cleanup

## Scope

- Added the first generated-frame cleanup boundary for archived AOT C value SemIR lowering.
- Affected layers: archived AOT C emission, generated function body exits, value SemIR typed return emission, source-contract tests, M6 plan text, and semantic documentation.

## Baseline

- Before this slice, generated AOT functions could return directly from fail, unsupported-dispatch, normal return, tail return, or typed inline return paths.
- Direct returns bypassed any generated layout-aware inline frame cleanup, which blocks non-POD value-type ownership work.
- RED: GCC `zr_vm_aot_c_source_contracts_test` first failed on missing generated cleanup markers, then on missing `backend_aot_c_frame_cleanup.*`.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_value_type_runtime.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c` constrained syntax checks
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c` constrained syntax checks
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c` constrained syntax checks
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c` constrained syntax checks

Boundary cases covered:

- Generated C now uses `ZR_AOT_C_RETURN(expr)` plus a shared `zr_aot_function_exit` label instead of direct protected returns.
- `ZR_AOT_C_FAIL()` routes through the shared generated exit.
- Generated function bodies track `zr_aot_frame_started`, `zr_aot_return_value`, and `zr_aot_skip_drop_slot`.
- Cleanup is emitted by `backend_aot_c_frame_cleanup.*`, walks inline struct frame slots in reverse order, resolves `SZrTypeLayout`, checks `dropKind != ZR_TYPE_LAYOUT_DROP_KIND_NONE`, and calls `ZrCore_TypeLayout_DropInline`.
- Typed inline return keeps the source slot alive by setting `zr_aot_skip_drop_slot` before returning through the shared exit.
- Hidden-return ownership, exception-aware partial initialization, and generated C fixture compilation remain future M6 work.

## Tooling Evidence

- GCC Debug build: `build/codex-wsl-gcc-debug`
- Clang Debug build: `build/codex-wsl-clang-debug`

Commands run:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_value_type_runtime_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test zr_vm_value_type_runtime_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
./build/codex-wsl-clang-debug/bin/zr_vm_value_type_runtime_test
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG "-DSZrAotWriterOptions=struct SZrAotWriterOptions" -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
```

## Results

- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before implementation on missing generated cleanup module and shared-exit source-contract markers.
- PASS: GCC build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 6 tests.
- PASS: GCC `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: GCC syntax-only checks for the cleanup, function-body, value-SemIR, and control lowering sources with the archived `SZrAotWriterOptions` incomplete-type stub. GCC emitted the expected incomplete-type stub warnings.
- PASS: Clang build of `zr_vm_aot_c_source_contracts_test` and `zr_vm_value_type_runtime_test`.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 6 tests.
- PASS: Clang `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: Clang syntax-only checks for the same focused AOT C sources with the archived `SZrAotWriterOptions` incomplete-type stub. Clang emitted the expected visibility warnings.

## Acceptance Decision

- Accepted for the narrow AOT generated-frame cleanup source-contract slice.

Remaining risks:

- This is a cleanup boundary only; it does not complete hidden-return copy/drop ownership.
- Partial initialization and exception-aware cleanup still need separate M6 work.
- The archived AOT include tree still needs separate cleanup for the removed `SZrAotWriterOptions` type; constrained syntax checks use a stub to isolate these source files.
