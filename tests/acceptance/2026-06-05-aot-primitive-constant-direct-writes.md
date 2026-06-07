# AOT Primitive Constant Direct Writes

## Scope

- Added direct generated-C lowering for immediate primitive `GET_CONSTANT` values: null, bool, signed integer, unsigned integer, and float.
- Added direct generated-C lowering for `SET_CONSTANT` by assigning the source stack value into the function constant table.
- Affected layers: archived AOT C value lowering, AOT C function-body dispatch, source-contract tests, hand-written ExecBC/AOT backend fixture expectations, generated-C shared-library smoke coverage, semantic documentation, and the active C# value-type/AOT milestone plan.

## Baseline

- Immediate primitive `GET_CONSTANT` generated a local `SZrTypeValue zr_aot_constant`, materialized the scalar into that temporary, then called `ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant)`.
- Generated AOT C `SET_CONSTANT` called `ZrLibrary_AotRuntime_SetConstant(state, &frame, ...)`, even though interpreter execution assigns the source value directly into the constant table.
- The hand-written ExecBC/AOT backend fixture expected C and LLVM to both expose `ZrLibrary_AotRuntime_SetConstant`.
- RED: GCC `zr_vm_aot_c_source_contracts_test` failed in `test_aot_c_source_lowers_primitive_constants_to_direct_value_writes` because `backend_aot_write_c_direct_primitive_constant` did not exist.

## Test Inventory

- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_aot_c_shared_library_smoke.c`
- `zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`

Boundary cases covered:

- Null constants lower through `ZrCore_Value_ResetAsNull(zr_aot_destination)`.
- Bool, signed integer, unsigned integer, and float constants lower through direct `ZR_VALUE_FAST_SET(zr_aot_destination, ...)`.
- Destination replacement first releases an existing owned or GC payload in the slot before writing the primitive scalar.
- Generated C no longer emits the old destination `ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant)` shape for immediate primitive constants.
- Generated C `SET_CONSTANT` emits `*zr_aot_constant = *zr_aot_source`, matching the interpreter's current raw assignment.
- Generated C no longer emits `ZrLibrary_AotRuntime_SetConstant` for `SET_CONSTANT`.
- Callable constants remain on the explicit materialization/runtime boundary.
- Non-immediate object/string/dynamic constants are not accepted into this direct primitive subset.
- LLVM still retains the current `ZrLibrary_AotRuntime_SetConstant` helper route; this slice updates the C backend only.

## Tooling Evidence

- GCC Debug build: `build/codex-wsl-gcc-debug`
- Clang Debug build: `build/codex-wsl-clang-debug`

Commands run:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_shared_library_smoke_test -j 8
./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_source_contracts_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_source_contracts_test
cmake --build build/codex-wsl-clang-debug --target zr_vm_aot_c_shared_library_smoke_test -j 8
./build/codex-wsl-clang-debug/bin/zr_vm_aot_c_shared_library_smoke_test
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c
gcc -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c
clang -std=c11 -fsyntax-only -DZR_PLATFORM_UNIX -DZR_DEBUG -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include -Izr_vm_library/include -Izr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared -j 8
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared -j 8
```

Attempted but not available in the current active CMake graph:

```bash
cmake --build build/codex-wsl-gcc-debug --target zr_vm_execbc_aot_pipeline_test -j 8
cmake --build build/codex-wsl-clang-debug --target zr_vm_execbc_aot_pipeline_test -j 8
```

Windows smoke commands:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

## Results

- RED: GCC `zr_vm_aot_c_source_contracts_test` failed before implementation because the direct primitive constant emitter was missing.
- PASS: GCC `zr_vm_aot_c_source_contracts_test`, 14 tests, 0 failures.
- PASS: Clang `zr_vm_aot_c_source_contracts_test`, 14 tests, 0 failures.
- PASS: GCC `zr_vm_aot_c_shared_library_smoke_test`, 4 tests, 0 failures. The primitive constant smoke verifies generated C contains `zr_aot_value_exec_primitive_constant`, omits the old copy/helper paths, compiles the generated C into a shared library, executes through the runtime loader, and returns integer `12`.
- PASS: Clang `zr_vm_aot_c_shared_library_smoke_test`, 4 tests, 0 failures for the same generated-C path.
- PASS: GCC and Clang syntax-only checks for `backend_aot_c_lowering_values.c` and `backend_aot_c_function_body.c`.
- PASS: GCC and Clang `zr_vm_parser_shared` builds.
- PASS: Windows MSVC CLI smoke in `build/codex-msvc-cli-debug`; the parser build compiled `backend_aot_c_function_body.c` and `backend_aot_c_lowering_values.c`, and `hello_world.zrp` printed `hello world`.
- NOTE: `zr_vm_execbc_aot_pipeline_test` is not present as a target in the current `build/codex-wsl-gcc-debug` or `build/codex-wsl-clang-debug` CMake graphs. The hand-written fixture source was updated to keep its C-side expectation aligned, but it could not be executed from these active build directories.

## Acceptance Decision

- Accepted for the narrow AOT C primitive constant and C-side `SET_CONSTANT` direct-write slice.

Remaining risks:

- LLVM still uses `ZrLibrary_AotRuntime_SetConstant`; LLVM parity is a separate follow-up.
- Direct `SET_CONSTANT` mirrors the interpreter's raw assignment and is not a general ownership-safe constant mutation model for object/string/managed constants.
- Callable and other non-immediate constants still require explicit materialization/runtime contracts before they can move to direct generated-C writes.
