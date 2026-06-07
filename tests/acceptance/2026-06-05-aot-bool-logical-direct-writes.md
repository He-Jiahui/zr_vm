# AOT Bool Logical Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for the bool binary logical bytecode opcodes onto direct C expressions:

- `LOGICAL_AND`
- `LOGICAL_OR`

The lowering lives in `backend_aot_c_lowering_generic_logical.c` next to generic primitive truthiness and equality.

## RED

`test_aot_c_source_lowers_bool_logical_and_or_to_direct_c` was added to `tests/parser/test_aot_c_logical_contracts.c`.

Initial failure:

```text
test_aot_c_source_lowers_generic_truthiness_to_direct_c:PASS
test_aot_c_source_lowers_generic_primitive_equality_to_direct_c:PASS
Missing source contract text: backend_aot_write_c_direct_logical_and(FILE *file
test_aot_c_source_lowers_bool_logical_and_or_to_direct_c:FAIL
3 Tests 1 Failures 0 Ignored
```

The old generated-C path still emitted `ZrLibrary_AotRuntime_LogicalAnd(state, &frame, ...)` and `ZrLibrary_AotRuntime_LogicalOr(state, &frame, ...)` from `backend_aot_c_function_body.c`.

## Implementation

The generated C now validates both operands are bool values, normalizes their stored `nativeBool` payloads, and emits direct C expressions:

- `zr_aot_left_bool && zr_aot_right_bool`
- `zr_aot_left_bool || zr_aot_right_bool`

The bool result is written with `ZR_VALUE_FAST_SET(zr_aot_destination, nativeBool, zr_aot_result, ZR_VALUE_TYPE_BOOL)`.

The following helper fallback paths are now forbidden for these bytecode opcodes in generated AOT C:

- `ZrLibrary_AotRuntime_LogicalAnd`
- `ZrLibrary_AotRuntime_LogicalOr`

## Boundary

This direct path is for the existing bool binary logical bytecode opcodes. Non-bool operands emit `unsupported AOT bool logical binary` and exit through `ZR_AOT_C_FAIL()`.

Source-level `&&` and `||` normally lower to short-circuit branch CFG before `LOGICAL_AND` / `LOGICAL_OR`. The shared-library smoke therefore validates the real frontend path by checking `zr_aot_jump_if_bool_false` and `zr_aot_bool_not_exec`, while the source-contract test locks the opcode direct-emitter shape.

## Validation

WSL GCC:

```text
cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_shared zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test -j 8
zr_vm_aot_c_logical_contracts_test: 3 tests, 0 failures
zr_vm_aot_c_logical_shared_library_smoke_test: 3 tests, 0 failures
```

The bool short-circuit smoke compiles a source project with `yes() && yes()`, `no() || yes()`, and `yes() && no()`, verifies generated C contains the short-circuit branch markers and omits the old `LogicalAnd` / `LogicalOr` helper strings, builds the generated C into a shared library, executes through `ZrLibrary_AotRuntime_ExecuteEntry`, and returns integer `13`.

WSL Clang:

```text
cmake --build build/codex-wsl-clang-debug --target zr_vm_parser_shared zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test -j 8
zr_vm_aot_c_logical_contracts_test: 3 tests, 0 failures
zr_vm_aot_c_logical_shared_library_smoke_test: 3 tests, 0 failures
```

The Clang build compiled `backend_aot_c_lowering_generic_logical.c.o`.

Windows MSVC focused contract smoke:

```text
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_parser_shared zr_vm_aot_c_logical_contracts_test -j 8
zr_vm_aot_c_logical_contracts_test.exe: 3 tests, 0 failures
```

Existing broad MSVC warnings remained outside this slice.

## Files

- `tests/parser/test_aot_c_logical_contracts.c`
- `tests/parser/test_aot_c_logical_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_generic_logical.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
