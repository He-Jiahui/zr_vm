---
related_code:
  - CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_lib_task/include/zr_vm_lib_task/module.h
  - zr_vm_lib_task/include/zr_vm_lib_task/runtime.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_cli/src/zr_vm_cli/project/project.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/plans/syntax/12-async-task-job-scheduler/m1-explicit-task-syntax-effect.md
  - docs/plans/syntax/12-async-task-job-scheduler/m2-task-frame-runtime.md
  - docs/plans/syntax/12-async-task-job-scheduler/m3-job-scheduler-runtime.md
tests:
  - tests/task/test_task_runtime.c
  - tests/task/test_task_job_scheduler.c
  - tests/thread/test_thread_runtime.c
doc_type: api-reference
---

# `zr.task` 调度器与 Task Runtime 参考

`zr.task` 的 source API、canonical `Task<T>/Job<T>/Scheduler` contract 与 Library C bridge
构成同一条任务链，但它们的 owner 责任不同：脚本创建/等待任务；provider 调度 job；Library
bridge 保证 task/callable 在异步交接期间被 GC root 持有；Core/Thread provider 决定实际执行域。
本页将它们按状态机和调用顺序展开。

当前 CLI 通过 `ZrCore_TaskRuntime_RegisterBuiltins` 注册 `zr.task` v3 descriptor。
`zr_vm_lib_task` 中的旧 descriptor 未进入顶层 CMake 构建，以下公开调用均以当前
`zr_vm_library/src/zr_vm_library/task_runtime.c` 的注册表为准。

语言级 `async` / `await` grammar 请先阅读[可调用对象、调用与异步迭代](../02-language/callable-async-iterator-reference.md)；
本页以运行时 API 和 C provider 实现为中心。

## 概念与所有权

| 对象 | 谁创建 | 谁拥有/何时失效 | 关键不变量 |
| --- | --- | --- | --- |
| `Job<T>` | 脚本或 provider | schedule 消费其 callable | 一个 Job 只能成功进入 prepare/schedule 一次。 |
| `Task<T>` | scheduler/Library bridge | caller-domain task handle | 完成前必须有可达 owner/root；完成后保存 result 或 fault。 |
| `Scheduler` | `zr.task`/线程 provider | state/global 的运行时对象 | queue、head、pumping flag 是私有字段。 |
| `WorkItem` | `PrepareJob` | provider 在执行/结算后或队列接管 Task 后 Release | 包含 GC root，必须恰好 release 一次。 |
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
delay(duration: Duration)    Task<void>  // descriptor 签名；实际 callback 读非负整数 turn
```

`delay` 的 descriptor 参数写为 `Duration`，当前回调却读取非负整数并按 scheduler turn
计数，不提供墙上时钟等待。TODO: 需统一参数契约与当前实现，并加入对应回归验证。

当前模块 materialize 时公开 `currentScheduler` 属性。`Async`、`spawn` 和脚本可调用的
`pump`/`step` 均不在当前 descriptor 中；不要用旧 task 模块的成员编写当前脚本。

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
    | FaultPreparedJob or callable failure      | callable returns normally /
    |                                           | CompletePreparedJob
   +-------------------------------------------+
                                               |
                                               v
                                           COMPLETED
```

状态机细节：

- `PrepareJob` 将 job 标为 consumed，取出 callable，建立 caller-domain task，并创建 GC root
  handle；任务不能在 root 创建之前交给异步 provider。
- `ExecutePreparedJob` 从 root resolve task，在当前合法 state/domain 执行 callable，并自行写入
  completed 或 faulted；其后不得再对同一 task 调用 `CompletePreparedJob`。
- `CompletePreparedJob` 只接受尚未完成的 task，将 result 写入并发布 completed；重复 complete
  是失败而非覆盖旧结果。
- `FaultPreparedJob` 将失败信息写入 task fault state；它不应伪造一个成功 `null` result。
- `ReleasePreparedJob` 释放 root 并清零 work item；执行/结算后或队列已接管 Task 可达性后，
  都由 work item 持有者调用一次 release。

`Task.result()`/`await` 先检查 completed/fault。仍 pending 时，runtime 尝试 provider await
hook；只有未注册有效 hook 时，才在未处于 pumping 的 scheduler 上推进 step。已注册的 hook
返回失败会直接中止本次等待，返回成功则重查 task 状态。若队列不能让
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
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_library/task_runtime.h"

if (!ZrCore_TaskRuntime_RegisterBuiltins(global)) {
    return ZR_FALSE;
}
```

`RegisterBuiltins` 附着 native registry，并注册当前内建 `zr.task` descriptor。
CLI 的标准模块入口已调用此函数；同一个 global 上不要重复注册另一份 `zr.task` descriptor。
若自己的 scheduler/provider 依赖 `zr.task`，应先让 registry/phase admission 成功，再提交 job。

### 最小安全 work item 模板

```c
ZrLibraryTaskRuntimeWorkItem item = {0};
SZrTypeValue taskValue;
if (!ZrLibrary_TaskRuntime_PrepareJob(
        state, schedulerObject, jobObject, &taskValue, &item)) {
    return ZR_FALSE;
}

/* 同域调用执行 callable，并由 Execute 写入 completed 或 faulted。 */
TZrBool executed = ZrLibrary_TaskRuntime_ExecutePreparedJob(state, &item);
ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &item);
if (!executed) return ZR_FALSE;
```

真实 provider 可在 `PrepareJob` 后将工作移交队列。若由外部执行者计算结果，应经
`CopyPreparedCallable` 获取 callable，并在 caller domain 以明确初始化的结果调用
`CompletePreparedJob`，失败则调用 `FaultPreparedJob`。`ExecutePreparedJob` 已自行结算任务，
不能再对同一任务调用 `CompletePreparedJob`。准备成功后，所有退出路径都必须
`ReleasePreparedJob`；不要复制 `item.taskRoot`。跨 GC domain 的 isolated worker 不得直接
释放 caller root；attached worker 在 caller domain 的合法 state 下可按 API 释放。

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

hook 接收 state、task 和 provider context。注册后它负责自己拥有的 pending task；若无法
推进，应返回失败，让本次等待终止，而不要在 hook 中长期阻塞或递归调用 `await`。
`AwaitProviderTask` 在调用有效 hook 前就设置 `outHandled=true`；仅未注册有效 hook 时返回
`outHandled=false` 并允许默认 step。registration 生命周期必须覆盖 scheduler 使用期。

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
| completion 后崩溃 | work item root 已提前 release 或跨域使用 | 释放前确认 Task 已由队列或其他 owner 保持可达；外部 completion 路径在结算后 release。 |
| `await` 编译失败 | async signature、Task operand、borrow/parameter contract | 修正 source effect/ownership，不修改 scheduler 私有字段。 |
| isolated result 损坏 | transport/quotas/owner domain | 在 caller domain 完整 decode 后 complete，否则 fault。 |
| hook 递归或卡死 | hook 重入默认 await/pump | 将 hook 设计为一次 provider-specific state transition。 |

`zr.thread` 的 worker/domain 配置、transfer quota 和 shutdown 顺序另见[线程 API](thread-api.md)；
Iterator 的 suspension/cleanup 规则见[迭代 API](iteration-api.md)。
