# SSA 01.05: oracle/ExecBC ownership DROP differential

## Scope

The pointer-free ExecBC projection now carries per-slot value facts and
executes `DROP` and `DROP_IF_INITIALIZED` with owner state and owned DROP
events. MOVE transfers an initialized owner's value and marks its source
MOVED. The AOT projection receives and frees the same value-fact metadata,
but remains non-runnable.

## Regression evidence

- `test_oracle_execbc_drop_differential` verifies each constructed ExecIR with
  `ZR_EXEC_IR_VERIFY_ALL` before lowering. Independent oracle and projected
  runs compare event payloads/source/order, return bits, DROP count, and
  physical owner states for ordinary DROP, two guarded drops (one event),
  and MOVE followed by a guarded no-op (zero events).
- A branching fixture defines the owner on one arm only. Its verifier-valid
  cleanup join emits one DROP on the initialized arm and no DROP on the
  uninitialized arm; both paths compare owner states and traces.
- A deliberately changed projected RETURN operand reads an already dropped
  owner; it reports INVALID_VALUE at the RETURN source and does not replace
  the previously published execution result. The existing MOVE regression
  also checks INVALID_VALUE and source identity for read-after-MOVE.

Only pointer-free values and bounded DROP events are covered. The runner is
not the production ExecBC dispatcher. Exceptional cleanup, ownership
callbacks, writeback, full effect/state-map parity, AOT execution, and
fault-injected event-allocation failures remain outside this slice and do not
close the full 01.05 milestone.

## Validation

The first GCC build was RED on the missing projected owner-state field. The
first verified cleanup fixture then rejected an unmarked cleanup block; after
marking it correctly, the branch join and guarded no-op cases passed. Clang
identified an uninitialized test-local range on the short-circuit failure
path, and MSVC identified two signed/unsigned comparisons in test assertions;
both were corrected before the final builds. Existing MOVE diagnostics were
updated to assert the oracle-matching INVALID_VALUE and source ID.

```text
wsl cmake --build build/ssa-gcc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_parser_shared -j 4
wsl ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error -j 4
wsl cmake --build build/ssa-clang-debug --target zr_vm_ssa_oracle_projections_test -j 4
wsl ctest --test-dir build/ssa-clang-debug -R ssa_oracle_projections --output-on-failure --no-tests=error
wsl cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_oracle_projections_test -j 4
wsl ctest --test-dir build/ssa-gcc-asan-phase80 -R ssa_oracle_projections --output-on-failure --no-tests=error
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build/ssa-msvc-debug --target zr_vm_ssa_oracle_projections_test zr_vm_ssa_differential_harness_test zr_vm_parser_static --config Debug -j 4
ctest --test-dir build/ssa-msvc-debug -C Debug -R ssa_oracle_projections --output-on-failure --no-tests=error
```

The final GCC Debug SSA-label sweep passed 80/80 (the other 79 test binaries
were not all freshly rebuilt). Clang Debug, Windows MSVC Debug, and GCC
ASan/UBSan each passed the focused `ssa_oracle_projections` target. The
sanitizer reported no memory or undefined-behavior errors. No fault-injected
allocation failure or exceptional cleanup was exercised.
