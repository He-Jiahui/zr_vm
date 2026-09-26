---
related_code:
  - CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_lib_task/include/zr_vm_lib_task/module.h
  - zr_vm_lib_task/src/zr_vm_lib_task/module.c
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/plans/syntax/README.md
tests:
  - tests/task/test_task_runtime.c
  - tests/task/test_task_job_scheduler.c
doc_type: api-reference
---

# `zr.task` API 参考

当前顶层构建和 CLI 注册的是 `ZrCore_TaskRuntime_RegisterBuiltins(global)` 提供的
`zr.task` v3 descriptor，公开 `Task<T>`、`Job<T>`、`Scheduler`、`currentScheduler`、
`yieldNow` 与 `delay`。仓库还保留 `zr_vm_lib_task` 旧版实现，但顶层 CMake 未纳入构建，
CLI 也不注册它；其 `Async/spawn/...` 名称不能当作当前 `zr.task` 的公开 API。

## 语言侧 async

```zr
let task = import("zr.task");
var job = init task.Job<int>(fn() => { return 17; });
var completion = task.currentScheduler.schedule<int>(job);
return completion.result();
```

这是 `tests/task/test_task_runtime.c` 的当前执行路径。`async fn` 仍须显式返回
`Task<T>`；`await` 只可用于允许挂起的 frame，不能使活动的 `ref`/`scoped` loan 或
thread-affine lock 越过挂起点。任务 fault 在 `result()`/`await` 边界传播，
`isCompleted()` 为真也可能表示 fault。

## Runtime descriptor 函数

| 函数 | 签名 | 说明 |
| --- | --- | --- |
| `yieldNow` | `(): Task<void>` | 创建经过一次 cooperative turn 的任务。 |
| `delay` | `(duration: Duration): Task<void>` | descriptor 的时长契约；当前回调行为见下文。 |

`currentScheduler` 在类型提示中标为只读 `Scheduler` 属性，并由模块 materialize 导出；使用
`task.currentScheduler.schedule(job)` 调度。它不是零参数函数。

TODO: `delay` descriptor 声明 `Duration`，当前回调却用 `ReadInt` 读取非负整数并把它
作为 cooperative turns 计数，没有读取时钟。需统一 duration contract 与实现并补测试；
当前行为不能解释为毫秒定时器。

## Task 和 Scheduler

| 类型 | 成员 |
| --- | --- |
| `Task<T>` | `result(): T`、`isCompleted(): bool` |
| `Scheduler` | `schedule<T>(job: Job<T>): Task<T>` |

内部 scheduler 使用 queue/head/pumping 字段推进任务；这些字段和 `pump/step` 不向脚本
公开。`result()` 对未完成任务先尝试 provider await hook，再按当前 scheduler 的状态推进
内部 step；无法到达终态时报告 pending 错误。`isCompleted()` 同时涵盖 completed 与 faulted。

## canonical Task/Job/Scheduler

canonical provider 的公开 contract 为：

```text
Task<T>      result(): T; isCompleted(): bool
Job<T>       cold value; meta constructor from fn() -> T or fn() -> zr.task.Task<T>
Scheduler    schedule(job: zr.task.Job<T>): zr.task.Task<T>
yieldNow()   zr.task.Task<void>
delay(d)     zr.task.Task<void>
```

`Job` 是 cold 工作描述，构造不会自动执行；`schedule` 负责把它放入队列并返回 Task。
构造器签名允许 callable 返回 `T` 或 `Task<T>`；当前 native 执行路径直接保存 callable 的
返回值。TODO: 嵌套 Task 是否由编译期或其它调用层展开，需沿生成路径核查。每个 Job 只能消费
一次，提交过程中发生失败也不能假定可以再次调度原 Job。

## Channel、Shared、Transfer

当前 `zr.task` descriptor 不注册这些类型。当前跨线程容器与所有权包装由 `zr.thread`
注册，类型、Send/Sync 限制和调用方式见[线程 API](thread-api.md)。

## Mutex 和 atomic

当前 `zr.task` 不导出旧版 `Mutex` 或 `AtomicBool/AtomicInt/AtomicUInt`。`zr.thread`
当前锁 API 使用 `UniqueMutex/SharedMutex` 与 `Lock/SharedLock`；旧 task 实现的成员表
不能用于当前导入模块的调用。

## 调度和传输机制

默认 scheduler 保存任务队列与私有 pumping 状态；Task 保存 callable、result、error 和
scheduler owner。`zr.thread` 的 ThreadScheduler 通过 Library work item bridge 使用同一
Task 完成协议，worker/domain 的传输配额和关闭顺序由线程 provider 管理，详见
[调度器与 Task Runtime 参考](task-scheduler-runtime-reference.md)。

## C bridge

```c
if (!ZrCore_TaskRuntime_RegisterBuiltins(global)) {
    return ZR_FALSE;
}

/* provider 在同域执行已准备的 Job：Execute 自行结算 Task。 */
ZrLibraryTaskRuntimeWorkItem item = {0};
SZrTypeValue taskValue;
if (!ZrLibrary_TaskRuntime_PrepareJob(state, scheduler, job, &taskValue, &item))
    return ZR_FALSE;
TZrBool executed = ZrLibrary_TaskRuntime_ExecutePreparedJob(state, &item);
ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &item);
if (!executed) return ZR_FALSE;
```

`PrepareJob` 在失败时可已消费 Job；成功后 work item 持有 caller-domain Task 的 GC root。
同域 `ExecutePreparedJob` 会自行写入 completed 或 faulted，不再调用
`CompletePreparedJob`。外部 provider 若自行执行 callable，则在 caller domain 使用
`CompletePreparedJob` 或 `FaultPreparedJob` 结算。所有准备成功的分支都调用一次
`ReleasePreparedJob`。

## 调试和失败

| 症状 | 可能原因 | 处理 |
| --- | --- | --- |
| `spawn`/`Async` 成员缺失 | 调用了未构建的旧 task surface | 改用当前 `Task/Job/Scheduler` 契约。 |
| await 后值失效 | ref/scoped loan 跨挂起 | 把值复制/转移到 frame-safe owner。 |
| task 永远 pending | provider await hook 或内部 scheduler 无法推进 | 检查 scheduler 的提交与完成路径。 |
| 结果标记 faulted | worker payload/异常 | 调用 `await` 读取原始 error。 |
| Channel recv 为 null | `zr.thread` 队列为空或已 close | 用线程模块的 `isClosed`/length 区分状态。 |
| isolated transfer rejected | quota、Send/Sync 或不可序列化 handle | 降低 payload 或改用 attached domain。 |

更多线程锁和 domain 规则见[线程 API](thread-api.md)，任务 frame 语义见[VM 运行时](../09-vm-runtime.md)。
