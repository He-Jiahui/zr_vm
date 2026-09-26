# SSA 01.05: projected CFG flags and sparse physical slots

## Scope

The verifier-valid INVOKE fixture checks that ExecBC and AOTIR carry each
original block's flags, including entry and exceptional handler identity.
The parallel-edge fixture checks that both projections leave synthetic split
blocks unflagged. AOTIR receives the same owned CFG record and remains
non-runnable; no C/LLVM emitter or runtime landing pad is claimed.

A freshly rebuilt parallel-edge fixture exposed an existing disagreement:
phi scheduling supported sparse physical slot IDs, but projection metadata
and runner allocation assumed one physical slot per logical value. The
projection now tracks the physical capacity separately, including reserved
frame storage; the runner allocates that capacity and phi temporaries. An
executed sparse-frame branch/phi returns through the selected edge, and the
existing cycle overflow, alias rejection, packed-frame, and failed-publication
assertions remain active. Tampering with the physical capacity reports
INVALID_PROJECTION without replacing the last successful execution. This
does not enable slot reuse.

## Validation

```text
wsl cmake --build build/ssa-gcc-debug --target zr_vm_parser_shared -j 4
wsl cmake --build build/ssa-gcc-debug --target zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_parallel_edges_test -j 4
wsl --exec ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4
wsl cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_parallel_edges_test -j 4
wsl --exec ctest --test-dir build/ssa-clang-debug -R 'ssa_oracle_(projections|parallel_edges)$' --output-on-failure --no-tests=error
wsl cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_parallel_edges_test -j 4
wsl --exec ctest --test-dir build/ssa-gcc-asan-phase80 -R 'ssa_oracle_(projections|parallel_edges)$' --output-on-failure --no-tests=error
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_parallel_edges_test -j 4'
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && ctest --test-dir build/ssa-msvc-debug -R "ssa_oracle_(projections|parallel_edges)" --output-on-failure --no-tests=error'
```

The new flag assertions first failed compilation because `SZrExecBcBlock`
had no `flags` member. After adding it, a freshly rebuilt parallel-edge
target rejected sparse slot ID `UINT32_MAX` as INVALID_PROJECTION before its
phi-cycle overflow check. Moving physical metadata construction after phi
scheduling restored the expected CAPACITY_OVERFLOW edge diagnostic without
replacing a previously published projection.

The production parser shared library built. Focused GCC tests passed 3/3;
Clang, MSVC x64, and GCC ASan/UBSan passed the two newly rebuilt projection
targets 2/2 each. Their separately built resume targets passed earlier after
the shared header change. The GCC SSA-label run was 79/80, not green:
`ssa_source_cleanup_cfg` fails two fixture assertions while registering
variables in pre-execution Semantic IR, before projection construction. No
attribution to this slice or plan-wide success is claimed. Only the directly
affected test targets were freshly rebuilt in each test directory; the wider
Clang directory still lacks unrelated test binaries.
