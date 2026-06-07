# AOT GET_GLOBAL Direct Writes

Date: 2026-06-05

## Scope

This slice moves generated AOT C lowering for `GET_GLOBAL` off the AOT runtime helper and onto direct generated C. It preserves the current VM contract: copy `state->global->zrObject` into the destination when the global object exists and is an object value; otherwise reset the destination slot to null.

## RED

`tests/parser/test_aot_c_global_contracts.c` was added as a focused source-contract target instead of extending the already large AOT source-contract file.

Initial failure:

```text
/mnt/e/Git/zr_vm/tests/parser/test_aot_c_global_contracts.c:87:test_aot_c_source_lowers_get_global_to_direct_c:FAIL:missing required source contract text
```

The failure showed that generated C still lacked the global include/direct global-object access contract and still routed `GET_GLOBAL` through `ZrLibrary_AotRuntime_GetGlobal`.

## Implementation

`backend_aot_write_c_direct_get_global()` now emits a direct generated-C block. It resolves the destination with `ZrCore_Stack_GetValue(frame.slotBase + slot)`, checks `state->global != ZR_NULL` and `state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT`, copies the global object with `ZrCore_Value_Copy`, and resets the destination to null when no valid global object is available.

Generated AOT C modules now include `zr_vm_core/global.h` so downstream compilation can dereference `state->global->zrObject`.

## Boundary

This covers the C backend `GET_GLOBAL` path only. LLVM still declares and calls `ZrLibrary_AotRuntime_GetGlobal` until LLVM parity is handled.

## Validation

WSL GCC:

```text
zr_vm_aot_c_global_contracts_test: 1 test, 0 failures
zr_vm_aot_c_global_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_logical_contracts_test: 4 tests, 0 failures
zr_vm_aot_c_shift_contracts_test: 1 test, 0 failures
zr_vm_aot_c_float_contracts_test: 1 test, 0 failures
zr_vm_aot_c_generic_numeric_contracts_test: 1 test, 0 failures
zr_vm_aot_c_power_contracts_test: 2 tests, 0 failures
```

WSL Clang:

```text
zr_vm_aot_c_global_contracts_test: 1 test, 0 failures
zr_vm_aot_c_global_shared_library_smoke_test: 1 test, 0 failures
zr_vm_aot_c_source_contracts_test: 17 tests, 0 failures
zr_vm_aot_c_logical_contracts_test: 4 tests, 0 failures
zr_vm_aot_c_shift_contracts_test: 1 test, 0 failures
zr_vm_aot_c_float_contracts_test: 1 test, 0 failures
zr_vm_aot_c_generic_numeric_contracts_test: 1 test, 0 failures
zr_vm_aot_c_power_contracts_test: 2 tests, 0 failures
```

Windows MSVC:

```text
zr_vm_aot_c_global_contracts_test.exe: 1 test, 0 failures
zr_vm_aot_c_global_shared_library_smoke_test.exe: 1 test, 0 failures, 1 ignored
```

A source scan found no generated-C `ZrLibrary_AotRuntime_GetGlobal` emission in the AOT C value/function-body lowering sources. The remaining `GetGlobal` helper references are LLVM-only.

## Files

- `tests/parser/test_aot_c_global_contracts.c`
- `tests/parser/test_aot_c_global_shared_library_smoke.c`
- `tests/CMakeLists.txt`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
- `docs/parser-and-semantics/csharp-value-type-semir-aot.md`
- `.codex/plans/CSharp 值类型与 IL2CPP 风格 SemIR lowering 计划.md`
- `.codex/sessions/20260605-041222-csharp-value-aot-continuation.md`
