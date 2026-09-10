---
related_code:
  - zr_vm_lib_task/include/zr_vm_lib_task/module.h
  - zr_vm_lib_task/include/zr_vm_lib_task/runtime.h
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime.c
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
implementation_files:
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime.c
  - zr_vm_lib_task/src/zr_vm_lib_task/runtime/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_core/src/zr_vm_core/task_runtime.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/task/test_task_module.c
  - tests/task/test_task_runtime.c
  - tests/library/test_task_runtime_bridge.c
  - tests/parser/test_syntax_reference_v1.c
doc_type: api-reference
---

# `zr.task` API 参考

当前仓库存在两层 task provider：`zr_vm_lib_task` 的 Runtime descriptor 提供通用
`Async/Scheduler/Channel/Shared/Atomic` surface；`zr_vm_library` 的 canonical Task bridge
提供 `Task<T>`、`Job<T>` 和 `Scheduler.schedule` contract。两层共享 core frame/await 机制，
但 descriptor 名称和 contract hash 不应混写。宿主应按项目注册表中实际 provider role 选择一层。

## 语言侧 async

```zr
async fn fetch(): zr.task.Task<string> {
    let handle = zr.task.spawn(() => "ok");
    return await handle;
}

fn main(): void {
    let task = fetch();
    let result = zr.task.await(task);
}
```

`async fn` 必须显式返回 `zr.task.Task<T>`。`await` 只能出现在允许挂起的 function/frame 中；
跨 await 的局部必须可 frame 化，活动 `ref`/`scoped` loan 和 thread-affine lock 会被拒绝。
任务 fault 会在 `result`/`await` 边界重新抛出，不能用 `isCompleted=false` 判断是否失败。

## Runtime descriptor 函数

| 函数 | 签名 | 说明 |
| --- | --- | --- |
| `spawn` | `spawn(fn): Async` | 把 callable 放入当前 scheduler。 |
| `spawnThread` | `spawnThread(fn): Async` | 需要 multithread project capability；在 worker 执行。 |
| `currentScheduler` | `(): Scheduler` | 取得当前 state 的 scheduler。 |
| `await` | `await(handle: Async): value` | 同步等待/恢复 callable 结果；脚本类型由 handle 保留。 |
| `yieldNow` | `(): null` | 让出当前 cooperative step。 |
| `sleep` | `(duration): null` | 当前实现为 scheduler placeholder；需要时间 provider 时查状态矩阵。 |

`spawnThread` 在项目未启用 `supportMultithread` 时抛 runtime error。scheduler 的默认
`autoCoroutine` 从 project manifest 读取，宿主可通过 C runtime policy 覆盖 worker domain。

## Async 和 Scheduler

| 类型 | 成员 |
| --- | --- |
| `Async<T>` | `result(): value`、`isCompleted(): bool` |
| `Scheduler` | `pump(): int`、`step(): bool`、`setAutoCoroutine(bool): null`、`getAutoCoroutine(): bool` |

`pump` 连续执行队列直到没有可运行 job，返回本轮处理数；`step` 只推进一个可运行 job。
`result` 在未完成时抛/阻塞取决于当前 provider 的调用路径，推荐使用 `await` 统一处理。
执行中的 scheduler 通过隐藏 queue/head/runtime 字段连接 C condition/mutex；脚本不应访问
这些字段。

## canonical Task/Job/Scheduler

canonical provider 的公开 contract 为：

```text
Task<T>      result(): T; isCompleted(): bool
Job<T>       cold value; meta constructor from fn() -> T or fn() -> zr.task.Task<T>
Scheduler    schedule(job: zr.task.Job<T>): zr.task.Task<T>
yieldNow()   zr.task.Task<void>
delay(d)     zr.task.Task<void>
```

`Job` 是 cold 工作描述，构造不会自动执行；`schedule` 负责把它放入队列并返回 Task。若
callable 已返回 Task，bridge 会按 provider contract 处理嵌套，而不是在脚本层强行复制结果。
`delay` 的 Duration 表示由 canonical provider 定义，不能把任意毫秒整数静默转换。

## Channel、Shared、Transfer

