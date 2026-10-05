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
also rooted on the task header until released (owned results release their
header handle when transferred by Await; plain results retain it until Free).
This retains the object without requiring a promoted frame. Root retention
and refreshing a copied raw pointer after actual movement are distinct facts;
the fixture limitations below do not establish the latter for header results.

## Completion, Fault, And Await

Plain results may be observed by more than one await. An ownership-bearing
result transfers from the task exactly once; later awaits return
`RESULT_CONSUMED`. A layout may also provide one `finally` callback. It runs
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
`Job`/`Scheduler`, schedule worker work, add artifact/AOT frame rows, project
debug or LSP state, or migrate the legacy `TaskRunner` surface. Those are
separate plan milestones. In particular, this module is not evidence that the
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
