---
related_code:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/05-async-frame-budget.md
  - user: 2026-08-05 完成 Syntax 10C official provider convergence
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
tests:
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
  - tests/acceptance/2026-10-02-task-scheduler-queue-reuse.md
  - tests/library/test_official_provider_convergence.c
  - tests/acceptance/2026-08-05-syntax-10c-official-provider-convergence.md
doc_type: module-detail
---

# `zr.task` Job and Scheduler Contract

## Scope

Syntax 12 M6.2 exposes the cold `Job<T>` handoff and cooperative `Scheduler`
surface as the sole `zr.task` scheduling contract. The descriptor module
version is `3.0.0`. It does not expose `TaskRunner`, coroutine scheduler
names, worker threads, or script-controlled pumping.

The product descriptor is registered by
`ZrCore_TaskRuntime_RegisterBuiltins`, declares Runtime phase, and publishes
`zr.task:v3:task-job-scheduler` as its public contract hash. The excluded
`zr_vm_lib_task` directory is not part of the root product graph and is not a
second accepted provider.

## Public Contract

- `zr.task.Job<T>` implements the Task Job protocol and is a non-Copy,
  single-consumption value.
- `init Job<T>(callable)` stores a cold callable. It does not queue or run
  work by itself.
- `zr.task.Scheduler` implements the Task Scheduler protocol.
- `Scheduler.schedule(Job<T>)` consumes its first argument and returns a
  `zr.task.Task<T>` completion handle. Reusing the submitted Job is a source
  ownership error.
- `zr.task.currentScheduler` resolves to the local cooperative scheduler.
- `zr.task.yieldNow()` and `zr.task.delay(...)` return completion Tasks via
  the same Task ABI. M3 establishes the call contract only; a public Duration
  value provider belongs to a later scheduler/provider milestone.

At runtime, preparation marks a Job consumed before task-handle allocation
and clears its callable on the following success and failure paths. These
writes use the historical void field setter. Pin or key-allocation failure can
leave a write unconfirmed while an upper layer still reports success, so the
source single-consumption contract is not a universal allocation-failure
guarantee. The checked queue transition below does not repair those writes.

## Private queue exhaustion and reuse

The cooperative scheduler keeps an array and an integer cursor in existing
private fields. A step advances the cursor before calling a Job, so a Job may
enqueue more work while the outer pump is active. Cooperative Tasks may append
themselves for their remaining turns. The pump therefore retains the current
queue until the cursor reaches its end, including work appended during a call.

At exhaustion, the runtime detaches that array by setting the queue field to
null. It leaves the exhausted cursor unchanged. Resetting the cursor while
retaining the array would replay completed Tasks whose callable has already
been cleared, turn their status from completed to faulted, and stop newly
submitted work from running. The next queue creation checks that resetting the
cursor to zero succeeded before allocating and attaching a replacement array.
A rejected detach leaves the old cursor at its exhausted position.

`task_runtime_scheduler_queue.inc` contains this private transition. It scans
existing hash pairs using borrowed string keys without allocating new keys.
Its update helper accepts only normalized values without ownership whose types
are null, array or int64. It calls the core boolean member setter so member
versions, lookup caches and GC barriers follow the ordinary object contract,
then checks the thread status and reads the same pair back. A missing or
malformed field, a rejected reset, or a rejected attach returns no queue.
These checks cover the queue transition; the other historical void field
setters keep their existing scope and are not changed by this repair.

During replacement allocation and attach, a native pin keeps the scheduler
alive and stable. Allocation and attach execute inside `Exception_TryRun`,
which allows the caller to unpin after both normal return and a caught memory
error. Failure removes this replacement if it was partially published; it
does not reset the cursor of a still-attached exhausted queue. Pin ownership
is balanced for the registration and flags added by this call. Replacement
cleanup attempts the checked null write but discards its return value; it does
not guarantee rollback if cleanup itself is rejected.

The focused tests compile real source for completed-result reuse, cooperative
yield, and reentrant Job scheduling. A host callback submits the inner cold Job
through the canonical `ScheduleJob` API while the outer Job is running, checks
that the inner Task remains queued, then verifies its result after the outer
pump returns. Additional private-helper checks use real core objects and
read-only field descriptors to reject reset/detach/attach, and reject both the
queue allocation and its GC retry to exercise caught OOM and pin cleanup.
Every new case releases its roots and destroys the VM before Unity assertions.
The exact RED/GREEN commands, immutable linked inputs, and coverage limits are
recorded in the linked acceptance document. This repair does not complete the
06.05 asynchronous wait, execution budget or background compiler milestones.

## Provider handoff lifetime

