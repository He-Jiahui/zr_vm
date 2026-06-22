# AOT M1.5 07-S5 Static U64 No-Arg Typed Thunk

Date: 2026-06-22 11:20:28 +08:00

## Scope

This slice adds a no-argument unsigned integer typed direct-call route.

Covered:

- `backend_aot_c_typed_u64_thunks.{h,c}` recognizes zero-parameter functions with unsigned integer callable return metadata and a constant `FUNCTION_RETURN`.
- The recognizer accepts current `uint` surface shapes, including `UINT32/U32` metadata and non-negative signed literal constants.
- Generated C emits `static TZrUInt64 zr_aot_typed_u64_fn_N(struct SZrState *state)` forward declarations and definitions.
- The emitted thunk returns the constant as `TZrUInt64`.
- `backend_aot_write_c_static_direct_u64_no_arg_function_call()` writes `zr_aot_uD = zr_aot_typed_u64_fn_N(state)`.
- Proven scalar-local-only destinations keep stack-sync elision and do not emit `SZrTypeValue *zr_aot_typed_destination` or typed-destination `ZR_VALUE_FAST_SET`.
- The no-arg typed direct-call router checks u64 before i64 in the no-arg helper path.
- The u64 shared-library smoke executes `answer(): uint { return 37; }`, verifies runtime result `42`, and rejects `CallStaticDirect` / `CallStackValue`.
- `backend_aot_c_typed_u64_thunks.c` is 118 physical / 99 non-empty lines.
- `backend_aot_c_typed_direct_calls.c` is 345 physical / 304 non-empty lines.
- `backend_aot_c_lowering_calls.c` is 426 physical / 404 non-empty lines.
- The u64 smoke is 152 physical / 140 non-empty lines.

Out of scope:

- u64 parameter typed thunks.
- u64 expression returns beyond no-arg constants.
- f64 typed thunk routes.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.
- Starting 08-12 work.

## RED

The updated call contract failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_can_emit_typed_u64_no_arg_thunk(const SZrFunction *function)`.

Interim smoke failures exposed the required `uint` surface handling and no-arg router ordering:

- `uint` currently appears as `UINT32/U32` metadata and may use a non-negative signed literal constant.
- The no-arg typed direct-call helper path must check u64 before i64 to avoid falling back to `CallStaticDirect`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 7/0.
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 1/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_call_contracts_test`: 7/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`: 2/0.
- `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 1/0.
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

This is a narrow typed-to-typed foothold for no-argument u64 constant-return callees. Wider u64 typed return ABI lowering, f64, inline structs, `in` / `out` writeback, and dynamic/deopt bridge work remain open.
