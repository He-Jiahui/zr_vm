---
related_code:
  - zr_vm_core/include/zr_vm_core/task_frame_runtime.h
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/include/zr_vm_core/value.h
  - tests/task/test_task_frame_runtime.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/task_frame_runtime.h
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
plan_sources:
  - user: 2026-07-25 execute Syntax 12 milestones and record each result
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
  - docs/plans/syntax/12-async-task-job-scheduler/m2-task-frame-runtime-implementation-plan.md
  - docs/plans/ssa/06-gc-domain/05-async-frame-budget.md
tests:
  - tests/task/test_task_frame_runtime.c
  - tests/acceptance/2026-07-25-syntax-12-m2-task-frame-runtime.md
  - tests/acceptance/2026-09-29-ssa-async-frame-gc-root-drop.md
doc_type: module-detail
---

# Task Frame Runtime

## Purpose

Syntax 12 M2 provides the core runtime state machine behind a future
`Task<T>` lowering. It is deliberately a typed runtime primitive: the task
and each suspended slot carry explicit state, GC-root, and cleanup facts.
It does not infer coroutine state from `zr.task` dynamic-object fields,
function names, or a legacy `TaskRunner` protocol.

## State And Poll Contract

`ZrCore_TaskFrameTask_Start` enters `RUNNING` and invokes a caller-supplied
poll function. The poll function either completes, faults, or calls
`ZrCore_TaskFrameTask_Suspend` with a state id from its declared layout.
Only the suspend transition obtains a frame from `SZrCoreTaskFramePool` and
changes the task to `SUSPENDED`; therefore a synchronously completed task has
zero frame allocations.

`Resume` moves a suspended task back to `RUNNING` and invokes the same poll
function. The saved state id is stable across multiple pending polls. The
runtime operations correspond to the plan's runtime half of `AsyncEnter`,
`AwaitPoll`, `Suspend`, `Resume`, `AsyncComplete`, and `AsyncFault`; source
and semantic-IR emission remain separate compiler work.

## Frame Layout, GC, And Cleanup

`SZrCoreTaskFrameLayout` declares the valid state-id range and one descriptor
per hoisted slot. Each descriptor records whether the slot is a GC root and
whether it requires a drop callback. Storing a slot replaces any prior
initialized value through one cleanup path, so overwrite, fault, terminal
free, and pool reuse cannot skip a registered drop.

If registering the GC root for a newly copied slot fails, the runtime rolls
that initialized slot back through the same cleanup path. Its registered drop
therefore observes the copied value before ownership is released, and the
cleared initialized bit prevents later task or pool cleanup from running the
drop again.

GC-rooted slots receive a `SZrGcRootHandle`. A load resolves that handle before
copying the value, which preserves a compacted object identity without
retaining a stale raw pointer. Completed task results and fault values are
also rooted on the task header until released. Await detaches the header
handle for every non-NONE ownership kind, including a copied BORROWED value;
NONE results retain it until Free.
This retains the object without requiring a promoted frame. Root retention
and refreshing a copied raw pointer after actual movement are distinct facts;
the fixture limitations below do not establish the latter for header results.

## Completion, Fault, And Await

NONE results may be observed by more than one await. Materialization transfers
and clears the source only for UNIQUE, LOANED, SHARED, and WEAK. BORROWED
results are copied without clearing the source; their outputs remain borrowed.
Await still detaches the header root and sets consumed for every non-NONE kind,
including BORROWED, so later awaits return `RESULT_CONSUMED`.
A layout may also provide one `finally` callback. It runs
before the frame's initialized slots are released on complete, fault, or an
early task free, and the task records that it has run so later cleanup cannot
invoke it a second time. Faulting then returns a leased frame to the pool and
exposes a rooted error value. These paths use ordinary `SZrTypeValue` ownership
operations rather than a second async-specific value model.

## Pooling Boundary

`SZrCoreTaskFramePool` stores only clean, idle frames. A suspended task leases
a frame whose capacity matches its layout; completion, fault, and task free
return it after releasing initialized slots and roots. Reacquisition clears
state and initialization bits, which prevents a later task from seeing a
prior task's live-slot or cleanup state.

## Scope Boundary

This M2 runtime foundation does not itself lower source `await`, define
`Job`/`Scheduler`, schedule worker work, add artifact/AOT frame rows, or
migrate the legacy `TaskRunner` surface. Those are separate plan milestones.
The existing `ProjectDebugTerminal` adapter only reads task status/provenance
and delegates terminal-event projection; it does not drive poll or consume
results. Broader scheduler debug and LSP integration remain separate work. In particular, this module is not evidence that the
old dynamic `TaskRunner` model has become the canonical `Task<T>` runtime.

## Test Coverage

