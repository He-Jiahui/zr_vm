# 2026-07-05 AOT 07-S2/S4 generic numeric i64 NEG local fold

## Scope

- Focused shape: `GET_CONSTANT int -> NEG -> RETURN`.
- Goal: extend the proven generic numeric signed integer scalar-local path to unary negation, emitting direct C `-`
  when the source and destination slots are already proven i64 locals.
- Non-goal: i64 DIV/MOD, u64 numeric arithmetic, mixed numeric arithmetic, dynamic/unproven operands, and broader
  value-copy migration still use existing conservative fallbacks.

## RED

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_neg_signed_int_local`.
- First WSL GCC focused run made generic numeric smoke 13 tests / 1 failure.
- Failure: generated C lacked `zr_aot_generic_numeric_i64_neg_scalar_local` and
  `zr_aot_s1 = -zr_aot_s0;`, still requiring the unary runtime boundary path for the focused signed-int shape.

## GREEN

- `backend_aot_c_lowering_generic_numeric_arithmetic.c` now tries a guarded i64 unary scalar-local NEG helper before
  the existing f64 helper and runtime `GenericNumericNeg` fallback.
- `backend_aot_c_scalar_locals.c` preserves immediate i64 constants for later generic NEG operands, records NEG
  destinations and exec writes as i64 locals, and treats generic NEG as an i64 local consumer only for the proven slot
  shape.
- Generated C for the focused shape now emits `zr_aot_scalar_constant_i64_local`,
  `zr_aot_generic_numeric_i64_neg_scalar_local`, direct `zr_aot_s1 = -zr_aot_s0;`, and direct i64 return.
- The focused generated C avoids `zr_aot_arith_exec_generic_numeric_unary_boundary`, `GenericNumericNeg`, and
  `SyncSignedIntLocal`.

## Verification

- WSL GCC `build-wsl-gcc`: generic numeric smoke 13/0, generic numeric contracts 1/0, source contracts 24/0, power
  smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy 4/0.
- WSL Clang `build-wsl-clang`: same matrix passed with 13/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug `build-msvc-aot-stack-copy`: focused targets built; generic numeric contracts 1/0, source
  contracts 24/0, and power contracts 2/0 passed. Unix-only shared-library smokes were expected ignored with 0
  failures: generic numeric 13 ignored, power 1 ignored, generic LOGICAL_NOT 8 ignored, generic equality stack-copy 4
  ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Remaining

- This closes only proven i64 scalar-local generic NEG in the focused straight-line shape.
- i64 generic DIV/MOD, u64 generic numeric arithmetic, mixed numeric arithmetic, dynamic/unproven operands, broader
  value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full
  zero-frame typed bodies remain open 07 work.
