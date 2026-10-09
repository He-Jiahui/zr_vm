---
related_code:
  - tests/core/test_ssa_exec_ir_module_copy.c
  - tests/core/support/ssa_exec_ir_module_copy_fixture.h
  - tests/core/support/ssa_exec_ir_module_copy_fixture.c
  - zr_vm_core/include/zr_vm_core/exec_ir_module_copy.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_module_copy.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
plan_sources:
  - .codex/plans/20261009-ssa-module-final-function-copy-design-v1.md
tests:
  - tests/core/test_ssa_exec_ir_module_copy.c
  - tests/core/support/ssa_exec_ir_module_copy_fixture.h
  - tests/core/support/ssa_exec_ir_module_copy_fixture.c
  - docs/testing-and-validation/exec-ir-module-copy.md
  - docs/acceptance/exec-ir-module-copy.md
doc_type: module-detail
status: green-observed-rootproof-commit-pending
---

# ExecIR whole-module selected-function copy

## Purpose and current state

`ZrCore_ExecIr_CloneModuleReplacingFunction` introduces an explicit storage
transaction: copy a whole module, replacing one function with a deep copy of
same-identity replacement storage. It uses existing typed module storage,
CloneModule, CloneFunction and FreeModule. A separate small implementation keeps
this orchestration out of the large existing exec_ir.c.

Current GREEN implements the complete candidate transaction described below.
Actual GREENr1 observes20PASS/0FAIL/0ignored and independently measures module
clone60/replacement clone28/composite88, with88 faults completed and disarmed
retries. CoreModel and generic14 independently PASS. Root current proof/commit
remain pending. Finite MSVC GREENr1 observes7/7 natural0/zero warnings,
compile-only; no Source GREEN is inferred.

Historical REDr1 was3 guard PASS/17 code28/attempts0 feature FAIL and is committed
as `fcc84729099e5bbb4de0a16dd3980c3a545be139`. The historical refusal is no longer
the current implementation. Finite evidence details remain in acceptance.

## Inputs and lifetime

The API takes const source module, selected function ID, const replacement
function, initialized owning destination module and optional diagnostic. Source,
replacement and every reachable typed header/pool must be genuinely initialized,
readable, stable and serialized throughout the call. The caller owns allocation
lifetimes and serializes mutation/publication; const is a caller contract.

Destination must be uniquely owned and initialized by ModuleInit or existing
storage APIs. Its header and the writable diagnostic must not overlap inputs,
reachable input storage or each other. Separate owning records cannot share
pools. Partial/header/shallow ownership aliases violate the contract. Checking
counts cannot make arbitrary pointer geometry safe.

Exact source==destination is refused. Replacement may borrow the selected source
slot, or a function in the old destination. The latter remains live until both
clones finish; success frees the old destination and invalidates that borrow.
Failure preserves the borrow and old destination. No input owning record is
blanket-zeroed, and no rollback across nonlocal exceptions is promised.

## Implemented transaction and storage preservation

1. Reset all nine diagnostic fields; reject NULL/exact source alias before range.
2. Reject selectedId zero/outside source.functionCount before slot access.
3. Initialize a private candidate and CloneModule(source,candidate). Preserve
   every lower diagnostic through candidate cleanup on failure.
4. Check selected candidate slot identity and immutable replacement identity in
   the exact order below. Do not rewrite IDs or use ExecutionContract_Check.
5. CloneFunction(replacement,candidate selected slot). Existing lower clone
   completes its temporary before releasing the candidate's old selected slot.
6. After all fallible work succeeds, FreeModule(destination), publish candidate,
   and relinquish local ownership. No fallible work follows publication.

Any refusal frees only candidate storage and preserves source, replacement and
the old destination. The source module's unselected functions and all constants,
layouts and sourceMaps retain their logical content. The selected function's
entire copied storage comes from replacement, including binding rows, source/GC/
state/deopt pools and frame headers. Optional present-empty GC/state/frame
headers keep presence. Existing clone validation applies to nested StateMaps.

