---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_module_contract.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
  - zr_vm_parser/include/zr_vm_parser/aot_ir_projection_descriptor.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_projection_descriptor.c
  - tests/parser/test_ssa_source_aot_descriptor.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_module_contract.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_module_contract.c
plan_sources:
  - .codex/plans/20261006-ssa-source-module-contract.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_source_aot_descriptor.c
  - tests/acceptance/ssa-source-aot-descriptor.md
doc_type: module-detail
status: finite-source-module-contract-green
---

# Real Source Module Contract and AOT Descriptor

## Purpose and call position

`ZrParser_ExecIr_BindSourceModuleContract` consumes a const current
compiler and the actual single-entry Core BuildModule result. It binds the
original module contract and the original entry function's module hash before
compaction. It supplies a finite missing prerequisite for the existing
`ZrParser_AotIrProjection_BuildDescriptor`, rather than modifying projections
or fabricating module identity. Actual RED is committed; the r2 functional gate passed 55 cases. MSVC compiled both changed TUs with zero warnings; full MSVC matrix remains OPEN.

The unchanged shared literal fixture produces real 9/8 source
metadata and module identities. ModuleInit leaves module contract versions
absent; ModuleAddFunction leaves the function's contract.moduleHash absent.
Compaction, frame attachment and canonical projection preserve those omissions.
The existing descriptor builder requires versions and exact module/function
binding. The call sequence is original Prepare/BuildModule → binder
→ compaction → actual host row/frame → canonical projection/host target →
BuildDescriptor → Core ValidateModule → RequireExecutableAbi.

## Three distinct hashes and contract facts

| Candidate field | Actual source and validation |
| --- | --- |
| versions | existing schema/ABI/logical definitions 6/17/1 |
| targetToken | unique actual MODULE token with nonzero RID, matching module |
| moduleHash | actual source moduleSignatureHash, recomputed through the existing `zr.md.script.entry.v1` helper |
| signatureHash | unique MODULE row signatureHash, validated against complete paired MODULE/SIGNATURE blob |
| layoutHash | zero: no separate physical module layout in this slice |
| generation | actual first-build entry contract generation 1 |
| capabilities/effects/reserved | zero for this pure source shape |

SCRIPT ABI hash, MODULE blob hash and canonical callable structural hash are
three distinct identities. They cannot substitute for one another. The module
layout hash is not the entry frame hash; an existing nonzero module layout is
refused, never erased. Actual generation is unrelated to source moduleVersion
(which remains NULL) and runtime metadataGeneration.

The live canonical callable and source SCRIPT_ENTRY MEMBER_DEF/signature pair
must agree with original BuildModule entry token, signature and generation.
Metadata record/heap/container ranges, products and address spans must be
checked before scanning. Complete blob validation and existing signature/hash
helpers provide the witness; numerical checks do not prove arbitrary memory
readability. All source and reachable owner storage must remain independently
valid, aligned, readable and live throughout the call.
The new header specifies `ZrCore_ZrpMetadata_ValidateSignatureBlob` and
`metadata_signature_hash_v1` for paired MODULE and SCRIPT_ENTRY blobs, then
independently recomputes the SCRIPT entry ABI through
`compiler_script_entry_metadata_hash`.

## Publication, diagnostics and ownership

All fallible checks precede exactly two final assignments: module.contract
receives the candidate and the sole original function.contract.moduleHash
receives the actual entry ABI hash. This is the binder publication boundary. Existing candidate fields may be zero
or equal; nonzero contradictions reject. Entry versions/generation/token and
signature must already be genuine BuildModule facts. Success is idempotent and
preserves pointers, source maps and owners. Failure preserves every source,
compiler, module and function byte. Optional writable diagnostic storage cannot
alias any source/module storage; callers must not reset, grow, free or mutate
the snapshot concurrently.

The diagnostic mapping uses existing ExecIR codes: INVALID_ARGUMENT
for null required objects; UNSUPPORTED for source shape; INVALID_RANGE for
numeric container/record/blob ranges; MODULE_MISMATCH for module identity or
binding; TARGET_MISMATCH for entry target; SIGNATURE_MISMATCH for canonical or
blob/signature disagreement; VERSION_MISMATCH for versions/reserved fields;
STALE_GENERATION, LAYOUT_MISMATCH and existing effect/capability codes for their
contradictions. The final header and real refusal tests use this mapping.
Witnesses use actual expected/actual values and actual function token once known.
Version diagnostics use expectedVersion/actualVersion; generation and hash
witnesses remain full width in expectedHash/actualHash. A reused hash helper
returning zero yields SIGNATURE_MISMATCH with actualHash zero and a real
published witness/nonzero requirement. Zero does not diagnose OOM or identify
the helper's internal cause, and is never converted to one. An already
published source moduleSignatureHash of zero is MODULE_MISMATCH instead.

The existing compiler script-entry hash helper may allocate temporary provider
storage; allocation failure is normal and all temporary storage must be freed.
No ownership transfers, retained source pointers or cancellation path are added.
Opaque helper allocation failure was not injected. Failure preservation covers
the binder source/module observations; no allocator bookkeeping rollback or
function-body equivalence promise is made.
Its compiler argument is const-qualified in its declaration and definition;
the helper body reads the compiler. The producer must reuse its domain and
existing signature validation, without cloning encoding or casting away const.

## Descriptor lifetime and finite boundary

Descriptors borrow actual projection views. Their source/projection owners
must stay live; descriptor release precedes projection, compacted graph,
original fixture and runtime teardown. Real CONSTANT/NOP/RETURN, instruction
IDs, source maps, row/frame tables and empty state-map identity remain intact.
Projection.runnable remains false. RequireExecutableAbi validates a contract;
it does not establish native or backend execution.

The [test guide](../../tests/acceptance/ssa-source-aot-descriptor.md) and
[acceptance record](../acceptance/ssa-source-aot-descriptor.md) preserve
actual prerequisite, committed RED and functional GREEN evidence. Real-frame scalar emission,
source/Oracle/backend/native execution, normal returned artifact retention,
Linux, full MSVC matrix, full 07.01 migration and full SSA47 remain OPEN.
No network/import loader, FFI, provider, capability or security execution is added.
