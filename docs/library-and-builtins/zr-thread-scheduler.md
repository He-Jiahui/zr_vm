---
related_code:
  - CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_wrappers.c
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/execution/execution_memory.c
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - user: 2026-08-05 完成 Syntax 10C official provider convergence
  - user: 2026-07-25 execute Syntax 12 milestones and record each completed milestone
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
  - docs/plans/syntax/12-async-task-job-scheduler/m4-attached-domain-thread-scheduler-implementation-plan.md
  - docs/plans/syntax/12-async-task-job-scheduler/m5-isolated-domain-transport-implementation-plan.md
tests:
  - tests/thread/test_thread_runtime.c
  - tests/library/test_official_provider_convergence.c
  - tests/acceptance/2026-07-25-syntax-12-m4-attached-domain-thread-scheduler.md
  - tests/acceptance/2026-07-25-syntax-12-m5-isolated-domain-transport.md
  - tests/acceptance/2026-08-05-syntax-10c-official-provider-convergence.md
doc_type: module-detail
---

# `zr.thread.ThreadScheduler`

## Purpose

`ThreadScheduler` is the `zr.thread` provider for the existing canonical
`zr.task.Scheduler.schedule(Job<T>)` contract. It does not define a second
Task, Job, or Scheduler family. A submission consumes the existing
`zr.task.Job<T>` once and returns the existing caller-domain `zr.task.Task<T>`
completion object.

The `zr.thread` descriptor explicitly declares Runtime phase and
`zr.thread:v1:isolated-scheduler-send` as its public contract hash. Syntax 10C
validates that it owns `ThreadScheduler` and `Send` while publishing no local
`Task` or `Job` TypeDef.

The M4 provider is specifically the AttachedDomain policy: a worker state is
a mutator of the caller's `GcDomain`. It is therefore not the legacy isolated
`Thread.start(TaskRunner)` implementation and does not use its transport
queues as evidence for this contract.

## Public Contract

- `new zr.thread.ThreadScheduler(workerCount: int)` creates a provider using
  the host-selected AttachedDomain or IsolatedDomain policy. The constructor
  checks only that the signed `int64` argument is positive before narrowing it
  to `uint32` for the worker limit. BUG: `4294967297` becomes 1; an upper-bound
  check is missing.
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
   safepoint before draining another request. Task terminal status can be
   observed before the worker exits its mutator and frees its state.
5. `Task.result()` delegates to the provider await hook. It waits on the
   scheduler condition until the same caller-domain Task is completed or
   faulted; it never steps a legacy scheduler queue as a fallback. This is a
   result wait, not a worker join.

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

The M4 milestone covered the same-domain AttachedDomain provider. The later
M5 provider added IsolatedDomain transport, described below. Neither path
restores a consumed Job after failed submission. Work stealing is outside the
current provider contract.

## IsolatedDomain Provider (M5)

The embedding host may select `IsolatedDomain` before constructing a
`ThreadScheduler`. The public ZR contract remains unchanged. A
caller-side FIFO retains prepared caller Task roots and a callable artifact
while worker capacity is unavailable. Once a worker has
created and published its independent `GcDomainIdentity`, the caller creates
and publishes separate request/capture envelopes for that target. The worker
claims and commits those envelopes in its own domain; its result returns in a
separate envelope for caller-domain commit.

The queue has no worker affinity: a capacity slot made available by a caller
completion or fault acknowledgement starts the next unclaimed launch. A
shutdown stops subsequent submissions and faults pending queued work without
waiting for live workers. ResourceMove and ImmutableHandle remain
canonical type-layout provider contracts. The scheduler receives only an
`SZrTypeValue`; if no provider metadata is available at that boundary, it
publishes a `FORBIDDEN` envelope and faults the already-consumed Job's Task.
It never infers a provider from a value category, type name, or raw pointer.

The caller decodes a result envelope and completes or faults the Task before
signaling `completionProcessed`. The isolated worker then wakes and frees its
worker global. BUG: `Task.result()` can return before that teardown; the
worker global's allocator arguments still refer to the caller global. Releasing
the caller global immediately after result or `ShutdownIsolatedSchedulers`
can therefore leave a dangling allocator context. Shutdown is documented to
reject new work and fault queued Jobs, not to join live workers. TODO: define
a separate host barrier if safe global destruction is required. TODO: confirm
whether the worker limit is intended to cap active Jobs or actual OS workers:
the slot is released on completion acknowledgement, before worker teardown.
TODO: capture envelopes are prepared when a queued launch obtains a worker
slot, rather than at `schedule` submission. Review when the capture ownership
contract becomes effective for queued Jobs.

## Test Coverage

`tests/thread/test_thread_runtime.c` verifies:

- descriptor protocol ids and the canonical schedule member role;
- a worker state attaching to the caller `GcDomain`;
- one Job completing through `ThreadScheduler`;
- two jobs draining in source submission order with `workerCount = 1`;
- IsolatedDomain zero-capture/scalar/StructuredClone capture and result paths;
- bounded multi-worker FIFO delivery, quota faults, forbidden payload faults,
  later shutdown rejection, and queued shutdown faults;
- rejection of a non-`Send` result; and
- rejection of a second submission of the same Job.

Those tests do not establish worker teardown after `Task.result()` or
`ShutdownIsolatedSchedulers`, nor do they cover the `int64` to `uint32`
`workerCount` overflow.

The M5 acceptance record captures the isolated GCC, Clang, and MSVC evidence,
the canonical provider transfer/race tests, and the pre-existing legacy
TaskRunner failures that remain outside this provider contract.
