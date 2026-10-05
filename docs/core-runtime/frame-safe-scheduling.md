---
related_code:
  - zr_vm_core/include/zr_vm_core/async_frame_budget.h
  - zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c
  - zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c
  - zr_vm_core/include/zr_vm_core/task_frame_runtime.h
  - zr_vm_core/include/zr_vm_core/execution_budget.h
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - tests/task/test_ssa_async_frame_budget.c
  - tests/task/ssa_async_compile_contract_cases.inc
  - tests/task/ssa_async_stale_handle_cases.inc
implementation_files:
  - zr_vm_core/include/zr_vm_core/async_frame_budget.h
  - zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c
  - zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/05-async-frame-budget.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - "user: 2026-09-14 implement 06.05 async frame budget"
tests:
  - tests/task/test_ssa_async_frame_budget.c
  - tests/task/ssa_async_compile_contract_cases.inc
  - tests/task/ssa_async_stale_handle_cases.inc
  - tests/acceptance/ssa-async-frame-budget.md
  - tests/acceptance/2026-09-29-ssa-async-frame-budget-ndebug-tests.md
  - tests/acceptance/2026-10-02-ssa-async-compile-contracts.md
  - tests/acceptance/2026-10-02-ssa-async-stale-handles.md
doc_type: module-detail
---

# Frame-safe asynchronous scheduling

## Purpose

`async_frame_budget.h` is the small, pointer-light contract between an
async/task lowering and a scheduler.  It does not replace the concrete
`SZrCoreTaskFrameTask` runtime or the existing VM execution budget.  Instead it
defines the invariants that adapters must preserve when they hand a frame back
to a scheduler: suspension is explicit, a state-map boundary is present,
non-crossing borrows and critical native sections never yield, and teardown
cannot race an active pin.

The same module contains two coordination records. Registry identity changes
use a spin lock; this contract makes no wait-free or lock-free progress promise.  The wait
registry closes the register/recheck lost-wakeup window with a single atomic
winner transition.  The compile queue owns a copied IR snapshot, so a worker
never reads a movable AST or VM allocation and never publishes a result whose
generation or contract hashes are stale.

## Frame and budget state machine

An initialized `SZrAsyncFrameBudget` starts in `IDLE` and enters `RUNNING` only
through `ZrCore_AsyncFrameBudget_Begin`.  `Poll` accounts caller-supplied work units. A future adapter chooses safe
poll sites; the current consumers are standalone SSA tests.  Reaching the configured work limit sets a
pending bit; it becomes `SUSPENDED` only when all of the following hold:

1. the caller is in an async contract with suspension enabled;
2. the poll is at a declared state-map boundary whose generation matches the
   frame generation;
3. no borrow, stack alias, non-suspendable lock guard, or native critical
   section is live.

If a native critical section is still running, a pending budget or cancellation
request is recorded and the poll remains `RUNNING`; the native instruction is
never split.  Cancellation is observed at the next coherent boundary and is a
normal `CANCELLED` outcome, not a guest exception.  `Pin`/`Unpin` are paired
 counters on the frame lease.  `Teardown` is idempotent after a terminal state,
 but rejects a running or pinned frame, which makes partial cleanup visible to
 the host instead of silently dropping a live continuation.

All diagnostics contain only enum values, source/instruction IDs, scalar
expectations, and a generation.  They can therefore cross a scheduler or
worker boundary without retaining an AST, VM object, or host address.

## Waiter registration and wakeup

`ZrCore_Execution_BeginAsyncWait` first validates the async request and
resource restrictions, reserves a caller-owned slot, and publishes
`REGISTERING`.  It then atomically publishes `WAITING` (or `READY` when the
initial condition is already true).  `Wake`, `Cancel`, and `Timeout` all race
through the same CAS from `REGISTERING`/`WAITING` to their terminal state.
Consequently a wake between registration and condition recheck is retained;
`Recheck` treats an already terminal winner as success rather than overwriting
it.  Only `Resume` can move a terminal waiter to `RESUMED`, and it increments
the resume counter exactly once.  `Release` accepts only a resumed waiter and
returns its slot to the registry, preventing a scheduler from freeing a still
waiting continuation.

The registry is fixed-capacity and caller-owned.  Capacity exhaustion and stale
handles are structured diagnostics, not an implicit blocking allocation.

## Background compilation and warm-up

`ZrCore_Execution_QueueCompilation` copies the supplied byte snapshot before
publishing a `QUEUED` record.  The request carries generation, module,
signature, and layout hashes; all must be non-zero and are checked again by
`ZrCore_CompileQueue_Complete`.  A worker claims a record as `RUNNING`, reads
the immutable snapshot, and then either:

