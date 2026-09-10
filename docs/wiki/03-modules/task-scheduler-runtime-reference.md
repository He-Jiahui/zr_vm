---
related_code:
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_lib_task/include/zr_vm_lib_task/module.h
  - zr_vm_lib_task/include/zr_vm_lib_task/runtime.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_lib_task/src/zr_vm_lib_task/module.c
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime.c
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime_workers.c
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime_transport.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/12-async-task-job-scheduler/m1-explicit-task-syntax-effect.md
  - docs/plans/syntax/12-async-task-job-scheduler/m2-task-frame-runtime.md
  - docs/plans/syntax/12-async-task-job-scheduler/m3-job-scheduler-runtime.md
tests:
  - tests/task/test_task_runtime.c
  - tests/task/test_task_job_scheduler.c
  - tests/library/test_task_runtime_bridge.c
  - tests/thread/test_thread_runtime.c
doc_type: api-reference
---

# `zr.task` 调度器与 Task Runtime 参考

`zr.task` 的 source API、canonical `Task<T>/Job<T>/Scheduler` contract 与 Library C bridge
构成同一条任务链，但它们的 owner 责任不同：脚本创建/等待任务；provider 调度 job；Library
bridge 保证 task/callable 在异步交接期间被 GC root 持有；Core/Thread provider 决定实际执行域。
本页将它们按状态机和调用顺序展开。

语言级 `async` / `await` grammar 请先阅读[可调用对象、调用与异步迭代](../02-language/callable-async-iterator-reference.md)；
本页以运行时 API 和 C provider 实现为中心。

## 概念与所有权

| 对象 | 谁创建 | 谁拥有/何时失效 | 关键不变量 |
| --- | --- | --- | --- |
| `Job<T>` | 脚本或 provider | schedule 消费其 callable | 一个 Job 只能成功进入 prepare/schedule 一次。 |
| `Task<T>` | scheduler/Library bridge | caller-domain task handle | 完成前必须有可达 owner/root；完成后保存 result 或 fault。 |
| `Scheduler` | `zr.task`/线程 provider | state/global 的运行时对象 | queue、head、pumping flag 是私有字段。 |
| `WorkItem` | `PrepareJob` | provider 直到 Execute/Fault/Complete 后 Release | 包含 GC root，必须恰好 release 一次。 |
| await hook | provider 注册 | scheduler registration 所有 | 不暴露脚本 pump 字段；hook 只处理所属 provider task。 |

`Job` 的“冷”特性意味着构造 job 不等于开始执行；`schedule` 才完成 callable 到 queue/task
的交接。即使 queue 分配或 provider 提交失败，也不能把已经被消费的 job 当作可重试的原对象。
创建新的 job 或基于明确的失败策略重试，才是安全的恢复方式。

## 脚本契约

```zr
let task = import("zr.task");

async fn fetchCount(): task.Task<int> {
    return 7;
}

async fn consume(): task.Task<int> {
    let value = await fetchCount();
    return value + 1;
}
```

canonical API 的可读模型：

```text
Task<T>       result(): T; isCompleted(): bool
Job<T>        cold callable: fn() -> T or fn() -> Task<T>
Scheduler     schedule(job: Job<T>): Task<T>
yieldNow()    Task<void>
delay(...)    Task<void>
```

实际 provider 还可能提供 `Async`、`spawn`、`currentScheduler`、`pump`/`step` 等兼容或
runtime descriptor surface；代码应以当前导入 module 的 descriptor/类型提示为准，不要把另一
层 provider 的成员假定为 canonical public contract。

### async effect 的前置条件

下列约束来自 task parser/semantic tests，而非纯风格建议：

1. `async fn` 必须显式返回 closed `zr.task.Task<T>`；裸 `int` return 不构成 async contract。
2. `await` 的 operand 必须是 `Task<T>`，且只允许出现在 async body 或 scheduler-managed
   顶层 coroutine。
3. async callable 不能用 `in`、`ref` 或 `out` 参数把 caller place 带入可挂起 frame。
4. `ref readonly`、scoped guard、未完成 out assignment、thread-affine lock 等不能跨 await。
5. Task fault 在 `await` / `result()` 边界传播；`isCompleted() == true` 不等于成功。

## Task 状态机

当前 runtime 使用创建、排队、完成、fault 等状态，并将 callable/result/error/scheduler owner
保存为 task 私有字段。可将可见行为理解为：

```text
                 PrepareJob / ScheduleJob
CREATED ----------------------------------> QUEUED
   |                                           |
   | queue/root creation failure               | ExecutePreparedJob / scheduler step
   v                                           v
FAULTED <------------------------------- RUNNING / completing
   ^                                           |
   | FaultPreparedJob or callable failure      | CompletePreparedJob
   +-------------------------------------------+
                                               |
                                               v
                                           COMPLETED
```

状态机细节：

- `PrepareJob` 将 job 标为 consumed，取出 callable，建立 caller-domain task，并创建 GC root
  handle；任务不能在 root 创建之前交给异步 provider。
- `ExecutePreparedJob` 从 root resolve task，再在当前合法 state/domain 执行其 callable。
- `CompletePreparedJob` 只接受尚未完成的 task，将 result 写入并发布 completed；重复 complete
  是失败而非覆盖旧结果。
- `FaultPreparedJob` 将失败信息写入 task fault state；它不应伪造一个成功 `null` result。
- `ReleasePreparedJob` 释放 root 并清零 work item；无论 execute/complete/fault 的哪个分支
  结束，调用者都必须走一次 release。

