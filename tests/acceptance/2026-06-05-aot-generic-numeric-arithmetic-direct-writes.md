# AOT Generic Numeric Arithmetic Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for the generic numeric arithmetic family onto direct C expressions:

- `ADD`
- `SUB`
- `MUL`
- `DIV`
- `MOD`
- `NEG`

The lowering now lives in `backend_aot_c_lowering_generic_numeric_arithmetic.c` instead of growing `backend_aot_c_lowering_values.c`.

## RED

`test_aot_c_source_lowers_generic_numeric_arithmetic_to_direct_c_expressions` was added to `tests/parser/test_aot_c_source_contracts.c`.

Initial failure:

```text
test_aot_c_source_lowers_generic_numeric_arithmetic_to_direct_c_expressions:FAIL: Expected Non-NULL
16 Tests 1 Failures 0 Ignored
```

The missing source was `backend_aot_c_lowering_generic_numeric_arithmetic.c`.

## Implementation

The generated C now emits direct numeric operations for float, signed integer, and unsigned integer runtime value tags:

- `zr_aot_left_float +|-|*|/ zr_aot_right_float`
- `zr_aot_left_int +|-|*|/|% zr_aot_right_int`
- `zr_aot_left_uint +|-|*|/|% zr_aot_right_uint`
- signed, unsigned, and float unary negation

`DIV` and `MOD` emit zero guards before evaluating the C operator. Unsupported nonnumeric, string, dynamic/meta, and unsupported float-modulo cases emit `unsupported AOT generic numeric arithmetic` and exit through `ZR_AOT_C_FAIL()` instead of falling back to a VM helper.

The following generated-C helper fallback paths are now forbidden for this lowering module:

- `ZrLibrary_AotRuntime_Add`
- `ZrLibrary_AotRuntime_Sub`
- `ZrLibrary_AotRuntime_Mul`
- `ZrLibrary_AotRuntime_Div`
- `ZrLibrary_AotRuntime_Mod`
- `ZrLibrary_AotRuntime_Neg`

## Boundary

This is a numeric-only AOT C subset for generic arithmetic opcodes. String concatenation, meta arithmetic, dynamic operands, and float modulo are explicit unsupported/dynamic boundaries until a typed SemIR/call lowering contract defines a direct non-helper AOT path for them.

`zr_vm_aot_c_shared_library_smoke_test` also gained a numeric arithmetic smoke that compiles and executes generated C containing direct arithmetic expressions. That fixture currently quickens to typed signed arithmetic, so it validates generated-C expression compilation/execution and absence of generic helper strings, while the source-contract test locks the generic opcode-specific lowering shape.

## Validation

WSL GCC:

```text
cmake -S . -B build/codex-wsl-gcc-debug
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
zr_vm_aot_c_source_contracts_test: 16 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 5 tests, 0 failures
```

WSL Clang:

```text
cmake -S . -B build/codex-wsl-clang-debug
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8
zr_vm_aot_c_source_contracts_test: 16 tests, 0 failures
zr_vm_aot_c_shared_library_smoke_test: 5 tests, 0 failures
```

The Clang build compiled `backend_aot_c_lowering_generic_numeric_arithmetic.c.o`; the GCC build graph had already picked up the same source after configure.

Windows MSVC CLI smoke:

```text
cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
zr_vm_cli.exe tests\fixtures\projects\hello_world\hello_world.zrp
hello world
```

The MSVC build compiled `backend_aot_c_lowering_generic_numeric_arithmetic.c`.

## Files

- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_aot_c_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_numeric_arithmetic.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
