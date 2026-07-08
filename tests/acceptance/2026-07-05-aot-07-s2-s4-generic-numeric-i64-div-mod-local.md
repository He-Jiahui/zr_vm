# 2026-07-05 AOT 07-S2/S4 generic numeric i64 DIV/MOD local fold

## Scope

- Focused shapes: `GET_CONSTANT int -> GET_CONSTANT int -> DIV -> RETURN` and
  `GET_CONSTANT int -> GET_CONSTANT int -> MOD -> RETURN`.
- Goal: complete the proven generic numeric signed integer scalar-local binary path by emitting direct C division and
  modulo with the same zero-divisor errors as the runtime fallback.
- Non-goal: u64 numeric arithmetic, mixed numeric arithmetic, dynamic/unproven operands, and broader value-copy
  migration still use existing conservative fallbacks.

## RED

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_int_local` and
  `test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_int_local`.
- First WSL GCC focused run made generic numeric smoke 15 tests / 2 failures.
- Failures: generated C lacked `zr_aot_generic_numeric_i64_div_scalar_local`,
  `zr_aot_s2 = zr_aot_s0 / zr_aot_s1;`, `zr_aot_generic_numeric_i64_mod_scalar_local`, and
  `zr_aot_s2 = zr_aot_s0 % zr_aot_s1;`.

## GREEN

- `backend_aot_c_lowering_generic_numeric_arithmetic.c` now tries guarded i64 DIV/MOD scalar-local helpers before the
  existing f64 helpers and runtime `GenericNumericDiv` / `GenericNumericMod` fallbacks.
- The i64 DIV helper emits `if (zr_aot_s1 == (TZrInt64)0)` with `ZrCore_Debug_RunError(state, "divide by zero")`
  before direct `/`.
- The i64 MOD helper emits `if (zr_aot_s1 == (TZrInt64)0)` with `ZrCore_Debug_RunError(state, "modulo by zero")`
  before direct `%`.
- `backend_aot_c_scalar_locals.c` extends the generic numeric i64 binary opcode proof from ADD/SUB/MUL to
  ADD/SUB/MUL/DIV/MOD, so immediate i64 constants, destination declarations, exec writes, and consumer reads stay in
  scalar locals for the proven slot shape.
- The focused generated C avoids `zr_aot_arith_exec_generic_numeric_binary_boundary`, `GenericNumericDiv`,
  `GenericNumericMod`, and `SyncSignedIntLocal`.

## Verification

- WSL GCC `build-wsl-gcc`: generic numeric smoke 15/0, generic numeric contracts 1/0, source contracts 24/0, power
  smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy 4/0.
- WSL Clang `build-wsl-clang`: same matrix passed with 15/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug `build-msvc-aot-stack-copy`: focused targets built; generic numeric contracts 1/0, source
  contracts 24/0, and power contracts 2/0 passed. Unix-only shared-library smokes were expected ignored with 0
  failures: generic numeric 15 ignored, power 1 ignored, generic LOGICAL_NOT 8 ignored, generic equality stack-copy 4
  ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Remaining

- This closes only proven i64 scalar-local generic DIV/MOD in the focused straight-line shapes.
- u64 generic numeric arithmetic, mixed numeric arithmetic, dynamic/unproven operands, broader value-copy migration,
  GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame typed bodies remain
  open 07 work.
