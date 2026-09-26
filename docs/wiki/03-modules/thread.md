---
related_code:
  - CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_wrappers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_wrappers.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
tests:
  - tests/thread/test_thread_runtime.c
doc_type: module-detail
---

# `zr.thread` 详细参考

**状态：`experimental`；Runtime provider，descriptor 版本 `1.0.0`，公开 contract
`zr.thread:v1:isolated-scheduler-send`。**

## 导出签名

| 类型/成员 | 签名 | 语义 |
| --- | --- | --- |
| `ThreadScheduler` | `init ThreadScheduler(workerCount: int)` | 创建线程调度器；默认采用 AttachedDomain，宿主可在构造前选择 IsolatedDomain |
| `ThreadScheduler.schedule` | `schedule<T>(job: zr.task.Job<T>): zr.task.Task<T>` | 消费 Job；`T: Send` |
| `Channel<T>` | `init Channel<T>()`；`send(value: T): null`；`recv(): T?`；`close(): null`；`isClosed(): bool`；`length(): int` | FIFO 包装器；空队列 `recv` 返回 `null`；关闭后不能再发送；`T: Send`，容器声明 Send/Sync |
| `Shared<T>` | `init Shared<T>(value: T)`；`load(): T?`；`store(value: T): null`；`clone(): Shared<T>`；`downgrade(): WeakShared<T>`；`release(): null`；`isAlive(): bool` | 强引用共享 cell；释放后 `load` 返回 `null`；`T: Send + Sync` |
| `WeakShared<T>` | `upgrade(): Shared<T>?`；`isAlive(): bool` | 弱句柄；目标死亡时 `upgrade` 返回 `null` |
| `Transfer<T>` | `init Transfer<T>(value: T)`；`take(): T`；`isTaken(): bool` | 一次性移动；`T: Send` |
| `UniqueMutex<T>` | `init UniqueMutex<T>(value: T)`；`lock(): Lock<T>` | 独占锁；`T: Send` |
| `SharedMutex<T>` | `init SharedMutex<T>(value: T)`；`read(): SharedLock<T>`；`write(): Lock<T>` | 读共享、写独占；`T: Send + Sync` |
| `Lock<T>` | `load(): T`；`store(value: T): null`；`unlock(): null` | affine 独占 guard，必须显式释放 |
| `SharedLock<T>` | `load(): T`；`unlock(): null` | affine 读 guard，必须显式释放 |

表中 `null` 表示 ZR 的 null 值类型；失败仍可能以 VM exception/Task fault 报告。guard
不能跨 `await`、线程或其所属 mutex 的生命周期。

线程 provider 的 descriptor 为了保持泛型类型信息，仍将 `recv`、`Shared.load` 和
`WeakShared.upgrade` 的 `returnTypeName` 记录为 `T` 或 `Shared<T>`；上表的 `?` 是运行时
空值语义的说明，不是另一套 overload。调用方必须先处理空值，再把结果当作有效对象使用。

TODO: `Channel` 构造器文案称同一 isolate FIFO，类型文案称跨 isolate FIFO，传输层也可编码其
native handle；当前测试未覆盖 Channel 跨域使用，需先统一并验证其可用范围。

## 能力约束

`Send` 和 `Sync` 是 descriptor protocol id，不是命名约定。primitive、递归 value-safe
array 和满足 provider layout 的 struct 才能自动实现；引用、借用、锁 guard、native pointer
默认拒绝。泛型约束在 canonical TypeId 上求解，错误包含具体参数位置。

## 两种执行策略

AttachedDomain worker 与调用方共享 `GcDomain`，但每个 worker 仍有独立 `SZrState` 和 call
stack；worker 在 safepoint 轮询后领取下一项。IsolatedDomain 为每个 worker 建独立 domain，
通过 capture/result envelope 做结构化传输，不能直接共享普通 heap object。quota 超限、
关闭后提交、被关闭的排队任务和 forbidden payload 经 Task fault 路径结算；已启动的任务
仍由后续 completion 消息结算。