Function contract.layoutHash deliberately may differ; legitimate finalized frame
storage and its actual hash come from replacement. Module contract.layoutHash
remains the source value. Capacities follow existing clone normalization, rather
than universal source capacity, padding or spare-byte preservation. Replacement
constant/layout references must use the copied module's same numbering; Core
does not prove their semantic compatibility. Double cloning the selected slot
adds transient allocations whose actual count must be measured independently.

## Complete diagnostic order

The diagnostic has code, functionToken, blockId, instructionId, sourceId,
expectedVersion, actualVersion, expectedHash and actualHash. Entry and successful
return reset every field to zero. Diagnostic may be NULL.

| Order / condition | Code | Active expected / actual payload |
| --- | --- | --- |
| 1 NULL input/output or exact source==destination | INVALID_ARGUMENT | All payload zero |
| 2 selectedId zero/outside count | INVALID_RANGE | Version: source.functionCount / selectedId; token zero |
| 3 CloneModule failure | Actual lower code | All nine actual lower fields preserved |
| 4 selected candidate slot.id mismatch | INVALID_BLOCK | Version: selectedId / slot.id |
| 5 replacement.id mismatch | INVALID_BLOCK | Version: selectedId / replacement.id |
| 6 functionToken mismatch | TARGET_MISMATCH | Version: selected / replacement token |
| 7 function signatureHash mismatch | SIGNATURE_MISMATCH | Hash: selected / replacement signature |
| 8–12 schemaVersion, abiVersion, logicalVersion, reserved0, reserved1 | VERSION_MISMATCH | Version: selected / replacement field, in stated order |
| 13 contract.generation | STALE_GENERATION | Hash: selected / replacement generation |
| 14 contract.targetToken | TARGET_MISMATCH | Version: selected / replacement target |
| 15 contract.signatureHash | SIGNATURE_MISMATCH | Hash: selected / replacement signature |
| 16 contract.moduleHash | MODULE_MISMATCH | Hash: selected / replacement module hash |
| 17 contract.requiredCapabilities | CAPABILITY_MISMATCH | Version: selected / replacement capabilities |
| 18 contract.declaredEffects | EFFECT_MISMATCH | Version: selected / replacement effects |
| 19 CloneFunction failure | Actual lower code | All nine actual lower fields preserved |

INVALID_RANGE/INVALID_BLOCK use ZR_EXEC_IR_DIAGNOSTIC enums; other identity
codes use ZR_EXECUTION_DIAGNOSTIC enums. Orders4–18 set functionToken to the
selected candidate token and leave blockId/instructionId/sourceId zero. A32-bit
comparison uses only version fields; A64-bit comparison uses only hash fields.
Every inactive field stays zero. There is no valueId diagnostic field.

Range wins over malformed source storage; CloneModule failure wins over identity
mismatch; identity mismatch wins over replacement storage refusal. Equal zero
identities, unknown capability/effect bits, nonzero reserved fields and noncurrent
versions are equality values, subject to existing lower storage validation.

## Admission and integration limits

Core copies storage without VerifyModule, VerifyFunction_ALL or execution
admission. It performs no CFG/SSA/map transformation, hash generation, ID repair,
canonical registry ownership or registry copy. Numeric registry identities are
retained values and do not extend the registry lifetime.

A later separate Parser consumer must prove actual source/canonical/frame
consistency and verify its full candidate before publishing. This API does not
silently finalize the returned-source mode4 uncompacted module, change AOT/NONE,
or establish default canonical output. Source runtime remains a separate pending
current acceptance gate. Linux/full MSVC, backend/normal runtime and all47 gates
remain OPEN.

- [Twenty-case validation design](../testing-and-validation/exec-ir-module-copy.md)
- [Pending finite evidence](../acceptance/exec-ir-module-copy.md)
