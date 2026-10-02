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
  - tests/core/ssa_generation_discard_prepared.inc
  - tests/library/test_ssa_capability_validation.c
  - tests/library/test_ssa_rollback_restricted.c
  - tests/acceptance/ssa-hotpatch-generation-handle-ownership.md
  - tests/acceptance/ssa-hotpatch-prepare-status-mapping.md
  - tests/acceptance/ssa-hotpatch-rollback-status-mapping.md
  - tests/acceptance/ssa-hotpatch-rollback-test-ndebug.md
  - tests/acceptance/ssa-hotpatch-generation-resolve-concurrency.md
  - tests/acceptance/2026-10-02-ssa-generation-lease-limit.md
  - tests/acceptance/2026-10-02-ssa-generation-discard-prepared.md
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/03-generation-publication.md
  - docs/plans/ssa/08-artifact-hotpatch/04-rollback-restricted.md
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

`DiscardPrepared` cancels a metadata candidate created by Prepare or Rollback
before publication. It requires an unleased input handle from this manager,
the matching generation in `PREPARED` state, no acquired leases on that record,
and a record distinct from active. The membership check precedes record reads;
state, full-width lease count and clearing share the manager lock with Acquire
and Publish. A numbered Acquire can pin a PREPARED candidate, so merely checking
the input handle's `leased` flag would not make cancellation safe.

On success the record's identity fields, generation and lease count are cleared,
its state becomes FREE, and the supplied handle is cleared. Active,
nextGeneration and manager count remain unchanged. The success diagnostic's
`actualGeneration` identifies the discarded generation. Slot reuse still gets
a fresh generation; an old copied handle cannot publish or cancel the new
candidate. Discard does not undo generation exhaustion.

Null/cleared/leased inputs return `INVALID_ARGUMENT`, matching Publish's input
contract. Foreign, mismatched or non-PREPARED records return `NOT_PREPARED`;
foreign diagnostics have actual generation zero. A pinned matching candidate
returns `INVALID_STATE`, with its generation and lease count in the diagnostic.
A defensively detected PREPARED record equal to active is also rejected as
`INVALID_STATE`. Failed cancellation preserves the handle and manager records;
callers can release acquired leases and retry. A cleared handle's repeated
cancellation is invalid; an old retained copy is no longer prepared.

The public `count` field retains its existing bookkeeping: Prepare/Rollback
increment it up to capacity, while CollectRetired and DiscardPrepared leave it
unchanged. It is not a defined live-occupancy count; its header TODO remains
open. Both allocation paths scan FREE slot states, so cancellation restores
capacity independently of count. This slice does not redefine that field.

Cancellation releases only the current registry's metadata slot. Records are
caller-owned, Prepare does not own artifact bytes or installed executable code,
and this API neither mutates the capability registry nor connects interpreter
frame/GC/code lifetimes. The independent fixture covers reuse, ABA, pinned and
foreign rejection, rollback candidates, generation exhaustion and bounded
competing operations; toolchain evidence and limitations are recorded in the
[prepared-cancellation acceptance](../../tests/acceptance/2026-10-02-ssa-generation-discard-prepared.md).

`AcquireActive` and `Acquire` share a per-record lease limit of `UINT32_MAX`.
The manager lock protects both the full-width atomic count check and the
increment. This matters because `atomic_uint_fast32_t` can be wider than the
public `TZrUInt32` count: accepting one more reader could truncate the count in
Release, or wrap the atomic to zero on a platform with a 32-bit fast type.
At the limit, Acquire returns `GENERATION_OVERFLOW`, leaves the output handle
cleared, and changes no record, active pointer, or generation allocation state.
The diagnostic identifies the record in `actualGeneration` and reports
`leaseCount == UINT32_MAX`; `Generation_StatusName` returns `overflow`.
Existing holders can still Resolve and Release. Once a holder releases a lease,
the same entry can acquire again. A failed Acquire on a retained retired record
does not make it eligible for collection.

The focused lease-limit fixture uses exclusive atomic count seeding to exercise
the last legal increment and both entry paths at the limit. It resets fictional
leases before manager deinitialization, including on failure. Debug/NDEBUG and
toolchain evidence are recorded in the
[lease-limit acceptance](../../tests/acceptance/2026-10-02-ssa-generation-lease-limit.md).
This bounds the existing metadata lease protocol; interpreter frame entry/exit
and executable installation remain separate work. Prepared metadata cancellation
uses the explicit DiscardPrepared contract above.

Every generation handle belongs to the manager whose `records` array contains
its record. `Publish`, `DiscardPrepared`, `Resolve`, and `Release` reject a handle from another
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
concurrent `Publish`. `Resolve` first checks slot membership by pointer equality
without dereferencing an untrusted foreign record, then takes the manager's
internal synchronization lock and copies all record fields plus lease count as
one view snapshot. The public API remains `const SZrHotPatchGenerationManager *`:
the cast is limited to the manager's internal `atomic_flag`, a logical-const
synchronization field. Record layout and status field types are unchanged. A
lease prevents `CollectRetired` from reusing a record, while the manager lock
serializes the view with `Publish` changing ACTIVE to RETIRED. The concurrency
acceptance records a bounded lock-gate scheduling observation, the old-code
RED observed in that run, concurrent publish/resolve stress, and toolchain
evidence. The worker signals immediately before Resolve, leaving a possible
preemption window; that check alone is not a mathematical proof of mutual
exclusion. MSVC direct and registered CTest passed before the final test-only
NDEBUG-safety refactor. The final-source MSVC target build, direct run, and
registered CTest also passed: build 2/2, direct 9/9 in 2.03s, and CTest 1/1
(1.10s test, 1.19s total). Standalone GCC Debug and NDEBUG runs pass, as does
a 500-publication GCC run.
ThreadSanitizer cannot start in the current WSL environment because its runtime
rejects the process memory mapping, so that run gives no race verdict. This
covers metadata snapshot consistency; it does not connect generation handles
to interpreter frame entry/exit. The generation test main keeps setup calls and
checks active under `NDEBUG`, and guards deinitialization with explicit
initialization and lease flags; Debug and NDEBUG evidence is recorded in the
same acceptance.

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

`Rollback` keeps an absent retained generation as `APPLY_ROLLBACK_NOT_FOUND`,
maps a full records array to `APPLY_CAPACITY`, and maps generation exhaustion to
`APPLY_GENERATION_OVERFLOW`. A zero target is rejected as
`APPLY_INVALID_ARGUMENT` before the generation API is called. The regression
fixture builds two real generations, leaving generation 1 retained as
`RETIRED` and generation 2 active; it verifies each failure leaves all records,
the active pointer, manager count and next-generation counter, and output handle
unchanged. Other generation statuses fail closed as `APPLY_ROLLBACK_FAILED`.

The rollback restricted test now executes initialization, apply, rollback, and
profile-validation calls as ordinary statements, then checks their returned
statuses with an always-active `TEST_CHECK`. `NDEBUG` therefore removes no test
work, and failed initialization exits before manager deinitialization. The
Debug and NDEBUG verification evidence is recorded in the test reliability
[acceptance](../../tests/acceptance/ssa-hotpatch-rollback-test-ndebug.md).
