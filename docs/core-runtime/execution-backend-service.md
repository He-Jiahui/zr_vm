---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/execution_backend.h
  - zr_vm_core/src/zr_vm_core/execution/execution_backend.c
  - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c
plan_sources:
  - docs/plans/ssa/10-jit-platforms/01-backend-service.md
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - "user: 2026-09-14 SSA 10.01 backend service contract implementation"
tests:
  - tests/core/test_ssa_backend_service.c
  - tests/acceptance/ssa-backend-service.md
doc_type: module-detail
---

# Execution Backend Service

## Purpose

The execution backend service is the core-owned runtime boundary for optional
ExecBC, AOT, and host-JIT implementations.  It keeps the frame thread
non-blocking: `CompileAsync` copies a scalar request into a caller-provided
job slot and returns `PENDING`; a worker or scheduler later advances the job
with `ProcessNext` and `Complete`.  Core never includes a compiler object or
an LLVM/JIT type, and no executable address is placed in a persistent
contract, artifact, call-binding row, or generation key.

The service is intentionally instance based.  Registration, queue, and code
arrays are supplied by the owner, so capacity and partial-initialization
behaviour are deterministic and do not require a hidden allocator on the
frame path.  The short `ZrCore_ExecutionBackend_*` functions are a process
local facade over an explicitly selected default service.

## Related files and ownership

`execution_backend.h` owns the C ABI: target descriptors, operation/map
capabilities, immutable compile witnesses, ticket/code states, diagnostics,
leases, and lifecycle APIs.  Its callback pointers and service pointers are
runtime-only.  `execution_backend.c` owns registration and queue transitions.
`execution_code_handle.c` owns code records, map/entry queries, lease balance,
and retirement.  `execution_contract.h` supplies the stable ABI/layout/
signature/module/effect fields; `exec_ir_state_map.h` supplies the opaque
interpreter-resume request consumed by the resume callback.  Parser and
backend implementations may depend on this C boundary, but core does not
reverse-depend on parser AST storage.

## Behavior model

### Namespaced generation witness

`SZrExecutionGenerationKey` contains domain identity, module identity, logical
generation, and backend-registration identity.  A key is valid only when all
four scalar identities are non-zero.  Every comparison is an exact four-field
comparison; invalidating generation `N` in one domain or module therefore
cannot retire a code record with the same numeric generation elsewhere.

The compile request repeats the execution contract and carries immutable IR and
compile-input hashes, source/instruction IDs, required operation bits, and
required map bits.  The service copies the request before releasing its lock.
The selected registration identity is inserted into that copy and into the
ticket.  A completion is installable only when generation, contract, hashes,
code identity, code size, and required map registrations all match.

### Job transitions

The normal transition is:

```text
FREE -> QUEUED -> COMPILING -> READY -> PUBLISHED
                  |             |
                  +-> FAILED    +-> CANCELLED
```

`CompileAsync` performs target/capability selection and invokes only
`queryTarget` after queueing; it never invokes `compileAsync` synchronously.
The service pins the registration while that capability callback is in flight,
so shutdown/finalization cannot destroy its user data underneath a concurrent
selection call.
`ProcessNext` changes one queued job to `COMPILING`, invokes the backend
callback without the service lock, and accepts either `PENDING` or a completed
code value.  `Complete` is single-install: a cancelled, stale, duplicate, or
contract-mismatched result is sent through `unregisterMaps` then `retire` and
is never put in the code table.  Failed/cancelled job slots are reusable while
published jobs remain pinned to their code record until reclamation.

Unsupported target or operation selection is observable.  If the request
allows it, the service returns an explicit `FALLBACK_EXECBC` or
`FALLBACK_AOT` status and records the selected mode in the output ticket;
otherwise it returns the backend-unavailable/unsupported reason.  No static
string lookup or exception path is used to choose a backend.

### Code, maps, and leases