`Task.result()`/`await` 先检查 completed/fault。仍 pending 时，runtime 尝试 provider await
hook；若无 hook 或 hook 未处理，再在未处于 pumping 的 scheduler 上推进 step。若队列不能让
该 task 到达终态，则产生“pending on active scheduler frame”运行时错误，而不是无限递归 pump。

## 同域、worker 与 isolated provider

| 执行方式 | Task/result 所在地 | callable/capture 的要求 | 典型交接 |
| --- | --- | --- | --- |
| 当前 scheduler | caller state/domain | 正常 frame-safe callable | queue -> local step -> complete。 |
| attached worker | 同一 GC domain 的 mutator state | 受 mutator/domain 规则约束 | provider worker 执行，caller task 被 root。 |
| isolated worker | 独立 domain | 可序列化/可传输值，无活动 borrow/native handle | copy prepared callable -> transport envelope -> caller-domain completion。 |

跨 isolated domain 时，worker 不应持有 caller-domain task root，也不能直接写 caller object。
Library bridge 的 `CopyPreparedCallable` 和 `CompletePreparedJob` 正是将“读取 callable”和“在
caller domain 结算 task”拆开的接口。transport 解码、配额或执行失败应走 fault path，而不能
发布部分解码的 result。

## C Provider 调用顺序

### 注册模块

```c
#include "zr_vm_lib_task/module.h"
#include "zr_vm_library/task_runtime.h"

if (!ZrVmTask_Register(global)) {
    return ZR_FALSE;
}
```

`ZrVmTask_GetModuleDescriptor()` 返回 provider descriptor 的借用静态数据；不要 free，也不要
假设所有 task 实现都使用同一个 internal field layout。若自己的 scheduler/provider 依赖
`zr.task`，应先让 registry/phase admission 成功，再提交 job。

### 最小安全 work item 模板

```c
ZrLibraryTaskRuntimeWorkItem item = {0};
SZrTypeValue taskValue;
SZrTypeValue resultValue;
TZrBool prepared = ZR_FALSE;

prepared = ZrLibrary_TaskRuntime_PrepareJob(
        state, schedulerObject, jobObject, &taskValue, &item);
if (!prepared) {
    return ZR_FALSE;
}

if (ZrLibrary_TaskRuntime_ExecutePreparedJob(state, &item)) {
    /* For an external provider, produce resultValue in the caller state/domain. */
    if (!ZrLibrary_TaskRuntime_CompletePreparedJob(state, &item, &resultValue)) {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state, &item, "provider completion failed");
    }
} else {
    ZrLibrary_TaskRuntime_FaultPreparedJob(state, &item, "provider job execution failed");
}

ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &item);
```

真实 provider 通常在 `PrepareJob` 后将工作移交队列，随后在适当时机执行或从 completion queue
调用 complete；上例只显示 ownership 的骨架。关键是：准备成功后，所有退出路径都必须
`ReleasePreparedJob`；不要复制 `item.taskRoot`、不要在 worker 直接释放 caller root。

### Provider await hook

```c
ZrLibraryTaskRuntimeAwaitRegistration registration = {
    .awaitHook = provider_await_task,
    .context = providerContext,
};

if (!ZrLibrary_TaskRuntime_RegisterAwaitHook(state, schedulerObject, &registration)) {
    return ZR_FALSE;
}
```

hook 接收 state、task 和 provider context。它只能处理自己拥有的 pending task；若不能处理，
应按 API contract 让 runtime 继续默认流程，而不是在 hook 中长期阻塞或递归调用 `await`。
`AwaitProviderTask` 输出 `outHandled`，由 runtime 区分“无 hook/未处理”与“hook 处理后应重查
task 状态”。registration 生命周期必须覆盖 scheduler 使用期。

## GC、异常、取消与安全点

- Work item 的 `SZrGcRootHandle` 防止 task/callable 在异步 handoff 时被 GC 回收；root 只在
  同一 state/domain 的正式 API 下 resolve/release。
- callback/task 执行若触发 exception，runtime 保存 error 并将 task 转入 fault；宿主不要清空
  current exception 后继续标记 completed。
- 取消和 execution budget 由 Core execution policy 管理。provider 应在允许的安全点轮询/退出，
  并通过 fault/cancellation contract 结算 task，不能只丢弃 queue entry。
- blocking native work 不应占用持有 VM stack/GC critical section 的 callback；使用 provider
  queue、detached policy 或真正的 worker/domain handoff。

细节可参阅[核心运行时宿主 API](../05-interop/core-runtime-host-api.md)和[核心 GC 与异常 API](../05-interop/core-gc-exception-api.md)。

## 排错表

| 现象 | 首先检查 | 正确处理 |
| --- | --- | --- |
| Job 第二次 schedule 失败 | job consumed 标志 | 新建 Job，不复用已消费 callable。 |
| Task 一直 pending | scheduler/hook 是否能推进该 task | 安装/修复 provider completion；不要 busy-loop。 |
| result 读取抛错 | task 是否 FAULTED | 读取/报告原始 error，检查 worker/transport。 |
| completion 后崩溃 | work item root 已提前 release 或跨域使用 | 保持 root 到 complete/fault 后，再 release。 |
| `await` 编译失败 | async signature、Task operand、borrow/parameter contract | 修正 source effect/ownership，不修改 scheduler 私有字段。 |
| isolated result 损坏 | transport/quotas/owner domain | 在 caller domain 完整 decode 后 complete，否则 fault。 |
| hook 递归或卡死 | hook 重入默认 await/pump | 将 hook 设计为一次 provider-specific state transition。 |

`zr.thread` 的 worker/domain 配置、transfer quota 和 shutdown 顺序另见[线程 API](thread-api.md)；
Iterator 的 suspension/cleanup 规则见[迭代 API](iteration-api.md)。
