# 2026-07-05 AOT 07-S2/S4 generic numeric u64 SUB/MUL local fold

## Scope

- Focused shapes: `GET_CONSTANT uint -> GET_CONSTANT uint -> SUB -> RETURN` and
  `GET_CONSTANT uint -> GET_CONSTANT uint -> MUL -> RETURN`.
- Goal: extend the proven unsigned 64-bit generic numeric scalar-local path from ADD to the remaining non-dividing
  binary operations.
- Affected layers: AOT C scalar-local proof, generic numeric arithmetic lowering, generated-C contracts, and
  shared-library smoke coverage.
- Non-goal: u64 DIV/MOD zero-guarded lowering, mixed numeric arithmetic, dynamic/unproven operands, and broader
  value-copy migration still use existing conservative fallbacks.

## Baseline

- The prior u64 ADD slice introduced the generic u64 binary scalar-local helper and proof, but the u64 generic numeric
  opcode set still accepted only `ADD`.
- RED: after adding unsigned u64 SUB/MUL smokes and contract needles, the first WSL GCC focused run built the three
  focused targets but failed in generic numeric smoke with 18 tests / 2 failures.
- Failures: `test_aot_c_generated_shared_library_compiles_generic_numeric_sub_unsigned_int_local` and
  `test_aot_c_generated_shared_library_compiles_generic_numeric_mul_unsigned_int_local` both failed with
  `Expected Non-NULL` because generated C lacked `zr_aot_generic_numeric_u64_sub_scalar_local`,
  `zr_aot_u2 = zr_aot_u0 - zr_aot_u1;`, `zr_aot_generic_numeric_u64_mul_scalar_local`, and
  `zr_aot_u2 = zr_aot_u0 * zr_aot_u1;`.

## Test Inventory

- Focused shared-library smoke: unsigned u64 constant SUB and MUL verify `zr_aot_scalar_constant_u64_local`, direct u64
  operation markers, direct u64 expressions, direct u64 return, and absence of `GenericNumericSub` /
  `GenericNumericMul` / `SyncUnsignedIntLocal`.
- Contract tests: generic numeric contracts and source contracts check the u64 helper, SUB/MUL markers, expanded u64
  opcode proof, slot-kind proof, write tracking, format string, and source-generation needles.
- Adjacent regressions: power, generic LOGICAL_NOT, and generic equality stack-copy smokes/contracts.
- Boundary/failure cases: proven u64 local operands are covered; unproven/dynamic operands remain non-goals and keep
  the existing runtime fallback path.

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

- WSL GCC focused GREEN: generic numeric smoke 18/0, generic numeric contracts 1/0, source contracts 24/0.
- WSL GCC adjacent GREEN: power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy
  4/0.
- WSL Clang GREEN: same focused and adjacent matrix passed with 18/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug GREEN: generic numeric contracts 1/0, source contracts 24/0, power contracts 2/0; Unix-only
  shared-library smokes were expected ignored with 0 failures: generic numeric 18 ignored, power 1 ignored, generic
  LOGICAL_NOT 8 ignored, generic equality stack-copy 4 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the narrow proven u64 scalar-local generic SUB/MUL slice.
- Remaining risks: u64 DIV/MOD, mixed numeric arithmetic, dynamic/unproven operands, broader value-copy migration,
  GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame typed bodies remain
  open 07 work.
