---
related_code:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/native_binding_registry_plugin.c
  - zr_vm_parser/src/zr_vm_parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/parser_types.c
  - zr_vm_cli/src/zr_vm_cli/project.c
  - tests/task/test_task_runtime.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/native_binding_registry_plugin.c
  - zr_vm_parser/src/zr_vm_parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/parser_types.c
  - zr_vm_cli/src/zr_vm_cli/project.c
plan_sources:
  - user: 2026-04-05 Task / Coroutine / Thread 并发模型重构计划
tests:
  - tests/task/test_task_runtime.c
doc_type: module-detail
---

# zr.task Built-in Runtime

## Scope

`zr.task` 不再由 `zr_vm_lib_task` 对外注册。当前实现把任务抽象与 `%async/%await` 的 builtin owner 收到 VM 内建注册路径，由 `ZrCore_TaskRuntime_RegisterBuiltins(...)` 把 `zr.task` 与 `zr.coroutine` 一起挂进 native registry。

这一轮已经落地的公开面是：

- `zr.task.IScheduler`
- `zr.task.TaskRunner<T>`
- `zr.task.Task<T>`
- `zr.task.defaultScheduler`

当前没有再公开旧接口：

- `zr.task.Async<T>`
- `zr.task.Scheduler`
- `zr.task.spawn(...)`
- `zr.task.spawnThread(...)`
- `zr.task.currentScheduler()`
- `zr.task.await(...)`
- `%mutex`
- `%atomic`

## `%async` / `%await` Lowering

`%async` 仍然先走 parser lowering，但目标已经改成 builtin `TaskRunner` 模型，而不是旧 `spawn()` 热启动模型。

源代码：

```zr
%async func f(): int {
    return 1;
}
```

当前会 lower 成：

- 返回类型从 `int` 包成 `TaskRunner<int>`
- 函数体返回 `zr.task.__createTaskRunner(...)`

也就是说，调用 `%async` 函数只会得到冷的 runner，不会立即排队执行。

`%await expr` 当前 lower 到 builtin hidden helper `zr.task.__awaitTask(expr)`。effect 检查也已经从旧 `zr.task.await(...)` helper 改到这个新的 hidden await call。

## Runtime Model

### TaskRunner

- `TaskRunner<T>` 保存冷启动 callable
- `TaskRunner.start()` 委托到 `zr.task.defaultScheduler`
- 同一个 runner 只能 `start()` 一次；重复启动直接抛运行时错误

### Task

- `Task<T>` 状态目前实现为：
  - `Created`
  - `Queued`
  - `Running`
  - `Suspended`
  - `Completed`
  - `Faulted`
- 当前调度器主路径实际会用到 `Created/Queued/Running/Completed/Faulted`
- `Task.result()` 与 `%await task` 共用同一条等待路径

### defaultScheduler

- `zr.task.defaultScheduler` 在模块 materialize 时初始化为当前 isolate 的 `zr.coroutine.coroutineScheduler`
- 它是模块根导出字段，脚本层可直接重绑
- `TaskRunner.start()` 每次都会重新读取这个字段，因此重绑会立即生效

## Context Safety

`compiler_task_effects.c` 继续保留 borrow-across-await 诊断，但 await 边界识别已经从旧公开 helper 改成 builtin hidden await helper。

当前保持的约束：

- `%await` 只能出现在 `%async` 上下文
- borrowed binding 不能跨 `await` 使用
- borrowed 值在 `await` 前使用、`await` 后不再读，仍然允许

## Module Materialization Hook

native binding descriptor 现在支持模块 materialize 回调。`zr.task` 与 `zr.coroutine` 利用这条路径把状态相关的根属性挂到每个 isolate 的模块对象上，而不是把 `defaultScheduler` / `coroutineScheduler` 硬编码成静态常量。

这条 hook 当前用于：

- `zr.task.defaultScheduler`
- `zr.coroutine.coroutineScheduler`

`zr.thread` 的 worker isolate 调度和跨线程 transport 另见 `zr-thread-runtime.md`。
