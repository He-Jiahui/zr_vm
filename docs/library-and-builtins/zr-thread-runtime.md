---
related_code:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_wrappers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/execution/execution_memory.c
implementation_files:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_wrappers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_transport.c
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
tests:
  - tests/thread/test_thread_runtime.c
doc_type: module-detail
---

# zr.thread Built-in Runtime

## Scope

`zr.thread` provides a ThreadScheduler with AttachedDomain and IsolatedDomain
policies. Attached workers share the caller GC domain through separate states;
isolated workers use separate domains and transport envelopes. Ordinary isolate
heap objects do not cross an isolated worker boundary directly; transferred
data follows the transport and `Send`/`Sync` contracts.

`supportMultithread = false` rejects `ThreadScheduler` submission but does not
change local `zr.task` scheduling.

## Public Surface

- marker interfaces `Send` and `Sync`
- `ThreadScheduler`, whose public operation is
  `schedule(Job<T>): Task<T>`
- `Channel<T: Send>`, `Transfer<T: Send>`
- `Shared<T: Send + Sync>`, `WeakShared<T: Send + Sync>`
- `UniqueMutex<T: Send>`, `SharedMutex<T: Send + Sync>`, `Lock<T>`, and
  `SharedLock<T>`

`ThreadScheduler` snapshots the host policy at construction and returns a
caller-domain completion Task. Isolated workers materialize Send captures and
results for cross-domain transfer. The constructor reads a signed `int64`
`workerCount`, checks only positivity, then narrows it to `uint32` for the worker
limit. BUG: values above `UINT32_MAX` may wrap; `4294967297` becomes 1.

## Removed Compatibility Surface

`Thread`, `Scheduler`, `spawnThread()`, `getCurrentThreadScheduler()`, and
`start`/`pump`/`step` methods are not registered. They are not aliases for
`ThreadScheduler.schedule(Job)`.

`%mutex`, `%atomic`, `AtomicBool`, `AtomicInt`, and `AtomicUInt` remain
rejected. `Lock<T>` and `SharedLock<T>` are affine, cannot cross `await`, and
do not implement `Send` or `Sync`.

## Provider Guarantees

The attached-domain and isolated-domain paths settle the caller Task through
the canonical bridge. Quota and queued-shutdown faults use the Task fault path;
non-Send/non-Sync payloads are rejected by the resolved generic contract. The
current descriptor does not recreate `TaskRunner` or use a member-name
fallback to choose a scheduler route.

An isolated-domain launch remains worker-owned until the caller publishes its
completion acknowledgement. Provider worker-slot accounting is released before
that publication; signaling `completionProcessed` is the final caller-side use
of the launch object. The waiting worker may free the launch immediately after
it wakes, so completion processing must not read the launch, its runtime pointer,
or its synchronization fields after the signal/unlock boundary.

BUG: completion acknowledgement precedes the isolated worker's global teardown.
`Task.result()` therefore does not prove that live workers have exited.
Shutdown requests rejection of new isolated work when its marker write succeeds,
and faults queued requests, but does not join workers. With a valid host global
and root, a missing or false marker can remain unchanged after a silent write
failure while the API returns true; subsequent submission can still be accepted. TODO: define a separate host barrier if shutdown must permit
safe global destruction. An isolated worker may
still dereference allocator arguments borrowed from the caller global, so the
host must keep that global alive until worker cleanup is independently known to
be complete. Attached workers likewise publish Task terminal status before
releasing the work item, leaving the mutator, and freeing their state; result
observation is not a worker join. The public API currently provides no safe
global-destruction barrier for these paths.

BUG: two `Shared` strong handles releasing concurrently can race between the
first decrement and a second mutex lock while the final release frees that
mutex. Ordinary GC loss of a `Shared` wrapper also does not decrement the
native strong count; callers currently need explicit `release()`.

BUG: `Channel` has no native drop path for its FIFO, synchronization objects,
or unread messages when the wrapper is collected. The native transport's
string duplication uses `strlen`, so strings with an embedded NUL lose their
trailing bytes in Channel, Shared, and legacy worker transport round trips.

BUG: `SetSchedulerExecutionPolicy` and `SetIsolatedTransferQuota` use field
setters without success feedback. Allocation or pin failure can leave an old
policy or partly updated quota while these C APIs still return true.

## 宿主 C 配置、停止与诊断窗口

公共 runtime.h 的五个 API 把 provider 查询、之后新建实例的配置、停止请求和完成诊断分开。
它们不转移 GlobalState、线程或 GC 域所有权。配置与停止沿有效 global->mainThread 调用，
宿主应串行管理对应主 VM 域；接口并未提供并发配置或全局销毁屏障。

| 入口 | 调用目的 | 使用边界 |
| --- | --- | --- |
| GetModuleDescriptor | 借用静态 Runtime provider 元数据。 | 查询无需 VM，不执行 Register/Attach；const 表及回调依赖 provider 持续加载。 |
| SetSchedulerExecutionPolicy | 选择之后 ThreadScheduler 构造的 AttachedDomain/IsolatedDomain 后端。 | 只接受两个枚举值；已构造实例保留原快照；void 字段写失败仍可返回 true。 |
| SetIsolatedTransferQuota | 设置之后隔离实例的捕获和结果复制预算。 | maxObjects/maxBytes/maxDepth 都非零；三个字段写非事务，失败仍 true 且可能混合新旧预算。 |
| ShutdownIsolatedSchedulers | marker 成功写入时请求拒绝新隔离提交，并 fault 已登记 runtime 的 pending 队列。 | 不是 join，也不等待在途 worker；合法 global/mainThreadState/root 且 marker 缺失或 false 时，写失败仍 true，新提交仍可能接受。 |
| GetLastSchedulerWorkerDomain | 按值读取最近一次成功 caller-side completion 的隔离 worker 身份。 | 有效输出先清零，记录无效返回 false；不清除记录，不保活域，不证明 worker 仍活动或已退出。 |

