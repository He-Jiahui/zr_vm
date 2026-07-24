---
related_code:
  - zr_vm_core/include/zr_vm_core/task_frame_runtime.h
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/ownership.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/task_frame_runtime.h
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
plan_sources:
  - user: 2026-07-25 execute Syntax 12 milestones and record each result
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
  - docs/plans/syntax/12-async-task-job-scheduler/m2-task-frame-runtime-implementation-plan.md
tests:
  - tests/task/test_task_frame_runtime.c
  - tests/acceptance/2026-07-25-syntax-12-m2-task-frame-runtime.md
doc_type: module-detail
---

# Task Frame Runtime

## Purpose

Syntax 12 M2 provides the core runtime state machine behind a future
`Task<T>` lowering. It is deliberately a typed runtime primitive: the task
and each suspended slot carry explicit state, GC-root, and cleanup facts.
It does not infer coroutine state from `zr.task` dynamic-object fields,
function names, or a legacy `TaskRunner` protocol.

## State And Poll Contract

`ZrCore_TaskFrameTask_Start` enters `RUNNING` and invokes a caller-supplied
poll function. The poll function either completes, faults, or calls
`ZrCore_TaskFrameTask_Suspend` with a state id from its declared layout.
Only the suspend transition obtains a frame from `SZrCoreTaskFramePool` and
changes the task to `SUSPENDED`; therefore a synchronously completed task has
zero frame allocations.

`Resume` moves a suspended task back to `RUNNING` and invokes the same poll
function. The saved state id is stable across multiple pending polls. The
runtime operations correspond to the plan's runtime half of `AsyncEnter`,
`AwaitPoll`, `Suspend`, `Resume`, `AsyncComplete`, and `AsyncFault`; source
and semantic-IR emission remain separate compiler work.

## Frame Layout, GC, And Cleanup

`SZrCoreTaskFrameLayout` declares the valid state-id range and one descriptor
per hoisted slot. Each descriptor records whether the slot is a GC root and
whether it requires a drop callback. Storing a slot replaces any prior
initialized value through one cleanup path, so overwrite, fault, terminal
free, and pool reuse cannot skip a registered drop.

GC-rooted slots receive a `SZrGcRootHandle`. A load resolves that handle before
copying the value, which preserves a compacted object identity without
retaining a stale raw pointer. Completed task results and fault values are
also rooted on the task header until they are awaited or released, so the
synchronous path remains safe even though it never promoted to a frame.

## Completion, Fault, And Await

Plain results may be observed by more than one await. An ownership-bearing
result transfers from the task exactly once; later awaits return
`RESULT_CONSUMED`. A layout may also provide one `finally` callback. It runs
before the frame's initialized slots are released on complete, fault, or an
early task free, and the task records that it has run so later cleanup cannot
invoke it a second time. Faulting then returns a leased frame to the pool and
exposes a rooted error value. These paths use ordinary `SZrTypeValue` ownership
operations rather than a second async-specific value model.

## Pooling Boundary

`SZrCoreTaskFramePool` stores only clean, idle frames. A suspended task leases
a frame whose capacity matches its layout; completion, fault, and task free
return it after releasing initialized slots and roots. Reacquisition clears
state and initialization bits, which prevents a later task from seeing a
prior task's live-slot or cleanup state.

## Scope Boundary

This M2 runtime foundation does not itself lower source `await`, define
`Job`/`Scheduler`, schedule worker work, add artifact/AOT frame rows, project
debug or LSP state, or migrate the legacy `TaskRunner` surface. Those are
separate plan milestones. In particular, this module is not evidence that the
old dynamic `TaskRunner` model has become the canonical `Task<T>` runtime.

## Test Coverage

`test_task_frame_runtime.c` covers synchronous no-allocation completion,
multiple pending/resume states, one-shot finally-before-cleanup on fault,
initialized-only fault cleanup including slot overwrite, GC survival for
suspended slots and completed header results, typed pool reuse, and
exactly-once transfer of a non-Copy result.
