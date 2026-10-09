---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_owned_module.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
plan_sources:
  - .codex/plans/20261009-ssa-execir-owned-module-design-v1.md
  - .codex/plans/20261009-ssa-execir-owned-module-tests-v1.md
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
tests:
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - docs/acceptance/ssa-execir-owned-module.md
doc_type: module-detail
status: finite-green-verified-commit-pending
---

# Independent Core ExecIR module ownership

## API and current implementation

`SZrExecIrOwnedModule` is an opaque unique owner. The public API declares
`ZrCore_ExecIr_CloneOwnedModule(source, destination, diagnostic)`, a borrowed
const `ZrCore_ExecIr_OwnedModuleView(owner)` and slot-clearing
`ZrCore_ExecIr_OwnedModuleFree(ownerSlot)`.

The constructor resets the complete optional diagnostic and rejects NULL source
or destination before allocation. It allocates one fresh envelope, initializes
its module and delegates to existing `ZrCore_ExecIr_CloneModule`.
View(NULL) returns NULL. Free accepts a NULL slot or empty slot; a genuine live
owner is removed from its slot before FreeModule and envelope free.

Historical r5's actual14-case RED run observed1 prerequisite PASS and13 constructor
feature FAIL, with0ignored and no ledger/UBSan diagnostic. Its driver receipt is
validatedtrue and Root separately checked current provider proof. Historical r4's
driver receipt remains validatedfalse because its parser expected a different
Unity message; Root's separate r4 raw review remains historical.
RED was committed in `ef37e514`; no owner success or new owner allocation-fault
coverage is credited by that refusal run. Current GREEN passes all14 cases,
including two61-site fault sweeps, with exact balanced records and no ledger or
UBSan diagnostic. Its separate commit remains pending.

## Snapshot contract

The source and all reachable typed storage must be initialized, genuine,
readable and stable during the call. The destination is an exclusive live slot,
initialized NULL or containing a unique live owner from this API. The slot and
optional writable diagnostic must not overlap the source, reachable storage,
old owner or each other. Arbitrary stale pointers and concurrent mutation are
outside this contract.

Failure releases only candidate storage and preserves source, slot and old
owner. Success publishes the candidate and releases the old owner after all
source reads. The old
owner's borrowed view is an allowed source. Failed replacement preserves that
borrow; successful replacement invalidates it and requires acquiring a new view.
Const borrowing also prohibits writes through nested pointers; top-level C
const does not enforce that nested-storage restriction.

This is a multi-allocation storage snapshot. Existing clone copies module
and function identities, contracts, logical counts and live rows without ID
remapping. Ordinary pools rebuild capacities through existing reserve growth;
binding rows reserve from source capacity, and StateMap pools use exact counts.
Optional NULL and present-empty GC/frame/StateMap headers retain existing clone
behavior. Spare bytes, padding and universal capacity equality are not promised.

## Diagnostic contract

The nine fields are code, functionToken, blockId, instructionId, sourceId,
expectedVersion, actualVersion, expectedHash and actualHash. NULL arguments
report INVALID_ARGUMENT with all payload fields zero. Envelope calloc failure
reports OUT_OF_MEMORY with expectedVersion1/actualVersion0 and all other fields
zero. Lower CloneModule failure preserves its complete diagnostic; success
leaves the complete diagnostic zero. Envelope allocation precedes lower shape
checks, so its OOM takes precedence over malformed non-NULL source counts.

## Scope

Existing clone shape, binding-row and logical-map checks are retained. This API
adds no Verify_ALL or execution admission, canonical identity/hash generator,
single arena, Function attachment or ordinary/default compilation migration.
Source-derived ownership, complete 01.01 migration, platform/backend parity and
all 47 SSA leaf gates remain OPEN. No source or generated native execution is
part of this gate.

- [Finite test design](../testing-and-validation/ssa-execir-owned-module.md)
- [Evidence and acceptance](../acceptance/ssa-execir-owned-module.md)