- publishes `COMPLETED` when every identity still matches and a non-zero result
  hash is supplied;
- marks the result `DISCARDED` for a generation/contract mismatch; or
- marks it cancelled/discarded when cancellation was requested.  A queued job
  can become `CANCELLED` immediately; a running job only records the atomic
  cancellation flag and stays `RUNNING` until the worker calls `Complete`.
  `Complete` is the worker acknowledgement and moves that job to
  `DISCARDED`, after which `Release` may free the snapshot.  `Release` rejects
  a running job, so cancellation cannot free bytes that a worker may still be
  reading.

The `WARMUP` request bit is metadata for a host loading phase; it does not
change execution semantics.  There is intentionally no wait-for-completion
operation in this contract.  A frame thread can enqueue, poll, or cancel and
continue running its baseline path.  Queue release frees the copied snapshot
exactly once, including cancellation and stale-result paths.

Queue requests carry both a queue pointer and an output-handle pointer.  The
implementation requires each to match the explicit arguments (and rejects
NULL identities), preventing a request from being published through an
unrelated queue or handle.  Queue record transitions are internally
serialized, so copied handle values may be used by a worker and an owner.  A
single handle object must not be concurrently mutated, the queue must remain
initialized until all jobs are terminal/released, and a borrowed snapshot
returned by `GetSnapshot` must be fully consumed before the worker calls
`Complete`.  `Deinit` is therefore a quiescent operation; it is not a worker
shutdown primitive.

## Integration boundaries

The files are standalone core sources so they can be integrated into the core
target without changing existing dispatch or task-runtime ownership.  The current `tests/cmake/ssa-tests.cmake:1218` executable includes the test,
wait C source (`:1220`) and compile C source (`:1221`), and registers
`ssa_async_frame_budget` at `:1226`. This is the actual current build entry,
not evidence that this comment candidate has run.  Existing `execution_budget.h/c` and
`task_frame_runtime.c` remain the owners of concrete VM budget counters and
GC-rooted task slots; an adapter should call this contract at their safe poll,
state-map, and cleanup boundaries rather than create a second counter or
continuation owner.

## 当前等待与帧契约的有限源码审阅（2026-10-04）

本节只记录 execution_async_wait.c 和其转发头的完整审阅；公开 record/field 全文
信用由独立 header 批次负责。当前上游是 SSA 测试及 included stale-handle case，
没有查到 VM、FFI、生成 C/LLVM、worker 或用户回调消费这组帧/等待 API。
公开但仓内未调用的 DiagnosticClear/DiagnosticName/StatusName/Fault/显式 Wait_Begin
保留具体仓外适配器 TODO；转发头没有仓内直接包含者，保留原兼容入口 TODO。

Init 建立空闲 ABI 记录，调用方补非零 identity/generation 和可暂停 state map。
Begin 重置计量并保留预启动取消；Resume 保留累计预算，不能视作预算 reset。
Pin/Unpin 只是适配器活跃引用计数，不能注册 GC root 或稳定 VM 对象地址；它们
阻止 Complete/Fault/Teardown，CanSuspend 本身不按 pinCount 拒绝暂停。
资源 flags 和边界布尔值是调用方声明，Validate 不能验证实际对象寿命。
Poll 饱和累计、native 临界区延后事件，在安全边界取消优先于预算暂停。
返回 FAULTED outcome 不总是把 frame.status 改成 FAULTED，需按实际失败路径区分。
RequestCancel 仅取消位原子写，status/pendingFlags 是普通字段；当前同线程测试
不能证明跨线程许可，适配器须明确同步并补并发验证，此处保留 TODO 而非 BUG。
Suspend 只改变标量，不注册等待；Resume 无用户回调，取消分支不承诺清空所有
reason/pending字段。Complete/Teardown 不释放 VM、槽数组或对象存储。

registry、数组、request、handle 都由调用方保持有效。deadlineMicros 只随槽保存，
Timeout 是显式外部事件，不读取时钟。Begin 注册前的拒绝不发布输出 handle，
两入口分别选请求内指针或显式参数，不把 CompileQueue 的指针相等门禁移植过来。
REGISTERING发布后CAS不覆盖已获胜的Wake/Cancel/Timeout；Recheck 成功也可表示
已有取消、超时或消费终态。Resume 只认领一次，resumeCount 非回调调用数。
Release 失败保留句柄和槽身份；成功清空本句柄、归还槽位，不释放外部数组。
调用方必须先结束同槽访问才能 Release；Deinit 必须在所有访问停止后进行，
原子state和registry锁不能替代寿命约束。token/generation拒绝旧身份不是GC保护。
staleWinners 的三个函数指针在 `tests/task/ssa_async_stale_handle_cases.inc:5` 登记，
真实间接调用在 `tests/task/ssa_async_stale_handle_cases.inc:45`，`:44`只是循环头。
私有 ASYNC_STAGE_COMPILE 是本wait C未使用的保留member，不承担 compile queue 诊断。