`workerCount` 从脚本 `int64` 读入，目前只校验正数，随后窄化成 `uint32` 作为 worker 上限。
BUG: `4294967297` 会被当作 1；调用方应将参数限制在 `1..4294967295`，实现仍须补充上界校验。

## 宿主控制

宿主可在创建 scheduler 前设置 execution policy 和 isolated transfer quota。每个 scheduler
在构造时快照配置；之后改动只影响新实例。`ShutdownIsolatedSchedulers` 阻止新的 isolated
提交并 fault 尚未启动的排队任务，不等待已启动 worker 退出；它们仍需 caller domain 处理
completion。TODO: 若宿主需要安全销毁门禁，须另定等待 worker 退出的接口。
BUG: `Task.result()` 返回只能证明任务到达终态，不能证明 worker 已完成清理。
AttachedDomain worker 在发布 Task 终态后仍要释放 work item、退出 mutator 并释放 state；
IsolatedDomain worker 收到 caller completion acknowledgment 后才释放其 global。
宿主不能仅凭上述返回值销毁 caller global，否则 worker 可能使用已释放的状态或分配器上下文。

## C 调用接口

自建宿主可在导入 `zr.thread` 前注册 descriptor；链接线程模块的 CLI 已在标准注册链中调用
`ZrVmThread_Register`。

```c
const ZrLibModuleDescriptor *descriptor = ZrVmThread_GetModuleDescriptor();
if (!ZrVmThread_Register(global)) {
    return ZR_FALSE;
}
if (!ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
            global, ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN) ||
    !ZrVmThread_Runtime_SetIsolatedTransferQuota(
            global, 10000U, 64U * 1024U * 1024U, 32U)) {
    return ZR_FALSE;
}
(void)descriptor; /* 借用静态 descriptor；宿主不释放。 */
```

| C 符号 | 约束 |
| --- | --- |
| `ZrVmThread_GetModuleDescriptor` / `ZrVmThread_Runtime_GetModuleDescriptor` | 返回静态 descriptor；不得释放 |
| `ZrVmThread_Register(global)` | 需在 scheduler 创建前调用；返回 `TZrBool` |
| `ZrVmThread_Runtime_SetSchedulerExecutionPolicy` | 每个新 scheduler 快照当前策略；构造后修改不追溯 |
| `ZrVmThread_Runtime_SetIsolatedTransferQuota` | 设置之后新建 scheduler 的 object/byte/depth 上限；任一值为 0 时返回 false |
| `ZrVmThread_Runtime_ShutdownIsolatedSchedulers` | 拒收新的 isolated 提交并 fault 排队任务；不等待 live worker，不是 global 销毁屏障 |
| `ZrVmThread_Runtime_GetLastSchedulerWorkerDomain` | 将最近 worker 的 `SZrGcDomainIdentity` 写入调用方提供的 out 参数 |

BUG: `SetSchedulerExecutionPolicy` 与 `SetIsolatedTransferQuota` 通过无成功反馈的字段写入
保存配置；内存分配或 pin 失败时仍可能返回 true。后续 scheduler 可能保留旧策略，或读到
新旧混合的 quota；宿主不能仅以返回 true 断言配置已生效。

共享库构建时入口 `ZrVm_GetNativeModule_v1()` 返回同一 descriptor。所有 worker 的
`SZrState` 和 domain 都由 provider 管理，宿主不得直接 free。当前 API 没有把
`ShutdownIsolatedSchedulers` 变成 worker join，宿主需保持 caller global 有效直到 worker
清理确已完成；现有公开接口未给出这一同步保证。

BUG: 两个 `Shared` strong handle 并发执行最后两次 `release()` 时，先减到 1 的线程可能
在另一个线程销毁 cell/mutex 后二次加锁，产生 use-after-free。普通 GC 丢弃 `Shared`
包装对象也不会自动减少 native strong count；当前生命周期依赖显式 `release()`。
