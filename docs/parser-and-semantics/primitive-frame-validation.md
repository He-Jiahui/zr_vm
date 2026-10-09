---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
  - tests/parser/test_ssa_primitive_frame_validation.c
  - tests/parser/support/ssa_primitive_frame_validation_fixture.h
  - tests/parser/support/ssa_primitive_frame_validation_fixture.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_validation.c
plan_sources:
  - .codex/plans/20261009-ssa-primitive-frame-validation-design-v1.md
  - .codex/plans/20261009-ssa-primitive-frame-validation-tests-v1.md
  - .codex/plans/20261009-ssa-primitive-frame-validation-extraction-addendum.md
tests:
  - tests/parser/test_ssa_primitive_frame_validation.c
  - docs/acceptance/primitive-frame-validation.md
doc_type: module-detail
status: functional-green-verified-commit-pending
---

# Const primitive source-frame validation

## Purpose and current GREEN boundary

The additive `ZrParser_ExecIr_ValidatePrimitiveSourceFrame` is an implemented
const typed predicate for an attached primitive INT64 frame. It checks storage,
Core verification and admission, regenerates the actual packed request, compares
frame fields/hash/slots and borrowed geometry, then frees its temporaries. It
publishes no frame/owner/ID and mutates no input. Actual GREEN r2 passes18 cases,
including18 independently measured validation faults/retries. Root final proof
and GREEN commit remain pending. Historical RED was committed as `d68281c`;
full receipts and failed attempts are preserved in the acceptance record.

## Lifetime and field predicate contract

Function, live SemanticContext, explicit layout rows and reachable typed storage
must be initialized, readable, stable, serialized and independently nonoverlapping.
Optional diagnostic storage must be independent and writable. Checked spans do
not establish arbitrary-pointer readability or same-compilation provenance.

Both sealed and unsealed Functions are valid validator inputs; Attach retains
its existing SEALED refusal. Existing primitive body/value rules apply, allowing
the present frame/hash. Values require flags0, UNKNOWN ownership, NONNULL,
ordinary nonzero bounded definitions, defining instruction type equality and
live declared PRIMITIVE INT64 nodes with nonzero structural hashes and target
rows. CONSTANT/NOP/RETURN are the supported instruction slice. Rich GC/state/
deopt/binding/phi/memory/place metadata remains unsupported.

Canonical IDs must be sorted unique nonzero values for the new validator. The
declared test context is a typed array owner, not an interner/Binder witness.
Numeric IDs/hashes do not authenticate source history or extend registry lifetime.
Context/rows remain live through validation; no borrowed pointer is retained.
Explicit layout size/alignment supplies geometry, not a target ABI certificate.

## Implemented request, comparison and cleanup

After preflight/Core verification/body/row/canonical/value admission, recreate
actual Attach's request: valueId/typeToken from each value, SCALAR class, actual
row byteSize/byteAlign, liveStart0/liveEnd=instructionCount, flags0. Request token
is actual functionToken; parameter prefix and return-buffer size/alignment are0.
Zero frameByteLimit imposes no limit; other limits are passed to the real producer.
Checked count multiplication precedes request-values allocation.

Call existing PackedFrameLayoutInit/LayoutPackedFrame. Reuse its sorting,
physical mapping and packed hash; do not synthesize layout or add row identity/
ABI to the existing hash domain. Compare logical32 fields in order:
storageSlotCount, logicalSlotCount, parameterPrefixCount, returnBufferOffset,
frameByteSize, frameByteAlign, parameterCount, localCount, slotCount. Then compare
regenerated hash to frame hash, followed by contract.layoutHash. Compare each
physical slot's slotId, byteOffset, byteSize, byteAlign, typeToken and kind.

Capacity equality, addresses, padding and spare bytes are excluded from clone
equality; each actual capacity must still cover live storage. Regenerated logical
ValueIds/mapping/classes validate the actual physical slots through shared
geometry. Actual frame headers have no fields for those temporary mappings.

The local packed comparison view borrows the actual frame and owns nothing.
Never pass it to PackedFrameLayoutFree/FrameLayoutFree. Free only the real
regenerated packed object and request values on every path. No persistent frame
header is allocated. Temporary cleanup preserves exact lower diagnostics;
Success returns with all nine fields zero. No input is repaired or mutated.

## Complete diagnostic order

