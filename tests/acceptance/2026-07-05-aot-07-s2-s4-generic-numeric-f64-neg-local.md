# 2026-07-05 AOT 07-S2/S4 generic numeric f64 NEG local fold

## Scope

- Focused shape: `GET_CONSTANT float -> NEG -> RETURN`.
- Goal: prove a generic numeric unary NEG whose source is already a written f64 scalar local can emit direct C unary
  arithmetic and avoid the runtime generic numeric boundary.
- Non-goal: dynamic/unproven numeric operands, non-f64 generic numeric paths, and broader value-copy migration still
  use existing conservative fallbacks.

## RED

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_neg_float_local`.
- First WSL GCC focused run made generic numeric smoke 8 tests / 1 failure.
- Failure: generated C kept the float source in a value slot and still called
  `ZrLibrary_AotRuntime_GenericNumericNeg(state, &frame, 1, 0)` through
  `zr_aot_arith_exec_generic_numeric_unary_boundary`; it lacked both `zr_aot_scalar_constant_f64_local` and
  `zr_aot_generic_numeric_f64_neg_scalar_local`.

## GREEN

- `backend_aot_c_lowering_generic_numeric_arithmetic.c` now tries a guarded f64 scalar-local NEG helper before the
  `GenericNumericNeg` boundary fallback.
- `backend_aot_c_scalar_locals.c` records generic NEG as an f64 destination/write/consumer when the source is proven
  f64, and preserves f64 constants that feed a later NEG scalar consumer.
- Generated C for the focused shape now emits `zr_aot_scalar_constant_f64_local`, `zr_aot_f0 = (TZrFloat64)7.5;`,
  `zr_aot_generic_numeric_f64_neg_scalar_local`, and `zr_aot_f1 = -zr_aot_f0;`.
- The focused generated C avoids `zr_aot_arith_exec_generic_numeric_unary_boundary`, `GenericNumericNeg`, and
  `SyncFloatLocal`.

## Verification

- WSL GCC `build-wsl-gcc`: generic numeric smoke 8/0, generic numeric contracts 1/0, source contracts 24/0, power
  smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy 4/0.
- WSL Clang `build-wsl-clang`: same matrix passed with 8/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug `build-msvc-aot-stack-copy`: focused targets built; generic numeric contracts 1/0, source
  contracts 24/0, and power contracts 2/0 passed. Unix-only shared-library smokes were expected ignored with 0
  failures: generic numeric 8 ignored, power 1 ignored, generic LOGICAL_NOT 8 ignored, generic equality stack-copy 4
  ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Remaining

- This closes only proven f64 scalar-local generic unary NEG in the focused straight-line shape.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing,
  performance counters, and full zero-frame typed bodies remain open 07 work.
