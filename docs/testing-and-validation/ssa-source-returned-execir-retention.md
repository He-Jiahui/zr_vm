---
related_code:
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - tests/parser/support/ssa_source_returned_execir_fixture.h
  - tests/parser/support/ssa_source_returned_execir_fixture.c
  - tests/parser/support/ssa_source_returned_execir_owner_shim.h
  - tests/parser/support/ssa_source_returned_execir_frame_shim.h
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/parser/support/ssa_source_returned_execir_cleanup.h
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/cmake/ssa-source-returned-execir-retention.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - tests/parser/support/ssa_source_returned_execir_fixture.c
  - tests/parser/support/ssa_source_returned_execir_owner_shim.h
  - tests/parser/support/ssa_source_returned_execir_frame_shim.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/core/support/ssa_exec_ir_owner_capture.c
plan_sources:
  - .codex/plans/20261009-ssa-source-returned-execir-retention-design-v1.md
  - .codex/plans/20261009-ssa-source-finalized-handoff-addendum-v1.md
tests:
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - tests/parser/test_ssa_source_returned_retention.c
  - tests/core/test_ssa_function_exec_ir_owner.c
  - tests/core/test_ssa_exec_ir_owned_module.c
doc_type: testing-guide
status: implemented-validated-finite
---

# Genuine returned-source finalized ExecIR tests

## Current evidence boundary

Current status is implemented-validated-finite for the Source handoff.
Production mode4 uses AttachPrimitiveSourceFrame, VerifyFunction_ALL and the
eleven-argument finalized helper. Actual direct Source22 and AOT25 both pass,
and the exact two-test CTest selection passes; all three phases exit0 naturally.
The twenty existing RUN_TEST names/order and both original sweep protocols are
preserved, with two passing Frame OOM cases appended. This is no release or
whole-leaf acceptance. Earlier r1/r2 input-pin and VerifyGlobs failures, later
r3/r4 helper failures and the older generic Source REDr3 remain historical.