A successful `PrepareJob` gives the provider a WorkItem containing a GC root
for the completion Task in the caller domain. Execute, fault and completion
operations settle or inspect that Task without releasing the root. The provider
must call `ReleasePreparedJob` in the same GC domain when the WorkItem is no
longer needed; it must not overwrite a live WorkItem with a new preparation.
Copying its callable does not establish worker-domain ownership or perform
cross-domain transfer.

Await-hook registration stores a borrowed pointer to the registration record;
the record, hook and context must outlive every possible scheduler use. The
registration function currently returns true after a void field write and
cannot confirm publication on the silent failure path. A missing registration
therefore selects the local queue fallback for a pending Task; that fallback does
not dispatch the provider queue. A Task already terminal is read before hook lookup.
Checked registration and failure-path coverage remain required before treating
that return value as proof of a working provider wait path.

## Canonical Metadata and Facts

The native descriptor publishes protocol masks and stable member contract
roles for the Job constructor and `Scheduler.schedule`. Parser semantic
reference facts carry the resolved role and ownership qualifier. A native
member with a non-zero contract role is an exact resolved fact even when it
has no source declaration `SymbolId`.

The ownership move pass consumes an argument only when the resolved call fact
identifies the Scheduler schedule role at argument zero. It does not inspect
the member spelling or synthesize legacy `start`/`pump` methods.

The compiler also rejects a standalone Task expression through the canonical
Task Handle protocol. A Task must be awaited, returned, or stored rather than
silently discarded.

## Migration Boundary

M6.2 removes `%async`, `%await`, `%async T`, `TaskRunner`, `Async`,
`defaultScheduler`, public scheduler pumping, and `zr.coroutine`. Source must
use `async fn ...: Task<T>`, direct `await`, and the resolved
`currentScheduler.schedule(Job<T>)` contract. Thread providers consume the
same Job/Scheduler role and do not recreate the deleted wrappers.

## 当前请求与核查边界

共享 queue helper 被生产 task_runtime.c 和测试 task_scheduler_queue_reuse_cases.inc
实际 include；测试 TU 的同名 static helper 不构成生产函数 caller。head 先推进后调用
Job 的目的允许 callback 追加工作；消费 Job 和创建 WorkItem 根位于参数 guard 之后。
同步 TryRun 回调不保存栈 request，调用者在返回后读取 result/completed 或 attached/queue。

PrepareJob 先消费 callable 再创建 completion Task 和域根；失败没有恢复可再次提交的
Job。ExecutePreparedJob 返回 true 可表示已故障结算；FaultPreparedJob 没有 terminal
guard，而 CompletePreparedJob 拒绝已完成 Task。ReleasePreparedJob 只释放根并清空
工作项，不等待 worker 退出。provider 按一次结算和同域释放安排自己的时序。

现存两个 void setter BUG 的静态前提沿 global.c:374 安装预算 allocator，进入
execution/execution_memory.c:65 上游分配及 :69–70 null 普通返回，再经 GC Ignore ARRAY
扩容失败和 native field pin guard 普通返回闭合；注册 true 和消费写入不等于成功发布。
没有新增运行重现。Duration/ReadInt 参数与 callback 内 moving GC 窗口仍按 runtime
文档的具体 TODO 入口核查，不将 cleanup 测试外推为完整回调 GC 证明。

## 本次静态证据锚点

实际同步调用与请求返回后读取：`zr_vm_library/src/zr_vm_library/task_runtime.c:593`、`zr_vm_library/src/zr_vm_library/task_runtime.c:615`、`zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc:161`。provider 先接管：`zr_vm_library/src/zr_vm_library/task_runtime.c:852`；无 hook 的本地 pumping guard：`zr_vm_library/src/zr_vm_library/task_runtime.c:859`。

消费与域根创建：`zr_vm_library/src/zr_vm_library/task_runtime.c:913`、`zr_vm_library/src/zr_vm_library/task_runtime.c:923`；释放根：`zr_vm_library/src/zr_vm_library/task_runtime.c:972`；结算 guard：`zr_vm_library/src/zr_vm_library/task_runtime.c:1021`。真实共享 helper include：`tests/task/task_scheduler_queue_reuse_cases.inc:11`。

allocator 安装与普通 null 返回：`zr_vm_core/src/zr_vm_core/global.c:374`、`zr_vm_core/src/zr_vm_core/execution/execution_memory.c:65`、`zr_vm_core/src/zr_vm_core/execution/execution_memory.c:70`。pin 失败返回：`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1385`、`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1386`。Duration 核查入口：`zr_vm_library/src/zr_vm_library/task_runtime.c:1219`。
