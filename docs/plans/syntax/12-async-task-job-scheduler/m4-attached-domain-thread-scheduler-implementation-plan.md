# Syntax 12 M4: AttachedDomain ThreadScheduler Plan

## Goal

Add the canonical `zr.thread.ThreadScheduler`, `Send`, and `Sync` contract
without creating a second Task/Job/Scheduler type family. A ThreadScheduler
consumes the existing `zr.task.Job<T>` at the same resolved Scheduler schedule
role and completes the caller-domain `zr.task.Task<T>` through the Task ABI.

## Initial Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| protocol contract | `zr_vm_core/include/zr_vm_core/object.h`, `zr_vm_common/include/zr_vm_common/zr_contract_conf.h` | Allocate ThreadScheduler/Send/Sync protocol ids and the scheduler handoff role without short-name tests. |
| thread descriptor/runtime | `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c`, `runtime_workers.c`, `runtime_internal.h`, `zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h` | Publish the canonical descriptors and attach worker mutators to the caller GcDomain for the AttachedDomain provider path. |
| Task bridge | `zr_vm_library/include/zr_vm_library/task_runtime.h`, `zr_vm_library/src/zr_vm_library/task_runtime.c` | Expose a narrow structured Job-to-Scheduler handoff used by providers; retain M3's one-time Job consumption semantics. |
| static facts | `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c`, `type_inference_call_semantic_facts.c`, `dataflow_ownership_moves.c` | Reuse protocol and contract-role facts for the ThreadScheduler call; no owner or member-name fallback. |
| tests | `tests/thread/test_thread_runtime.c`, new focused scheduler test, `tests/CMakeLists.txt` | Descriptor, same-domain attach, Send/Sync negative cases, exactly-once Job/Drop and caller Task affinity. |
| docs | `docs/library-and-builtins/zr-thread-scheduler.md`, this plan, M4 record, acceptance record | Public contract, legacy boundary, and validation evidence. |

## Execution Order

1. RED: establish descriptor tests for `ThreadScheduler`/`Send`/`Sync` and
   prove that legacy `Thread.start(TaskRunner)` is not evidence for the new
   Job/Scheduler surface.
2. Green: introduce a narrow Task runtime provider bridge that consumes the
   canonical Job exactly once, attaches worker execution to the caller domain,
   and publishes completion with the existing Task completion identity.
3. Static closure: allow only resolved Scheduler schedule facts to move Job
   argument zero. Diagnose non-Send moves and non-Sync shared captures from
   canonical capability facts.
4. Acceptance: replay focused GCC/Clang/MSVC targets and the provider/CLI
   smoke on an isolated `HEAD + M4 exact overlay` snapshot, update this record,
   then exact-path commit through a private Git index.

## Explicit Exclusions

- IsolatedDomain serialization, provider quota, resource prepare/commit/abort,
  result envelopes, work stealing, and domain shutdown belong to M5.
- Artifact/debug/LSP projection and removal of legacy `Thread`, `TaskRunner`,
  `spawnThread`, `pump`, and `autoCoroutine` surfaces belong to M6.
- M4 does not infer Send/Sync from source spelling, a shared heap, or a native
  wrapper name. It consumes only canonical closed capability facts.

## Implementation Outcome

Completed on 2026-07-25. `ThreadScheduler` now constructs an AttachedDomain
runtime with a positive worker limit and registers the Task-runtime provider
await hook. A request owns a prepared canonical Job/Task handoff until the
worker executes or faults it; queue publication and completion notification
are protected by the runtime mutex and condition.

Workers are created from the caller global state and enter through
`ZrCore_State_MutatorLaunch`, so their `gcDomain` is the caller domain rather
than a new isolated global state. Empty-queue worker retirement clears the
live-worker slot while holding the queue mutex, allowing a racing submitter to
start a replacement. Worker-creation failure faults and releases a request
that has not been claimed.

Static constraint handling resolves `Send` and `Sync` through the imported
protocol mask (`THREAD_SEND` / `THREAD_SYNC`). The resolved Task Scheduler
schedule role, rather than a member spelling, is the only ownership move
trigger for argument zero.
