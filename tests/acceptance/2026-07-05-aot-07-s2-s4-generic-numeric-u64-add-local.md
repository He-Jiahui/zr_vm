# 2026-07-05 AOT 07-S2/S4 generic numeric u64 ADD local fold

## Scope

- Focused shape: `GET_CONSTANT uint -> GET_CONSTANT uint -> ADD -> RETURN`.
- Goal: emit a direct unsigned 64-bit generic numeric ADD when destination, left operand, and right operand are all
  proven scalar locals.
- Affected layers: AOT C scalar-local proof, generic numeric arithmetic lowering, generated-C contracts, and
  shared-library smoke coverage.
- Non-goal: u64 SUB/MUL/DIV/MOD, mixed numeric arithmetic, dynamic/unproven operands, and broader value-copy migration
  still use existing conservative fallbacks.

## Baseline

- Existing f64 and i64 generic numeric scalar-local paths already handled proven local arithmetic; u64 generic ADD still
  fell through the generic binary runtime boundary.
- RED: after adding the unsigned u64 ADD smoke and contracts, the first WSL GCC focused run failed in
  `test_aot_c_generated_shared_library_compiles_generic_numeric_add_unsigned_int_local` because generated C lacked the
  `zr_aot_generic_numeric_u64_add_scalar_local` marker and direct `zr_aot_u2 = zr_aot_u0 + zr_aot_u1;` expression.
- Regression found during GREEN: the first implementation misclassified an adjacent f64 result stack-copy ADD as u64,
  producing `zr_aot_u4` / `CopyStack` instead of the existing f64 stack-copy path. The final fix guards the stack-copy
  consumer kind on an already-proven U64 candidate.

## Test Inventory

- Focused shared-library smoke: unsigned u64 constant ADD verifies `zr_aot_scalar_constant_u64_local`, direct u64 ADD,
  direct u64 return, and absence of `GenericNumericAdd` / `SyncUnsignedIntLocal`.
- Contract tests: generic numeric contracts and source contracts check the new u64 helper, marker, opcode proof,
  slot-kind proof, write tracking, format string, and source-generation needles.
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
  `cmd.exe /c "call ""E:\Visual Studio\Common7\Tools\VsDevCmd.bat"" -arch=x64 && cmake --build build-msvc-aot-stack-copy --target zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_generic_numeric_shared_library_smoke_test --config Debug -- /m:1 && cd /d E:\Git\zr_vm && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_source_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_shared_library_smoke_test.exe"`.
- MSVC Debug adjacent command:
  `cmd.exe /c "call ""E:\Visual Studio\Common7\Tools\VsDevCmd.bat"" -arch=x64 && cmake --build build-msvc-aot-stack-copy --target zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test --config Debug -- /m:1 && cd /d E:\Git\zr_vm && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_power_shared_library_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_power_contracts_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test.exe && build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_equality_stack_copy_local_smoke_test.exe"`.

## Results

- WSL GCC focused GREEN: generic numeric smoke 16/0, generic numeric contracts 1/0, source contracts 24/0.
- WSL GCC adjacent GREEN: power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy
  4/0.
- WSL Clang GREEN: same focused and adjacent matrix passed with 16/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug GREEN: focused contracts 1/0 and 24/0; generic numeric shared-library smoke reported 0 failures /
  16 expected ignores. Adjacent contracts/smokes reported power contracts 2/0 plus Unix-only expected ignores with 0
  failures for power, generic LOGICAL_NOT, and generic equality stack-copy.
- A post-documentation WSL GCC/Clang parallel rerun timed out after about 184 seconds without producing test results;
  the same target/test matrix was rerun serially for GCC and Clang and passed with the results listed above. The MSVC
  Debug matrix completed during that same post-documentation validation.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the narrow proven u64 scalar-local generic ADD slice.
- Remaining risks: u64 SUB/MUL/DIV/MOD, mixed numeric arithmetic, dynamic/unproven operands, broader value-copy
  migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame typed
  bodies remain open 07 work.
