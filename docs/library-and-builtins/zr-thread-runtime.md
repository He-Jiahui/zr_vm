---
related_code:
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
Shutdown rejects new isolated work and faults queued requests, but does not
join workers. TODO: define a separate host barrier if shutdown must permit
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