`test_task_frame_runtime.c` covers synchronous no-allocation completion,
multiple pending/resume states, one-shot finally-before-cleanup on fault,
initialized-only fault cleanup including slot overwrite, nonnull GC slot
loads and completed-result domain membership after a requested full GC, typed
pool reuse, and
exactly-once transfer of a non-Copy result. An injected root-table growth
failure also checks that the copied GC slot receives one drop and that repeated
task/pool cleanup does not call it again. The 2026-09-29 acceptance record
reports a historical MSVC Debug snapshot passing all seven tests; it also
records the unregistered CTest target and absence of an `NDEBUG` build.
That historical result is not a new execution of this comment-review snapshot.

## 当前 fixture 的静态审查范围

`tests/task/test_task_frame_runtime.c` 的 Unity main 注册七例；默认 runner
逐例调用 setUp、用例函数、tearDown。每例直接调用 Start/Resume，poll
由 runtime 同步派发；没有注册调度器 wake/cancel。layout、pool 和回调
userData 从用例栈借用，正常路径先 Task_Free，再 Pool_Free，最后 Unity
销毁 VM。首次 Suspend 才获取宿主 calloc 帧，后续 Resume 保留同一帧。

drop 在释放已初始化 slot 的值和根之前执行；finally 在终态 slot 清理之前
运行，所以其 LoadSlot 可观察 slot 0。根表注入拒绝新的非零 ARRAY 请求，
不是按根表类型筛选所有分配；当前序列在填满表后由 StoreSlot 注册根触发。
StoreSlot 先复制并设置 initialized，再尝试 root；失败用正常 cleanup_slot
回滚，drop 观察复制值，后续重复 Free 不再 drop。普通对象由句柄保活，
LoadSlot 解析 root 更新 slot 地址；外部借用裸指针不会因此自动更新。
unique 结果从 poll 物化到 header，再经第一次 Await 转移给用例，需显式
ReleaseValue；第二次 Await 只断言已消费。

**BUG（完整静态可达链，无动态复现）**：pending 用例第一次 Start 暂停，
随后合法 Resume 第二次派发 `task_frame_multi_suspend`。该次自动对象
`value` 未初始化就传给 LoadSlot，后者经 Value_Copy 的覆盖准备读取目标
ownershipKind（并可能读控制指针）。前一次 poll 的 InitAsInt 不初始化新一次
调用的自动对象；源码仅标记问题，本轮不增加 Reset 或改变行为。

**TODO**：Unity 断言长跳转会跳过局部 Task/Pool_Free；tearDown 只销毁 VM，
宿主 calloc 帧的失败收尾需另核。完成 GC 结果用例只检查非空与域归属；没有
证明对象实际移动、完整内容有效，或 Await 对 header.result 的移动地址更新。
暂停 slot 用例也未强制或量化实际移动。相关 root 保活机制不能扩大为移动后
所有对象内容或失败场景已验证。

本轮完整 fixture 注释与台账为静态审查，未构建、未运行、没有新的 runtime、
CTest 或平台配置通过信用；历史接受记录与本轮静态问题分别保留。

## 当前生产 ABI 的静态审查边界

仓内公开驱动入口是 task-frame 与 debug 测试；生产 debug 投影器消费状态枚举。
guest task/job 实现未见直接调用此 frame ABI；未来 lowering、调度器与仓外宿主的
串行驱动、callback异常退出和 VM 销毁前释放约定尚待在真实接入点核查。
此边界不把测试注册当作生产 scheduler/wake/cancel 集成证据。

LoadSlot 的 outValue 必须先初始化：Value_Copy 覆盖准备会读取旧 ownership 元数据。
它先 Resolve rooted slot 的当前地址，再复制输出；root 保活不为输出新增独立句柄。
普通 GC result 输出及仍借用的 BORROWED 输出跨 GC/task Free 必须遵守各自值/根生命周期。
StoreSlot 先清旧值，再复制并发布 initialized；根创建失败仍走正常 drop 回滚，旧值不会恢复。
direct unique 的内部镜像 Copy 约束不能扩大为任意可独立释放的 spill owner。

**TODO**：现有 drop 仅计数/比较地址，finally 只有限 LoadSlot；核查外部 callback 的
GC、异常与同 slot/task 清理重入。finallyRan 只阻止重复派发 finally；cleanup_slot
在 drop 返回后才撤 initialized，也未在 drop 前 Resolve 移动地址。
poll 输出每次先 Reset，但非 COMPLETE 输出和异常路径的清理约定待真实 callback 接入核查。

**TODO**：完成结果 root 失败释放 result，却尚未 finally/归还frame/发布 COMPLETED；
Start/Resume 可返回 false 并保留 RUNNING。fault error root 失败已清 frame/finally，
却尚未发布 FAULTED/provenance。核查调用方失败后 Free、状态投影及预期终态责任。
Await 读取 header.result/error 没有 Resolve 根；现有请求 full GC 后的非空/域戳断言
不证明实际移动后的 header 地址或完整内容更新，需在真实移动入口补充强内容证据。

上述为完整生产 H/C 注释和台账的静态审查；BORROWED 分流依据当前 materialization/Copy
实际分支，未增加运行探针。本批不授予新的构建、GC、CTest、MSVC或runtime信用。
