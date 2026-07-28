---
related_code:
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_isolated_domain.c
  - tests/thread/test_thread_runtime.c
doc_type: module-detail
---

# zr.thread Built-in Runtime

## Scope

`zr.thread` provides isolate-first worker scheduling and transport. Ordinary
isolate heap objects never cross a worker boundary directly; data crosses only
through canonical transport materialization and the `Send`/`Sync` contract.

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

`ThreadScheduler` resolves scheduler policy from canonical provider facts and
returns the caller-domain completion task. Its worker path materializes Send
captures and results before they cross the boundary.

## Removed Compatibility Surface

`Thread`, `Scheduler`, `spawnThread()`, `getCurrentThreadScheduler()`, and
`start`/`pump`/`step` methods are not registered. They are not aliases for
`ThreadScheduler.schedule(Job)`.

`%mutex`, `%atomic`, `AtomicBool`, `AtomicInt`, and `AtomicUInt` remain
rejected. `Lock<T>` and `SharedLock<T>` are affine, cannot cross `await`, and
do not implement `Send` or `Sync`.

## Provider Guarantees

The attached-domain and isolated-domain implementations preserve M4/M5
transport behavior: completion is delivered exactly once, quota and shutdown
faults settle the caller task, and non-Send/non-Sync payloads are rejected by
the resolved generic contract. No provider recreates a `TaskRunner` or uses a
member-name fallback to choose a scheduler route.

An isolated-domain launch remains worker-owned until the caller publishes its
completion acknowledgement. Provider worker-slot accounting is released before
that publication; signaling `completionProcessed` is the final caller-side use
of the launch object. The waiting worker may free the launch immediately after
it wakes, so completion processing must not read the launch, its runtime pointer,
or its synchronization fields after the signal/unlock boundary.
