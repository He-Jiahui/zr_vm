# 2026-07-05 AOT 07-S2/S4 generic numeric i64 SUB/MUL local fold

## Scope

- Focused shapes: `GET_CONSTANT int -> GET_CONSTANT int -> SUB -> RETURN` and
  `GET_CONSTANT int -> GET_CONSTANT int -> MUL -> RETURN`.
- Goal: extend the proven generic numeric signed integer scalar-local path from ADD to the other non-dividing binary
  operations, emitting direct C arithmetic when destination and both operands are already proven i64 locals.
- Non-goal: i64 DIV/MOD/NEG, u64 numeric arithmetic, mixed numeric arithmetic, dynamic/unproven operands, and broader
  value-copy migration still use existing conservative fallbacks.

## RED

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_sub_signed_int_local` and
  `test_aot_c_generated_shared_library_compiles_generic_numeric_mul_signed_int_local`.
- First WSL GCC focused run made generic numeric smoke 12 tests / 2 failures.
- Failures: generated C lacked `zr_aot_generic_numeric_i64_sub_scalar_local`,
  `zr_aot_s2 = zr_aot_s0 - zr_aot_s1;`, `zr_aot_generic_numeric_i64_mul_scalar_local`, and
  `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`.
- The generic numeric contract target also failed after requiring the new i64 SUB/MUL markers.

## GREEN

- `backend_aot_c_lowering_generic_numeric_arithmetic.c` now tries the guarded i64 binary scalar-local helper for SUB
  and MUL before the existing f64 helper and runtime `GenericNumericSub` / `GenericNumericMul` fallbacks.
- `backend_aot_c_scalar_locals.c` centralizes the i64 generic numeric binary opcode check for ADD/SUB/MUL, preserves
  immediate i64 constants for later generic SUB/MUL operands, records their destinations and exec writes as i64 locals,
  and treats generic SUB/MUL as i64 local consumers only for the proven slot shape.
- Generated C for the focused shapes now emits `zr_aot_scalar_constant_i64_local`,
  `zr_aot_generic_numeric_i64_sub_scalar_local` / `zr_aot_generic_numeric_i64_mul_scalar_local`, direct `-` / `*`
  expressions, and direct i64 return.
- The focused generated C avoids `zr_aot_arith_exec_generic_numeric_binary_boundary`, `GenericNumericSub`,
  `GenericNumericMul`, and `SyncSignedIntLocal`.

## Verification

- WSL GCC `build-wsl-gcc`: generic numeric smoke 12/0, generic numeric contracts 1/0, source contracts 24/0, power
  smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy 4/0.
- WSL Clang `build-wsl-clang`: same matrix passed with 12/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug `build-msvc-aot-stack-copy`: focused targets built; generic numeric contracts 1/0, source
  contracts 24/0, and power contracts 2/0 passed. Unix-only shared-library smokes were expected ignored with 0
  failures: generic numeric 12 ignored, power 1 ignored, generic LOGICAL_NOT 8 ignored, generic equality stack-copy 4
  ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Remaining

- This closes only proven i64 scalar-local generic SUB/MUL in the focused straight-line shapes.
- i64 generic DIV/MOD/NEG, u64 generic numeric arithmetic, mixed numeric arithmetic, dynamic/unproven operands,
  broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full
  zero-frame typed bodies remain open 07 work.
