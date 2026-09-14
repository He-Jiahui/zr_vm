---
related_code:
  - zr_vm_core/include/zr_vm_core/async_frame_budget.h
  - zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c
  - zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c
  - zr_vm_core/include/zr_vm_core/task_frame_runtime.h
  - zr_vm_core/include/zr_vm_core/execution_budget.h
  - zr_vm_library/include/zr_vm_library/task_runtime.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/async_frame_budget.h
  - zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c
  - zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/05-async-frame-budget.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - "user: 2026-09-14 implement 06.05 async frame budget"
tests:
  - tests/task/test_ssa_async_frame_budget.c
  - tests/acceptance/ssa-async-frame-budget.md
doc_type: module-detail
---

# Frame-safe asynchronous scheduling

## Purpose

`async_frame_budget.h` is the small, pointer-light contract between an
async/task lowering and a scheduler.  It does not replace the concrete
`SZrCoreTaskFrameTask` runtime or the existing VM execution budget.  Instead it
defines the invariants that adapters must preserve when they hand a frame back
to a scheduler: suspension is explicit, a state-map boundary is present,
non-crossing borrows and critical native sections never yield, and teardown
cannot race an active pin.

The same module contains two non-blocking coordination records.  The wait
registry closes the register/recheck lost-wakeup window with a single atomic
winner transition.  The compile queue owns a copied IR snapshot, so a worker
never reads a movable AST or VM allocation and never publishes a result whose
generation or contract hashes are stale.

## Frame and budget state machine

An initialized `SZrAsyncFrameBudget` starts in `IDLE` and enters `RUNNING` only
through `ZrCore_AsyncFrameBudget_Begin`.  `Poll` accounts bounded work units at
loop backedges and call boundaries.  Reaching the configured work limit sets a
pending bit; it becomes `SUSPENDED` only when all of the following hold:

1. the caller is in an async contract with suspension enabled;
2. the poll is at a declared state-map boundary whose generation matches the
   frame generation;
3. no borrow, stack alias, non-suspendable lock guard, or native critical
   section is live.

If a native critical section is still running, a pending budget or cancellation
request is recorded and the poll remains `RUNNING`; the native instruction is
never split.  Cancellation is observed at the next coherent boundary and is a
normal `CANCELLED` outcome, not a guest exception.  `Pin`/`Unpin` are paired
 counters on the frame lease.  `Teardown` is idempotent after a terminal state,
 but rejects a running or pinned frame, which makes partial cleanup visible to
 the host instead of silently dropping a live continuation.

All diagnostics contain only enum values, source/instruction IDs, scalar
expectations, and a generation.  They can therefore cross a scheduler or
worker boundary without retaining an AST, VM object, or host address.

## Waiter registration and wakeup

`ZrCore_Execution_BeginAsyncWait` first validates the async request and
resource restrictions, reserves a caller-owned slot, and publishes
`REGISTERING`.  It then atomically publishes `WAITING` (or `READY` when the
initial condition is already true).  `Wake`, `Cancel`, and `Timeout` all race
through the same CAS from `REGISTERING`/`WAITING` to their terminal state.
Consequently a wake between registration and condition recheck is retained;
`Recheck` treats an already terminal winner as success rather than overwriting
it.  Only `Resume` can move a terminal waiter to `RESUMED`, and it increments
the resume counter exactly once.  `Release` accepts only a resumed waiter and
returns its slot to the registry, preventing a scheduler from freeing a still
waiting continuation.

The registry is fixed-capacity and caller-owned.  Capacity exhaustion and stale
handles are structured diagnostics, not an implicit blocking allocation.

## Background compilation and warm-up

`ZrCore_Execution_QueueCompilation` copies the supplied byte snapshot before
publishing a `QUEUED` record.  The request carries generation, module,
signature, and layout hashes; all must be non-zero and are checked again by
`ZrCore_CompileQueue_Complete`.  A worker claims a record as `RUNNING`, reads
the immutable snapshot, and then either:

- publishes `COMPLETED` when every identity still matches and a non-zero result
  hash is supplied;
- marks the result `DISCARDED` for a generation/contract mismatch; or
- marks it cancelled/discarded when cancellation was requested.  A queued job
  can become `CANCELLED` immediately; a running job only records the atomic
  cancellation flag and stays `RUNNING` until the worker calls `Complete`.
  `Complete` is the worker acknowledgement and moves that job to
  `DISCARDED`, after which `Release` may free the snapshot.  `Release` rejects
  a running job, so cancellation cannot free bytes that a worker may still be
  reading.

The `WARMUP` request bit is metadata for a host loading phase; it does not
change execution semantics.  There is intentionally no wait-for-completion
operation in this contract.  A frame thread can enqueue, poll, or cancel and
continue running its baseline path.  Queue release frees the copied snapshot
exactly once, including cancellation and stale-result paths.

Queue requests carry both a queue pointer and an output-handle pointer.  The
implementation requires each to match the explicit arguments (and rejects
NULL identities), preventing a request from being published through an
unrelated queue or handle.  Queue record transitions are internally
serialized, so copied handle values may be used by a worker and an owner.  A
single handle object must not be concurrently mutated, the queue must remain
initialized until all jobs are terminal/released, and a borrowed snapshot
returned by `GetSnapshot` must be fully consumed before the worker calls
`Complete`.  `Deinit` is therefore a quiescent operation; it is not a worker
shutdown primitive.

## Integration boundaries

The files are standalone core sources so they can be integrated into the core
target without changing existing dispatch or task-runtime ownership.  The
parent build integration must register both C sources and
`tests/task/test_ssa_async_frame_budget.c` under the planned
`ssa_async_frame_budget` CTest.  Existing `execution_budget.h/c` and
`task_frame_runtime.c` remain the owners of concrete VM budget counters and
GC-rooted task slots; an adapter should call this contract at their safe poll,
state-map, and cleanup boundaries rather than create a second counter or
continuation owner.

## Test coverage

The focused test exercises budget exhaustion before and at a coherent boundary,
borrow rejection, pin balancing, cancellation and idempotent teardown, wake
between registration and recheck, cancel/timeout winner races, exactly-once
resume, immutable snapshot copying, stale-generation disposal, warm-up tagging,
and malformed snapshot rejection.  Exact commands and tool versions are
recorded in `tests/acceptance/ssa-async-frame-budget.md`.

## Out of scope

This contract does not allocate or move a real VM stack frame, implement a
platform event loop, interrupt a native callback, compile ExecIR itself, or
publish machine code.  Those actions remain in the concrete task runtime and
backend services; they must consume the scalar state and diagnostics defined
here.
