---
related_code:
  - tests/parser/test_ssa_primitive_frame_validation.c
  - tests/parser/support/ssa_primitive_frame_validation_fixture.h
  - tests/parser/support/ssa_primitive_frame_validation_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
implementation_files:
  - tests/parser/test_ssa_primitive_frame_validation.c
  - tests/parser/support/ssa_primitive_frame_validation_fixture.h
  - tests/parser/support/ssa_primitive_frame_validation_fixture.c
plan_sources:
  - .codex/plans/20261009-ssa-primitive-frame-validation-design-v1.md
  - .codex/plans/20261009-ssa-primitive-frame-validation-tests-v1.md
  - .codex/plans/20261009-ssa-primitive-frame-validation-extraction-addendum.md
tests:
  - tests/parser/test_ssa_primitive_frame_validation.c
  - docs/acceptance/primitive-frame-validation.md
doc_type: testing-guide
status: functional-red-verified-commit-pending
---

# Primitive frame validation finite protocol

## Genuine Attach output, declared field provenance

The fixture uses declared sorted primitive INT64 canonical rows IDs113/271 and
explicit nonzero fixture hashes, with target geometry8/8 and16/16. These rows
are live typed field witnesses, not actual interner/source-authenticated IDs.
Use Core APIs to build two CONSTANT definitions and RETURN with actual returned
ValueIds/instruction IDs/result/operand ranges and block/entry links. Explicitly
check definition binding after AppendInstruction; never patch omitted results.

Register initialized Function/clone/packed owners before assertions. Core verify,
actual Attach with the same context/rows, reverify, actual CloneFunction and clone
verify establish real lower prerequisites. Persistent frame/header/slots must
come from Attach/packed production. No semantic-context lifecycle, interning,
Compiler, source wrapper, Oracle, native/backend or provider execution is used.
Declared arrays are never passed to Free. Borrowed packed views own nothing.

## Exact18 cases and actual RED r2

The frozen main implements these18 names. Actual RED r2 is1 PASS (case4 NULL
guards),17 precise feature FAIL and0 ignored. Each feature stops at the initial
legal validation refusal; later GREEN predicates and allocation sweeps are unexecuted.

| Case name suffix after test_ | Required observation |
| --- | --- |
| actual_attached_frame_is_accepted | Real Attach output validates |
| sealed_and_unsealed_frames_are_accepted | Const validation accepts both; Attach SEALED behavior remains |
| actual_clone_capacity_normalization_is_accepted | Actual clone/live-capacity policy |
| null_arguments_reset_all_diagnostic_fields | Complete NULL reset |
| null_diagnostic_preserves_validation_behavior | Optional diagnostic equivalence |
| readable_storage_headers_and_overlap_are_refused | Bounded readable malformed headers/overlap |
| declared_canonical_ids_must_be_sorted_and_nonzero | Zero/duplicate/unsorted row IDs |
| core_verifier_failure_precedes_later_admission | Actual lower diagnostic/precedence |
| unsupported_body_profiles_are_refused | Every body predicate/order |
| invalid_and_duplicate_layout_rows_are_refused | Every row field/duplicate ID/type |
| value_and_declared_type_predicates_are_refused | Value/type/definition/target row predicates |
| missing_frame_is_refused | Restore registered borrow before assertion/free |
| each_logical_frame_scalar_mismatch_is_refused | Nine ordered32-bit frame fields |
| frame_and_contract_hash_mismatches_are_refused | Both64-bit hashes/precedence |
| each_physical_slot_field_mismatch_is_refused | Both live slots and six ordered fields |
| frame_limit_and_paired_error_order_are_exact | Zero/sufficient/insufficient limits and simultaneous faults |
| repeated_validation_preserves_all_live_inputs | Constness and repeated temporary balance |
| every_measured_validation_allocation_fault_cleans_up_and_retries | Independent phase/event sweep and retry |

Every non-NULL-only case first establishes genuine lower prerequisites and calls
the unchanged legal framed input. Under RED, a refusal must first compare all9
fields to UNSUPPORTED28/tokenT/other fields0, unchanged input owners/content/
addresses, no new allocation and healthy ledger. Only then print
`PRIMITIVE_FRAME_FEATURE code=28 attempts=0` and assert the exact feature message
`actual const primitive frame validation must succeed`. This is the observed RED r2
protocol:17 markers and1 PASS/17 FAIL/0 ignored were actually observed. Later
GREEN predicates and the validation fault sweep remain unexecuted.

