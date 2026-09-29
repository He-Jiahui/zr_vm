# SSA Async Frame GC Root Registration Failure Acceptance

## Scope

This record covers the existing task-frame runtime failure path where
`ZrCore_TaskFrameTask_StoreSlot` has copied a GC value into a slot with a
registered drop callback, but growing the GC domain root table fails before a
root handle is created. The expected rollback runs the slot's regular drop and
ownership/root cleanup path exactly once. It does not accept the wider
06.05 waiter, execution-budget, or background-compilation milestone.

## Baseline

Before the fix, a root-handle creation failure released the copied value and
cleared the slot's initialized bit directly. That made subsequent task/pool
cleanup skip the registered drop callback. The path is reachable when
`ZrCore_GcRootHandle_Create` must grow a full root table and its backing
`ARRAY` allocation fails.

## Test Inventory

`tests/task/test_task_frame_runtime.c` adds
`test_gc_root_registration_failure_drops_copied_slot_once`. A test allocator
first rejects one root-table growth to establish a full root table, then
rejects the next growth during `StoreSlot` from a suspended task. The test
checks both injected failures, the task's faulted status, that the drop sees
the copied object exactly once, and that repeated `Task_Free` and `Pool_Free`
do not call the drop again. It releases the filler roots and checks the domain
root count returns to its starting value.

The fixture initially declared one valid suspend state while attempting to
suspend with state id 1. The first test-only run therefore stopped at the
injection-count assertion (`expected 2`, observed `1`) before reaching the drop
assertion. The layout was corrected to declare two states; the corrected
fixture then reached the intended `StoreSlot` failure.

## Tooling Evidence

The root agent owns the shared MSVC build window and ran the corrected test-only
RED snapshot with artifacts under
`D:\tmp\zr_vm\ssa-artifact-v6-msvc`:

```text
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_task_frame_runtime_test -j 2'
D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_task_frame_runtime_test.exe
```

The corrected test-only build exited 0 (2/2 build steps). Direct execution
exited 1 after running seven tests, with exactly one failure at the drop-count
assertion: expected 1, observed 0. The second rejected `ARRAY` allocation and
the `FAULTED` status assertions passed, demonstrating that the injected
failure was the root-table growth during `StoreSlot`, not fixture setup.

Final-source Debug verification used these commands from PowerShell:

```text
cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_call_binding_artifact_test zr_vm_task_frame_runtime_test -j 2'
D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_task_frame_runtime_test.exe
ctest --test-dir D:\tmp\zr_vm\ssa-artifact-v6-msvc -N -R 'task_frame'
```

The final-source MSVC Debug build of `zr_vm_call_binding_artifact_test` and
`zr_vm_task_frame_runtime_test` completed all 19/19 build steps with exit code
0. Direct execution of `zr_vm_task_frame_runtime_test.exe` exited 0: seven
tests, zero failures, including the injected second root-table `ARRAY`
rejection, copied-object observation by the drop, repeated task/pool cleanup,
and restoration of the root-count baseline. `ctest -N -R task_frame` reported
zero registered tests for this target, so the direct executable run is the
acceptance evidence; no CTest execution is claimed.

## Results

The failing assertion reproduced the skipped-drop defect. The production
rollback now calls the existing `task_frame_cleanup_slot` helper after root
registration fails. This restores the normal drop-before-release order and
clears the initialized state through the same cleanup path used by later task
and pool cleanup.

The final-source MSVC Debug direct run passed all seven tests. No `NDEBUG`
build was run because there was no existing `NDEBUG` cache in the shared
artifact directory. The added test uses Unity's always-active assertions; this
does not constitute Release-build evidence. No GCC, Clang, or sanitizer run is
claimed. The unrun configurations are outside this bounded cleanup-path
acceptance and are not reported as blockers.

## Acceptance Decision

**Accepted for the bounded MSVC Debug contract.** Failure injection reached
the intended production path and produced a single targeted RED. The fix
reuses existing cleanup ownership, and the final-source direct test passed
7/7, including the drop-once and repeated-cleanup assertions. No `NDEBUG`
result is claimed; a Release build is outside this acceptance record's
verification evidence.
