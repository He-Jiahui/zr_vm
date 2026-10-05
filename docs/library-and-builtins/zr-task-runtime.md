---
related_code:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - tests/task/test_task_runtime.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
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
- `Job<T>` is a non-Copy struct value that owns one cold callable and is
  consumed by scheduling. Source constructs it with `init Job<T>(...)`.
- `Scheduler` is the canonical scheduler capability. Its only public operation
  is `schedule(Job<T>): Task<T>`.

The module provides `currentScheduler` as the local provider. `yieldNow()` and
`delay(...)` retain their Task-returning ABI. A completed task is observed with
`Task.result()` or by direct `await` inside an `async` callable.

```zr
let task = import("zr.task");
var job = init task.Job<int>(fn() => { return 17; });
var completion = task.currentScheduler.schedule<int>(job);
return completion.result();
```

`Job<T>` is move-only. Submission marks the source Job consumed before
allocating its completion handle and clears the callable on the subsequent
success and failure paths. These private writes currently use a void field
setter: pin or key-allocation failure can return without confirming the write.
The source ownership rule therefore does not establish reliable runtime
single-consumption under every allocation failure; checked field publication
and failure injection remain follow-up work.

## Source And Runtime Boundary

The source form is explicit:

```zr
async fn fetch(pending: zr.task.Task<int>): zr.task.Task<int> {
    return await pending;
}
```

`Task.result()` first observes completed or faulted state, then tries the
await hook registered on the owning scheduler. A handled successful hook is
followed by another terminal-state check. Only the local queue fallback is
suppressed while that scheduler is pumping; the provider hook is tried first. The queue, pump state, and
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

## 当前同步调用与失败边界

注册入口由 CLI/LSP、测试状态创建和线程 worker 初始化调用；native descriptor
经注册表和 native dispatch 调用 Job 构造、schedule、Task.result、yieldNow 与 delay。
guest Task 对象保存状态/结果，WorkItem 使用域根保活；这些调用没有直接调用 core
TaskFrame 的 poll/resume/free API，不能据此授予 core frame 调度或释放信用。

`task_runtime_execute_task` 在同步 TryRun 内借用栈请求调用 callable。回调不得保存或
逃逸 request；外层在 TryRun 返回后读取 result/completed，再恢复栈锚点并结算 Task。
queue 创建请求同样在返回后读取 attached/queue。provider hook 在本地 pumping 检查前
获得机会：attached provider 可按自己的条件变量策略等待。无 hook 的本地 pending
Task 在 pumping 时不递归推进队列；这不是所有 provider 同步等待的禁令。

两个现存 void setter 缺陷有静态合法链：global.c 的 ExecutionBudgetAllocate 安装
进入 execution_memory.c 的 upstream 分配，upstream 返回 null 时 wrapper 普通返回。
GC Ignore 登记满容量后的 ARRAY 扩容可返回失败；native field setter 的 pin guard
随即普通返回，未写字段，而 Job 消费或 await-hook 注册上层仍可能报告成功。
该链不把已 ignored 对象或仍有空位的路径当作触发证据，也没有本次运行或故障注入。

TODO：delay 的 descriptor 参数仍为 Duration，而 callback 使用 ReadInt 并将非负数
解释为 cooperative turns；需在 native 参数校验、Duration provider 与整数转换入口
核查契约。TODO：callback 内部 moving GC 的 callable/raw handle 生命周期需沿
CallValue 的 VM 根和返回后的 local-root 清理核查；清理阶段根测试不能证明整个回调窗口。

本次整合仅注释、台账和文档；没有新增 native/build/link/runtime/GC/CTest 运行证据。

## 本次静态证据锚点

实际同步调用与请求返回后读取：`zr_vm_library/src/zr_vm_library/task_runtime.c:593`、`zr_vm_library/src/zr_vm_library/task_runtime.c:615`、`zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc:161`。provider 先接管：`zr_vm_library/src/zr_vm_library/task_runtime.c:852`；无 hook 的本地 pumping guard：`zr_vm_library/src/zr_vm_library/task_runtime.c:859`。

消费与域根创建：`zr_vm_library/src/zr_vm_library/task_runtime.c:913`、`zr_vm_library/src/zr_vm_library/task_runtime.c:923`；释放根：`zr_vm_library/src/zr_vm_library/task_runtime.c:972`；结算 guard：`zr_vm_library/src/zr_vm_library/task_runtime.c:1021`。真实共享 helper include：`tests/task/task_scheduler_queue_reuse_cases.inc:11`。

allocator 安装与普通 null 返回：`zr_vm_core/src/zr_vm_core/global.c:374`、`zr_vm_core/src/zr_vm_core/execution/execution_memory.c:65`、`zr_vm_core/src/zr_vm_core/execution/execution_memory.c:70`。pin 失败返回：`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385`、`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1386`。Duration 核查入口：`zr_vm_library/src/zr_vm_library/task_runtime.c:1219`。
