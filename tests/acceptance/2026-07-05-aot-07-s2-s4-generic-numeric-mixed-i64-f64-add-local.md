# 2026-07-05 AOT 07-S2/S4 generic numeric mixed i64 f64 ADD local fold

## Scope

- Focused shape: `GET_CONSTANT int -> GET_CONSTANT float -> ADD -> RETURN`.
- Goal: remove the generic numeric binary runtime boundary for a proven mixed signed-int/float ADD while preserving
  runtime numeric promotion semantics where any float operand produces a double result.
- Affected layers: AOT C scalar-local proof, generic numeric arithmetic lowering, generated-C contracts, and
  shared-library smoke coverage.
- Non-goal: mixed SUB/MUL/DIV/MOD, mixed u64/f64, mixed i64/u64 integer arithmetic, dynamic/unproven operands,
  broader value-copy migration, and complete zero-frame typed bodies still use existing conservative fallbacks.

## Baseline

- The prior slices completed proven same-kind f64/i64/u64 numeric arithmetic and u64 NEG-to-i64.
- Runtime evidence from `aot_runtime_generic_numeric_binary` shows float promotion is selected whenever either
  operand is float. Signed integer operands are converted to `TZrFloat64`, and the result is stored as
  `ZR_VALUE_TYPE_DOUBLE`.
- RED: after adding the mixed signed-int/float ADD smoke, the first WSL GCC focused run built the smoke target and
  stopped with generic numeric smoke 22 tests / 1 failure.
- Failure: `test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_int_float_local` failed because
  generated C still emitted `ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)` and lacked
  `zr_aot_generic_numeric_mixed_f64_add_scalar_local` plus the direct expression
  `zr_aot_f2 = (TZrFloat64)zr_aot_s0 + zr_aot_f1;`.

## Test Inventory

- Focused shared-library smoke: signed int constant plus float constant verifies `zr_aot_scalar_constant_i64_local`,
  `zr_aot_scalar_constant_f64_local`, `zr_aot_generic_numeric_mixed_f64_add_scalar_local`,
  `zr_aot_f2 = (TZrFloat64)zr_aot_s0 + zr_aot_f1;`, and absence of `GenericNumericAdd` / `SyncFloatLocal`.
- Contract tests: generic numeric contracts and source contracts check the new lowering helper, marker, direct
  expression, mixed f64 destination proof, exec-write proof, and consumer proof.
- Adjacent regressions: power, generic LOGICAL_NOT, and generic equality stack-copy smokes/contracts.
- Boundary/failure cases: proven mixed i64/f64 ADD is covered; unproven/dynamic operands remain non-goals and keep the
  existing runtime fallback path.

## Tooling Evidence

- WSL GCC focused GREEN command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`.
- WSL GCC adjacent command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_power_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test"`.
- WSL Clang command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_power_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test && ./build-wsl-clang/bin/zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test"`.
- MSVC Debug command:
  `cmd.exe /c "call ""E:\Visual Studio\Common7\Tools\VsDevCmd.bat"" -arch=x64 && cmake --build build-msvc-aot-stack-copy --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test --config Debug -- /m:1 && cd /d E:\Git\zr_vm && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_shared_library_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_source_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_power_shared_library_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_power_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test.exe"`.

## Results

- WSL GCC focused GREEN: generic numeric smoke 22/0, generic numeric contracts 1/0, source contracts 24/0.
- WSL GCC adjacent GREEN: power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy
  4/0.
- WSL Clang GREEN: same focused and adjacent matrix passed with 22/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug GREEN: generic numeric contracts 1/0, source contracts 24/0, power contracts 2/0; Unix-only
  shared-library smokes were expected ignored with 0 failures: generic numeric 22 ignored, power 1 ignored, generic
  LOGICAL_NOT 8 ignored, generic equality stack-copy 4 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the narrow proven mixed i64/f64 generic ADD scalar-local slice.
- Remaining risks: mixed SUB/MUL/DIV/MOD, mixed u64/f64, mixed i64/u64 integer arithmetic, dynamic/unproven operands,
  broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full
  zero-frame typed bodies remain open 07 work.