本次只进行静态审阅、after-aware 48行检查和readonly patch check；没有 native、
build 或测试执行。下列历史验收保持其原时间与范围，不能转记为本次候选执行。

## 公开预算头与编译队列的完整静态契约审阅（2026-10-04）

本节覆盖整个 async_frame_budget.h 的类型、90 个字段、40 个 API、六个公开枚举及其成员，
并覆盖 execution_compile_queue.c 的完整 21 个函数、私有 stage 和七个有意义块。
execution_compile_queue.h 为只读转发头，没有当前仓内直接包含者；保留仓外兼容入口 TODO。
公开操作的当前消费者是 SSA fixtures、included contract/stale-handle cases 和两个 C 实现内部调用。
CMake 收集源码、umbrella 导出声明均不等于生产调度器、GC、worker 或用户回调接入。
原等待 C 批次的完整审查保持原范围；本批不重新授予整个依赖模块或历史执行信用。

### 所有权、时序与失败状态

队列借用 queue、records 数组及 handle，只有 malloc 复制的 IR snapshot 由队列持有。
Init 建立空容器；禁止覆盖活跃容器。Deinit 在 worker、handle 和所有访问静止后回收复制体，
不执行 Cancel/Complete 握手、不释放外部数组，不是 worker shutdown 或 VM/GC teardown。
发布前先复制，失败不发布 record/输出 handle；零长 IR 可用空指针，maxSnapshotBytes 为零只允许空 IR。
Queue 与 Warmup 必须让 request 内 queue/outputHandle 匹配显式参数，不能类推到 Wait 的显式 Begin。
jobId 防槽复用混淆，计数耗尽后拒绝、不回绕；终态仍占 activeCount，直到 Release。
ClaimNext 按槽扫描，不承诺 FIFO 或 warmup 优先级。COMPLETED 只保存结果哈希，不安装机器码。
QUEUED 取消可立即终结，RUNNING 取消只登记请求；worker 必须 Complete 确认后才可 Release。
Complete 因取消或身份/契约改变返回失败时可已置 DISCARDED；非法状态失败则不得假定已终结。
GetSnapshot 返回借用指针，锁不构成借用 pin。worker 应先 Claim，读完后 Complete；
若直接读取 QUEUED snapshot，外部必须排除 Cancel 与 Release。失败时 snapshot 输出不更新。
同一个 handle 对象不得与 Release 并发访问，所有 copied handles 也须在 Deinit 前停止访问。

frame.pinCount 仅为 Complete/Fault/Teardown 的标量门禁，不是 GC root/retain，也不阻止 CanSuspend。
flags/state-map 是调用方报告的契约，API 不检查真实 VM 对象的保活或地址稳定性。
RequestCancel 的 cancel 位原子访问不使 status/pendingFlags 成为原子字段；跨线程同步仍需适配器定义。
Suspend 不注册 wait，Resume 不回调、不重置预算；取消恢复分支保留 reason/pending。
帧取消、wait 取消、compile 取消互不自动联动。wait deadline 仅保存，Timeout 是显式事件。
诊断 clear 路径清空整个输出；wrapper 前置拒绝只写 helper 的六个字段，sourceId/instructionId 保留原值。
NONE 是清空/成功类别。BUDGET_EXHAUSTED 当前只被名称 switch 使用，预算路径未发送此诊断码；
其外部 ABI/调度器用途从未来接入入口核查，保留具体 TODO。POLL_COMPLETED 的当前发出路径同样未见。

以下逐项用途来自已接受的完整 caller-first 审阅；名称查询和未有仓内调用的公开入口不据此判死代码。
没有本批 native、build、运行测试或全模块验证信用。

### 40 个公开 API

