# 2026-07-05 AOT 07-S2/S4 generic numeric u64 DIV/MOD local fold

## Scope

- Focused shapes: `GET_CONSTANT uint -> GET_CONSTANT uint -> DIV -> RETURN` and
  `GET_CONSTANT uint -> GET_CONSTANT uint -> MOD -> RETURN`.
- Goal: complete the proven unsigned 64-bit generic numeric scalar-local binary path, including the same zero-divisor
  behavior as the runtime fallback.
- Affected layers: AOT C scalar-local proof, generic numeric arithmetic lowering, generated-C contracts, and
  shared-library smoke coverage.
- Non-goal: mixed numeric arithmetic, dynamic/unproven operands, broader value-copy migration, and complete zero-frame
  typed bodies still use existing conservative fallbacks.

## Baseline

- The prior u64 ADD/SUB/MUL slices proved unsigned constants and non-dividing binary operations, but the u64 generic
  numeric opcode set still excluded `DIV` and `MOD`.
- RED: after adding unsigned u64 DIV/MOD smokes and contract needles, the first WSL GCC focused run built the three
  focused targets but failed in generic numeric smoke with 20 tests / 2 failures.
- Failures: `test_aot_c_generated_shared_library_compiles_generic_numeric_div_unsigned_int_local` failed at line 1294
  and `test_aot_c_generated_shared_library_compiles_generic_numeric_mod_unsigned_int_local` failed at line 1369 with
  `Expected Non-NULL` because generated C lacked the u64 DIV/MOD local markers, zero guards, and direct `/` / `%`
  expressions.

## Test Inventory

- Focused shared-library smoke: unsigned u64 constant DIV and MOD verify `zr_aot_scalar_constant_u64_local`,
  `zr_aot_generic_numeric_u64_div_scalar_local`, `zr_aot_generic_numeric_u64_mod_scalar_local`, zero-divisor guards,
  direct u64 expressions, direct u64 return, and absence of `GenericNumericDiv` / `GenericNumericMod` /
  `SyncUnsignedIntLocal`.
- Contract tests: generic numeric contracts and source contracts check the u64 DIV/MOD helpers, markers, zero-guard
  format strings, direct `/` / `%` expressions, expanded u64 opcode proof, slot-kind proof, write tracking, and
  source-generation needles.
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

- WSL GCC focused GREEN: generic numeric smoke 20/0, generic numeric contracts 1/0, source contracts 24/0.
- WSL GCC adjacent GREEN: power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy
  4/0.
- WSL Clang GREEN: same focused and adjacent matrix passed with 20/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug GREEN: generic numeric contracts 1/0, source contracts 24/0, power contracts 2/0; Unix-only
  shared-library smokes were expected ignored with 0 failures: generic numeric 20 ignored, power 1 ignored, generic
  LOGICAL_NOT 8 ignored, generic equality stack-copy 4 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the narrow proven u64 scalar-local generic DIV/MOD slice.
- This also closes the proven u64 scalar-local generic binary set for ADD/SUB/MUL/DIV/MOD.
- Remaining risks: mixed numeric arithmetic, dynamic/unproven operands, broader value-copy migration, GC
  roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame typed bodies remain open
  07 work.
