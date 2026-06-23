# AOT M1.5 07-S5 Static I64 Two-Arg Modulo Typed Thunk Acceptance

Date: 2026-06-23 17:11:47 +08:00
Status: Completed sub-slice; 07-S5/M1.5/07 remain partial; 08-12 not started.

## Scope

This slice adds the narrow signed two-argument typed direct-call route for `int` callees that return `left % right`.
It also performs the required arithmetic shared-library smoke support split so the test file does not keep growing past
the local 1000-line maintenance threshold.

## Support Split

- `tests/parser/aot_c_typed_direct_call_arithmetic_smoke_support.h` now owns the reusable i64 arithmetic shared-library
  smoke runner.
- `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c` now stores each case as data and calls
  the shared runner, dropping the main smoke source from roughly 900 lines to roughly 250 lines before adding modulo.
- The behavior-preserving support split was validated first with the existing arithmetic smoke passing 6/0.

## RED

- `zr_vm_aot_c_typed_call_contracts_test` first failed on missing source contract text:
  `backend_aot_c_try_get_i64_arg0_arg1_modulo_return(`.
- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test` then failed the new `remainder(left, right)`
  case with `Expected Non-NULL`, proving generated C still lacked an i64 modulo typed thunk and direct call.

## Implementation

- `backend_aot_c_try_get_i64_arg0_arg1_modulo_return()` now recognizes two-i64-parameter callees whose body is
  `MOD_SIGNED` followed by `FUNCTION_RETURN`.
- `backend_aot_c_can_emit_typed_i64_two_arg_thunk()` includes the modulo shape.
- `backend_aot_write_c_typed_i64_thunks()` emits a two-argument `TZrInt64` thunk with a zero-denominator guard:
  `ZrCore_Debug_RunError(state, "generated AOT signed modulo by zero")`, defensive `(TZrInt64)0`, and otherwise
  `(TZrInt64)(zr_aot_arg0 % zr_aot_arg1)`.
- The arithmetic shared-library smoke now executes `remainder(left: int, right: int): int { return left % right; }`,
  requires `zr_aot_static_i64_two_arg_direct_call`, and rejects `CallStaticDirect`, `CallStackValue`, and unnecessary
  typed-destination stack sync.

## GREEN

- Focused WSL GCC validation:
  - Support split: `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 6/0.
  - Final focused: `zr_vm_aot_c_typed_call_contracts_test`: 4/0.
  - Final focused: `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 7/0.
- Broader WSL GCC AOT validation:
  - Contracts: source 19/0, call 4/0, typed call 4/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0,
    frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0.
  - Shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, arithmetic 7/0, bool 26/0, u64 23/0,
    f64 19/0, global 9/0, logical 4/0, power 1/0.

## Open

- General typed-return ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridges.
- Full 07-S5 acceptance and stages 08-12.
