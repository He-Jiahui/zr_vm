# AOT Callable Constant Materialization Boundary

Date: 2026-06-05

Scope: generated AOT C `GET_CONSTANT` lowering for callable constants whose target cannot be resolved to a generated function thunk at compile time.

## Contract

- Resolved callable constants continue to lower through direct generated native-closure construction.
- Immediate null/bool/signed/unsigned/float constants continue to lower through direct destination writes.
- Plain constants continue to copy directly from the function constant table.
- Unresolved callable constant materialization no longer emits `ZrLibrary_AotRuntime_CopyConstant` from the generated-C function body.
- Instead, generated C emits `zr_aot_value_unsupported_callable_constant_materialization`, resolves the constant source and destination slot, checks callable metadata, reports `unsupported AOT callable constant materialization`, and exits through `ZR_AOT_C_FAIL()`.
- LLVM still keeps its `ZrLibrary_AotRuntime_CopyConstant` route until LLVM parity and real generated callable-thunk materialization are designed.

## RED

The focused contract was added before production code and failed as expected:

```text
Missing source contract text: backend_aot_write_c_unsupported_callable_constant_materialization(
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_constant_contracts.c:87:test_aot_c_source_makes_unresolved_callable_constant_explicit_boundary:FAIL:missing required source contract text
1 Tests 1 Failures 0 Ignored
```

## Validation

- GCC `zr_vm_aot_c_constant_contracts_test`: 1 test, 0 failures.
- GCC nearby regressions: `zr_vm_aot_c_source_contracts_test` 17 tests, `zr_vm_aot_c_global_contracts_test` 1 test, `zr_vm_aot_c_shared_library_smoke_test` 6 tests, all 0 failures.
- Clang same focused set: source contracts 17 tests, constant contract 1 test, global contract 1 test, generated shared-library smoke 6 tests, all 0 failures.
- MSVC focused set in `build/codex-msvc-aot-constant-debug`: source contracts 17 tests, constant contract 1 test, global contract 1 test, all 0 failures.
- A C-backend source scan found no `ZrLibrary_AotRuntime_CopyConstant` emission in `backend_aot_c_function_body.c` or `backend_aot_c_lowering_values.c`; remaining references are LLVM-only.

## Files

- `tests/parser/test_aot_c_constant_contracts.c`
- `tests/CMakeLists.txt`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
