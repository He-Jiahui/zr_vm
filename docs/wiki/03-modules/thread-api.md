---
related_code:
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_task/include/zr_vm_lib_task/module.h
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/descriptor.c
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/thread/test_thread_module.c
  - tests/thread/test_thread_runtime.c
  - tests/task/test_task_runtime.c
doc_type: api-reference
---

# `zr.thread` API 参考

`zr.thread` 在 `zr.task` 之上增加 worker scheduler、Send/Sync 协议以及锁对象。线程模块
不把任意 ZR object 直接暴露给另一个执行域：值要么在 attached domain 中由明确的同步协议
访问，要么经过 isolated transfer encoder 复制/转移。项目 manifest 必须打开 multithread
支持，宿主还可以在创建 scheduler 前选择执行策略。

## marker protocol

| 类型 | 语义 | 典型实现 |
| --- | --- | --- |
| `Send` | 所有权可以移动到另一个 worker/domain，原 place 不能再使用。 | `Transfer<T>`、满足约束的 `Channel<T>`。 |
| `Sync` | 多个执行者可以通过共享协议访问；具体原子性由对象方法保证。 | `Shared<T>`、`Channel<T>`、mutex。 |

实现 Send/Sync 不是给 class 加一个无条件标记。compiler 会递归检查字段 layout、native
owner、borrow 和 protocol relation；带活动 `Ptr`、FileStream、Lock 或 callback 的对象默认
不能传输。

## ThreadScheduler

```zr
let scheduler = init zr.thread.ThreadScheduler(4);
let task = scheduler.schedule(init zr.task.Job<int>(() => 40 + 2));
let answer = await task;
```

`ThreadScheduler(workerCount: int)` 是初始化器；`workerCount` 必须为正且受宿主线程上限
约束。`schedule(job: zr.task.Job<T>): zr.task.Task<T>` 返回异步任务。scheduler 同时满足
task scheduler 和 thread scheduler protocol；worker 完成后在 owner scheduler 中合并结果。
没有 multithread capability 时构造或 schedule 会抛 runtime error，而不会静默退回主线程。

## Channel、Shared 和 Transfer

| 类型 | 方法 | 生命周期/并发规则 |
| --- | --- | --- |
| `Channel<T>` | 构造、`send(T): null`、`recv(): T`、`close(): null`、`isClosed(): bool`、`length(): int` | 发送端 close 后不能再 send；空队列 recv 返回 null。实现 Send/Sync（T 需满足对应约束）。 |
| `Shared<T>` | `load`、`store`、`clone`、`downgrade`、`release`、`isAlive` | 强引用计数；最后一次 release 执行 drop。不是 atomic transaction。 |
| `WeakShared<T>` | `upgrade`、`isAlive` | 不延长生命周期，upgrade 可能返回 null。 |
| `Transfer<T>` | `take`、`isTaken` | 一次性所有权转移；take 后 source 不可读。 |

```zr
let channel = init zr.thread.Channel<string>();
channel.send("message");
let item = channel.recv();
if (channel.isClosed() && item == null) {
    // 生产者已关闭且队列为空
}
```

`length` 是观察值，不保证与下一次 recv 原子一致。跨域传输会检查最大对象数、字节数和
嵌套深度；超限时 source 和 destination 都保持可恢复状态。

## Mutex、SharedMutex 和 lock view

| 类型 | 成员 | view 规则 |
| --- | --- | --- |
| `UniqueMutex<T>` | 构造、`lock(): Lock<T>` | 独占写锁；实现 Send/Sync。 |
| `SharedMutex<T>` | 构造、`read(): SharedLock<T>`、`write(): Lock<T>` | 多读单写。 |
| `Lock<T>` | `load`、`store`、`unlock` | affine、可写、不能跨 await/thread。 |
| `SharedLock<T>` | `load`、`unlock` | affine、只读、不能跨 await/thread。 |

```zr
let mutex = init zr.thread.UniqueMutex<int>(0);
using (let guard = mutex.lock()) {
    guard.store(guard.load() + 1);
}
```

lock view 是 scoped resource；离开 using 前必须 unlock。compiler 会在 `await` 前拒绝仍活动
的 guard，避免恢复后在线程错误的 domain 使用锁。运行时也会检查 owner/generation，stale
lock 访问抛 runtime error。

## 原子值

`zr.task.AtomicBool`、`AtomicInt<T>`、`AtomicUInt<T>` 由 task provider 注册，thread provider
使用相同 memory-safe contract：

| 操作 | 结果 |
| --- | --- |
| `load()` | 读取当前值。 |
| `store(value)` | 写入值。 |
| `compareExchange(expected, desired)` | 比较并交换，失败时写回 observed 值。 |
| `fetchAdd(delta)` / `fetchSub(delta)` | 整数原子加/减并返回旧值。 |

内存序由宿主实现选定，脚本层不暴露裸 C `memory_order`；需要复合不变量时使用 mutex 或
channel，不能把多次 load/store 假定成一个事务。

## Attached 与 Isolated domain

宿主在创建 scheduler 前设置策略：

```c
ZrVmThread_Runtime_SetSchedulerExecutionPolicy(
    global, ZR_VM_THREAD_SCHEDULER_EXECUTION_POLICY_ISOLATED_DOMAIN);
ZrVmThread_Runtime_SetIsolatedTransferQuota(global, 10000u, 16u * 1024u * 1024u, 32u);
```

| 策略 | 特点 | 可传对象 |
| --- | --- | --- |
| `ATTACHED_DOMAIN` | worker 共享 global/GC domain，由锁和协议保护。 | 满足 Send/Sync 且未持有 thread-affine handle 的值。 |
| `ISOLATED_DOMAIN` | worker 有独立 GC domain，通过 transfer message 合并结果。 | 可序列化 primitive/struct/array；native pointer、open stream、活动 loan 被拒绝。 |

每个 scheduler 在创建时快照 policy；之后修改 global 不会改变已存在 scheduler。quota 参数
是 maxObjects、maxBytes、maxDepth，0 表示 provider 的默认上限而不是无限。关闭前调用
`ZrVmThread_Runtime_ShutdownIsolatedSchedulers`，再销毁 global。

## C runtime API

| 函数 | 作用 |
| --- | --- |
| `ZrVmThread_Runtime_SetSchedulerExecutionPolicy(global, policy)` | 选择 attached/isolated。 |
| `ZrVmThread_Runtime_SetIsolatedTransferQuota(global, maxObjects, maxBytes, maxDepth)` | 设置 transfer 预算。 |
| `ZrVmThread_Runtime_ShutdownIsolatedSchedulers(global)` | 请求所有 isolated worker 停止并排空消息。 |
| `ZrVmThread_Runtime_GetLastSchedulerWorkerDomain(global, outDomain)` | 读取最近 worker 的 domain identity。 |
| `ZrVmThread_GetModuleDescriptor` / `ZrVmThread_Register` | 查询并注册脚本 descriptor。 |

这些 API 返回 `TZrBool`；失败时读取 global 的 registry/exception 信息。`outDomain` 是值
快照，调用方不拥有内部 domain 指针。

## 传输失败和诊断

- project `supportMultithread=false`：构造/schedule 直接失败；
- T 不满足 Send/Sync：generic constraint error；
- quota 超限：transfer quota error，原任务保持 faulted/pending 明确状态；
- stale generation/closed lock：runtime error；
- 在 await 或线程边界持有 `ref`、`scoped`、Ptr 或 callback：ownership/loan error。

任务结果合并和 C bridge 见[任务 API](task-api.md)；所有权基础规则见[类型、布局与所有权](../02-language/types-ownership.md)。
