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
status: finite-red-ready-to-submit-commit-pending
---

# Independent Core ExecIR module ownership

## API and current implementation

`SZrExecIrOwnedModule` is an opaque unique owner. The public API declares
`ZrCore_ExecIr_CloneOwnedModule(source, destination, diagnostic)`, a borrowed
const `ZrCore_ExecIr_OwnedModuleView(owner)` and slot-clearing
`ZrCore_ExecIr_OwnedModuleFree(ownerSlot)`.

The current RED constructor resets the complete optional diagnostic, returns
false and allocates nothing. NULL source or destination reports INVALID_ARGUMENT;
other non-NULL arguments report UNSUPPORTED. All eight diagnostic payload fields
remain zero. It does not inspect source contents or change the destination.
View(NULL) returns NULL. Free accepts a NULL slot or empty slot; a genuine live
owner is removed from its slot before FreeModule and envelope free. The RED
constructor cannot create such a live owner; tests must not fabricate one.

The current r5 actual14-case RED run observed1 prerequisite PASS and13 constructor
feature FAIL, with0ignored and no ledger/UBSan diagnostic. Its driver receipt is
validatedtrue and Root separately checked current provider proof. Historical r4's
driver receipt remains validatedfalse because its parser expected a different
Unity message; Root's separate r4 raw review remains historical.
RED is ready to submit, with its commit pending. No owner success or new owner
allocation-fault coverage is credited by this refusal run.

## Intended snapshot contract

The source and all reachable typed storage must be initialized, genuine,
readable and stable during the call. The destination is an exclusive live slot,
initialized NULL or containing a unique live owner from this API. The slot and
optional writable diagnostic must not overlap the source, reachable storage,
old owner or each other. Arbitrary stale pointers and concurrent mutation are
outside this contract.

The future GREEN implementation will allocate a fresh envelope, initialize its
module and delegate to existing `ZrCore_ExecIr_CloneModule`. Failure will release
only candidate storage and preserve source, slot and old owner. Success will
publish the candidate and release the old owner after all source reads. The old
owner's borrowed view is an allowed source. Failed replacement preserves that
borrow; successful replacement invalidates it and requires acquiring a new view.
Const borrowing also prohibits writes through nested pointers.

This will be a multi-allocation storage snapshot. Existing clone copies module
and function identities, contracts, logical counts and live rows without ID
remapping. Ordinary pools rebuild capacities through existing reserve growth;
binding rows reserve from source capacity, and StateMap pools use exact counts.
Optional NULL and present-empty GC/frame/StateMap headers retain existing clone
behavior. Spare bytes, padding and universal capacity equality are not promised.

## Diagnostic contract

The nine fields are code, functionToken, blockId, instructionId, sourceId,
expectedVersion, actualVersion, expectedHash and actualHash. The future envelope
calloc failure will report OUT_OF_MEMORY with expectedVersion1/actualVersion0
and all other fields zero. Lower CloneModule failure will preserve its complete
diagnostic; success will leave the complete diagnostic zero. Envelope allocation
will precede lower shape checks, so its OOM takes precedence over malformed
non-NULL source counts. These GREEN behaviors are not supplied by the RED stub.

## Scope

Existing clone shape, binding-row and logical-map checks are retained. This API
adds no Verify_ALL or execution admission, canonical identity/hash generator,
single arena, Function attachment or ordinary/default compilation migration.
Source-derived ownership, complete 01.01 migration, platform/backend parity and
all 47 SSA leaf gates remain OPEN. No source or generated native execution is
part of this gate.

- [Finite test design](../testing-and-validation/ssa-execir-owned-module.md)
- [Evidence and acceptance](../acceptance/ssa-execir-owned-module.md)