`Complete` creates a `READY` code record containing only scalar metadata and
marks the requested roots/EH/debug/deopt/import map bits registered.  `Publish`
is a separate operation.  Publishing a newer code record for the same exact
target token and namespaced generation retires the prior record, but an active
lease keeps it executable.  `AcquireCode` is the only route to an entry
address; it returns an opaque `SZrExecutionCodeHandle` and increments the
execution lease.  `AcquireDependencyLease` separately protects map/import/
deoptimization dependencies.  `ReleaseCode` refuses to drop an execution
lease while any dependency lease on that code record remains, including a
dependency held by another handle; ownership imbalance remains explicit.

`LookupEntry` and `QueryMap` copy the code metadata while locked, then invoke
the backend vtable without the lock.  If a backend has no map query callback,
the scalar map hash in the code record is returned.  `CollectRetired` marks a
zero-lease record `RECLAIMING`, unregisters maps first, retires code second,
and only then clears the record and its job slot.  A callback failure leaves a
`RETIRE_FAILED` record for an explicit retry; it is never silently freed.

### Invalidation, interpreter recovery, and shutdown

`InvalidateGeneration` marks queued/ready jobs cancelled and ready/published
code retired for one exact key.  Compiling jobs receive `cancelCompile` after
the lock is released.  A worker completion racing with invalidation observes
the cancellation bit and disposes its unpublished result.  Existing leases
are allowed to finish; reclamation waits for both lease counters to reach
zero.  The service also keeps a bounded (16-entry) runtime tombstone table;
subsequent requests for an invalidated key are rejected as
`STALE_GENERATION`, even after the old job and code records have been
reclaimed.  Re-invalidating an existing tombstone is idempotent; exhausting
the table returns an explicit `CAPACITY` status.

`ResumeInterpreter` forwards an `SZrExecIrResumeRequest` and structured
`SZrExecIrDiagnostic` to the configured interpreter callback without holding
the service lock.  Missing callbacks return an explicit unsupported
diagnostic, and callback failures preserve the callback's source/state data.

`Shutdown` prevents new registration/compile/publish work, cancels queued
jobs, requests cancellation of in-flight jobs, and retires ready/published
code.  `IN_FLIGHT` covers remaining compiling jobs, target queries and
backend callbacks counted by the service; it is broader than a compile callback.
`FinalizeShutdown` first verifies that all in-flight jobs and code leases are
gone, then collects retired code, destroys backend registrations, clears job
slots, and marks the service destroyed.  Registrations stay alive while a
retired code record still has a lease so a later release can reach the owning
backend.  `Deinit` only invalidates the caller-owned service shell after
successful finalization.

## Design rationale and constraints

The fixed-capacity arrays avoid hidden allocation and make OOM/capacity
failures testable, while the immutable scalar snapshot prevents a compiler
worker from observing moving VM objects.  Callback pointers are copied under
the lock and called outside it; this both prevents lock inversion and allows a
backend callback to query or report service state.  The two lease counters
express distinct ownership obligations instead of treating an executable
address as permanently valid.  AOT/ExecBC fallback remains available when a
host JIT is absent, so the optional backend cannot block the release path.

This module does not create worker threads, executable memory, unwind tables,
or parser IR.  Those facilities belong to a registered backend.  It also does
not serialize callback pointers, service addresses, or code handles.  The
caller must keep the descriptor user data and the three backing arrays alive
until shutdown/finalization completes.

## Test coverage

`tests/core/test_ssa_backend_service.c` is a C-only mock-backend test.  It
covers queued-versus-worker execution, synchronous and pending compilation,
source/instruction diagnostics for immutable-input rejection, explicit
ExecBC fallback, generation-scoped invalidation, stale completion disposal,
duplicate code identity disposal, map and entry queries, dependency lease
balance, active-lease-delayed reclamation, cancellation during shutdown,
interpreter resume, and backend destroy ordering.  Assertions check the
unregister-before-retire event sequence and the exact source/instruction IDs
on a failure path.

