# SSA 01.05: oracle/ExecBC BARRIER event differential

## Scope

The pointer-free projection runner now executes verifier-valid `BARRIER`
instructions as bounded effect observations. The ExecIR instruction retains
the managed-heap read and managed-heap/GC write versions; execution validates
its one operand and records source/instruction identity before returning.
This does not perform a production GC write barrier or establish AOT parity.

## Regression evidence

- One `ZR_EXEC_IR_VERIFY_ALL` fixture emits two barriers with different
  operands, then returns. Independent oracle and projected runs compare
  barrier source, instruction, value, order and return through the
  differential harness; both backend identities are recorded.
- Mutating the second recorded payload reports `EVENT_MISMATCH` at index one.
  An out-of-range projected operand reports INVALID_VALUE at instruction
  2/source 901 without replacing the already published execution result.
- TDD RED first rejected a zero-operand fixture under the schema's mandatory
  arity of one. After correcting the fixture, RED reached the expected
  `runnable == false` assertion before projected execution was added.

Unsupported effects, exceptions, suspend/resume, state-point event sets,
production ExecBC, and C/LLVM execution are outside this slice. The full
01.05 and M1 gates remain open.

## Validation

```text
wsl cmake --build build/ssa-gcc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test zr_vm_parser_shared -j 4
wsl ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4
wsl cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_projections_test zr_vm_parser_shared -j 4
wsl ctest --test-dir build/ssa-clang-debug -R ssa_oracle_projections --output-on-failure --no-tests=error
wsl cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_projections_test -j 4
wsl ctest --test-dir build/ssa-gcc-asan-phase80 -R ssa_oracle_projections --output-on-failure --no-tests=error
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_parser_static --config Debug -j 4
ctest --test-dir build/ssa-msvc-debug -C Debug -R ssa_oracle_projections --output-on-failure --no-tests=error
```

The GCC Debug SSA-label sweep passed 80/80, including the freshly rebuilt
oracle and differential-harness targets; the other 78 test binaries were not
all rebuilt. Clang Debug,
Windows MSVC Debug, and GCC ASan/UBSan each passed the focused target. No
sanitizer failure or changed-file compiler warning was reported. Existing
MSVC `/W3`-overridden-by-`/W4` and unrelated long-object-path CMake warnings
remain. Allocation failure was not injected.