Nine fields are code, functionToken, blockId, instructionId, sourceId,
expectedVersion, actualVersion, expectedHash and actualHash. Every wrapper
refusal resets all fields. T below is the actual readable functionToken; every
unlisted field is zero. Version pairs carry32-bit values; hash pairs carry64-bit.

| Order / predicate | Code and active payload |
| --- | --- |
| 1 NULL top-level guards | INVALID_ARGUMENT; all payload zero/token0 |
| 2 storage count/capacity/null/overlap | INVALID_RANGE; tokenT only |
| 2 capacity-width/address overflow or exhausted33 spans | CAPACITY_OVERFLOW; tokenT only, within storage traversal order |
| 3 temporary value products in PackedValue/u32/ValueId/FrameSlot/SlotClass order | CAPACITY_OVERFLOW; tokenT only |
| 4 Core VERIFY_ALL refusal | Every actual lower field unchanged |
| 5 body profile in existing order | UNSUPPORTED; tokenT only |
| 6 layout rows, then sorted/nonzero canonical IDs | UNSUPPORTED for rows; INVALID_RANGE for IDs; tokenT only |
| 7 value/type admission | UNSUPPORTED; tokenT, actualVersion=value.id, instructionId=definition; bounded actual defining instruction sourceId only |
| 8 missing frame | LAYOUT_MISMATCH; tokenT, versions1/0 |
| 9 request-values calloc fails after all values admitted | OUT_OF_MEMORY; tokenT only |
| 10 actual LayoutPackedFrame failure | Every actual lower field unchanged |
| 11 ordered logical32 frame fields | LAYOUT_MISMATCH; tokenT, versions regenerated/actual |
| 12 frame hash then contract hash | LAYOUT_MISMATCH; tokenT, hashes regenerated/actual |
| 13 physical slot index then ordered fields | LAYOUT_MISMATCH; tokenT, versions regenerated/actual |
| 14 actual-frame shared geometry | LAYOUT_MISMATCH; tokenT only |
| 15 success/cleanup | All nine fields zero |

INVALID_ARGUMENT/LAYOUT_MISMATCH use execution diagnostic enums; storage,
capacity, UNSUPPORTED and OOM use ExecIR enums. Lower failures are not normalized
to guessed OOM. NULL diagnostic has the same validation/lifetime behavior.
The exact storage/body/value predicate traversal is frozen in the design;
simultaneous mutations test that first-error order, including earlier Core errors.

## Attach preservation and allocation closure

Implemented shared-helper extraction keeps Attach's actual order unchanged: NULL,
storage, count products, Core VERIFY_ALL, SEALED, absent frame/hash and body,
rows, value admission, request allocation, LayoutPackedFrame, geometry, header
malloc, borrowed Function view VERIFY_ALL, frame/hash publication and cleanup.
No new sorted-ID guard is added to Attach. Its empty StateMap identity, lower
diagnostics, direct values/header allocation order and ownership remain intact.

The accepted extraction addendum defines eight private shared helpers: fail,
count_fits, storage, body_metadata, rows, admit_values, prepare_request and geometry.
Admission is nonallocating; validator checks missing frame before request allocation.
Attach invokes admission and request preparation consecutively in its original order.
These eight helpers are implemented in the shared TU/internal header; pool/span
helpers remain private. The exact33-span budget covers objects/pools/optional
headers. Admission precedes missing-frame refusal and request allocation.

Root's actual selected23-source small direct closure is Core15, Parser Attach/
layout/validator3, actual lookup1 and test main/fixture/allocator/Unity4, within
51 total configured TUs. GREEN runtime r2 rebuilt only the main test TU and linked; the
remaining objects were verified current by hashes. Parser3 and Core allocation/
free pairs share the strict ledger;
producer setup is unarmed and stays live during fault tests. The main FI fixture actually measures both Attach values/header sites and verifies
failure preservation, retry and publication. Normal/direct legacy registrations
select both real Attach/shared TUs with the old shim; two extra MSVC units prove
compile compatibility only. No old PackedFree callback runtime is credited.

No Binder/source wrapper, interner/registry retention, whole-module finalization,
mode4/default migration, target ABI, complete04.01/04.02/04.04 or all47 acceptance
is credited. Normal/Linux/full MSVC and backend runtime parity remain OPEN.

- [Eighteen-case finite validation](../testing-and-validation/primitive-frame-validation.md)
- [Actual finite evidence](../acceptance/primitive-frame-validation.md)
