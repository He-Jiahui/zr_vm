# AOT Legacy Int Arithmetic Direct Writes

Date: 2026-06-05

## Scope

This slice moves the generated AOT C lowering for the legacy quickened integer arithmetic family onto direct C expressions:

- `ADD_INT`
- `ADD_INT_CONST`
- `SUB_INT`
- `SUB_INT_CONST`

The lowering now lives in `backend_aot_c_lowering_legacy_int_arithmetic.c` instead of growing `backend_aot_c_lowering_values.c`.

## RED

`test_aot_c_source_lowers_legacy_int_arithmetic_to_direct_c_expressions` was added to `tests/parser/test_aot_c_source_contracts.c`.

Initial failure:

```text
Missing source contract text: zr_aot_arith_exec_int
15 Tests 1 Failures 0 Ignored
```

## Implementation

The generated C now validates integer operands and emits direct expressions:

- `zr_aot_left_int + zr_aot_right_int`
- `zr_aot_left_int + zr_aot_right_literal`
- `zr_aot_left_int - zr_aot_right_int`
- `zr_aot_left_int - zr_aot_right_literal`

Const forms read the function constant table during C emission and format signed, unsigned, and bool integer-like constants as `TZrInt64` literals. Missing or unsupported constants emit an explicit generated error and exit through `ZR_AOT_C_FAIL()`.

The following generated-C helper fallback paths are now forbidden for this lowering module:

- `ZrLibrary_AotRuntime_AddIntConst`
- `ZrLibrary_AotRuntime_SubInt`
- `ZrLibrary_AotRuntime_SubIntConst`

## Boundary

The interpreter still has historical numeric fallback behavior for some `ADD_INT` / `SUB_INT` paths when operands are not integer values. This AOT C slice intentionally does not preserve that behavior through a helper fallback. The generated C covers the integer subset directly and fails explicitly outside that subset. Dynamic/generic arithmetic remains on its separate runtime-helper contracts.

## Validation

WSL GCC:

```text
cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
zr_vm_aot_c_source_contracts_test: 15 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 4 tests, 0 failures
```

The GCC build compiled `backend_aot_c_lowering_legacy_int_arithmetic.c.o`.

WSL Clang:

```text
cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
zr_vm_aot_c_source_contracts_test: 15 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 4 tests, 0 failures
```

The Clang build compiled `backend_aot_c_lowering_legacy_int_arithmetic.c.o`.

Windows MSVC CLI smoke:

```text
cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
zr_vm_cli.exe tests\fixtures\projects\hello_world\hello_world.zrp
hello world
```

The MSVC build compiled `backend_aot_c_lowering_legacy_int_arithmetic.c`.

## Files

- `tests/parser/test_aot_c_source_contracts.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_legacy_int_arithmetic.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