The acceptance record in `tests/acceptance/ssa-backend-service.md` records
historical direct GCC/Clang and sanitizer commands. Current
`tests/cmake/ssa-tests.cmake` registers the focused source, both service
translation units and the `ssa_backend_service` CTest entry. This comment
integration adds no native/build/link/runtime/CTest execution evidence.

## Open issues / follow-up

The service currently uses a small spin lock suitable for the short metadata
critical sections; a host scheduler may wrap `ProcessNext` in its own queue.
Concrete AOT, ExecBC, and host-JIT adapters remain separate tasks and must
supply their own executable/map ownership proofs. Current build registration
is source linkage evidence; it does not prove a completed compiler matrix or
execution of a backend entry.

## 当前 code-handle 生命周期与失败后状态

`AcquireCode` 先清空新输出槽，仅从 PUBLISHED 作业与记录加一份执行 lease；
复制 handle 字节不会再加引用。owner 需协调同一 handle 的查询和释放，并让 lease
覆盖入口使用期。`QueryCode` 只复制标量状态；入口与 map 查询的 callback 计数保护
descriptor/userData，不是额外 code lease。失败回调可能留下输出值，失败输出不可使用。
`QueryMap` 不自动加 dependency lease；没有 queryMap 回调时只返回已登记 bit 的标量 hash。

有记录的回收先认领 RECLAIMING，撤图失败不调用 retire，保留 RETIRE_FAILED 与
mapsRegistered 以供重试；撤图成功而 retire 失败，重试不再撤图。全部清理成功才归还
code/job 槽。`CollectRetired` 遇硬失败保留已成功回收的数量，不保证整轮回滚。
未入表产物的清理不同：撤图失败仍尝试 retire，任一失败报告 RETIRE_FAILED，
没有 code record 保存自动重试状态。固定容量拒收不是 allocator OOM 回滚证明。

TODO：`Complete` 把作业置 FAILED/CANCELLED 并解锁后，到
`dispose_unpublished` 登记 callback 计数之间，复制的 descriptor/userData 存活窗口
仍须沿 `Unregister`/`FinalizeShutdown` 的实际并发 owner 契约核查；已登记计数的保护
不能证明此前窗口。本次没有合法并发运行或 BUG 证明。

mock descriptor 将 lookupEntry/queryMap/unregisterMaps/retire 注册到 service；
service 复制 descriptor 后锁外派发。mock 只产生哨兵地址、hash 和事件。当前生产
`FinalizeShutdown` 确实调用 CollectRetired，然后才按 codeCount 门禁决定 backend
destroy；不能将另一 `SZrHostJitCodeHandle`/manager 家族作为此 service 句柄的消费者。
公共 C ABI 的仓外 adapter 未知，仍需在其真实 Register/vtable 与 entry 使用、卸载入口
验证执行和依赖 lease 归还责任。默认 facade 只是借用 service，不隐藏句柄 ownerref。

### 当前有限证据

消费与释放：`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:229`、`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:357`、`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:369`。入口与 map 派发：`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:476`、`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:569`。回收状态：`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:653`、`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:691`。

未入表产物清理：`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:94`、`zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:105`；Complete 窗口：`zr_vm_core/src/zr_vm_core/execution/execution_backend.c:929`、`zr_vm_core/src/zr_vm_core/execution/execution_backend.c:936`、`zr_vm_core/src/zr_vm_core/execution/execution_backend.c:940`。mock 注册：`tests/core/test_ssa_backend_service.c:229`、`tests/core/test_ssa_backend_service.c:232`；service descriptor 复制：`zr_vm_core/src/zr_vm_core/execution/execution_backend.c:469`；生产回收调用：`zr_vm_core/src/zr_vm_core/execution/execution_backend.c:1491`；当前 CTest 登记：`tests/cmake/ssa-tests.cmake:977`、`tests/cmake/ssa-tests.cmake:984`。
