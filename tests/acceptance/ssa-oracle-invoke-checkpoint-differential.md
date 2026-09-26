# SSA 01.05: INVOKE checkpoint/event differential

## Scope

The verified resume fixture pauses the direct oracle at INVOKE `AFTER_EFFECT`
on either the normal or exceptional edge. Before resuming, it compares the
complete CALL event kind, source/instruction identity, and operand snapshot
against an independently run ExecBC projection. After resume it compares
ordered CALL/RETURN observations, source IDs, selected block, instruction
count and return bits through the shared differential harness. Changing the
observed CALL operand yields EVENT_MISMATCH at index zero. The oracle callback
runs exactly once despite changing its edge choice after the checkpoint.

The existing repeated-loop resume fixture had lacked a loop-carried effect
phi and was rejected by the strengthened effect verifier after a fresh build.
The fixture now carries the entry token zero and its loop-back effect token.
Structure, oracle, and projection preflight agree that zero is valid for an
initial effect/memory token phi, but not for an ordinary value phi. The loop
also lowers to a runnable projection after full verification. The core model
test explicitly rejects zero in an ordinary value-phi incoming.

The projection is run uninterrupted. No projected pause/resume or physical
frame reconstruction, production ExecBC dispatch, AOT execution, or C/LLVM
parity is claimed. Full state-point and M1 parity remain open. The
single-purpose resume test file is close to 1000 lines; future checkpoint
families should move to their own fixture instead of extending it further.

## Validation

```text
wsl cmake --build build/ssa-gcc-debug --target zr_vm_ssa_core_model_test zr_vm_ssa_effects_verifier_test zr_vm_ssa_state_maps_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_cfg_effects_builder_test -j 4
wsl --exec ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4
wsl cmake --build build/ssa-clang-debug --target zr_vm_ssa_core_model_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_projections_test -j 4
wsl --exec ctest --test-dir build/ssa-clang-debug -R 'ssa_(core_model|oracle_resume|oracle_projections)$' --output-on-failure --no-tests=error
wsl cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_core_model_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_projections_test -j 4
wsl --exec ctest --test-dir build/ssa-gcc-asan-phase80 -R 'ssa_(core_model|oracle_resume|oracle_projections)$' --output-on-failure --no-tests=error
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && cmake --build build/ssa-msvc-debug --target zr_vm_ssa_core_model_test zr_vm_ssa_oracle_resume_test zr_vm_ssa_oracle_projections_test -j 4'
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && ctest --test-dir build/ssa-msvc-debug -R "ssa_(core_model|oracle_resume|oracle_projections)" --output-on-failure --no-tests=error'
```

The test was RED at link time before the resume target included the projection
and differential support sources. A newly rebuilt old loop fixture then went
RED on its missing effect phi, followed by structure and oracle preflight
disagreement over the initial token. All three lower-layer diagnoses were
observed before changing the shared validation boundary.

GCC SSA label: 80/80 passed. Focused Clang, MSVC x64, and GCC ASan/UBSan:
3/3 passed each. Only the affected targets were freshly rebuilt in these
directories. A wider Clang SSA-label run was not green: 34 test executables
had not been built in that directory, and `ssa_runtime_objects` failed its
inactive-cleanup state-map construction assertion. That failure is outside
the focused targets and has not been attributed to this slice.
