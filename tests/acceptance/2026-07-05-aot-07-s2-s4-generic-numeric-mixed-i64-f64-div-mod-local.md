# 2026-07-05 AOT 07-S2/S4 generic numeric mixed i64 f64 DIV/MOD local fold

## Scope

- Focused shapes:
  - `GET_CONSTANT int -> GET_CONSTANT float -> DIV -> RETURN`.
  - `GET_CONSTANT int -> GET_CONSTANT float -> MOD -> RETURN`.
- Goal: remove the generic numeric binary runtime boundary for proven mixed signed-int/float DIV and MOD while
  preserving runtime float promotion, divide-by-zero, modulo-by-zero, and `fmod` semantics.
- Non-goal: mixed u64/f64, mixed i64/u64 integer arithmetic, dynamic/unproven operands, broader value-copy migration,
  and complete zero-frame typed bodies still use existing conservative fallbacks unless separately proven.

## Baseline

- Prior slices completed proven mixed i64/f64 ADD/SUB/MUL.
- RED command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`.
- RED result: generic numeric smoke built and ran 26 tests with 2 failures.
- Failures:
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_int_float_local` failed because generated C
    lacked `zr_aot_generic_numeric_mixed_f64_div_scalar_local` and still used the runtime DIV boundary.
  - `test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_int_float_local` failed because generated C
    lacked `zr_aot_generic_numeric_mixed_f64_mod_scalar_local` and still used the runtime MOD boundary.

## Results

- WSL GCC focused GREEN: generic numeric smoke 26/0, generic numeric contracts 1/0, source contracts 24/0.
- WSL GCC adjacent GREEN: power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic bool equality 5/0,
  generic equality stack-copy 4/0, generic not-equal stack-copy jump-if 1/0, and call-result stack-copy equality 1/0.
- WSL Clang GREEN: same focused and adjacent matrix passed with 26/0, 1/0, 24/0, 1/0, 2/0, 8/0, 5/0, 4/0, 1/0, and
  1/0.
- Windows MSVC Debug GREEN: generic numeric contracts 1/0, source contracts 24/0, power contracts 2/0; Unix-only
  shared-library smokes were expected ignored with 0 failures: generic numeric 26 ignored, power 1 ignored, generic
  LOGICAL_NOT 8 ignored, generic bool equality 5 ignored, generic equality stack-copy 4 ignored, generic not-equal
  stack-copy jump-if 1 ignored, and call-result stack-copy equality 1 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the narrow proven mixed i64/f64 generic DIV/MOD scalar-local slice.
- Generated C now keeps the int and float constants local, emits mixed f64 DIV/MOD markers, checks the f64 divisor for
  zero, emits `"divide by zero"` / `"modulo by zero"`, uses direct `/` for DIV, uses `fmod` for MOD, and avoids
  `GenericNumericDiv` / `GenericNumericMod` plus `SyncFloatLocal` for these focused shapes.
