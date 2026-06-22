# AOT M1.5 07-S5 Static I64 One-Arg Typed Thunk

Date: 2026-06-22 05:56:00 +08:00

## Scope

This slice extends the narrow typed-to-typed direct C call path from no-arg constant returns to one int64 argument identity returns.

Covered:

- The AOT C emitter recognizes typed single-parameter int64 functions whose body returns parameter slot 0.
- Generated C emits `zr_aot_typed_i64_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0)` forward declarations and definitions.
- Function-body lowering routes eligible static one-arg i64 calls directly to the typed thunk after proving `functionSlot + 1` is an initialized i64 scalar local.
- The direct writer assigns the destination scalar local from `zr_aot_typed_i64_fn_N(state, zr_aot_sArg)` and syncs the current destination stack slot with `ZR_VALUE_FAST_SET` for remaining frame-backed consumers.
- The dedicated shared-library smoke executes `echo(value: int): int { return value; }`, verifies runtime result `42`, and rejects the old helper call path in generated source.

Out of scope:

- Multiple typed arguments.
- Non-identity or general typed returns.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Removing the temporary destination stack-slot sync.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The updated call contract first failed because generated source and backend declarations lacked:

- `backend_aot_c_can_emit_typed_i64_one_arg_thunk(const SZrFunction *function)`.
- `backend_aot_write_c_static_direct_i64_one_arg_function_call(FILE *file, ...)`.
- `backend_aot_c_try_get_i64_identity_return(...)`.
- One-arg typed thunk forward declarations/definitions.
- The generated `zr_aot_static_i64_one_arg_direct_call` marker and `zr_aot_typed_i64_fn_N(state, zr_aot_sArg)` call shape.

The typed direct-call shared-library smoke also failed before implementation because generated C had no one-arg typed thunk/direct-call shape.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 2/0.

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

This is still a deliberately small typed direct-call foothold. The call argument now crosses the typed-to-typed boundary as a C `TZrInt64`, but only for the one-arg identity-return form. The stack-slot sync remains because parts of the current generated pipeline still consume frame-backed values after the direct call.
