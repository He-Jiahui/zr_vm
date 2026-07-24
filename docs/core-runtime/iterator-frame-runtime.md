---
related_code:
  - zr_vm_core/include/zr_vm_core/iterator_runtime.h
  - zr_vm_core/src/zr_vm_core/iterator/frame.c
  - zr_vm_core/src/zr_vm_core/iterator/dispatch.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/ownership.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/iterator_runtime.h
  - zr_vm_core/src/zr_vm_core/iterator/frame.c
  - zr_vm_core/src/zr_vm_core/iterator/dispatch.c
plan_sources:
  - user: 2026-07-24 execute Syntax 13 milestones and record each result
  - docs/plans/syntax/2026-07-20-13-iterator-enumerator-yield-design.md
  - docs/plans/syntax/13-iterator-enumerator-yield/m3-iterator-frame-runtime-implementation-plan.md
tests:
  - tests/iterator/test_iterator_runtime.c
  - tests/iterator/test_iterator_gc_drop.c
  - tests/acceptance/2026-07-25-syntax-13-m3-iterator-frame-runtime.md
doc_type: module-detail
---

# Iterator Frame Runtime

## Purpose

Syntax 13 M3 provides a small core-runtime primitive for synchronous iterator
execution. It gives a producer callback a caller-owned `SZrIteratorFrame` and
exposes the normal consumer operations: advance, retrieve the current value,
complete, fault, or close. The primitive deliberately does not know about
source syntax, pre-SemIR, bytecode, generated code, task scheduling, or a
language-server request.

## State And Control Flow

`ZrCore_IteratorFrame_MoveNext` drives one producer callback invocation. The
frame starts in `READY`; a successful producer call must publish one value and
enter `YIELDED`, or explicitly move to `COMPLETED` or `FAULTED`. A following
advance changes `YIELDED` back to `READY` before invoking the producer. A
producer that returns without making a transition faults the frame.

`COMPLETED`, `FAULTED`, and `CLOSED` are terminal. They never invoke the
producer again. Recursive `MoveNext` on the same frame is rejected while the
outer producer call is active, so a producer cannot overwrite its own current
value or cleanup state. All terminal paths share one cleanup rule: release the
current value and its GC root, then invoke the registered callback exactly
once.

## Current Value And GC Safety

`Publish` clears the previous current value, copies the new `SZrTypeValue`,
and creates a `SZrGcRootHandle` for a garbage-collectable object. `Current`
resolves that handle before returning the object pointer. This means a compact
collection may relocate the object while the frame is yielded without exposing
the pre-move pointer to the consumer.

The terminal transition releases the root before user cleanup. This prevents a
cleanup callback from observing stale frame-owned storage, while still letting
the callback release its own `Unique` resource through the normal ownership
control-block path. Replacing a yielded object releases the old root before
installing the next root, keeping one active frame root rather than retaining a
root for every value produced.

## Storage Paths

The ordinary API initializes caller-owned storage and does not allocate a
frame. `SZrIteratorFramePool` is an explicit alternative for clients that need
reusable heap-backed storage. It holds a typed free list, allocates only when
the list is empty, and reports `allocationCount` and `reuseCount` for
verification. Releasing to the pool requires a terminal frame; acquisition
reinitializes every field so producer, current-value, root, and cleanup state
cannot cross leases. `Pool_Free` frees only the pool's currently idle nodes.

## Constraints And Follow-up

M3 is intentionally synchronous and callback-driven. It does not lower M2
`YIELD_*` facts into this API, add iterator function bytecode or interpreter
opcodes, implement async suspension/wait states, modify execution dispatch,
define artifact/AOT rows, add debug or LSP projections, or provide a dynamic
object-property fallback. Those consumers must be introduced by later syntax
milestones through canonical runtime contracts rather than bypassing this
state machine.

## Test Coverage

`test_iterator_runtime.c` covers ordered yields, immutable first-terminal
state, missing-producer faulting, recursive-entry rejection, root replacement,
and typed-pool address reuse. `test_iterator_gc_drop.c` forces compact collection
while a value is yielded and verifies that completion, fault, and early close
release the frame root before exactly-once cleanup drops a direct `Unique`
resource.