| API | 用途 |
|---|---|
| `ZrCore_AsyncFrameBudget_DiagnosticClear` | 清空可选诊断输出；空指针无操作。 |
| `ZrCore_AsyncFrameBudget_DiagnosticName` | 返回静态诊断名称；未知值返回 unknown。 |
| `ZrCore_AsyncFrameBudget_StatusName` | 返回静态帧状态名称；未知值返回 unknown。 |
| `ZrCore_AsyncFrameBudget_Init` | 初始化帧 ABI 与空闲状态；不分配资源。 |
| `ZrCore_AsyncFrameBudget_Validate` | 校验标识、代际、标志及 state map 一致性。 |
| `ZrCore_AsyncFrameBudget_Begin` | 从 IDLE 进入 RUNNING 并重置用量；保留此前收到的取消请求。 |
| `ZrCore_AsyncFrameBudget_CanSuspend` | 检查异步许可、活跃资源与 state map 边界；失败填写诊断。 |
| `ZrCore_AsyncFrameBudget_Poll` | 累计工作量并兑现可安全处理的取消或预算暂停。 |
| `ZrCore_AsyncFrameBudget_Suspend` | 将可暂停帧按指定原因置为 SUSPENDED；须通过 CanSuspend 的边界检查。 |
| `ZrCore_AsyncFrameBudget_Complete` | 标记帧完成；活跃 pin 阻止终结。 |
| `ZrCore_AsyncFrameBudget_Fault` | 将非终态帧标为故障；活跃 pin 阻止终结。 |
| `ZrCore_AsyncFrameBudget_Pin` | 增加 Complete/Fault/Teardown 的门禁计数；过量时拒绝。 |
| `ZrCore_AsyncFrameBudget_Unpin` | 减少一份门禁计数；零计数时拒绝，不释放外部对象。 |
| `ZrCore_AsyncFrameBudget_RequestCancel` | 请求在下一个安全边界取消；Begin 保留启动前的请求。 |
| `ZrCore_AsyncFrameBudget_Resume` | 恢复暂停帧；若已请求取消，则改为 CANCELLED。 |
| `ZrCore_AsyncFrameBudget_Teardown` | 回收非运行帧；活跃 pin 被拒绝，重复 teardown 可成功。 |
| `ZrCore_AsyncWaitRegistry_Init` | 借用调用方槽位数组建立等待注册表；拒绝零容量。 |
| `ZrCore_AsyncWaitRegistry_Deinit` | 清空注册表状态；仅在所有访问与 handle 生命周期结束后调用。 |
| `ZrCore_Execution_BeginAsyncWait` | 用请求内的 registry/outHandle 注册等待；可能与唤醒并发。 |
| `ZrCore_AsyncWait_Begin` | 显式传入注册表与输出 handle 的等待注册入口。 |
| `ZrCore_AsyncWait_Recheck` | 注册后重新检查条件；已获胜的唤醒/取消/超时不会被覆盖。 |
| `ZrCore_AsyncWait_Wake` | 尝试将 REGISTERING/WAITING 原子转成 READY。 |
| `ZrCore_AsyncWait_Cancel` | 尝试让等待由取消赢得终态。 |
| `ZrCore_AsyncWait_Timeout` | 调度器显式触发超时，尝试赢得终态。 |
| `ZrCore_AsyncWait_Resume` | 对 READY/CANCELLED/TIMED_OUT 槽位只恢复一次。 |
| `ZrCore_AsyncWait_Release` | RESUMED 后释放槽位并清空 handle；不可与任何同槽句柄访问并发。 |
| `ZrCore_AsyncWait_State` | 查询匹配 handle 的槽位状态；失效时返回 FREE。 |
| `ZrCore_AsyncWait_ResumeCount` | 查询匹配槽位的恢复次数；失效时返回零。 |
| `ZrCore_CompileQueue_Init` | 借用固定记录数组创建队列，并设置单任务快照字节上限。 |
| `ZrCore_CompileQueue_Deinit` | 释放队列仍持有的快照；须在所有 worker 与 handle 停止后调用。 |
| `ZrCore_Execution_QueueCompilation` | 用请求中的 queue/outHandle 排入任务。 |
| `ZrCore_CompileQueue_Queue` | 复制 IR 快照并原子发布 QUEUED 任务；满队列或分配失败不发布记录。 |
| `ZrCore_CompileQueue_QueueWarmup` | 按相同身份约束排入带 WARMUP 标志的任务。 |
| `ZrCore_CompileQueue_ClaimNext` | worker 认领一个 QUEUED 任务，转成 RUNNING 并得到 handle。 |
| `ZrCore_CompileQueue_Cancel` | 取消排队任务，或对运行任务只记录待取消请求。 |
| `ZrCore_CompileQueue_GetSnapshot` | 借用队列中的不可变 IR 快照；worker 须在 Complete 前读完。 |
| `ZrCore_CompileQueue_Complete` | worker 按代际及哈希确认结果；过期或已取消任务转为 DISCARDED。 |
| `ZrCore_CompileQueue_Release` | 仅对终态任务释放快照与槽位，并清空调用方 handle。 |
| `ZrCore_CompileQueue_State` | 查询匹配 handle 的记录状态；失效时返回 FREE。 |
| `ZrCore_CompileQueue_IsWarmup` | 查询匹配任务的预热标志；失效时返回假。 |