AttachedDomain worker 使用 caller GC 域中的独立 state；IsolatedDomain 新建 GlobalState/GcDomain，
捕获与结果走 Send 及跨域复制预算。策略及 quota 都在 scheduler 构造时快照，已有实例不追溯修改。
具体入口为 `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1271`、`:1284`、`:1315`、`:1322`，
提交端 `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:823` 消费实例预算。

BUG: 配置与 shutdown marker 都经 void object 字段 setter；对象 ignore 注册扩容、字段 key 创建/固定或
对象字段分配失败可静默返回，外层仍报告 true。此处是静态可达链，没有故障注入或运行时复现信用。
前提是 global/mainThreadState/根对象合法，且 shutdown 字段原先缺失或 false。停止路径
`zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:415` 写 marker 后，`:416` 只处理已登记 runtime 的 pending 队列。
真实宿主用例 `tests/thread/test_thread_runtime.c:928` 至 `:947` 在停止后新建 scheduler；构造器
`zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1271` 至 `:1296` 消费配置，后续公开 schedule
经 `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c:1315` 至 `:1320` 选择隔离后端，
`zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:829` 至 `:839` 仍读取旧 false 时放行，
`:902` 至 `:912` 继续排队/派发并可启动新隔离域；成功写 true 时同一检查会 fault 已准备的 job。
核查低层入口为 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385`、`:1393`、`:1400`，
以及 `zr_vm_core/src/zr_vm_core/object/object.c:1855`、`:1863` 的最终字段存储失败。

成功 completion 的诊断写入在 `runtime.c:621`、`:642`，读取在 `runtime.c:446` 至 `:467`；
Shutdown 或新建实例不会自动清除旧记录。TODO: id/generation 两个字段分别写入，需沿 `runtime.c:343`
核对部分写失败及未来 generation 重用契约；当前新 worker 的 `gc_domain.c:183` 固定 generation=1，
本轮不宣称已经证明混合身份。身份副本不是 GC root、活动 handle 或停止完成证明。

TODO: 如果宿主需要停止后安全释放 global，必须另定义 await/join 屏障。当前 caller 的 completion ack
发生在隔离 worker GlobalState_Free 之前，仍须推进完成消息并保持借用的 global/allocator 有效，直到 worker
清理结束得到独立确认。核查 `runtime_isolated_domain.c:460` 至 `:466`，Task.result 成功不替代 worker join。

TODO: runtime-head 的 void 登记写失败可能使本次 pending 遍历遗漏实例；核查
`zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c:786` 至 `:792` 的登记失败，
以及 `:676` 的根链读取如何处理未登记 runtime 的队列。本轮仅记录该核查入口。

本节只覆盖公开头部的 11 个单元；runtime.c 的完整单元、文档格式和整个 thread 模块仍待独立重核。

## 内部 callback 借用与结果交接

runtime_internal.h 为跨 Thread 编译单元的内部接口保存参数、返回及生命周期契约；
runtime.c 的定义只补非显然限制或关联说明。本阶段覆盖请求类型、三个成员、
Self 适配、结果标签选择、对象结果交接、字段写入四个 helper，以及三个内部声明，
共 11 条正式记录（5 条返工、6 条新增字段/声明）；两个源码文件和整个模块仍为 pending。

| 单元 | 对调用链的职责 | 调用限制 |
| --- | --- | --- |
| ZrVmTaskExecuteRequest | 在同步 TryRun 的 void callback 和旧执行链之间交付 CallValue 结果及单次 bool。 | 栈本地请求不能逃逸；TryRun 状态、completed 与 VM threadStatus 分别判断。 |
| zr_vm_task_self_object | 为 ThreadScheduler、Channel 和 wrapper callback 借用 Self 对象。 | NULL context 安全返回 NULL；标签筛选不验证业务包装类型，返回指针没有独立根。 |
| zr_vm_task_value_type_for_object | 统一对象/数组的 callback 结果标签。 | 只投影 internalType；唯一 caller 已拒绝 NULL，对象有效性留在调用方。 |
| zr_vm_task_finish_object | 将有效对象 raw 初始化进 callback 输出槽。 | 不 Copy、retain 或建立 root；true 不证明业务字段成功，旧槽管理和对象保活由调用链承担。 |
| zr_vm_task_set_value_field | 统一 Thread 状态、队列和包装对象的 Native 字段写入约定。 | void 不报告成功；正常返回不保证字段已保存，不提供多字段事务或回滚。 |

TODO: ZrVmTaskExecuteRequest 所在 create/pump/wait 旧链尚未找到公开 descriptor 的入口；
沿 zr_vm_task_create_async_handle、zr_vm_task_scheduler_pump_internal 和
zr_vm_task_wait_for_handle 继续核对保留意图。公开 ThreadScheduler.schedule 将
canonical Job 委派到 attached/isolated 后端，不能作为旧请求的生产调用点。

TODO: 旧链借用 callable 字段后先写状态字段，才进入 CallValue；需核对这一阶段的
字段地址和对象保活保证。CallValue 在入口已复制 stableCallable，后续 VM 栈复制
不是首次读取。若旧链预期可达且结果带 ownership metadata，继续核对将结果复制到
handle 或用于 fault fallback 后的临时槽释放与保活责任。以上疑点不记作已证 BUG。

本阶段的验证范围为实际 checkout 的两个 native Windows C11 编译单元
runtime.c、runtime_wrappers.c（后者包含内部声明），并比较前后依赖。
未运行 Thread 测试、故障注入或旧链运行复现，编译证据不代表完整模块验收。
