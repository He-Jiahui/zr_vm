# 2026-07-05 AOT 07-S2/S4 generic numeric i64 ADD local fold

## Scope

- Focused shape: `GET_CONSTANT int -> GET_CONSTANT int -> ADD -> RETURN`.
- Goal: prove generic numeric signed integer ADD whose operands are already written i64 scalar locals can emit direct
  C arithmetic and avoid the runtime generic numeric boundary.
- Non-goal: i64 SUB/MUL/DIV/MOD/NEG, u64 numeric arithmetic, mixed numeric arithmetic, dynamic/unproven operands, and
  broader value-copy migration still use existing conservative fallbacks.

## RED

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_int_local`.
- First WSL GCC focused run made generic numeric smoke 10 tests / 1 failure.
- Failure: generated C still called `ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)` through
  `zr_aot_arith_exec_generic_numeric_binary_boundary`; it lacked `zr_aot_generic_numeric_i64_add_scalar_local` and
  the direct `zr_aot_s2 = zr_aot_s0 + zr_aot_s1;` expression.

## GREEN

- `backend_aot_c_lowering_generic_numeric_arithmetic.c` now tries a guarded i64 scalar-local ADD helper before the
  existing f64 helper and runtime `GenericNumericAdd` fallback.
- `backend_aot_c_scalar_locals.c` preserves immediate i64 constants that feed a later generic ADD, records generic i64
  ADD destinations and exec writes, and treats generic ADD as an i64 local consumer only for the proven i64 slot shape.
- Generated C for the focused shape now emits `zr_aot_scalar_constant_i64_local`,
  `zr_aot_generic_numeric_i64_add_scalar_local`, `zr_aot_s2 = zr_aot_s0 + zr_aot_s1;`, and direct i64 return.
- The focused generated C avoids `zr_aot_arith_exec_generic_numeric_binary_boundary`, `GenericNumericAdd`, and
  `SyncSignedIntLocal`.
- The generic numeric smoke also keeps a right-side f64 copied-result SUB guardrail green; that guardrail documents an
  already-covered f64 proof shape rather than a new optimization slice.

## Verification

- WSL GCC `build-wsl-gcc`: generic numeric smoke 10/0, generic numeric contracts 1/0, source contracts 24/0, power
  smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy 4/0.
- WSL Clang `build-wsl-clang`: same matrix passed with 10/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug `build-msvc-aot-stack-copy`: focused targets built; generic numeric contracts 1/0, source
  contracts 24/0, and power contracts 2/0 passed. Unix-only shared-library smokes were expected ignored with 0
  failures: generic numeric 10 ignored, power 1 ignored, generic LOGICAL_NOT 8 ignored, generic equality stack-copy 4
  ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Remaining

- This closes only proven i64 scalar-local generic ADD in the focused straight-line shape.
- i64 generic SUB/MUL/DIV/MOD/NEG, u64 generic numeric arithmetic, mixed numeric arithmetic, dynamic/unproven operands,
  broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full
  zero-frame typed bodies remain open 07 work.
