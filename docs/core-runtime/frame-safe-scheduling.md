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
