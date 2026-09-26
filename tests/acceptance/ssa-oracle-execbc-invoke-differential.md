# SSA 01.05: oracle/ExecBC INVOKE handler differential

## Scope

A fully verified three-block function executes INVOKE with one pointer-free
argument on both the direct oracle and ExecBC projection. The normal edge
returns the call result; the exceptional edge reads a separately provided
handler payload and leaves the normal result undefined. Both paths compare
ordered CALL event source/instruction/operand, executed instruction count,
selected return block, and return value. A mutated operand observation fails
with EVENT_MISMATCH. Missing INVOKE/payload providers, rejected INVOKE, and
undefined normal result preserve a previously published result and report
the failing source/instruction. A malformed zero-successor INVOKE is not
advertised runnable.

This covers pointer-free handler selection only. There is no native exception
object, production ExecBC wiring, AOT execution, checkpoint/resume, or C/LLVM
parity. The 01.05/M1 exit gates remain open. Event-allocation OOM timing has
not been fault-injected; the projection reserves before calling the provider,
while the direct oracle appends afterward.

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

The test was RED at the MSVC compile boundary before the new provider fields.
Only the directly affected targets are rebuilt for the focused compiler
checks. The GCC SSA-label sweep uses existing builds for other targets.
GCC Debug passed the SSA-label sweep 80/80; focused Clang Debug, Windows
MSVC Debug, and GCC ASan/UBSan each passed 1/1. No changed-file compiler
warning or sanitizer error was reported; the pre-existing MSVC D9025 warning
remains. The ExecBC runner stays a single-purpose dispatch file at 953 lines;
the next added provider family should extract CALL/INVOKE helpers to a
dedicated internal module rather than keep expanding this file.
