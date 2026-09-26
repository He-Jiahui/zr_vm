# SSA 01.05: oracle/ExecBC terminal THROW differential

## Scope

A pointer-free, verifier-valid THROW now records one bounded payload event
and marks the projected result `terminatedByThrow`. It does not return or
execute a successor path. The oracle and projection independently compare
the payload, source/instruction ID, exception observation, current block,
instruction count and non-returning status.

The first GCC build was RED because the projected execution result lacked
`terminatedByThrow`. An out-of-range projected operand reports INVALID_VALUE
at instruction 2/source 972 and preserves the earlier published THROW result.
Mutating the observed payload yields EVENT_MISMATCH at index zero. The fixture
uses `ZR_EXEC_IR_VERIFY_ALL` with the required scheduler-task memory output.

This slice does not claim handler entry, landing pads, catch semantics,
suspend/resume, state-point event sets, production ExecBC or C/LLVM parity.
The full 01.05/M1 gates remain open.

## Validation

```text
wsl cmake --build build/ssa-gcc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_parser_shared -j 4
wsl ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4
wsl cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_projections_test zr_vm_parser_shared -j 4
wsl ctest --test-dir build/ssa-clang-debug -R ssa_oracle_projections --output-on-failure --no-tests=error
wsl cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_projections_test -j 4
wsl ctest --test-dir build/ssa-gcc-asan-phase80 -R ssa_oracle_projections --output-on-failure --no-tests=error
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_parser_static --config Debug -j 4
ctest --test-dir build/ssa-msvc-debug -C Debug -R ssa_oracle_projections --output-on-failure --no-tests=error
```

GCC Debug passed the SSA-label sweep 80/80; only the affected oracle target
and parser library were freshly rebuilt. Focused Clang Debug, Windows MSVC
Debug, and GCC ASan/UBSan tests passed. No sanitizer error or changed-file
compiler warning was reported; pre-existing MSVC warning D9025 and unrelated
long-path CMake warnings remain. No event-allocation fault was injected.