## Diagnostics, mutation and geometry bounds

Assert the frozen orders1–15 and all9 fields, including inactive fields and
actual lower locations. Restore each mutation and disarm before assertions or
cleanup. Malformed geometry remains readable; checked overflow tests must refuse
before oversized traversal, without advertising fictitious huge allocations.
Earlier Core Verify refusal wins over later wrapper branches where applicable.

Observe bounded logical fields, live allocator record IDs and numeric addresses.
Same-input constness may compare initialized representation before/after one
call; clone equality excludes padding/spare bytes/capacity equality. Spare-frame
tests require genuinely allocated readable spare storage. Never grow capacity
over a smaller allocation or dereference producer pointers after release.

## Independent allocation sweep and strict teardown

The same generic ledger instruments Core15 plus Parser Attach/shared-validator/
frame_layout allocations and frees. Lookup remains the real allocation-free
provider. Generic allocator32768 records/65536 events has no dynamic bookkeeping
or Unity callbacks while armed. Producer preparation stays unarmed and live.

Measure actual Core Verify scratch, request-values, packed-layout and whole
validation phase counts. Core88/generic61/Function61 are unrelated prerequisites,
never this denominator. Layout performs four eager malloc calls before checking
NULL: completed attempts may exceed the failed ordinal. Scan the bounded event
range for exactly one failed event; do not assume the last event failed.

Each fault compares actual kind/count/width/bytes and all9 lower fields. Verify
faults use direct same-input Verify; packed faults use a reconstructed actual
Attach request and shifted direct LayoutPackedFrame witness. Request allocation
has exact wrapper OOM/tokenT/other fields0. Free direct and validator temporaries
before assertions, preserving all producer records and prior live count. Disarmed
retry succeeds with zero diagnostic and balanced temporary records.

Use measured N and conservative3*N*N record/event bound plus explicitly measured
extra witnesses; never Reset while producers live. No cap substitutes for an
observed path. Print completed marker only after every iteration/retry passes:
`PRIMITIVE_FRAME_FAULT_SWEEP verify=V request=R packed=P measured=N completed=N`.
All numbers must come from actual traces; none are observed yet.

Teardown disarms and frees registered owners, reporting `PRIMITIVE_FRAME_LEDGER`
errors/live before recovery. Assert healthy balance before fallback can hide a
leak. No Unity longjmp inside production/allocator callbacks, unknown-free waiver
or guessed OOM normalization is allowed.

## Pending gate scope

Root owns selected membership/FI/providers, legacy Attach/OOM migration, fresh
UBSan/assertion/settings/objects/raw logs and exact natural phase exits. Candidate
The23-source design alone does not establish a configured count. Actual MSVC7 compiles
public declaration, Attach, frame_layout, validator, main, fixture and allocator;
Parser3 use the generic FI shim and test fixture is unredirected. No link/runtime
is performed by that compile-only driver. Actual RED r2 selected23 sources/FI18 with9 providers in a51-TU configuration;
MSVC7 is compile-only, all natural exit0/zero warnings. Current Root proof/commit
remain pending; full receipts and failed r1 history are in the acceptance record.
Normal/Linux/full MSVC, genuine source/Binder/interner, registry lifetime, target
ABI, default adoption/finalized module and all47 remain OPEN.

- [API/diagnostic contract](../parser-and-semantics/primitive-frame-validation.md)
- [Pending evidence](../acceptance/primitive-frame-validation.md)

## Observed lower fixture geometry

Source values and frame slots have capacity3; their actual Core clone capacities
are4, not equal to live count. Other observed Core pools have their own capacity4. Six tested pools and frame slots have initialized
spare storage; the finite witness does not cover every spare byte of every pool.
Source-map IDs501/502 are nonzero declared field witnesses copied by actual clone.
They establish no Source/Binder provenance. RED proves real lower Attach/Verify/Clone
preparation and healthy teardown before its precise feature refusal. It proves no
validator request regeneration or fault/retry loop. Independent GREEN measurement
and complete diagnostic tests remain required.
