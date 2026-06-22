# AOT M1.5 07-S5 Static Bool No-Arg Typed Thunk

Date: 2026-06-22 10:44:07 +08:00

## Scope

This slice adds the first bool typed direct-call route for static zero-argument callees that return a bool constant.

Covered:

- `backend_aot_c_typed_bool_thunks.{h,c}` recognizes zero-parameter bool callees that return a constant bool value.
- Generated C emits `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state)` forward declarations and definitions.
- `backend_aot_write_c_static_direct_bool_no_arg_function_call()` writes `zr_aot_bD = zr_aot_typed_bool_fn_N(state)`.
- The bool direct-call writer only syncs the destination value slot when scalar-local analysis says a later frame-backed consumer still needs it.
- `backend_aot_c_typed_direct_calls.{h,c}` now owns static typed direct-call route selection for the bool no-arg path and existing i64 no/one/two-arg paths.
- `tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c` executes `func yes(): bool { return true; }` through a static call, verifies result `42`, and rejects `CallStaticDirect`, `CallStackValue`, and typed destination sync in the scalar-local path.
- `backend_aot_c_typed_bool_thunks.c` is 103 physical / 85 non-empty lines.
- `backend_aot_c_typed_direct_calls.c` is 250 physical / 219 non-empty lines.
- `backend_aot_c_function_body.c` is 2109 physical / 2061 non-empty lines after moving route selection out.
- The bool smoke is 155 physical / 143 non-empty lines.

Out of scope:

- Bool parameters.
- One-argument or two-argument bool typed thunks.
- u64 or f64 typed thunk families.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.
- Starting 08-12 work.

## RED

The updated call contract failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function)`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 6/0.
- `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`: 1/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_call_contracts_test`: 6/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`: 1/0.
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

This is a narrow typed-to-typed foothold for bool no-argument constant-return callees. Wider typed return ABI lowering, bool parameters, inline structs, `in` / `out` writeback, and dynamic/deopt bridge work remain open.
