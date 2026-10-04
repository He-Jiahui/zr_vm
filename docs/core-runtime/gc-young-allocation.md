---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_young_allocation.h
  - zr_vm_core/src/zr_vm_core/gc/gc_tlab.c
  - zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c
  - zr_vm_core/src/zr_vm_core/gc/gc_minor.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_tlab.c
  - zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c
  - zr_vm_core/src/zr_vm_core/gc/gc_minor.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/01-young-allocation.md
tests:
  - tests/core/test_ssa_young_allocation.c
doc_type: runtime-contract
status: implemented
---

# Young allocation and minor collection contract

The young-generation contract keeps worker-local TLAB cursors separate from
collector state. `ZrCore_GcTlab_AllocateFast` performs checked aligned bump
allocation and never advances the cursor on an exhausted or malformed request;
refill requires the caller to report a safepoint. Retirement itself has no
safepoint parameter and only updates the TLAB record. Native-visible,
pinned, and large objects are classified for non-moving storage instead of
being silently placed in the nursery.

Old-to-young stores mark a bounded card table. Card and remembered-root records
contain only card/object tokens, so they are safe to inspect in diagnostics and
cannot persist a process address. Duplicate tokens are coalesced and scans are
bounded by the recorded count; the epoch is stored but not checked by scan.
Clearing is explicit, and card marking does not itself register remembered roots.

`SZrGcMinorTransaction` models the stop-the-world boundary:

```text
EVACUATE -> REWRITE_REFERENCES -> VERIFY -> RESUME -> COMPLETE
```

Mutators cannot resume until every region has been processed, forwarding is
resolved, and the transaction reports a consistent boundary. To-space failure,
missing roots, stale phases, and unresolved forwarding return structured
diagnostics without advancing the partially completed region. `Abort` leaves
mutators stopped and marks the transaction terminal, allowing the collector to
retry with a smaller nursery or a promotion policy.

The focused `ssa_young_allocation` test asserts aligned allocation, endpoint
zeroing, exhaustion with an unchanged cursor, safepoint refill rejection,
retirement accounting, explicit card-token deduplication and ordered root scan,
promotion/allocation decisions, and scalar transaction phase gates after
to-space or unresolved-forwarding failure. It does not construct arithmetic
overflow. Its assertions, including API calls inside assert, require NDEBUG
to be undefined; reading these assertions is not execution evidence.
The API is a verified state/decision contract; integration with a particular
collector's physical evacuation remains owned by the GC implementation.
