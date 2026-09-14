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
lease while a dependency lease remains, making ownership imbalance explicit.

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
code.  It returns `IN_FLIGHT` only when a compile callback is still active.
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

The acceptance record in `tests/acceptance/ssa-backend-service.md` records the
direct GCC/Clang and sanitizer commands.  The shared CMake/CTest registration
is intentionally left to the parent integration task; this isolated change
therefore does not claim a full repository build or an end-to-end JIT.

## Open issues / follow-up

The service currently uses a small spin lock suitable for the short metadata
critical sections; a host scheduler may wrap `ProcessNext` in its own queue.
Parent integration should register the two implementation translation units
and the focused test in the SSA CMake list, then run the repository's full
GCC/Clang/MSVC matrix.  Concrete AOT, ExecBC, and host-JIT adapters remain
separate tasks and must supply their own executable/map ownership proofs.
