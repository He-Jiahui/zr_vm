# AOT M1.5 07-S5 Static I64 Two-Arg Typed Thunk

Date: 2026-06-22 07:04:09 +08:00

## Scope

This slice extends the narrow typed direct-call route with a two-argument i64 typed thunk for functions that return the sum of their first two i64 parameters.

Covered:

- The AOT C emitter recognizes typed two-parameter int64 functions that return `arg0 + arg1`.
- The recognizer handles `ADD_SIGNED`, `ADD_SIGNED_PLAIN_DEST`, and the current SemIR scalar binary `ADD_SIGNED_LOAD_STACK` form followed by `FUNCTION_RETURN`.
- Generated C emits `zr_aot_typed_i64_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1)` forward declarations and definitions.
- The typed thunk returns `(TZrInt64)(zr_aot_arg0 + zr_aot_arg1)` directly.
- Function-body lowering proves both stack argument slots, `functionSlot + 1` and `functionSlot + 2`, are initialized i64 scalar locals before routing the static call to the typed thunk.
- Proven scalar-local-only destinations keep the previous stack-sync elision and do not emit `SZrTypeValue *zr_aot_typed_destination` or typed-destination `ZR_VALUE_FAST_SET`.
- The dedicated shared-library smoke executes `sum(left: int, right: int): int { return left + right; }`, verifies runtime result `42`, and rejects `CallStaticDirect` / `CallStackValue` in this path.

Out of scope:

- General multi-argument typed ABI beyond this two-i64 add-return shape.
- General expression returns.
- Inline struct direct call/return lowering.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

The updated call contract failed before implementation:

- `zr_vm_aot_c_call_contracts_test`: missing `backend_aot_c_can_emit_typed_i64_two_arg_thunk(const SZrFunction *function)`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_call_contracts_test`: 5/0.
- `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 4/0.

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

A first combined focused build-and-test command timed out without useful output, so the same focused validation was split into shorter build/test commands. The split runs passed. This remains a deliberately small typed-to-typed direct C call foothold and does not generalize typed return ABI lowering or remove the remaining dynamic/deopt bridge work.
