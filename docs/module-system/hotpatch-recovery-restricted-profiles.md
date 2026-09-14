---
doc_type: module-system
---

# Hotpatch Recovery And Restricted Profiles

Validated patches are registered by `patchId` and content hash. Reapplying an
active or retired entry with the same hash is idempotent; a different hash for
the same identifier is rejected before generation allocation. Rollback republishes
retained code metadata under a fresh generation, so existing leases remain valid.
Rollback changes code selection only: external I/O and already committed state
are intentionally not undone.

The iOS and WASM interpreter profiles accept only pointer-free ExecIR, ExecBC,
state-map, and binding sections. Relocation or unknown sections are rejected
before execution; machine-code and native-import capabilities remain forbidden
by capability validation. v1 does not migrate private layouts or reinterpret
existing heap, closure, task, or module-global bytes.

Focused coverage is in `tests/library/test_ssa_rollback_restricted.c` and checks
idempotence, ID collision, fresh-epoch rollback, and restricted-section rejection.
