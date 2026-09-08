---
related_code:
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
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
| `ThreadScheduler` | `init ThreadScheduler(workerCount: int)` | 创建附着当前 `GcDomain` 的 worker 集合 |
| `ThreadScheduler.schedule` | `schedule<T>(job: zr.task.Job<T>): zr.task.Task<T>` | 消费 Job；`T: Send` |
| `Channel<T>` | `init Channel<T>()`；`send(value: T): null`；`recv(): T?`；`close(): null`；`isClosed(): bool`；`length(): int` | 同一 isolate FIFO；空队列 `recv` 返回 `null`；关闭后不能再发送；`T: Send + Sync` |
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

## 能力约束

`Send` 和 `Sync` 是 descriptor protocol id，不是命名约定。primitive、递归 value-safe
array 和满足 provider layout 的 struct 才能自动实现；引用、借用、锁 guard、native pointer
默认拒绝。泛型约束在 canonical TypeId 上求解，错误包含具体参数位置。

## 两种执行策略

AttachedDomain worker 与调用方共享 `GcDomain`，但每个 worker 仍有独立 `SZrState` 和 call
stack；worker 在 safepoint 轮询后领取下一项。IsolatedDomain 为每个 worker 建独立 domain，
通过 capture/result envelope 做结构化传输，不能直接共享普通 heap object。quota、shutdown、
forbidden payload 都以 Task fault 结算。

## 宿主控制

宿主可在创建 scheduler 前设置 execution policy 和 isolated transfer quota。所有配置必须
在提交前完成；提交后变更只影响后续 request。`ShutdownIsolatedSchedulers` 会停止新提交、
fault 队列并等待 worker acknowledgment，返回后才能销毁 global。

## C 调用接口

```c
const ZrLibModuleDescriptor *descriptor = ZrVmThread_GetModuleDescriptor();
if (!ZrVmThread_Register(global)) {
    /* inspect the native-registry error on global */
}
ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
        global, ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN);
ZrVmThread_Runtime_SetIsolatedTransferQuota(global, 10000U, 64U * 1024U * 1024U, 32U);
ZrVmThread_Runtime_ShutdownIsolatedSchedulers(global);
```

| C 符号 | 约束 |
| --- | --- |
| `ZrVmThread_GetModuleDescriptor` / `ZrVmThread_Runtime_GetModuleDescriptor` | 返回静态 descriptor；不得释放 |
| `ZrVmThread_Register(global)` | 需在 scheduler 创建前调用；返回 `TZrBool` |
| `ZrVmThread_Runtime_SetSchedulerExecutionPolicy` | 每个新 scheduler snapshot 当前策略；提交后修改不追溯 |
| `ZrVmThread_Runtime_SetIsolatedTransferQuota` | 设置 object/byte/depth 三个上限；非法或已启动状态返回 false |
| `ZrVmThread_Runtime_ShutdownIsolatedSchedulers` | 停止提交、fault 队列并等待 acknowledgment；返回后才可销毁 global |
| `ZrVmThread_Runtime_GetLastSchedulerWorkerDomain` | 将最近 worker 的 `SZrGcDomainIdentity` 写入调用方提供的 out 参数 |

共享库构建时入口 `ZrVm_GetNativeModule_v1()` 返回同一 descriptor。所有 worker 的
`SZrState` 和 domain 都由 provider 管理，宿主不得直接 free；先 shutdown，再释放 global。
