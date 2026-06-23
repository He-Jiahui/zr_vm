# AOT M1.5 07-S5 Static I64 Two-Arg Divide Typed Thunk Acceptance

Date: 2026-06-23 16:50:50 +08:00
Status: Completed sub-slice; 07-S5/M1.5/07 remain partial; 08-12 not started.

## Scope

This slice adds the narrow signed two-argument typed direct-call route for `int` callees that return `left / right`.
It covers direct i64 division with two typed scalar parameters and does not claim the broader typed-return ABI.

## RED

- `zr_vm_aot_c_typed_call_contracts_test` first failed on missing source contract text:
  `backend_aot_c_try_get_i64_arg0_arg1_divide_return(`.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test` then failed the new `ratio(left, right)` case
  with `Expected Non-NULL`, proving the generated C still lacked an i64 divide typed thunk and direct call.

## Implementation

- `backend_aot_c_try_get_i64_arg0_arg1_divide_return()` now recognizes two-i64-parameter callees whose body is
  `DIV_SIGNED` followed by `FUNCTION_RETURN`.
- `backend_aot_c_can_emit_typed_i64_two_arg_thunk()` includes the divide shape.
- `backend_aot_write_c_typed_i64_thunks()` emits a two-argument `TZrInt64` thunk with a zero-denominator guard:
  `ZrCore_Debug_RunError(state, "generated AOT signed divide by zero")`, defensive `(TZrInt64)0`, and otherwise
  `(TZrInt64)(zr_aot_arg0 / zr_aot_arg1)`.
- The arithmetic shared-library smoke now executes `ratio(left: int, right: int): int { return left / right; }`,
  requires `zr_aot_static_i64_two_arg_direct_call`, and rejects `CallStaticDirect`, `CallStackValue`, and unnecessary
  typed-destination stack sync.

## GREEN

- Focused WSL GCC validation:
  - `zr_vm_aot_c_typed_call_contracts_test`: 4/0.
  - `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 6/0.
- Broader WSL GCC AOT validation:
  - Contracts: source 19/0, call 4/0, typed call 4/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0,
    frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0.
  - Shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, arithmetic 6/0, bool 26/0, u64 23/0,
    f64 19/0, global 9/0, logical 4/0, power 1/0.
  - The first all-smoke command exceeded the 240s tool timeout while the final power smoke was still linking; the
    leftover process exited, and the smoke group was rerun in smaller batches to completion.

## Open

- i64 modulo typed thunk parity.
- General typed-return ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridges.
- Full 07-S5 acceptance and stages 08-12.
