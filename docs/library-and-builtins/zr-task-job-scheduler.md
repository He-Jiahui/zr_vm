---
related_code:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/05-async-frame-budget.md
  - user: 2026-08-05 完成 Syntax 10C official provider convergence
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
tests:
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
  - tests/acceptance/2026-10-02-task-scheduler-queue-reuse.md
  - tests/library/test_official_provider_convergence.c
  - tests/acceptance/2026-08-05-syntax-10c-official-provider-convergence.md
doc_type: module-detail
---

# `zr.task` Job and Scheduler Contract

## Scope

Syntax 12 M6.2 exposes the cold `Job<T>` handoff and cooperative `Scheduler`
surface as the sole `zr.task` scheduling contract. The descriptor module
version is `3.0.0`. It does not expose `TaskRunner`, coroutine scheduler
names, worker threads, or script-controlled pumping.

The product descriptor is registered by
`ZrCore_TaskRuntime_RegisterBuiltins`, declares Runtime phase, and publishes
`zr.task:v3:task-job-scheduler` as its public contract hash. The excluded
`zr_vm_lib_task` directory is not part of the root product graph and is not a
second accepted provider.

## Public Contract

- `zr.task.Job<T>` implements the Task Job protocol and is a non-Copy,
  single-consumption value.
- `init Job<T>(callable)` stores a cold callable. It does not queue or run
  work by itself.
- `zr.task.Scheduler` implements the Task Scheduler protocol.
- `Scheduler.schedule(Job<T>)` consumes its first argument and returns a
  `zr.task.Task<T>` completion handle. Reusing the submitted Job is a source
  ownership error.
- `zr.task.currentScheduler` resolves to the local cooperative scheduler.
- `zr.task.yieldNow()` and `zr.task.delay(...)` return completion Tasks via
  the same Task ABI. M3 establishes the call contract only; a public Duration
  value provider belongs to a later scheduler/provider milestone.

At runtime, consuming a Job clears its callable even when task-handle
allocation fails. A failed queue handoff therefore cannot restore the source
owner or execute the callable twice.

## Private queue exhaustion and reuse

The cooperative scheduler keeps an array and an integer cursor in existing
private fields. A step advances the cursor before calling a Job, so a Job may
enqueue more work while the outer pump is active. Cooperative Tasks may append
themselves for their remaining turns. The pump therefore retains the current
queue until the cursor reaches its end, including work appended during a call.

At exhaustion, the runtime detaches that array by setting the queue field to
null. It leaves the exhausted cursor unchanged. Resetting the cursor while
retaining the array would replay completed Tasks whose callable has already
been cleared, turn their status from completed to faulted, and stop newly
submitted work from running. The next queue creation checks that resetting the
cursor to zero succeeded before allocating and attaching a replacement array.
A rejected detach leaves the old cursor at its exhausted position.

`task_runtime_scheduler_queue.inc` contains this private transition. It scans
existing hash pairs using borrowed string keys without allocating new keys.
Its update helper accepts only normalized values without ownership whose types
are null, array or int64. It calls the core boolean member setter so member
versions, lookup caches and GC barriers follow the ordinary object contract,
then checks the thread status and reads the same pair back. A missing or
malformed field, a rejected reset, or a rejected attach returns no queue.
These checks cover the queue transition; the other historical void field
setters keep their existing scope and are not changed by this repair.

During replacement allocation and attach, a native pin keeps the scheduler
alive and stable. Allocation and attach execute inside `Exception_TryRun`,
which allows the caller to unpin after both normal return and a caught memory
error. Failure removes this replacement if it was partially published; it
does not reset the cursor of a still-attached exhausted queue. Pin ownership
is balanced for the registration and flags added by this call.

The focused tests compile real source for completed-result reuse, cooperative
yield, and reentrant Job scheduling. A host callback submits the inner cold Job
through the canonical `ScheduleJob` API while the outer Job is running, checks
that the inner Task remains queued, then verifies its result after the outer
pump returns. Additional private-helper checks use real core objects and
read-only field descriptors to reject reset/detach/attach, and reject both the
queue allocation and its GC retry to exercise caught OOM and pin cleanup.
Every new case releases its roots and destroys the VM before Unity assertions.
The exact RED/GREEN commands, immutable linked inputs, and coverage limits are
recorded in the linked acceptance document. This repair does not complete the
06.05 asynchronous wait, execution budget or background compiler milestones.

## Canonical Metadata and Facts

The native descriptor publishes protocol masks and stable member contract
roles for the Job constructor and `Scheduler.schedule`. Parser semantic
reference facts carry the resolved role and ownership qualifier. A native
member with a non-zero contract role is an exact resolved fact even when it
has no source declaration `SymbolId`.

The ownership move pass consumes an argument only when the resolved call fact
identifies the Scheduler schedule role at argument zero. It does not inspect
the member spelling or synthesize legacy `start`/`pump` methods.

The compiler also rejects a standalone Task expression through the canonical
Task Handle protocol. A Task must be awaited, returned, or stored rather than
silently discarded.

## Migration Boundary

M6.2 removes `%async`, `%await`, `%async T`, `TaskRunner`, `Async`,
`defaultScheduler`, public scheduler pumping, and `zr.coroutine`. Source must
use `async fn ...: Task<T>`, direct `await`, and the resolved
`currentScheduler.schedule(Job<T>)` contract. Thread providers consume the
same Job/Scheduler role and do not recreate the deleted wrappers.
