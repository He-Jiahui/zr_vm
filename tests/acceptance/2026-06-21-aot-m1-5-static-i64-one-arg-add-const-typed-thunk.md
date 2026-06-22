# AOT M1.5 07-S5 Static I64 One-Arg Add-Const Typed Thunk

Date: 2026-06-22 06:38:57 +08:00

## Scope

This slice extends the narrow one-argument i64 typed direct-call route from identity returns to functions that return the i64 argument plus a signed integer constant.

Covered:

- The AOT C emitter recognizes typed single-parameter int64 functions that return `arg0 + signed-constant`.
- The recognizer handles the SemIR scalar binary shape emitted as `ADD_SIGNED_LOAD_STACK_CONST` followed by `FUNCTION_RETURN`.
- Generated C emits `zr_aot_typed_i64_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0)` forward declarations and definitions.
- The typed thunk returns `(TZrInt64)(zr_aot_arg0 + (TZrInt64)K)` directly.
- Function-body lowering reuses the existing static one-arg i64 direct-call route after proving the argument slot is an initialized i64 scalar local.
- Proven scalar-local-only destinations keep the previous stack-sync elision and do not emit `SZrTypeValue *zr_aot_typed_destination` or typed-destination `ZR_VALUE_FAST_SET`.
- The dedicated shared-library smoke executes `inc(value: int): int { return value + 1; }`, verifies runtime result `42`, and rejects `CallStaticDirect` / `CallStackValue` in this path.

Out of scope:

- Multiple typed arguments.
- General expression returns beyond this one-arg add-constant form.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The updated call contract first failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_try_get_i64_arg0_add_constant_return(`.

After the bytecode add-constant recognizer was introduced, the dedicated typed direct-call shared-library smoke exposed the SemIR shape:

- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: missing the one-arg typed thunk forward declaration for the `value + 1` callee.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 3/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_shared_library_smoke_test`: 8/0.
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: 1/0.
- `zr_vm_aot_c_power_contracts_test`: 2/0.
- `zr_vm_aot_c_power_shared_library_smoke_test`: 1/0.
- `zr_vm_aot_c_source_contracts_test`: 19/0.
- `zr_vm_aot_c_generic_numeric_contracts_test`: 1/0.
- `zr_vm_aot_c_generic_numeric_shared_library_smoke_test`: 1/0.
- `zr_vm_aot_c_global_shared_library_smoke_test`: 9/0.
- `zr_vm_aot_c_logical_contracts_test`: 4/0.
- `zr_vm_aot_c_logical_shared_library_smoke_test`: 4/0.
- `zr_vm_aot_c_typed_scalar_test`: 1/0.
- `zr_vm_aot_c_return_contracts_test`: 1/0.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.

## Notes

This is still a deliberately small typed-to-typed direct C call foothold. It proves one-arg i64 arithmetic-return thunking for the compiler shape currently produced by `value + constant`, but it does not generalize typed return ABI lowering or remove the remaining dynamic/deopt bridge work.
