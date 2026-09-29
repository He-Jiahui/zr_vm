---
related_code:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
tests:
  - tests/core/test_ssa_generation_publication.c
  - tests/library/test_ssa_capability_validation.c
  - tests/acceptance/ssa-hotpatch-generation-handle-ownership.md
  - tests/acceptance/ssa-hotpatch-prepare-status-mapping.md
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/03-generation-publication.md
doc_type: core-runtime-contract
status: implemented
---

# Module generation lifetime

`hotpatch_generation.h` provides a process-local generation registry over a
caller-owned, fixed-capacity records array. A validated patch's identity
metadata is copied into a `PREPARED` record, then published with an
acquire/release active pointer swap. Prepare does not copy artifact bytes or
install executable code. The caller must keep the validated token's borrowed
content stable as required by the API precondition; `ApplyValidated` separately
rehashes the captured byte span before entering this preparation path.

An entry hook can use `AcquireActive`; the registry lock protects the
active-pointer read and lease increment as one operation. Existing frames may
retain an older handle and continue resolving that immutable version while new
entries observe the new active generation. Handles include the generation
number, so reclaimed records cannot satisfy an old (ABA) link. The current
repository has not connected these calls to interpreter frame entry/exit.

Retired records are reclaimed only when their lease count reaches zero and
they are no longer active. `CollectRetired` clears such records for reuse;
stale or non-leased handles return a structured `STALE_LINK`/state error.
Rollback and publication callers must allocate a fresh generation rather than
mutating an existing record in place.

Every generation handle belongs to the manager whose `records` array contains
its record. `Publish`, `Resolve`, and `Release` reject a handle from another
manager before reading its record or changing either manager's state. A
foreign handle reports `actualGeneration == 0`, since its record is not part
of the requested manager. Membership uses pointer equality over that manager's
slots; relational comparison between pointers from separate arrays is
undefined in C. This check costs O(capacity). `GenerationManager_Init` accepts
the caller's `TZrUInt32` capacity without imposing a smaller limit. Current
repository call sites for `Resolve` and `Release` are the generation tests;
interpreter frame entry/exit has not yet been connected to these APIs. If that
changes, profile the ownership scan before treating it as a hot-path lookup.

This ownership check does not solve the separate race between `Resolve` and a
concurrent `Publish`, because the record state is non-atomic. Callers must
serialize those operations until that boundary is implemented.

`ApplyValidated` preserves the three failures currently returned by
`Generation_Prepare`: invalid arguments map to `APPLY_INVALID_ARGUMENT`, a
full records array maps to `APPLY_CAPACITY`, and a wrapped next generation maps
to `APPLY_GENERATION_OVERFLOW`. The new overflow value is appended to the Apply
status enum, preserving earlier numeric values. Each has an explicit
`ApplyStatusName`. A Prepare failure occurs before Publish and registry
registration; the capability validation fixture checks the diagnostic and
confirms the manager, registry, and output handle remain unchanged. The current
Prepare implementation has no other failure return; an unexpected future
status fails closed as `APPLY_PUBLISH_FAILED` rather than being reported as
capacity.
