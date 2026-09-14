---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_young_allocation.h
  - zr_vm_core/src/zr_vm_core/gc/gc_tlab.c
  - zr_vm_core/src/zr_vm_core/gc/gc_remembered_set.c
  - zr_vm_core/src/zr_vm_core/gc/gc_minor.c
doc_type: runtime-contract
status: implemented
---

# Young allocation and minor collection contract

The young-generation contract keeps worker-local TLAB cursors separate from
collector state. `ZrCore_GcTlab_AllocateFast` performs checked aligned bump
allocation and never advances the cursor on an exhausted or malformed request;
refill and retirement are explicit safepoint operations. Native-visible,
pinned, and large objects are classified for non-moving storage instead of
being silently placed in the nursery.

Old-to-young stores mark a bounded card table. Card and remembered-root records
contain only card/object tokens, so they are safe to inspect in diagnostics and
cannot persist a process address. Duplicate tokens are coalesced and scans are
epoch-bounded; clearing is explicit.

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

The focused `ssa_young_allocation` test covers alignment/overflow, safepoint
refill, card marking and scan, promotion reasons, and partial-region failure.
The API is a verified state/decision contract; integration with a particular
collector's physical evacuation remains owned by the GC implementation.
