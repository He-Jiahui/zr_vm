# 2026-07-05 AOT 07-S2/S4 generic numeric u64 NEG to i64 local fold

## Scope

- Focused shape: `GET_CONSTANT uint -> NEG -> RETURN`.
- Goal: remove the generic numeric unary runtime boundary for a proven unsigned source while preserving the runtime
  contract that unsigned `NEG` produces a signed i64 result.
- Affected layers: AOT C scalar-local proof, generic numeric arithmetic lowering, generated-C contracts, and
  shared-library smoke coverage.
- Non-goal: u64 wrapping negation, mixed numeric arithmetic, dynamic/unproven operands, broader value-copy migration,
  and complete zero-frame typed bodies still use existing conservative fallbacks.

## Baseline

- The prior u64 slices completed the proven unsigned binary set ADD/SUB/MUL/DIV/MOD. Unary NEG still only had local
  paths for signed i64 and f64 sources.
- Runtime evidence from `ZrLibrary_AotRuntime_GenericNumericNeg` shows unsigned input is cast to `TZrInt64`, negated,
  and stored as `ZR_VALUE_TYPE_INT64`; therefore a u64-local NEG must write an i64 local destination, not a u64 result.
- RED: after adding the unsigned u64 NEG-to-i64 smoke and contract needles, the first WSL GCC focused run built the
  three focused targets but stopped in generic numeric smoke with 21 tests / 1 failure.
- Failure: `test_aot_c_generated_shared_library_compiles_generic_numeric_neg_unsigned_int_to_signed_local` failed at
  line 1846 with `Expected Non-NULL` because generated C lacked `zr_aot_generic_numeric_u64_neg_to_i64_scalar_local`
  and `zr_aot_s1 = -(TZrInt64)zr_aot_u0;`.

## Test Inventory

- Focused shared-library smoke: unsigned u64 constant NEG verifies `zr_aot_scalar_constant_u64_local`,
  `zr_aot_generic_numeric_u64_neg_to_i64_scalar_local`, direct i64 expression `zr_aot_s1 = -(TZrInt64)zr_aot_u0;`,
  direct i64 return, and absence of `GenericNumericNeg` / `SyncSignedIntLocal`.
- Contract tests: generic numeric contracts and source contracts check the new lowering helper, marker, direct
  expression, u64-source/i64-destination scalar-local write proof, and source-generation needles.
- Adjacent regressions: power, generic LOGICAL_NOT, and generic equality stack-copy smokes/contracts.
- Boundary/failure cases: proven u64 local source is covered; unproven/dynamic operands remain non-goals and keep the
  existing runtime fallback path.

## Tooling Evidence

- WSL GCC: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`.
- WSL Clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`.
- Windows MSVC: `Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35228 for x64`.
- RED command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`.
- WSL GCC GREEN command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`.
- WSL GCC adjacent command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_power_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test"`.
- WSL Clang command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_power_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test"`.
- MSVC Debug command:
  `cmd.exe /c "call ""E:\Visual Studio\Common7\Tools\VsDevCmd.bat"" -arch=x64 && cmake --build build-msvc-aot-stack-copy --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test --config Debug -- /m:1 && cd /d E:\Git\zr_vm && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_shared_library_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_source_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_power_shared_library_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_power_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test.exe"`.

## Results

- WSL GCC focused GREEN: generic numeric smoke 21/0, generic numeric contracts 1/0, source contracts 24/0.
- WSL GCC adjacent GREEN: power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy
  4/0.
- WSL Clang GREEN: same focused and adjacent matrix passed with 21/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0. The first
  Clang attempt timed out before complete output and was not used as acceptance evidence; the longer rerun completed.
- Windows MSVC Debug GREEN: generic numeric contracts 1/0, source contracts 24/0, power contracts 2/0; Unix-only
  shared-library smokes were expected ignored with 0 failures: generic numeric 21 ignored, power 1 ignored, generic
  LOGICAL_NOT 8 ignored, generic equality stack-copy 4 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the narrow proven u64 scalar-local generic NEG-to-i64 slice.
- Remaining risks: mixed numeric arithmetic, dynamic/unproven operands, broader value-copy migration, GC
  roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame typed bodies remain open
  07 work.
