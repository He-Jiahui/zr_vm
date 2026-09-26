# SSA 01.05: owned logical AOTIR state map

## Scope

A verifier-valid CALL/RETURN function builds a logical state map with effect
checkpoints and managed roots. AOTIR lowering clones its checkpoint entries
and live-value, root, and owner-state side pools. The original can be mutated
and freed while the projected entries remain readable. A malformed input
pool count is diagnosed as INVALID_PROJECTION and does not replace a previously
published AOTIR projection. Valid but uncloneable storage is reported as
OUT_OF_MEMORY; the existing core clone owns rollback of any partial copy.

ExecBC remains a pointer-free executable projection and does not own this
recovery map. AOTIR remains non-runnable: this slice does not materialize a
physical frame, attach a native landing pad, or connect the C/LLVM emitters.

## Validation

```text
wsl cmake --build build/ssa-gcc-debug --target zr_vm_parser_shared zr_vm_ssa_oracle_resume_test zr_vm_ssa_state_maps_test zr_vm_ssa_oracle_projections_test -j 4
wsl --exec ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4
wsl cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_resume_test zr_vm_ssa_state_maps_test zr_vm_ssa_oracle_projections_test -j 4
wsl --exec ctest --test-dir build/ssa-clang-debug -R 'ssa_(oracle_resume|state_maps|oracle_projections)$' --output-on-failure --no-tests=error
wsl cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_resume_test zr_vm_ssa_state_maps_test zr_vm_ssa_oracle_projections_test -j 4
wsl --exec ctest --test-dir build/ssa-gcc-asan-phase80 -R 'ssa_(oracle_resume|state_maps|oracle_projections)$' --output-on-failure --no-tests=error
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_resume_test zr_vm_ssa_state_maps_test zr_vm_ssa_oracle_projections_test -j 4'
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && ctest --test-dir build/ssa-msvc-debug -R "ssa_(oracle_resume|state_maps|oracle_projections)" --output-on-failure --no-tests=error'
```

The new fixture was RED at compile time before the AOTIR record owned a
state-map value. Its input is verified at STRUCTURE/SSA/EFFECT levels before
state-map building and AOT lowering. OOM injection is not part of this
fixture. The shared `StateMapClone` implementation has transactional
failure handling, but an AOT clone allocation failure has not been
fault-injected in this validation.

## Results (2026-09-26)

- GCC production parser and the three focused targets built; focused tests 3/3 passed.
- Clang focused tests 3/3 passed; MSVC x64 focused tests 3/3 passed.
- GCC ASan/UBSan focused tests 3/3 passed.
- Full GCC SSA label: 79/80 passed. The existing `ssa_source_cleanup_cfg`
  fixture still has two pre-execution semantic variable-registration failures
  in conditional-invoke/throw cleanup cases; it is outside this projection
  change and is not counted as passing.
