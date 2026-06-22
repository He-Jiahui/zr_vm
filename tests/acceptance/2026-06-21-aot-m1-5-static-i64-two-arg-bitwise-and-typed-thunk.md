# AOT M1.5 07-S5 Static I64 Two-Arg Bitwise-And Typed Thunk

Date: 2026-06-22 08:34:57 +08:00

## Scope

This slice extends the two-argument i64 typed direct-call route to signed integer bitwise-and.

Covered:

- The typed i64 thunk module recognizes typed two-parameter int64 functions that return `arg0 & arg1`.
- The recognizer handles `BITWISE_AND` followed by `FUNCTION_RETURN`.
- Generated C emits `zr_aot_typed_i64_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1)` forward declarations and definitions.
- The typed thunk returns `(TZrInt64)(zr_aot_arg0 & zr_aot_arg1)` directly.
- Function-body lowering reuses the existing two-arg i64 direct-call proof that both stack argument slots are initialized i64 scalar locals.
- Proven scalar-local-only destinations keep the previous stack-sync elision and do not emit `SZrTypeValue *zr_aot_typed_destination` or typed-destination `ZR_VALUE_FAST_SET`.
- The dedicated arithmetic shared-library smoke executes `mask(left: int, right: int): int { return left & right; }`, verifies runtime result `42`, and rejects `CallStaticDirect` / `CallStackValue` in this path.

Out of scope:

- General expression returns beyond this two-arg bitwise-and form.
- General multi-argument typed ABI.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The updated call contract failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_try_get_i64_arg0_arg1_bitwise_and_return(`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 5/0.

Broader WSL GCC focused AOT group:

- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
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

This slice reuses the two-argument direct-call gate and only broadens the callee body recognizer plus emitted thunk expression. Wider typed return ABI lowering and dynamic/deopt bridge work remain open. After this slice, the arithmetic typed direct-call smoke is 863 physical lines, so the next broad arithmetic expansion should split that file first.
