# AOT Public ABI And Writer Options

## Scope

- Promoted the generated AOT module ABI header into the real public include tree.
- Restored AOT writer options and AOT C/LLVM writer entrypoint declarations to the public parser writer header.
- Affected layers: `zr_vm_common` public headers, `zr_vm_parser` writer API, archived AOT C emitter source contract, generated-C syntax validation, semantic documentation, and the active C# value-type/AOT milestone plan.

## Baseline

- Generated AOT C includes `zr_vm_common/zr_aot_abi.h`, but the real `zr_vm_common/include/zr_vm_common` tree did not contain that header.
- Previous syntax checks of `backend_aot_c_emitter.c` either failed on the missing header or had to use a `.codex/minimal-verify-current` header copy, which masked the public include-surface gap.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in `test_aot_c_generated_abi_header_is_public` with `Expected Non-NULL`.
- After the ABI header was added, `backend_aot_c_emitter.c` still needed a temporary `SZrAotWriterOptions` typedef for syntax checks because `writer.h` did not declare the public AOT writer options type.
- RED: GCC `zr_vm_aot_c_source_contracts_test` then failed in `test_aot_c_writer_options_are_public` on missing `#include "zr_vm_common/zr_aot_abi.h"` in `writer.h`.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
- `zr_vm_parser/include/zr_vm_parser/writer.h`
- `zr_vm_common/include/zr_vm_common/zr_aot_abi.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c` constrained syntax checks
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c` constrained syntax checks
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c` constrained syntax checks
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c` constrained syntax checks
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c` constrained syntax checks
- `tests/parser/test_value_type_runtime.c`

Boundary cases covered:

- The ABI header must live under `zr_vm_common/include/zr_vm_common/zr_aot_abi.h`.
- The header exposes backend kind, input kind, thunk, descriptor, loader function pointer, ABI version, and export macro declarations consumed by generated C.
- `writer.h` exposes `SZrAotWriterOptions`, its source/binary hashes, embedded module blob fields, strict lowering flag, and AOT C/LLVM writer option entrypoints.
- `backend_aot_c_emitter.c` must include the same public header and emit `ZrAotCompiledModule`, `ZR_VM_AOT_ABI_VERSION`, `ZR_AOT_BACKEND_KIND_C`, and `ZrVm_GetAotCompiledModule`.
- Syntax checks use the real public include paths without a local `SZrAotWriterOptions` typedef.

## Tooling Evidence

- GCC Debug build: `build/codex-wsl-gcc-debug`
- Clang Debug build: `build/codex-wsl-clang-debug`

Commands run:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_value_type_runtime_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
./build/codex-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test zr_vm_value_type_runtime_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
./build/codex-wsl-clang-debug/bin/zr_vm_value_type_runtime_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
```

## Results

- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before implementation because the public ABI header was missing.
- RED: GCC `zr_vm_aot_c_source_contracts_test` then failed before the writer API fix because `writer.h` did not publish AOT writer options.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 8 tests.
- PASS: GCC `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 8 tests.
- PASS: Clang `zr_vm_value_type_runtime_test`, 13 tests.
- PASS: GCC and Clang syntax-only checks for `backend_aot_c_emitter.c` with the real `zr_vm_common/include` and `zr_vm_parser/include` paths.
- PASS: GCC and Clang syntax-only checks for the cleanup, function-body, value-SemIR, and control lowering sources without a local `SZrAotWriterOptions` typedef.

## Acceptance Decision

- Accepted for the narrow generated AOT C public ABI and writer-options slice.

Remaining risks:

- This does not complete generated C fixture compilation or link/load execution.
- The generated C fixture still needs a compile/link/load smoke that exercises an emitted module, not only backend source syntax.
