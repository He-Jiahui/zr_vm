# SSA 01.05: oracle/ExecBC terminal SUSPEND differential

## Scope

The projected pointer-free runner now executes verifier-valid terminal
`SUSPEND`: it publishes a bounded SUSPEND event, copies the first operand to
the suspension payload and SSA result, and marks the execution `suspended`.
Every variadic operand is checked even when the event only retains four.

The first GCC build was RED because the projected result lacked `suspended`.
Fixtures with one and five operands each pass `ZR_EXEC_IR_VERIFY_ALL` with
the scheduler-task memory input/output versions. Independent oracle and
projected runs compare terminal state, current block, return payload, SSA
result, executed instruction count, and ordered bounded event snapshots.
Mutating an event payload reports EVENT_MISMATCH at index zero. An invalid
fifth operand reports INVALID_VALUE at the SUSPEND instruction/source and
leaves the earlier published result intact.

The projection does not capture a checkpoint, restore state maps, resume a
fiber, enter exception handlers, or execute production ExecBC or C/LLVM.
These remain open in the full 01.05/M1 gates.

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

GCC Debug passed the SSA-label sweep 80/80 (the other 79 test binaries were
not all freshly rebuilt). Clang Debug, Windows MSVC Debug, and GCC ASan/UBSan
passed the focused target. MSVC found an initially uninitialized test-local
instruction ID on a short-circuit failure path; initializing it removed the
new warning. Existing D9025 and unrelated long-object-path CMake warnings
remain. No sanitizer failure or fault-injected event allocation was observed.