Actual raw logs and validation are under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-current-direct/`.
`receipt.json` SHA is
`1AA01EE774524549B5D8C7160ACCDA311791A8F5F63D82BE68607E52165EBC55`;
`runtime-validation.json` SHA is
`DC730FF6F1FB4A5F93E31383BC7FF19DEE539C26242D6481BDC364CF400ECA52`.
Ninja dependency and archive-member relative-path handling, and the exact CTest
environment preamble were corrected in the metadata verifiers using the same
authenticated raw logs. Those native phases were not rerun. The two generated
AOT UBSAN_OPTIONS preamble rows are checked separately from child output.
Independent SourceSol review accepts the current physical dependency/provider
evidence and exact raw runtime protocols without another native run.

The measured inputs are independently recorded shared-checkout files, including
external migrations. Finite commit scope excludes foreign changes and this run
does not establish a clean full build from the finite commit alone. Actual initial
build, current-input refresh and latest dependency records delimit this evidence.

The historical expected diagnostic RED was11 PASS/9 specified feature FAIL/0 ignored: cases5–7
and15–20 fail only with `RETURNED_EXEC_MISSING_FINALIZED_OWNER`; other cases PASS.
The Source child must actually exit9, the independent AOT25 child0 and the exact
two-test CTest selection8. Those historical expected values are retained here
without promoting failed orchestration receipts into functional RED evidence.
No build, cleanup, observer or setup failure may substitute for a feature FAIL.

## Preserved twenty-case protocol

| Case | RUN_TEST name | Required observation |
| --- | --- | --- |
| 1 | test_none_nine_returns_without_owners | Ordinary9 returns no owner |
| 2 | test_zero_eight_and_null_options_nine_without_owners | Zero8/NULL9 remain NONE |
| 3 | test_aot_nine_returns_only_aot | AOT9 only |
| 4 | test_aot_eight_returns_only_aot | AOT8 only |
| 5 | test_execir_nine_survives_actual_producer_cleanup | Finalized9 owner survives cleanup |
| 6 | test_execir_eight_preserves_actual_source_metadata | Finalized8 keeps genuine identities/maps |
| 7 | test_execir_null_diagnostic_preserves_content | Optional diagnostic preserves behavior |
| 8 | test_unknown_two_refused_before_preparation | Unknown2 refused early |
| 9 | test_mixed_aot_execir_refused_before_preparation | Mixed5 refused early |
| 10 | test_other_unknown_mode_refused_before_preparation | Other unknown enum refused early |
| 11 | test_execir_null_state_refused_without_producer | NULL state refused early |
| 12 | test_execir_null_source_and_zero_length_refused | NULL source/zero length refused early |
| 13 | test_execir_bool_reports_actual_constant_refusal | Genuine bool bridge diagnostic |
| 14 | test_execir_float_reports_actual_constant_refusal | Genuine float bridge diagnostic |
| 15 | test_two_returned_execir_owners_are_independent | Two independently owned outputs |
| 16 | test_returned_aot_and_execir_owners_coexist | AOT/ExecIR Functions coexist |
| 17 | test_aot_replacement_preserves_returned_execir | Later AOT replacement preserves ExecIR |
| 18 | test_function_free_then_global_shutdown_releases_execir | Real free/shutdown release once |
| 19 | test_sixteen_alternating_compile_free_lifecycles | Sixteen existing9/8 lifecycles |
| 20 | test_each_source_snapshot_allocation_fault_cleanup_retry | Measured whole-finalizer faults/retries |

The original twenty-row table above is unchanged. Two implemented cases follow it:

| Case | RUN_TEST name | Required observation |
| --- | --- | --- |
| 21 | test_source_frame_values_oom_cleanup_retry | Actual values calloc OOM, FRAME diagnostic/input preservation, cleanup, fresh Global retry |
| 22 | test_source_frame_header_oom_cleanup_retry | Actual header malloc OOM, values/packed cleanup, no publication, fresh Global retry |

Both new cases require exactly one injected failure, zero finalizer calls on
refusal, and candidate release recorded before shutdown. A successful disarmed
retry uses a fresh Global, publishes real frame/header/slots, reaches the finalizer
and observes exactly-once release. All nine lower diagnostic fields and the full
Source diagnostic are compared; callbacks defer errors without Unity assertions.

The actual direct result is Source22 PASS/0FAIL/0ignored and child0; independent
AOT25 PASS/child0 has its exact42 diagnostic rows. CTest passes exactly those
two selections with exit0. Source stdout requires22 unique ordered PASS
rows, the footer "22 Tests 0 Failures 0 Ignored ", OK and the healthy FINAL row.
The unchanged feature/sweep sequence is F^(26+n9), sweep9, F^(1+n8), sweep8,
followed by these exact stderr rows:

    RETURNED_EXEC_FEATURE returned=1 stage=0 result=0 append=1 finalizer=1
    RETURNED_EXEC_FRAME_OOM site=values failed=1 retry=1
    RETURNED_EXEC_FEATURE returned=1 stage=0 result=0 append=1 finalizer=1
    RETURNED_EXEC_FRAME_OOM site=header failed=1 retry=1

Here F is that exact FEATURE row; n9/n8 are independently measured and completed
whole-finalizer sweep counts. Total FEATURE rows must be29+n9+n8. Direct and CTest
protocols must agree without unknown, missing or duplicated observer rows.
The actual n9/n8 values are156/156; each sweep measures156 and completes156.
Thus the observed FEATURE count is341. Both Frame OOM rows report failed=1 retry=1.
FINAL reports closed=1 support=1 with vm_live=0 vm_errors=0 owner_live=0
owner_errors=0 observer_errors=0. Direct and CTest child protocols agree.
Four child NULL/zero cases injected through Source remain open. The four lower
AST empty-child checks at `963a81af3502d11361cab3dd64a9c0b9a5dc46a7` have separate
validation and do not supply this Source injection coverage.

## Actual producer observation and logical expectations

The selected producer shim observes real layout append and the private
`source_snapshot_finalized_primitive_module`. The main's typed wrapper passes
all eleven genuine arguments unchanged. The implemented producer sequence is
AttachPrimitiveSourceFrame, VerifyFunction_ALL, then that helper, as observed by
the passing current fixture. No original-only FunctionSnapshot replacement, fake
Snapshot report, module substitution or Function reconstruction is used.

Original module pools and the selected compacted/frame-attached function have
separate logical witnesses. Original instructions retain CONSTANT/PLACE_BASE/
RETURN; the finalized profile checks the genuine compacted NOP and return rows,
source maps, identities, contracts and physical frame. Published complete module
witnesses retain constants/layouts and contain the selected finalized function.
Original/source facts are checked while live rather than equating the published
function with uncompacted storage. Compiler and Function constants keep the
actual original constantPoolIndex. Padding, spare capacity and allocation bytes
are not logical equality requirements.

Successful metadataTokenRecords checks establish actual module/script-entry
identity consistency. They do not prove a full before/after record snapshot for
failed calls. Borrowed source witnesses are checked before their actual cleanup;
no stale Function, source view or producer pointer is inspected afterward.

## Whole helper records, failure window and cleanup

Producer records before the helper remain tracked and disarmed. The complete
helper attempt range includes original Verify, frame validation, replacement
copy, candidate validation and final owned clone. Transient records are freed
before return. On success the explicit output-record ID set is derived from
actual live records, and each returned module/span must map uniquely into it.
After actual Function release all those IDs must have exactly one free.

Faults arm only the real helper window. Each9/8 successful call must establish
its own measured ordered count/width/bytes trace. Each injected ordinal compares
a direct helper call using the same genuine inputs, frees its independent output,
then calls the actual producer helper with a separate destination. Saved lower9,
Source range/result/stage and original/finalized facts are compared after cleanup.
No lower89/88/61-site denominator is imported. Each attempt and retry uses a
fresh Global and a disarmed balanced lifecycle. The current Source denominators
are156 sites for literal9 and156 for literal8. These remain fault iterations
inside the existing case20, rather than additional Unity cases.

Callbacks use pure TryCapture/TryArm/TryEvent/TryRecordFrees and defer errors.
Close disarms, frees registered direct owner, performs real Global shutdown,
saves VM attempts/frees/live/errors/diagnosticCount/dropped and the first ledger
report, then runs accepted recovery support. Cleanup finishes before assertions.
Pre-recovery health and record frees must be good; recovery cannot launder health.
Init verifies the previous saved report before any Reset. Existing asserting
Open/Name/Compile wrappers do not prove compilation after prior Unity failure;
ignored paths remain OPEN.

## Current private provider and build selection

The actual selection has18 roles/786 TUs/29 FI TUs. It uses15 private Core owner
providers plus one private module-copy
TU,8 private Parser providers,2 private Frame providers and one observed producer.
The ten parser-side providers remain build, state_maps, dead_source_places,
source_frame, source_frame_validation, frame_layout, finalized_module,
finalized_module_validation, callable_query and canonical_type_lookup. Only the
two source_frame objects move from private_parser to private_frame. Historical
static784-TU expectations are superseded by the actual786-TU graph and current
command/dependency evidence.
Only Frame2 receives ssa_source_returned_execir_frame_shim.h. The observed producer
uses the typed AttachFrame/finalizer shim; all five Frame callbacks must be unique
strong Source-main providers with exact linked/importing object evidence.
Generic allocator FI stays on reviewed paired providers. Descriptor/AOT owner
keep separate CRT ledgers; cleanupC and captureC each remain included once.

Two residual roles are required: Source Core excludes17 exact members; Parser
excludes12. Function residual16 remains configured only. Ordinary Library is a
real ordinary archive. No runtime_support, SourceExecute, Oracle, network/FFI
provider substitution is admitted. Fresh command/Ninja/header/map/member proofs
establish current membership; a configured source list alone is not execution.

The historical diagnostic r2 freeze contains806 repository pins,4755 header pins,11 tool
pins and17 selected target groups, including the actual Parser460 graph and the
explicit text include utf8proc_data.c. Same-tree regeneration and incremental
recompilation are permitted. The helper records before/after object hashes,
requires changed/missing/stale inputs to truly compile and requires every selected
object's latest dependency record VALID. It never claims all objects fresh or
historical object-byte identity. Four raw-byte logs retain each phase slice,
actual argv, naturally terminal children and both drained streams. W4/UNDEBUG/
C11/MD and no-recovery UBSan stay enabled; diagnostic warnings are recorded and
release_accepted stays false. Those r2 counts and preview checks are historical.
The later r3/r4 helper gates and their failed receipts remain historical; they
are not replaced with a claim that a strict helper accepted the current run.
The actual current direct build performs775 compilations and two executable
links. After shared input changes, its actual refresh performs27 compilations
and two links, with759 exact cached objects and811 unique current dependency
paths checked VALID. The current graph and symbol/member evidence cover all18
roles/786 TUs. No fresh full rebuild or atomic shared-checkout snapshot is claimed.
The initial build emits61 warnings and the refresh22 warnings. The complete
build warning audit retains three coordinate migrations and the checkpoint
size1208-to1232 message change. These are observed diagnostics, not W0; neither
the historical62-warning budget nor its fourteen coordinate migrations grants
fullW0 or release credit.

Current execution credit covers Source compilation and real Global metadata
initialization/shutdown. SourceExecute, returned Function execution, VM execution,
imports/plugins, network/FFI and security probes are not executed or credited.
The finite handoff is functionally GREEN; full strictW0, Linux and complete MSVC
validation remain required.

The lower [finalized-module gate](../acceptance/source-finalized-module.md)
proved24 cases/89 faults/16 declared-profile lifetimes at commit `d475d47494d5b7566e258dd379466ca099b3da6f`.
Those finite lower observations and independent AOT25/Function20/generic14 gates
do not supply the Source denominator or its functional credit; the current
Source22 run supplies its own evidence above. Full Source/family migration, registry durability,
Linux/full MSVC, normal project, backend/runtime parity and all47 stay OPEN.

- [Finalized producer contract](../parser-and-semantics/ssa-source-returned-execir-retention.md)
- [Finite functional acceptance and remaining gates](../acceptance/ssa-source-returned-execir-retention.md)
