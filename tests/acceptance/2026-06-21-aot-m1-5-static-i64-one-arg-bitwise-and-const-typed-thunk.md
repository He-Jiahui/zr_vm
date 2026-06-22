# AOT M1.5 07-S5 Static I64 One-Arg Bitwise-And-Constant Typed Thunk

Date: 2026-06-22 09:34:08 +08:00

## Scope

This slice extends the one-argument i64 typed direct-call route to `return arg0 & signed-constant`.

Covered:

- The typed i64 thunk module recognizes typed one-parameter int64 functions that return a bitwise-and with a signed constant.
- The recognizer accepts a proven `GET_STACK` / `SET_STACK` parameter-copy prefix from parameter slot 0 before the constant and bitwise operation.
- The recognizer handles `GET_CONSTANT`, `BITWISE_AND`, and `FUNCTION_RETURN`, with the constant operand accepted on either side.
- Generated C emits `zr_aot_typed_i64_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0)` forward declarations and definitions.
- The typed thunk returns `(TZrInt64)(zr_aot_arg0 & (TZrInt64)K)` directly.
- Function-body lowering reuses the existing one-arg i64 direct-call proof that the stack argument slot is an initialized i64 scalar local.
- Proven scalar-local-only destinations keep stack-sync elision and do not emit `SZrTypeValue *zr_aot_typed_destination` or typed-destination `ZR_VALUE_FAST_SET`.
- The dedicated bitwise shared-library smoke executes `maskBy(value: int): int { return value & 47; }`, verifies runtime result `42`, and rejects `CallStaticDirect` / `CallStackValue` in this path.

Out of scope:

- General expression returns beyond this one-arg bitwise-and-constant form.
- Division, modulo, or shift direct thunks, because the current direct i64 return ABI cannot report divide-by-zero or shift-range failures.
- General multi-argument typed ABI.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The updated call contract failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_try_get_i64_arg0_bitwise_and_constant_return(`.

The first implementation then exposed a shape gap in the smoke test:

- The generated callee used a parameter-copy prefix before `GET_CONSTANT` and `BITWISE_AND`; the recognizer only accepted the shorter three-instruction form.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test`: 4/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test`: 4/0.
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

This slice reuses the one-argument direct-call gate and only broadens the callee body recognizer plus emitted thunk expression. Wider typed return ABI lowering and dynamic/deopt bridge work remain open.
