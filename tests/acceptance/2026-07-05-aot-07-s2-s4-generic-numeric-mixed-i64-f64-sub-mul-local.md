# 2026-07-05 AOT 07-S2/S4 generic numeric mixed i64 f64 SUB/MUL local fold

## Scope

- Focused shapes:
  - `GET_CONSTANT int -> GET_CONSTANT float -> SUB -> RETURN`.
  - `GET_CONSTANT int -> GET_CONSTANT float -> MUL -> RETURN`.
- Goal: remove the generic numeric binary runtime boundary for proven mixed signed-int/float SUB and MUL while
  preserving runtime numeric promotion semantics where any float operand produces a double result.
- Non-goal: mixed DIV/MOD, mixed u64/f64, mixed i64/u64 integer arithmetic, dynamic/unproven operands, broader
  value-copy migration, and complete zero-frame typed bodies still use existing conservative fallbacks.

## Baseline

- The prior slice completed proven mixed i64/f64 ADD.
- Runtime evidence from `aot_runtime_generic_numeric_binary` shows float promotion is selected whenever either operand
  is float. Signed integer operands are converted to `TZrFloat64`, and the result is stored as `ZR_VALUE_TYPE_DOUBLE`.
- RED command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`.
- RED result: generic numeric smoke built and ran 24 tests with 2 failures.
- Failures:
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_sub_signed_int_float_local` failed because generated C
    still emitted `ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 2, 0, 1)` and lacked
    `zr_aot_generic_numeric_mixed_f64_sub_scalar_local` plus `zr_aot_f2 = (TZrFloat64)zr_aot_s0 - zr_aot_f1;`.
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_mul_signed_int_float_local` failed because generated C
    still emitted `ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 2, 0, 1)` and lacked
    `zr_aot_generic_numeric_mixed_f64_mul_scalar_local` plus `zr_aot_f2 = (TZrFloat64)zr_aot_s0 * zr_aot_f1;`.

## Test Inventory

- Focused shared-library smokes: signed int constant plus float constant verifies
  `zr_aot_scalar_constant_i64_local`, `zr_aot_scalar_constant_f64_local`,
  `zr_aot_generic_numeric_mixed_f64_sub_scalar_local` / `zr_aot_generic_numeric_mixed_f64_mul_scalar_local`, direct
  cast expressions, and absence of `GenericNumericSub` / `GenericNumericMul` / `SyncFloatLocal`.
- Contract tests: generic numeric contracts and source contracts check the generalized mixed f64 binary lowering helper,
  ADD/SUB/MUL markers, operator-token expression format, mixed f64 destination proof, exec-write proof, and consumer
  proof.
- Adjacent regressions: power, generic LOGICAL_NOT, generic bool equality, generic equality stack-copy, not-equal
  stack-copy jump-if, and call-result stack-copy equality smokes/contracts.

## Results

- WSL GCC focused GREEN: generic numeric smoke 24/0, generic numeric contracts 1/0, source contracts 24/0.
- WSL GCC adjacent GREEN: power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic bool equality 5/0,
  generic equality stack-copy 4/0, generic not-equal stack-copy jump-if 1/0, and call-result stack-copy equality 1/0.
- WSL Clang GREEN: same focused and adjacent matrix passed with 24/0, 1/0, 24/0, 1/0, 2/0, 8/0, 5/0, 4/0, 1/0, and
  1/0.
- Windows MSVC Debug GREEN: generic numeric contracts 1/0, source contracts 24/0, power contracts 2/0; Unix-only
  shared-library smokes were expected ignored with 0 failures: generic numeric 24 ignored, power 1 ignored, generic
  LOGICAL_NOT 8 ignored, generic bool equality 5 ignored, generic equality stack-copy 4 ignored, generic not-equal
  stack-copy jump-if 1 ignored, and call-result stack-copy equality 1 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the narrow proven mixed i64/f64 generic SUB/MUL scalar-local slice.
- Remaining risks: mixed DIV/MOD, mixed u64/f64, mixed i64/u64 integer arithmetic, dynamic/unproven operands, broader
  value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame
  typed bodies remain open 07 work.