### 90 个字段

| 字段 | 消费者用途与限制 |
|---|---|
| `SZrAsyncFrameDiagnostic.code` | 无错误分类或具体失败分类；常规入口先 clear，拒绝 helper 覆写 code；wrapper 前置拒绝不先 clear 整个 diagnostic |
| `SZrAsyncFrameDiagnostic.stage` | 失败实现私有阶段编号；wait与compile编号域独立 |
| `SZrAsyncFrameDiagnostic.state` | 失败时采集的所属状态机标量 |
| `SZrAsyncFrameDiagnostic.sourceId` | 等待资源拒绝时复制请求 sourceId；经过先 clear 的入口其余路径为零；wrapper 前置拒绝不先 clear，保留已有 sourceId |
| `SZrAsyncFrameDiagnostic.instructionId` | 等待资源拒绝时复制请求 instructionId；经过先 clear 的入口其余路径为零；wrapper 前置拒绝不先 clear，保留已有 instructionId |
| `SZrAsyncFrameDiagnostic.expected` | 本次失败的预期标量 |
| `SZrAsyncFrameDiagnostic.actual` | 本次失败的实测标量 |
| `SZrAsyncFrameDiagnostic.generation` | 失败所属帧/等待/作业代际，不持有对象 |
| `SZrAsyncFrameBudget.magic` | Init设置并由Validate核验ABI标记 |
| `SZrAsyncFrameBudget.schemaVersion` | Init设置并与共享schema版本匹配 |
| `SZrAsyncFrameBudget.flags` | 调用方报告异步许可与活跃资源，Validate拒绝未知位 |
| `SZrAsyncFrameBudget.status` | 帧转移门禁；调用者必须串行化普通字段访问 |
| `SZrAsyncFrameBudget.suspendReason` | 暂停原因，Resume正常分支清NONE；取消恢复保留原原因 |
| `SZrAsyncFrameBudget.pendingFlags` | 预算/取消待处理位；WAIT位无实现消费者 |
| `SZrAsyncFrameBudget.stateMapId` | STATE_MAP_VALID时要求非零的恢复边界标识 |
| `SZrAsyncFrameBudget.pinCount` | Pin/Unpin维护的终结门禁计数，不注册GC根或持有真实对象 |
| `SZrAsyncFrameBudget.frameId` | 调用方初始化的非零帧标识 |
| `SZrAsyncFrameBudget.generation` | 调用方初始化的非零代际 |
| `SZrAsyncFrameBudget.stateMapGeneration` | 声明state map有效时必须等于帧generation |
| `SZrAsyncFrameBudget.maxWorkUnits` | 零表示无工作量上限，Resume不会清已消费用量 |
| `SZrAsyncFrameBudget.consumedWorkUnits` | Poll累积并饱和到UINT64_MAX，Begin清零 |
| `SZrAsyncFrameBudget.pollCount` | RUNNING有效Poll的饱和计数 |
| `SZrAsyncFrameBudget.suspensionCount` | 成功预算/显式暂停的饱和计数 |
| `SZrAsyncFrameBudget.cancellationRequested` | 原子取消位仅接受0或1；其他普通字段仍需外部同步 |
| `SZrAsyncFrameBudget.reserved` | Validate要求为零 |
| `SZrAsyncWaitSlot.state` | 注册、获胜终态与恢复状态的原子门禁 |
| `SZrAsyncWaitSlot.resumeCount` | 获胜Resume增加一次；Release/Deinit清零 |
| `SZrAsyncWaitSlot.token` | 同registry生命期单调分配的槽身份；Release清零 |
| `SZrAsyncWaitSlot.frameId` | 请求复制的帧标识，不连接真实frame对象 |
| `SZrAsyncWaitSlot.generation` | 请求复制并与handle共同核对的代际 |
| `SZrAsyncWaitSlot.deadlineMicros` | 只保存请求期限；Timeout不读取时钟或比较期限 |
| `SZrAsyncWaitRegistry.lock` | 串行化槽领取/Release清身份；不是全API访问寿命锁 |
| `SZrAsyncWaitRegistry.nextToken` | 从1分配token，UINT64_MAX之后置零拒绝新请求 |
| `SZrAsyncWaitRegistry.slots` | 借用调用方数组；Deinit不释放该存储 |
| `SZrAsyncWaitRegistry.capacity` | Init要求非零的槽数量 |
| `SZrAsyncWaitRegistry.reserved` | Init清零；当前运行入口不读取该字段 |
| `SZrAsyncWaitHandle.registry` | 借用registry；Release和Deinit前结束同槽所有访问 |
| `SZrAsyncWaitHandle.slotIndex` | 槽边界校验，不独自构成身份 |
| `SZrAsyncWaitHandle.token` | 与槽token匹配，拒绝重用后的旧handle |
| `SZrAsyncWaitHandle.generation` | 与槽generation匹配；同代际重用仍靠token区分 |
| `SZrAsyncWaitRequest.schemaVersion` | Begin校验共享schema |
| `SZrAsyncWaitRequest.registry` | 仅包装入口使用，显式Begin使用形参registry |
| `SZrAsyncWaitRequest.outHandle` | 仅包装入口使用，显式Begin使用形参outHandle |
| `SZrAsyncWaitRequest.frameId` | 非零标量身份，不保活VM或帧 |
| `SZrAsyncWaitRequest.generation` | 非零标量代际，复制到槽与handle |
| `SZrAsyncWaitRequest.deadlineMicros` | 复制到槽；调用者负责触发Timeout事件 |
| `SZrAsyncWaitRequest.sourceId` | 资源拒绝诊断位置标识 |
| `SZrAsyncWaitRequest.instructionId` | 资源拒绝诊断位置标识 |
| `SZrAsyncWaitRequest.conditionReady` | 初始就绪条件；不覆盖既有terminal winner |
| `SZrAsyncWaitRequest.allowSuspend` | Begin必须为真 |
| `SZrAsyncWaitRequest.holdsBorrow` | 为真拒绝跨等待借用 |
| `SZrAsyncWaitRequest.holdsStackAlias` | 为真拒绝跨等待栈别名 |
| `SZrAsyncWaitRequest.holdsLockGuard` | 为真拒绝跨等待锁guard |
| `SZrAsyncWaitRequest.inNativeCritical` | 为真拒绝native临界区等待 |
| `SZrAsyncWaitRequest.reserved0` | 当前Begin未读取，外部适配器使用约定待核 |
| `SZrCompileJobRecord.state` | 队列锁内发布的原子作业状态 |
| `SZrCompileJobRecord.cancellationRequested` | Cancel标记；RUNNING由Complete确认后才能释放 |
| `SZrCompileJobRecord.jobId` | 非零槽身份，Release清零 |
| `SZrCompileJobRecord.requestedGeneration` | 请求复制的代际，用于Complete拒绝过期结果 |
| `SZrCompileJobRecord.moduleHash` | Complete必须匹配的模块契约标量 |
| `SZrCompileJobRecord.signatureHash` | Complete必须匹配的签名契约标量 |
| `SZrCompileJobRecord.layoutHash` | Complete必须匹配的布局契约标量 |
| `SZrCompileJobRecord.resultHash` | 成功Complete保存非零结果标识；没有实际代码安装 |
| `SZrCompileJobRecord.snapshot` | 队列malloc复制并拥有的IR；Release或静止Deinit释放 |
| `SZrCompileJobRecord.snapshotLength` | 复制IR字节数，零长度允许空快照 |
| `SZrCompileJobRecord.flags` | 请求预热位；IsWarmup查询，不改调度顺序 |
| `SZrCompileJobRecord.reserved` | Queue/Release写零；没有运行消费者 |
| `SZrCompileQueue.lock` | 串行化记录身份与转移；初始化/析构寿命仍需外部静止 |
| `SZrCompileQueue.nextJobId` | 从1递增且不回绕；耗尽后置零并拒绝Queue |
| `SZrCompileQueue.records` | 借用固定调用方数组，队列仅拥有其中snapshot |
| `SZrCompileQueue.capacity` | 记录数组槽数，Init拒绝零 |
| `SZrCompileQueue.activeCount` | 包括QUEUED/RUNNING和未Release终态的占槽数量 |
| `SZrCompileQueue.maxSnapshotBytes` | 单次请求复制字节上限，零仅容许空快照 |
| `SZrCompileJobHandle.queue` | 借用队列，调用者保持queue/records存活 |
| `SZrCompileJobHandle.slotIndex` | 槽边界检查，必须联合jobId核验 |
| `SZrCompileJobHandle.jobId` | 与当前记录匹配，拒绝同槽重用后的旧handle |
| `SZrCompileQueueRequest.schemaVersion` | Queue必须匹配共享schema |
| `SZrCompileQueueRequest.queue` | 必须等于显式Queue/QueueWarmup队列实参 |
| `SZrCompileQueueRequest.outHandle` | 必须等于显式输出实参；失败保持旧输出 |
| `SZrCompileQueueRequest.irSnapshot` | 只在Queue复制期间借用源IR；复制前调用者保持稳定 |
| `SZrCompileQueueRequest.irLength` | 零允许空源；非零须有源且不超过maxSnapshotBytes |
| `SZrCompileQueueRequest.requestedGeneration` | 非零请求代际，不等于jobId |
| `SZrCompileQueueRequest.moduleHash` | 非零模块契约标识 |
| `SZrCompileQueueRequest.signatureHash` | 非零签名契约标识 |
| `SZrCompileQueueRequest.layoutHash` | 非零布局契约标识 |
| `SZrCompileQueueRequest.flags` | 只接受KNOWN_MASK；Warmup包装按位加预热 |
| `SZrCompileQueueRequest.reserved0` | Queue要求为零 |
| `SZrCompileQueueRequest.reserved1` | Queue要求为零 |
| `SZrCompileQueueRequest.reserved2` | Queue要求为零 |
| `SZrCompileQueueRequest.reserved3` | Queue要求为零 |

