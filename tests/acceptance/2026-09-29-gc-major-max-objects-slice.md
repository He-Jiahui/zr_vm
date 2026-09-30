# Concurrent-major `maxObjects` slice limit

Date: 2026-09-29

## Scope

These regressions exercise the real global state, collector, concurrent-major
initial snapshot, public `ZrCore_Gc_SetBudget` API, active-mark root insertion,
and public `ZrCore_GarbageCollector_GcStep` path. A configured nonzero
`maxObjects` must lower the caller's queue-pop cap for one concurrent-mark
invocation. It cannot raise that cap. Zero retains the caller cap. The limit is
per call and does not promise a wall-clock bound for scanning one large object
or bound the complete major cycle. The scalar `EvaluateBudgetStep` API remains
unconnected to runtime telemetry.

## Fixture and expected RED

`test_concurrent_major_slice_honors_max_objects` first registers one explicit
root, starts a major cycle through the real collector, then creates and
initializes 16 objects while that cycle is active. It adds each fixture object
through `ZrCore_GarbageCollector_IgnoreObject`; the active-mark root helper
prepends each object to `waitToScanObjectList`. Before the slice, the fixture
records that all 16 objects remain in the collector object list, are gray, and
occupy the first 16 queue positions directly ahead of the pre-existing root.
It also checks that this earlier root is still present in the original queue.
No worker thread is created; worker count is set to one so the uncapped caller
limit is eight objects.

The test records how many fixture objects are referenced after exactly one
public `GcStep`, then drains the cycle and unignores all fixture objects before
performing any Unity assertions. The root-owned test-first direct run exercised
12 tests and failed only this regression: expected one referenced object but
observed eight. The other 11 tests passed. Its live/gray state, exact queue
prefix and tail, pre-existing root, and cleanup preconditions all passed. The
target has no CTest registration.

The real-collector fixture now runs three cases: `maxObjects=1` expects one
object, `maxObjects=0` expects the caller's eight-object cap, and
`maxObjects=64` also expects eight because the configured value cannot enlarge
the caller's cap. Since an all-zero budget is invalid, the zero case sets an
otherwise unused `maxWorkUnits=1` to pass `SetBudget` validation; this slice
does not consume that dimension. Every case completes or force-cleans its
active major cycle and removes temporary roots before making Unity assertions.

The implementation clamps the shared concurrent mark slice only when a budget
is configured, `maxObjects` is nonzero, and the value is below the caller cap.
It does not connect `EvaluateBudgetStep`: budget telemetry remains at
`ACCEPTED/IDLE` with cursor/work zero after configuration. `SetBudget` updates
and the mark-slice read now share the GC domain's recursive mutation lock, which
serializes those accesses for a registered domain.
`ZrCore_GarbageCollector_GetBudget`,
`ZrCore_GarbageCollector_EvaluateBudgetStep`,
`ZrCore_GarbageCollector_GetBudgetStats`, `ZrCore_Gc_GetStats`, and
`ZrCore_GarbageCollector_GetStatsSnapshot` remain outside this synchronization
guarantee. The snapshot getter does not take the mutation lock, and mark-slice
counters are updated after the slice releases that lock. Callers must serialize
overlapping budget reads/evaluation and snapshot access with `SetBudget` or GC
stats updates. With no initialized GC domain, the lock helpers are no-ops.

## Verification status

- Test target: `zr_vm_gc_concurrent_major_test`.
- CTest: this target has no CTest registration; acceptance requires direct
  execution of the built Unity binary.
- Test-first build and direct execution: root-owned MSVC Debug build succeeded;
  the direct binary reported 12 tests, 1 failure, exit 1 as described above.
- Earlier root-owned MSVC Debug focused build, before the temporary malformed
  comment was introduced: all 16 requested native targets completed, 471/471
  steps, exit 0. This is not evidence that the later corrected header text
  compiled. Exact command from the repository root:

  ```powershell
  python D:/tmp/zr_vm/ssa-control/run_native.py final-focused-build build zr_vm_ssa_core_model_test zr_vm_ssa_static_binding_facts_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_pass_manager_scalar_test zr_vm_ssa_binding_rows_artifact_test zr_vm_ssa_value_validation_test zr_vm_ssa_escape_ownership_test zr_vm_ssa_interprocedural_inlining_test zr_vm_ssa_loops_specialization_test zr_vm_ssa_generated_fusion_test zr_vm_container_specialization_test zr_vm_ssa_aot_projection_descriptor_test zr_vm_ssa_exec_ir_execbc_vm_test zr_vm_ssa_exec_ir_artifact_v6_test zr_vm_ssa_capability_validation_test zr_vm_gc_concurrent_major_test
  ```

