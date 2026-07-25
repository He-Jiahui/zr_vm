# Syntax 12 M5: IsolatedDomain Transport Plan

## Goal

Make `zr.thread.ThreadScheduler` selectable by the embedding host as an
`IsolatedDomain` provider without creating another source-level Scheduler,
Task, Job, or transport API. The provider must execute the cold Job in a
separate `GcDomain`, return the existing caller-domain `Task<T>`, and use the
canonical ownership transfer envelope for every cross-domain payload.

## Constraints

- The public ZR surface remains `ThreadScheduler.schedule(Job<T>): Task<T>`.
  Policy selection is an embedding/runtime configuration, not a keyword,
  constructor overload, or type-name branch in source code.
- Request and result each own a distinct `SZrOwnershipTransferEnvelope`.
  `Prepare -> Publish -> Claim -> Commit/Abort` is the only cross-domain
  ownership path.
- No worker-domain `SZrObject`, closure, frame, GC root, or native pointer is
  stored in the caller domain. Code is reloaded from the compiled callable
  artifact; values cross only through a `DomainTransferKind` contract.
- The existing envelope's release/acquire state transitions, generation and
  claimant checks, quota accounting, and `DROP_ON_FAILURE` behavior remain
  authoritative. M5 must not recreate any of them in the thread runtime.
- A consumed Job stays consumed if policy selection, encode, enqueue, worker
  construction, decode, result commit, or shutdown fails. The caller receives
  a faulted Task and every request/result envelope reaches one terminal state.
- M4 AttachedDomain remains the same-domain fast path. M5 may share provider
  mechanics but must not use same-domain roots as evidence of an isolated
  transfer.

## Exact Write Set

| Layer | Paths | Responsibility |
|---|---|---|
| host policy | `zr_vm_lib_thread/include/zr_vm_lib_thread/runtime.h`, `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c`, `runtime_internal.h` | Store a host-selected immutable provider policy on each ThreadScheduler instance; expose no source-level control. |
| request/result transport | `zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c`, `runtime_transport.c` | Replace legacy take-before-decode worker payloads with envelope-backed request/result queues, artifact reload, claim epochs, completion delivery, and shutdown draining. |
| task provider bridge | `zr_vm_library/include/zr_vm_library/task_runtime.h`, `zr_vm_library/src/zr_vm_library/task_runtime.c` | Export narrow prepared-Job callable and external completion/fault operations without exposing Job fields to providers. |
| canonical transfer | `zr_vm_core/include/zr_vm_core/ownership_transfer.h`, `zr_vm_core/src/zr_vm_core/ownership_transfer*.c` only if integration exposes a missing lifecycle invariant | Consume the established DomainTransferKind/envelope contract; extend only with test-proven support gaps. |
| tests | `tests/thread/test_thread_runtime.c`, existing `tests/core/test_resource_cross_domain_transfer*.c`, `tests/CMakeLists.txt` only when adding a target | Prove isolated request/result cloning, absence of cross-domain GC edges, quota/decode/commit failures, duplicate claim rejection, source moved, exactly-once drop, and shutdown behavior. |
| docs | `docs/library-and-builtins/zr-thread-scheduler.md`, this plan, `m5-isolated-domain-transport.md`, acceptance record | Document host policy, exact transfer lifecycle, error categories, and toolchain evidence. |

## Dependency Order

1. **Policy and provider state.** Add a host-only policy value with
   `AttachedDomain` as the compatibility-preserving default and
   `IsolatedDomain` as the explicit alternative. Constructor snapshots the
   value so later host changes cannot alter queued work semantics.
2. **Provider bridge RED.** Add lower-layer tests that require an isolated
   provider to obtain a prepared callable and to settle a caller Task from a
   caller-domain completion queue. The bridge must keep only caller roots on
   the caller side.
3. **Request envelope.** Serialize the compiled callable artifact separately
   from capture payloads. For every capture, select the existing canonical
   `DomainTransferKind` contract, prepare and publish an envelope before
   queue visibility, then claim and commit it only in the worker domain.
4. **Result envelope.** The worker prepares a second envelope to the caller
   domain, publishes an immutable completion message, and the caller commits
   it before marking the original Task completed. Worker exceptions and all
   transport errors become structured Task faults.
5. **Failure and shutdown closure.** Cover quota, prepare, decode, commit,
   duplicate claim, stale generation, worker exit, and shutdown. Drain every
   unclaimed request exactly once; do not retry a claimed Job.
6. **Acceptance.** Replay focused tests on an isolated `HEAD + M5 overlay`
   snapshot with GCC, Clang, and MSVC. Record true process exits and any
   pre-existing baseline failures before the exact-path commit.

## Required Test Matrix

- Isolated `ThreadScheduler` executes a zero-capture `Job<int>` and completes
  the caller-domain `Task<int>`.
- A structured-clone capture and result arrive in distinct target-domain
  objects; identity checks prove no worker heap object is exposed to caller.
- Unsupported or forbidden capture, quota exhaustion, target decode failure,
  and result commit failure fault the Task after the source Job is moved.
- Each request/result envelope rejects a second claim; all terminal branches
  report one abort or one commit and release payloads once.
- Scheduler shutdown faults queued requests and rejects later submissions;
  it never leaves a caller Task pending.
- Existing core transfer tests continue to prove ValueCopy, StructuredClone,
  ImmutableHandle, and ResourceMove provider semantics. M5 integration must
  consume those contracts rather than infer behavior from a type name.

## Promotion Gate

M5 is not complete until request and result have independent envelope
snapshots, all cross-domain tests verify target-domain ownership and
exactly-once cleanup, the original source is moved across every provider
failure, and the GCC/Clang/MSVC acceptance evidence records the same M5
results. Work stealing and provider shutdown are part of this gate, not an
optional postscript.
