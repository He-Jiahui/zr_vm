---
related_code:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
tests:
  - tests/task/test_task_runtime.c
  - tests/task/test_task_job_scheduler.c
  - tests/thread/test_thread_runtime.c
doc_type: module-detail
---

# `zr.task` 与 `zr.thread`

两个模块共享一套 Task/Job/Scheduler 协议。`zr.task` 提供本地协作调度，`zr.thread` 提供
同一 `schedule(Job<T>)` contract 的 worker/isolate provider；后者不再复制另一套 Task 类型。

## Task、Job、Scheduler

```zr
let task = import("zr.task");
var job = init task.Job<int>(fn() => { return 17; });
let done = task.currentScheduler.schedule<int>(job);
let answer = await done;
```

`Job<T>` 是 move-only 的 cold callable：构造不执行、不排队；第一次 schedule 消费 callable，
即使队列分配失败也不会把 Job 恢复为可执行状态。`Task<T>` 是完成句柄，`result()` 或
`await` 取得值/异常。`yieldNow()`、`delay(...)` 返回 Task；调度器 pump 和队列是 runtime
私有实现，不暴露脚本控制。

async 函数必须显式返回 `zr.task.Task<T>`。`await` 是语义 suspension boundary，borrow/loan
和 affine guard 不能跨越；Task 表达式必须被 await、return 或存储，不能静默丢弃。

## ThreadScheduler

```zr
let th = import("zr.thread");
let scheduler = new th.ThreadScheduler(2);
let result = await scheduler.schedule<int>(
    init task.Job<int>(fn() => { return 42; }));
```

`Send` 表示值可移动到 worker，`Sync` 表示可共享；borrowed、loaned、Shared/Weak shell
不会仅因内部是 primitive 就自动获得能力。`ThreadScheduler` 返回调用方 domain 的
Task。AttachedDomain worker 通过 `ZrCore_State_MutatorLaunch` 接入同一 `GcDomain`；
IsolatedDomain 则序列化 capture/result，通过独立 domain envelope 交付。

## Transport、锁与关闭

`Channel<T: Send + Sync>` 提供 `send/recv/close/isClosed/length`；`Transfer<T>` 是一次消费的
`take/isTaken`；`Shared<T>`/`WeakShared<T>` 提供 `load/store/clone/downgrade/upgrade`。
`UniqueMutex<T>`/`SharedMutex<T>` 产生 `Lock<T>`/`SharedLock<T>` guard，guard 是 affine，
不能跨 await，也不实现 Send/Sync。

worker 启动、队列发布和完成确认有明确 mutex release/acquire 边界。关闭时，后续提交立即
失败，已排队 Job 被 fault；完成 envelope 的 worker slot 在发布确认前释放，避免容量泄漏。

## C 接口与配置入口

```c
const ZrLibModuleDescriptor *d = ZrVmThread_GetModuleDescriptor();
ZrVmThread_Register(global);
ZrVmThread_Runtime_SetSchedulerExecutionPolicy(global, policy);
ZrVmThread_Runtime_SetIsolatedTransferQuota(global, maxObjects, maxBytes, maxDepth);
ZrVmThread_Runtime_ShutdownIsolatedSchedulers(global);
```

`ZrVmThread_Runtime_GetLastSchedulerWorkerDomain` 可读取最近 worker 的 domain identity。
`zr_vm_lib_task` 目录保留旧实验实现，但不在顶层产品图中；生产文档和 provider inventory
只认可 `zr.task` canonical contract 与 `zr.thread` provider。