- Direct GC binary command:

  ```powershell
  python D:/tmp/zr_vm/ssa-control/run_suite.py msvc gc
  ```

  The runner invoked
  `D:/tmp/zr_vm/ssa-artifact-v6-msvc/bin/zr_vm_gc_concurrent_major_test.exe`.
  It reported `14 Tests 0 Failures 0 Ignored`, exit 0; all three maxObjects
  cases passed. This direct run used the binary from the build preceding the
  temporary malformed-comment edit. Logs: `D:/tmp/zr_vm/ssa-control/final-focused-build.log`
  and `D:/tmp/zr_vm/ssa-control/gc-direct.log`.

- A later GCC/WSL integration attempt used this 18-target command and failed
  while compiling the temporary malformed `gc.h` comment. It stopped at 6/969
  planned build steps, exit 1; this was a C compilation failure, not a CMake
  configuration failure:

  ```bash
  wsl -d Ubuntu-22.04 -- bash /mnt/d/tmp/zr_vm/ssa-control/run_wsl.sh gcc build zr_vm_ssa_core_model_test zr_vm_ssa_static_binding_facts_test zr_vm_ssa_oracle_projections_test zr_vm_ssa_pass_manager_scalar_test zr_vm_ssa_binding_rows_artifact_test zr_vm_ssa_value_validation_test zr_vm_ssa_escape_ownership_test zr_vm_ssa_interprocedural_inlining_test zr_vm_ssa_loops_specialization_test zr_vm_ssa_generated_fusion_test zr_vm_container_specialization_test zr_vm_ssa_aot_projection_descriptor_test zr_vm_ssa_exec_ir_execbc_vm_test zr_vm_ssa_exec_ir_artifact_v6_test zr_vm_ssa_capability_validation_test zr_vm_gc_concurrent_major_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_gc_root_frame_test
  ```

  The exact failed invocation is preserved in
  `D:/tmp/zr_vm/ssa-control/gcc-build.log`. This failure used the temporary
  malformed header comment; that comment was corrected afterward and this old
  GCC failure does not describe the current header.

- Current-header root-owned MSVC Debug incremental build covered these five
  targets and completed 352/352 steps, exit 0:

  ```powershell
  python D:/tmp/zr_vm/ssa-control/run_native.py header-final-build build zr_vm_gc_concurrent_major_test zr_vm_ssa_exec_ir_artifact_v6_test zr_vm_ssa_capability_validation_test zr_vm_ssa_exec_ir_execbc_vm_test zr_vm_ssa_oracle_projections_test
  ```

  Log: `D:/tmp/zr_vm/ssa-control/header-final-build.log`.

- After that build, the root-owned direct GC Unity run used:

  ```powershell
  python D:/tmp/zr_vm/ssa-control/run_suite.py msvc gc
  ```

  It ran `D:/tmp/zr_vm/ssa-artifact-v6-msvc/bin/zr_vm_gc_concurrent_major_test.exe`,
  reported 14/14, exit 0. The root reported the output in its active session;
  this run has no dedicated log file.

- A root-owned Clang GC Unity direct run started but did not complete. GDB
  observed Unity state `TestFailures=0`, `TestIgnores=0`, and
  `NumberOfTests=8`, with
  `test_concurrent_marker_and_mutator_serialize_object_storage` as the active
  case. Therefore the preceding seven cases completed without a reported
  failure; these include all three `maxObjects` cases at positions 3–5 in the
  Unity order. This is partial Clang evidence, not a full 14/14 pass. State
  evidence: `D:/tmp/zr_vm/ssa-control/clang-gc-unity-state-gdb.log`.

- The all-thread GDB capture confirmed a deadlock in that eighth case:
  the main thread waited for the mutation mutex in
  `garbage_collector_concurrent_major_mark_slice`, while the worker waited in
  the safepoint condition from a nested `MutationBegin` during
  `ZrCore_Object_SetValue`. See
  `D:/tmp/zr_vm/ssa-control/clang-gc-deadlock-gdb.log`. After verifying the
  owned process executable path, PID 45229 was terminated with SIGTERM; the
  termination record is `D:/tmp/zr_vm/ssa-control/clang-gc-deadlock-termination.log`.
  The Clang GC suite was interrupted and is not green.

- At the time of this record the large GCC build was still running. The earlier
  Clang configure exit 0 is configuration-only evidence; the GC test evidence
  above is partial and must not be described as a completed Clang build/test
  matrix.

- Production changes cover `gc_concurrent_major.c`, `gc_budget_runtime.c`, and
  `gc.h`: the mark slice reads its cap under the domain mutation lock, and
  `SetBudget` replaces the configuration plus its budget snapshot under that
  same lock. No CMake file changed.
- Not verified here: completion of the current GCC build or full Clang GC test
  suite, Release/NDEBUG, sanitizer runs, complete major pause bounds, or
  budget-step telemetry integration.
