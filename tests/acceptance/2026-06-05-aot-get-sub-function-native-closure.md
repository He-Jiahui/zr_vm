# AOT GET_SUB_FUNCTION Native Closure Lowering

Date: 2026-06-05

Scope: generated AOT C `GET_SUB_FUNCTION` lowering for statically known child functions.

## Contract

- Zero-capture child functions lower to direct generated native-closure allocation with the child function's generated thunk pointer.
- Generated C emits `zr_aot_value_exec_get_sub_function_native_closure`, resolves the destination slot, validates `frame.function->childFunctionList[childFunctionIndex]`, allocates `SZrClosureNative` with zero captures, assigns `zr_aot_closure->nativeFunction = zr_aot_fn_N`, and sets `aotShimFunction` to the child metadata function.
- Captured, unresolved, or missing child functions emit `zr_aot_value_unsupported_get_sub_function_materialization`, record `zr_aot_capture_count`, report `unsupported AOT GET_SUB_FUNCTION materialization`, and fail through `ZR_AOT_C_FAIL()`.
- Callable provenance remains valid only for the direct zero-capture child-closure path.
- Generated AOT C no longer emits `ZrLibrary_AotRuntime_GetSubFunction` from the function-body/value-lowering path. LLVM still keeps the helper route until LLVM parity and captured child-closure materialization are designed.

## RED

The focused contract was added before production code and failed as expected:

```text
Missing source contract text: backend_aot_write_c_direct_get_sub_function(
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_constant_contracts.c:87:test_aot_c_source_lowers_get_sub_function_to_direct_native_closure:FAIL:missing required source contract text
3 Tests 1 Failures 0 Ignored
```

## Validation

- GCC `zr_vm_aot_c_constant_contracts_test`: 3 tests, 0 failures.
- GCC nearby regressions: `zr_vm_aot_c_source_contracts_test` 17 tests, `zr_vm_aot_c_global_contracts_test` 1 test, `zr_vm_aot_c_shared_library_smoke_test` 6 tests, all 0 failures.
- Clang same focused set: source contracts 17 tests, constant contracts 3 tests, global contract 1 test, generated shared-library smoke 6 tests, all 0 failures.
- MSVC focused set in `build/codex-msvc-aot-constant-debug`: source contracts 17 tests, constant contracts 3 tests, global contract 1 test, all 0 failures.
- A C-backend source scan found no `ZrLibrary_AotRuntime_GetSubFunction` emission in `backend_aot_c_function_body.c` or `backend_aot_c_lowering_values.c`; remaining references are LLVM-only.

## Files

- `tests/parser/test_aot_c_constant_contracts.c`
- `zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
