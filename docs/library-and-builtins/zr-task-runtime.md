---
related_code:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - tests/task/test_task_runtime.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
plan_sources:
  - docs/plans/syntax/12-async-task-job-scheduler/m6-artifact-debug-lsp-migration-implementation-plan.md
tests:
  - tests/task/test_task_runtime.c
doc_type: module-detail
---

# zr.task Built-in Runtime

## Public Contract

`zr.task` exposes exactly three task-facing types:

- `Task<T>` is the completion handle.
- `Job<T>` owns one cold callable and is consumed by scheduling.
- `Scheduler` is the canonical scheduler capability. Its only public operation
  is `schedule(Job<T>): Task<T>`.

The module provides `currentScheduler` as the local provider. `yieldNow()` and
`delay(...)` retain their Task-returning ABI. A completed task is observed with
`Task.result()` or by direct `await` inside an `async` callable.

```zr
var task = %import("zr.task");
var job = init task.Job<int>(() => { return 17; });
var completion = task.currentScheduler.schedule<int>(job);
return completion.result();
```

`Job<T>` is move-only. Submission consumes the source Job even when queue
allocation fails, so a failed handoff cannot execute the callable twice.

## Source And Runtime Boundary

The source form is explicit:

```zr
async fn fetch(pending: zr.task.Task<int>): zr.task.Task<int> {
    return await pending;
}
```

`Task.result()` drives the task's owning provider only when it is not already
inside that provider's active scheduler frame. The queue, pump state, and
provider wait loop are runtime-private; source code cannot select or mutate
them through a legacy scheduler API.

## Removed Compatibility Surface

The following are rejected or absent from native metadata:

- `%async`, `%await`, and `%async T`
- `TaskRunner<T>`, `Async<T>`, `__createTaskRunner`, and `__awaitTask`
- `defaultScheduler`, `start`, `step`, `pump`, `setAutoCoroutine`, and
  `getAutoCoroutine`
- the `zr.coroutine` module and its scheduler aliases
- the project `autoCoroutine` setting

Dynamic source member access may be represented until execution, but an absent
native descriptor is never synthesized as a compatibility fallback and fails
when invoked. Consumers must use the resolved Task/Job/Scheduler contract,
not a member name or a source-text migration heuristic.

## Context Safety

The compiler derives async effect from the explicit declaration AST. `await`
requires the Task-handle protocol with one payload type, rejects non-Task
operands, and establishes a suspension boundary for borrow, loan, and affine
guard checks. No Task name or diagnostic text is used to infer that boundary.
