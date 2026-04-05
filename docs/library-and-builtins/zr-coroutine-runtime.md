---
related_code:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/parser_reserved_task.c
  - tests/task/test_task_runtime.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
plan_sources:
  - user: 2026-04-05 Task / Coroutine / Thread 并发模型重构计划
tests:
  - tests/task/test_task_runtime.c
doc_type: module-detail
---

# zr.coroutine Built-in Runtime

## Public Surface

`zr.coroutine` 当前作为 builtin 模块公开：

- `Scheduler`
- `coroutineScheduler`
- `start(runner: TaskRunner<T>): Task<T>`

`Scheduler` 同时实现 `zr.task.IScheduler` 这一组最小调度器契约：

- `start(...)`
- `step()`
- `pump()`
- `setAutoCoroutine(bool)`
- `getAutoCoroutine()`

## Scheduler Storage

每个 isolate 当前只创建一个内建 coroutine scheduler，并把它挂到根对象隐藏字段里：

- `__zr_coroutine_scheduler`

模块 materialize 时，`zr.coroutine.coroutineScheduler` 会指向这同一份对象。`zr.task.defaultScheduler` 默认也绑定到它。

## Execution Behavior

- `start(runner)` 把冷 `TaskRunner<T>` 转成已排队的 `Task<T>`
- `autoCoroutine = true` 时，runner 入队后会立即自动 pump
- `autoCoroutine = false` 时，需要显式 `step()` 或 `pump()`
- `%await task` 在任务仍未完成时，会优先尝试驱动所属 scheduler；如果自动泵关闭且任务仍 pending，会报运行时错误，要求调用方先显式 pump

## Current Limits

这一轮的 coroutine 路径已经能支撑单线程 `%async/%await` 与手动 pump，但还没有把 suspend/resume 降到独立 IR/opcode。当前仍是 builtin hidden helper 加 scheduler queue 的实现方式，目的是先把公开语义、默认调度器和上下文安全收实，再继续往 thread / transport / deeper VM lowering 推进。