| 类型 | 操作 | 约束 |
| --- | --- | --- |
| `Channel<T>` | 构造、`send(T):null`、`recv():T`、`close`、`isClosed`、`length` | recv 空队列返回 null；跨线程需 T 满足 Send/Sync。 |
| `Shared<T>` | `load`、`store`、`clone`、`downgrade`、`release`、`isAlive` | 强引用计数；最后一个 release 才 drop。 |
| `WeakShared<T>` | `upgrade`、`isAlive` | upgrade 可能返回 null，不保持存活。 |
| `Transfer<T>` | `take`、`isTaken` | 一次性 move；take 后旧 owner 失效。 |

Channel close 后 send 失败；recv 已清空时返回 null。Shared 的 `load/store` 仍受 T 的 layout
和线程协议约束，不等于无锁 atomic。Transfer 适合把唯一 owner 交给 worker，不能 clone。

## Mutex 和 atomic

| 类型 | 成员 |
| --- | --- |
| `Mutex<T>` | 构造、`load`、`lock`、`unlock(value)`、`isLocked` |
| `AtomicBool` | 构造、`load`、`store`、`compareExchange` |
| `AtomicInt<T>` / `AtomicUInt<T>` | 构造、`load`、`store`、`compareExchange`、`fetchAdd`、`fetchSub` |

`Mutex.lock` 返回 scoped/affine view 或阻塞到取得锁；必须在同一 lexical scope 调用 unlock。
atomic 操作按宿主平台 memory order 实现，`compareExchange` 同时报告是否交换成功并按
descriptor 写回 observed value。不要以普通 `load/store` 组合出跨线程复合事务。

## 调度和传输机制

当前 C runtime 为每个 scheduler 保存 queue、queue head、pending worker list、status/result/
error 和 runtime pointer。worker 完成后把序列化 payload 放入外部 message queue；owner
scheduler 在 safepoint 解码并更新 handle。payload 解码失败会把 task 标记 faulted，而不是
把半解码值当成功结果。

`spawnThread` 的 worker 传输受 object/byte/depth quota 约束（由 `zr.thread` C API 设置）。
跨 isolated domain 的值必须经过 transfer encoder；native pointer、开放 file/socket handle
和活动 borrow 不可传输。

## C bridge

```c
const ZrLibModuleDescriptor *task = ZrVmTask_GetModuleDescriptor();
if (!ZrVmTask_Register(global)) {
    return ZR_FALSE;
}

/* library bridge: every prepared job is released exactly once */
ZrLibraryTaskRuntimeWorkItem item;
SZrTypeValue result;
if (ZrLibrary_TaskRuntime_PrepareJob(state, scheduler, job, &result, &item)) {
    if (ZrLibrary_TaskRuntime_ExecutePreparedJob(state, &item)) {
        ZrLibrary_TaskRuntime_CompletePreparedJob(state, &item, &result);
    } else {
        ZrLibrary_TaskRuntime_FaultPreparedJob(state, &item, "job execution failed");
    }
    ZrLibrary_TaskRuntime_ReleasePreparedJob(state, &item);
}
```

实际 bridge 函数名和参数以 `zr_vm_library/task_runtime.h` 为准；核心规则是
`Prepare -> Execute/Complete 或 Fault -> Release` 成对调用。`ZrVmTask_Runtime_GetModuleDescriptor`
是 runtime-specific descriptor；不要把它与 `ZrVmTask_GetModuleDescriptor` 的公开模块 contract
混用。

## 调试和失败

| 症状 | 可能原因 | 处理 |
| --- | --- | --- |
| `spawnThread` runtime error | project 未启用 multithread | 在 manifest 开启 capability 或使用 spawn。 |
| await 后值失效 | ref/scoped loan 跨挂起 | 把值复制/转移到 frame-safe owner。 |
| task 永远 pending | scheduler 未 pump/step | 驱动当前 scheduler 或安装 worker。 |
| 结果标记 faulted | worker payload/异常 | 调用 `await` 读取原始 error。 |
| Channel recv 为 null | 队列为空或已 close | 用 `isClosed`/length 区分协议状态。 |
| isolated transfer rejected | quota、Send/Sync 或不可序列化 handle | 降低 payload 或改用 attached domain。 |

更多线程锁和 domain 规则见[线程 API](thread-api.md)，任务 frame 语义见[VM 运行时](../09-vm-runtime.md)。
