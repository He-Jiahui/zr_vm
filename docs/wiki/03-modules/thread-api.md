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
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_common/include/zr_vm_common/zr_contract_conf.h
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_wrappers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
  - docs/plans/syntax/README.md
tests:
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
let task = import("zr.task");
let thread = import("zr.thread");
var scheduler = new thread.ThreadScheduler(4);
var job = init task.Job<int>(fn() => { return 40 + 2; });
var completion = scheduler.schedule<int>(job);
var answer = completion.result();
```

`ThreadScheduler(workerCount: int)` 是初始化器；当前只检查 `workerCount > 0`，再把
`int64` 值窄化成 `uint32` 的 worker 上限。BUG: 超过 `UINT32_MAX` 的值可能回绕，例如
`4294967297` 变成 1；实现需要上界检查。`schedule(job: zr.task.Job<T>): zr.task.Task<T>`
返回 caller-domain Task。scheduler 同时声明 task scheduler 和 thread scheduler protocol。
没有 multithread capability 时 `schedule` 会报告 runtime error；构造器目前不检查该能力。
TODO: 对 AttachedDomain 的 `Task.result()` 条件等待及 mutex 条件等待补做并发 GC 验证：
当前 GC-aware native 回调没有在等待期间进入阻塞域或主动轮询 safepoint。

## Channel、Shared 和 Transfer

| 类型 | 方法 | 生命周期/并发规则 |
| --- | --- | --- |
| `Channel<T>` | 构造、`send(T): null`、`recv(): T`、`close(): null`、`isClosed(): bool`、`length(): int` | 发送端 close 后不能再 send；空队列 recv 返回 null。实现 Send/Sync（T 需满足对应约束）。 |
| `Shared<T>` | `load`、`store`、`clone`、`downgrade`、`release`、`isAlive` | native strong 引用计数；需显式 release，普通 GC 丢弃包装对象不扣计数。 |
| `WeakShared<T>` | `upgrade`、`isAlive` | 不延长生命周期，upgrade 可能返回 null。 |
| `Transfer<T>` | `take`、`isTaken` | 一次性所有权转移；take 后 source 不可读。 |

```zr
let thread = import("zr.thread");
var channel = new thread.Channel<string>();
channel.send("message");
let item = channel.recv();
if (channel.isClosed() && item == null) {
    // 生产者已关闭且队列为空
}
```

`Channel<T>` 的类型参数约束为 `T: Send`，容器 descriptor 声明 Send/Sync。`length` 是观察值，
不保证与下一次 recv 原子一致。跨域传输会检查最大对象数、字节数和嵌套深度；超限会使
相应 Job Task fault。

BUG: 两个 strong handle 并发执行最后两次 `release()` 时，先减到 1 的线程可在另一线程
销毁 cell/mutex 后再次加锁，导致 use-after-free。当前实现不能保证这类并发释放安全。
BUG: `Channel` 没有释放其 native FIFO、同步原语和未读消息的 drop 路径；丢弃脚本包装对象
不会回收这些 native 资源。TODO: `Channel.send` 先编码并标记 `Transfer` taken，再检查队列
是否已关闭；需确认发送失败时是否应恢复 source 的所有权。

## Mutex、SharedMutex 和 lock view

| 类型 | 成员 | view 规则 |
| --- | --- | --- |
| `UniqueMutex<T>` | 构造、`lock(): Lock<T>` | 独占写锁；实现 Send/Sync。 |
| `SharedMutex<T>` | 构造、`read(): SharedLock<T>`、`write(): Lock<T>` | 多读单写。 |
| `Lock<T>` | `load`、`store`、`unlock` | affine、可写、不能跨 await/thread。 |
| `SharedLock<T>` | `load`、`unlock` | affine、只读、不能跨 await/thread。 |

```zr
let thread = import("zr.thread");
var mutex = new thread.UniqueMutex<int>(0);
var guard = mutex.lock();
guard.store(guard.load() + 1);
guard.unlock();
```

lock view 是 scoped resource；离开使用范围前必须 unlock。compiler 会在 `await` 前拒绝仍活动
的 guard，避免恢复后在线程错误的 domain 使用锁。运行时也会检查 owner/generation，stale
lock 访问抛 runtime error。

## 原子值

当前已构建的 `zr.task` v3 与 `zr.thread` descriptor 都未注册
`AtomicBool`、`AtomicInt<T>`、`AtomicUInt<T>`。需要复合不变量时使用当前公开的 mutex 或
channel，不能把多次 `Shared.load/store` 假定成一个事务。

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

每个 scheduler 在创建时快照 policy 与 quota；之后修改 global 不会改变已存在 scheduler。
quota 参数是 maxObjects、maxBytes、maxDepth，三个值都必须非零。`ShutdownIsolatedSchedulers`
只拒收后续 isolated 提交并 fault 尚未启动的排队任务，不等待已启动 worker 清理完成。
TODO: 若要将停止接单用作安全销毁门禁，还需提供等待 worker 退出的接口。
BUG: `Task.result()` 返回早于 worker 清理完成；attached worker 仍可能在释放 state，
isolated worker 仍可能在释放其 global。宿主不能据此立即销毁 caller global。

## C runtime API

| 函数 | 作用 |
| --- | --- |
| `ZrVmThread_Runtime_SetSchedulerExecutionPolicy(global, policy)` | 选择 attached/isolated。 |
| `ZrVmThread_Runtime_SetIsolatedTransferQuota(global, maxObjects, maxBytes, maxDepth)` | 设置 transfer 预算。 |
| `ZrVmThread_Runtime_ShutdownIsolatedSchedulers(global)` | 阻止后续 isolated 提交并 fault 排队任务；不等待 live worker。 |
| `ZrVmThread_Runtime_GetLastSchedulerWorkerDomain(global, outDomain)` | 读取最近 worker 的 domain identity。 |
| `ZrVmThread_GetModuleDescriptor` / `ZrVmThread_Register` | 查询并注册脚本 descriptor。 |

BUG: `SetSchedulerExecutionPolicy` 与 `SetIsolatedTransferQuota` 的底层字段写入没有成功反馈；
内存分配或 pin 失败时仍可能返回 true，使后续 scheduler 沿用旧策略或混用新旧 quota。

除 `GetModuleDescriptor` 返回借用 descriptor 指针外，表中 API 返回 `TZrBool`。
`outDomain` 是值快照，调用方不拥有内部 domain 指针。

## 传输失败和诊断

- project `supportMultithread=false`：构造/schedule 直接失败；
- T 不满足 Send/Sync：generic constraint error；
- quota 超限：transfer quota error，原任务保持 faulted/pending 明确状态；
- stale generation/closed lock：runtime error；
- 在 await 或线程边界持有 `ref`、`scoped`、Ptr 或 callback：ownership/loan error。

任务结果合并和 C bridge 见[任务 API](task-api.md)；所有权基础规则见[类型、布局与所有权](../02-language/types-ownership.md)。
