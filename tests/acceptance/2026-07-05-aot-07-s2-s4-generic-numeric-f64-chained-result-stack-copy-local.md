# 2026-07-05 AOT 07-S2/S4 generic numeric f64 chained result stack-copy local coverage

## Scope

- Focused shape: `GET_CONSTANT float -> GET_CONSTANT float -> DIV -> SET_STACK -> SET_STACK -> GET_CONSTANT float -> MUL -> RETURN`.
- Goal: prove a generic numeric f64 result can flow through two consecutive `SET_STACK` value-copy hops and still feed a later generic numeric f64 operation without materializing either copy or the later operation through the runtime boundary.
- Non-goal: dynamic/unproven numeric operands, arbitrary control-flow joins, non-f64 copied numeric chains, and full value-copy migration remain later 07 work.

## Coverage Result

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_chained_result_stack_copy_mul_float_local`.
- The first WSL GCC focused run passed immediately: generic numeric smoke 37/0.
- No production RED was observed and no production code was changed in this slice.
- This records coverage closure for the existing f64-before-instruction and stack-copy chain proof.

## Generated C Contract

- The focused generated C keeps the first division local with `zr_aot_generic_numeric_f64_div_scalar_local` and `zr_aot_f2 = zr_aot_f0 / zr_aot_f1;`.
- It emits both copy hops as f64 local copies:
  `zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2` / `zr_aot_f4 = zr_aot_f2;` and
  `zr_aot_scalar_stack_copy_f64 dstSlot=6 srcSlot=4` / `zr_aot_f6 = zr_aot_f4;`.
- It emits the later multiply as `zr_aot_generic_numeric_f64_mul_scalar_local` with `zr_aot_f7 = zr_aot_f6 * zr_aot_f3;`.
- The focused generated C avoids targeted `CopyStack`, `GenericNumericMul`, `SyncFloatLocal`, and copied-result value-slot materialization for slots 4 and 6.

## Verification

- WSL GCC `build-wsl-gcc`: generic numeric smoke 37/0, generic numeric contracts 1/0, source contracts 24/0, power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic bool equality 5/0, generic equality stack-copy 4/0, generic not-equal stack-copy jump-if 1/0, call-result stack-copy equality 1/0.
- WSL Clang `build-wsl-clang`: same matrix passed with 37/0, 1/0, 24/0, 1/0, 2/0, 8/0, 5/0, 4/0, 1/0, and 1/0.
- Windows MSVC Debug `build-msvc-aot-stack-copy`: focused targets built; generic numeric contracts 1/0, source contracts 24/0, and power contracts 2/0 passed. Unix-only shared-library smokes were expected ignored with 0 failures: generic numeric 37 ignored, power 1 ignored, generic LOGICAL_NOT 8 ignored, generic bool equality 5 ignored, generic equality stack-copy 4 ignored, generic not-equal stack-copy jump-if 1 ignored, call-result stack-copy equality 1 ignored.

## Remaining

- This closes only the straight-line f64 chained result-copy shape feeding a later proven f64 `MUL`.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame typed bodies remain open 07 work.
