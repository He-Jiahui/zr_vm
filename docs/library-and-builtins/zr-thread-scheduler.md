---
related_code:
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
plan_sources:
  - user: 2026-07-25 execute Syntax 12 milestones and record each completed milestone
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
  - docs/plans/syntax/12-async-task-job-scheduler/m4-attached-domain-thread-scheduler-implementation-plan.md
tests:
  - tests/thread/test_thread_runtime.c
  - tests/acceptance/2026-07-25-syntax-12-m4-attached-domain-thread-scheduler.md
doc_type: module-detail
---

# `zr.thread.ThreadScheduler`

## Purpose

`ThreadScheduler` is the `zr.thread` provider for the existing canonical
`zr.task.Scheduler.schedule(Job<T>)` contract. It does not define a second
Task, Job, or Scheduler family. A submission consumes the existing
`zr.task.Job<T>` once and returns the existing caller-domain `zr.task.Task<T>`
completion object.

The M4 provider is specifically the AttachedDomain policy: a worker state is
a mutator of the caller's `GcDomain`. It is therefore not the legacy isolated
`Thread.start(TaskRunner)` implementation and does not use its transport
queues as evidence for this contract.

## Public Contract

- `new zr.thread.ThreadScheduler(workerCount: int)` creates a provider with a
  positive concurrent-worker bound.
- `scheduler.schedule<T>(job: zr.task.Job<T>): zr.task.Task<T>` consumes
  `job` at the resolved Task Scheduler schedule contract role.
- `T` must satisfy the descriptor-backed `zr.thread.Send` protocol. The
  generic constraint is resolved from the imported protocol mask, not from a
  short type name or source spelling.
- `zr.thread.Send` and `zr.thread.Sync` are separate protocol ids. Primitive
  and recursively value-safe array values satisfy either capability; borrowed,
  loaned, shared, and weak ownership shells do not become thread-safe merely
  because their inner value is primitive.

`ThreadScheduler` itself is deliberately not `Send`, so a job returning the
scheduler is rejected by the same generic capability path. Reusing a submitted
Job is rejected by the canonical Unique move fact before runtime execution.

## AttachedDomain Execution Flow

1. The provider bridge calls `ZrLibrary_TaskRuntime_PrepareJob`. This consumes
   the Job callable, creates the caller-domain Task, and roots that Task for
   queue ownership.
2. A fully initialized request is published under the scheduler mutex. The
   mutex unlock/lock pair is the queue release/acquire boundary; no partially
   initialized request is visible to a worker.
3. A worker is created with the caller global state and enters through
   `ZrCore_State_MutatorLaunch`, which attaches it to the caller `GcDomain`.
4. The worker claims one request, executes its prepared Job, publishes the
   existing Task completion ABI, releases the root, and then polls the domain
   safepoint before draining another request.
5. `Task.result()` delegates to the provider await hook. It waits on the
   scheduler condition until the same caller-domain Task is completed or
   faulted; it never steps a legacy scheduler queue as a fallback.

When a worker sees an empty queue, it clears its live-worker slot while holding
the queue mutex. A racing submitter then observes capacity and starts a
replacement worker instead of leaving a newly queued request without a
consumer. If worker creation itself fails before a worker owns the request,
the provider removes that request, faults its prepared Task, releases the
root, and keeps the consumed Job consumed.

## Canonical Static Facts

Native metadata publishes `THREAD_SCHEDULER`, `THREAD_SEND`, and `THREAD_SYNC`
protocol ids and the Task Scheduler schedule member role. Native import keeps
the task parameter's canonical owner module identity, so a source
`task.Job<int>` and the descriptor contract resolve to one closed generic type.

The semantic-reference fact is considered resolved from the member contract
role even when a native declaration has no source `SymbolId`. Ownership
dataflow moves argument zero only for that resolved role. It does not use a
member name, receiver name, or a legacy `start`/`pump` spelling.

## Boundaries

This milestone implements the same-domain AttachedDomain provider only. It
does not convert the old isolate-first worker implementation into evidence for
the new provider. Cross-domain serialization, result transport, quotas,
work-stealing, and provider/domain shutdown protocols remain separate
milestones. The runtime uses the normal Task fault path for a failed local
submission; it does not restore a consumed Job.

## Test Coverage

`tests/thread/test_thread_runtime.c` verifies:

- descriptor protocol ids and the canonical schedule member role;
- a worker state attaching to the caller `GcDomain`;
- one Job completing through `ThreadScheduler`;
- two jobs draining in source submission order with `workerCount = 1`;
- rejection of a non-`Send` result; and
- rejection of a second submission of the same Job.

The acceptance record captures the isolated GCC, Clang, and MSVC evidence and
separately lists the pre-existing legacy TaskRunner failures that are outside
this provider contract.
