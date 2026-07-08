# 2026-07-05 AOT 07-S2/S4 generic numeric mixed i64/u64 ADD local fold

## Scope

- Focused shape: `GET_CONSTANT int -> GET_CONSTANT uint -> ADD -> RETURN`.
- Goal: remove the generic numeric binary runtime boundary for the proven mixed signed-int/unsigned-int ADD shape while
  preserving runtime integer mixed-sign semantics: when the operands are not both unsigned and neither is float, the
  runtime returns signed i64 and casts the unsigned operand to `TZrInt64`.
- Non-goal: mixed i64/u64 SUB/MUL/DIV/MOD, mixed u64/f64 arithmetic, dynamic/unproven operands, broader value-copy
  migration, and complete zero-frame typed bodies still use existing conservative fallbacks unless separately proven.

## Baseline

- Prior slices completed proven i64, u64, u64 NEG-to-i64, and mixed i64/f64 generic numeric scalar-local arithmetic.
- RED command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`.
- RED result: generic numeric smoke built and ran 27 tests with 1 failure.
- Failure: `test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_unsigned_int_local` failed because generated
  C lacked `zr_aot_generic_numeric_mixed_i64_u64_add_scalar_local` and still used the runtime ADD boundary.

## Results

- WSL GCC focused GREEN: generic numeric smoke 27/0, generic numeric contracts 1/0, source contracts 24/0.
- WSL GCC adjacent GREEN: power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic bool equality 5/0,
  generic equality stack-copy 4/0, generic not-equal stack-copy jump-if 1/0, and call-result stack-copy equality 1/0.
- WSL Clang GREEN: same focused and adjacent matrix passed with 27/0, 1/0, 24/0, 1/0, 2/0, 8/0, 5/0, 4/0, 1/0, and
  1/0.
- Windows MSVC Debug GREEN: generic numeric contracts 1/0, source contracts 24/0, power contracts 2/0; Unix-only
  shared-library smokes were expected ignored with 0 failures: generic numeric 27 ignored, power 1 ignored, generic
  LOGICAL_NOT 8 ignored, generic bool equality 5 ignored, generic equality stack-copy 4 ignored, generic not-equal
  stack-copy jump-if 1 ignored, and call-result stack-copy equality 1 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the narrow proven mixed i64/u64 generic ADD scalar-local slice.
- Generated C now keeps the signed and unsigned constants local, emits the mixed i64/u64 ADD marker, runs
  `zr_aot_s2 = zr_aot_s0 + (TZrInt64)zr_aot_u1;`, returns the i64 local directly, and avoids
  `GenericNumericAdd` plus `SyncSignedIntLocal` for this focused shape.