### 公开枚举成员

| 成员 | 当前语义 |
|---|---|
| `ZR_ASYNC_FRAME_DIAGNOSTIC_NONE` | 零初始化或 clear 后的无错误分类，并由 DiagnosticName 映射为 none；成功场景可检查该值，不是拒绝 helper 发出的错误；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SCHEMA` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_FLAGS` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_BORROW_ACROSS_SUSPEND` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_STACK_ALIAS_ACROSS_SUSPEND` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_LOCK_GUARD_ACROSS_SUSPEND` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_NATIVE_CRITICAL` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_STATE_MAP_REQUIRED` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_BUDGET_EXHAUSTED` | 当前仅由 DiagnosticName 映射静态名称 budget-exhausted；没有实际拒绝发码路径，预算耗尽通过 pendingFlags/PollOutcome/暂停原因处理；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_PIN_UNDERFLOW` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_CAPACITY` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_ALREADY_RESUMED` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CAPACITY` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SNAPSHOT` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CANCELLED` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_STALE_GENERATION` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_OUT_OF_MEMORY` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_OVERFLOW` | 由具体拒绝入口写diagnostic.code，DiagnosticName返回对应静态名称；并非自动异常传播；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_DIAGNOSTIC_COUNT` | 所属enum的边界哨兵，用于范围校验或名称unknown分支；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_IDLE` | Init后的可Begin状态；Validate本身不开始执行；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_RUNNING` | Poll/Complete允许的运行状态；Teardown拒绝；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_SUSPENDED` | 可Resume，也允许Complete；Poll直接报告暂停且不计工作量；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_RESUMING` | Resume正常路径的瞬时赋值，立即RUNNING；Teardown拒绝；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_COMPLETED` | 成功Complete的终态，RequestCancel/Fault拒绝；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_CANCELLED` | safe-boundary Poll或已取消Resume置此态；仍须Teardown；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_FAULTED` | 显式Fault或预算暂停违反资源契约后置此态；仍须Teardown；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_TORN_DOWN` | Teardown后的标量终态，可重复Teardown；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_STATUS_COUNT` | 所属enum的边界哨兵，用于范围校验或名称unknown分支；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_SUSPEND_NONE` | 非暂停默认原因；取消Resume不会自动清旧原因；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_SUSPEND_WAIT` | 显式Suspend原因，未自动调用AsyncWait注册；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_SUSPEND_BUDGET` | 预算耗尽safe-boundary暂停原因；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_SUSPEND_COMPILE` | 显式Suspend允许的原因，未自动调用CompileQueue；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_SUSPEND_COUNT` | 所属enum的边界哨兵，用于范围校验或名称unknown分支；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_POLL_RUNNING` | 本次继续运行，包括nativecritical和未达边界；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_POLL_SUSPENDED` | 已暂停或本次预算暂停，不代表wait注册已完成；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_POLL_CANCELLED` | 本次安全边界置帧CANCELLED；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_POLL_COMPLETED` | 当前Poll未返回该值，保留ABI类别待适配器核查；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_FRAME_POLL_FAULTED` | 本次不能继续；Validate/invalid-status路径不一定改frame.status；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_WAIT_STATE_FREE` | 可领取槽位，身份字段已清或尚初始化；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_WAIT_STATE_REGISTERING` | 身份已发布但注册后的CAS尚未决定WAITING/READY；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_WAIT_STATE_WAITING` | 等待Wake/Cancel/Timeout/Recheck争获胜终态；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_WAIT_STATE_READY` | 就绪获胜，须Resume后才可Release；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_WAIT_STATE_CANCELLED` | 取消获胜，须Resume后才可Release；不自动取消frame/compile；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_WAIT_STATE_TIMED_OUT` | 显式超时获胜，须Resume后才可Release；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_WAIT_STATE_RESUMED` | 获胜恢复已执行一次，可Release；不执行用户回调；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_ASYNC_WAIT_STATE_COUNT` | 所属enum的边界哨兵，用于范围校验或名称unknown分支；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_COMPILE_JOB_FREE` | 可领取记录；jobId零或无效handle查询也返回此值；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_COMPILE_JOB_QUEUED` | 已复制快照尚未worker认领；Cancel立即终态；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_COMPILE_JOB_RUNNING` | worker借用快照期间；Cancel仅置请求，不允许Release；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_COMPILE_JOB_COMPLETED` | 成功保存resultHash，仍占槽且须Release；未安装实际代码；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_COMPILE_JOB_CANCELLED` | 未运行作业取消的终态，可Release；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_COMPILE_JOB_DISCARDED` | worker确认取消或契约失配的终态，可Release；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |
| `ZR_COMPILE_JOB_COUNT` | 所属enum的边界哨兵，用于范围校验或名称unknown分支；与所属enum及实际转移/测试使用绑定；未发现消费者者只保留ABI，不推断可达 |

私有 COMPILE_STAGE_VALIDATE/QUEUE/CLAIM/COMPLETE 只标记队列诊断阶段；不是作业状态或等待 C 的 stage。

## Historical test coverage

The focused test exercises budget exhaustion before and at a coherent boundary,
borrow rejection, pin balancing, cancellation and idempotent teardown, wake
between registration and recheck, cancel/timeout winner races, exactly-once
resume, immutable snapshot copying, stale-generation disposal, warm-up tagging,
and malformed snapshot rejection.  Exact commands and tool versions are
recorded in `tests/acceptance/ssa-async-frame-budget.md`.

`ssa_async_compile_contract_cases.inc` adds four completion failures whose
generation still matches: changed module, signature, or layout hash, and a zero
result hash. Each must report `CONTRACT_MISMATCH`, leave the record `DISCARDED`
with no published result hash, and retain its snapshot until `Release`. The
test uses one slot for all four failures and then a valid completion. After
each release it checks zero active jobs and a cleared snapshot, length, and
result hash; the final completion checks the exact published hash. These
checks cover contract rejection and slot recovery without changing the
production completion protocol. Validation evidence is recorded in
`tests/acceptance/2026-10-02-ssa-async-compile-contracts.md`.

`ssa_async_stale_handle_cases.inc` copies a handle, releases its original
record, and reuses the only slot at the same generation. The old wait token
must not wake, cancel, time out, recheck, resume, or release the new wait. The
old compile job ID must not retrieve the new snapshot, cancel the new job,
publish a result through it, or release it. The tests exercise stale resume
against a `READY` wait and stale release against `RESUMED`/`COMPLETED` records,
where the current handle is allowed to perform those operations. Exact
`WAIT_NOT_FOUND` diagnostics and the new record's state, identity, resume
count, snapshot ownership, cancellation flag, result hash, and active count
make accidental admission observable. Current handles then finish and release
normally. These are sequential slot-reuse checks with the registry and queue
kept initialized; they do not extend the existing quiescent-release contract
or validate concurrent release/reuse or deinitialization/reinitialization.
Validation evidence is in
`tests/acceptance/2026-10-02-ssa-async-stale-handles.md`.

The harness uses an always-evaluated `TEST_CHECK` instead of the C `assert`
macro, so state-changing expressions remain active when `NDEBUG` is defined.
Wait-registry and compile-queue initialization results are recorded before
checks; active handles and the claimed worker snapshot lease are tracked. Each
cleanup path releases a live handle and only deinitializes an initialized,
quiescent registry or queue. A strict standalone GCC run with `-DNDEBUG`
exercises the same 11 test functions; its canary and build evidence are in
`tests/acceptance/2026-09-29-ssa-async-frame-budget-ndebug-tests.md`. This is
focused harness evidence, not a full CMake Release configuration. The final
MSVC Debug executable also passed its 11 source-level test functions, and the
registered `ssa_async_frame_budget` CTest passed 1/1; the acceptance record
contains the exact commands and platform boundaries.

## Out of scope

This contract does not allocate or move a real VM stack frame, implement a
platform event loop, interrupt a native callback, compile ExecIR itself, or
publish machine code.  Those actions remain in the concrete task runtime and
backend services; they must consume the scalar state and diagnostics defined
here.
