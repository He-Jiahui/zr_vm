---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_telemetry.c
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/object/object.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_telemetry.c
plan_sources:
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
tests:
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/gc/gc_tests.c
doc_type: module
---

# GcDomain concurrent major collection

Syntax 04 M7 turns a major collection into a domain-local multi-step operation. It keeps the
M5 mutator registry and safepoint protocol, but does not hold stop-the-world for the complete
mark. Each domain owns its own phase, mark queue, telemetry and mutation lock; a collection in
one domain never becomes a process-wide pause.

## Phase contract

A major cycle advances through these ordered phases:

1. `INITIAL_SNAPSHOT`: stop only the target domain, snapshot roots and seed the mark queue;
2. `CONCURRENT_MARK`: resume mutators and consume a bounded number of mark entries per step;
3. `REMARK`: stop the target domain again, rescan roots and barrier-fed work to a fixed point;
4. `SWEEP`: reclaim unreachable nodes while the domain remains paused;
5. optional `COMPACT`: relocate only when the caller supplies nonzero compact budget;
6. publish the completed generation and release the pause.

The public step API is budgeted. A zero compact budget deliberately defers relocation instead of
silently turning the operation into an unbounded full collection. `ZrCore_GcFull` remains the
explicit synchronous path: it cancels any partial concurrent cycle under the same mutation lock
and performs one coherent full collection.

## Barrier and root invariant

Object membership and mark-state access share the domain mutation lock with the concurrent
marker. All object member/index writes first validate domain identity and then publish the
generational/concurrent barrier before the new edge can be missed by remark. The initial snapshot
and remark both scan domain roots plus every registered VM/AOT mutator root under the existing
coordination protocol.

The direct stress contract mutates black objects while mark slices are running, creates new
edges after the initial snapshot, and proves the targets survive remark. The no-barrier control
case is not a supported write path; production object mutation cannot bypass the barrier by
choosing a different member/index entry point.

## Per-domain telemetry

`ZrCore_GcStats` exposes the exact domain identity/generation together with active mutator count,
cycle count, initial-pause, mark, remark, compact, barrier and safepoint counters. Cross-domain
transport events are attributed independently to their source or target domain: prepare/publish
and outbound payload belong to the source; claim/commit and inbound payload belong to the target;
abort is recorded on the domain that owns the terminal cleanup.

Telemetry updates validate the captured domain identity and generation under the coordination
lock. A stale envelope cannot charge a newly reused domain, and callers do not infer attribution
from thread names, diagnostic text or payload shape.

## Current boundary

- The marker is incremental and mutator-concurrent; it is not a parallel worker pool and does not
  make collector workers language mutators.
- Weak/finalizer policy keeps the existing major-cycle semantics; M7 changes scheduling and
  barrier closure, not user-visible finalization order.
- Compaction remains a short domain-local pause and is controlled by an explicit byte budget.
- Scheduler policy and domain topology remain Syntax 12 work. Runtime telemetry is an input to
  that policy, not an implicit scheduler.
