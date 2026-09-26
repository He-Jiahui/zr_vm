# SSA 01.05: owned source spans in backend projections

## Scope

The verifier-valid INVOKE fixture has an explicit source-map entry with
source ID, instruction ID, byte offsets, and start/end line/column. Both
ExecBC and the non-runnable AOTIR projection retain the full span and map
the one-based instruction ID to a zero-based PC. Mutating the ExecIR input
after projection leaves both owned records intact. An out-of-range source-map
instruction ID reports INVALID_RANGE without replacing the published AOTIR
record. The existing normal and exceptional execution differential still
runs after the input is restored.

No C/LLVM emitter consumes these spans yet, and no AOT execution parity is
claimed. This slice only closes loss of debug location data at the shared
projection boundary.

## Validation

```text
wsl cmake --build build/ssa-gcc-debug --target zr_vm_parser_shared zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_parallel_edges_test -j 4
wsl --exec ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4
wsl cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_parallel_edges_test -j 4
wsl --exec ctest --test-dir build/ssa-clang-debug -R 'ssa_oracle_(resume|projections|parallel_edges)$' --output-on-failure --no-tests=error
wsl cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_parallel_edges_test -j 4
wsl --exec ctest --test-dir build/ssa-gcc-asan-phase80 -R 'ssa_oracle_(resume|projections|parallel_edges)$' --output-on-failure --no-tests=error
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_parallel_edges_test -j 4'
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && ctest --test-dir build/ssa-msvc-debug -R "ssa_oracle_(resume|projections|parallel_edges)" --output-on-failure --no-tests=error'
```

The first new assertion failed to compile because the projected source-map
record had no span fields. The implementation copies the source fields by
value while keeping existing PC translation and failure-atomic publication.

GCC production parser shared library built. Focused GCC, Clang, MSVC x64,
and GCC ASan/UBSan tests passed 3/3 each. GCC SSA-label was 79/80, not green:
`ssa_source_cleanup_cfg` still fails two pre-execution Semantic IR variable
registration assertions before entering the projection path. The broader
Clang directory lacks unrelated binaries; only the listed test targets were
freshly rebuilt on each platform.
