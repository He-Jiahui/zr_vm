# AOT M1.5 07-S5 Static Bool One-Arg Typed Thunk

Date: 2026-06-22 10:56:26 +08:00

## Scope

This slice extends the bool typed direct-call route to one-argument identity-return callees.

Covered:

- `backend_aot_c_typed_bool_thunks.c` recognizes one-parameter bool functions that return parameter slot 0 directly.
- Generated C emits `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrBool zr_aot_arg0)` forward declarations and definitions.
- The emitted one-arg bool thunk returns `zr_aot_arg0` directly.
- `backend_aot_write_c_static_direct_bool_one_arg_function_call()` writes `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA)`.
- The one-arg bool direct-call route proves `functionSlot + 1` is an initialized bool scalar local before emitting the typed direct call.
- Proven scalar-local-only destinations keep stack-sync elision and do not emit `SZrTypeValue *zr_aot_typed_destination` or typed-destination `ZR_VALUE_FAST_SET`.
- The bool shared-library smoke now executes `pass(flag: bool): bool { return flag; }`, verifies runtime result `42`, and rejects `CallStaticDirect` / `CallStackValue`.
- `backend_aot_c_typed_bool_thunks.c` is 144 physical / 121 non-empty lines.
- `backend_aot_c_typed_direct_calls.c` is 298 physical / 262 non-empty lines.
- `backend_aot_c_lowering_calls.c` is 391 physical / 371 non-empty lines.
- The bool smoke is 305 physical / 282 non-empty lines.

Out of scope:

- Bool expression returns beyond direct identity.
- Two-argument or variadic bool typed thunks.
- u64 or f64 typed thunk families.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.
- Starting 08-12 work.

## RED

The updated call contract failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function)`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 6/0.
- `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`: 2/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_call_contracts_test`: 6/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`: 2/0.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test`: 6/0.
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

This is a narrow typed-to-typed foothold for one-argument bool identity-return callees. Wider typed return ABI lowering, bool expression returns, inline structs, `in` / `out` writeback, and dynamic/deopt bridge work remain open.
