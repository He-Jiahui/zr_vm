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
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - tests/parser/test_ssa_source_integer_expression_retention.c
  - tests/parser/test_ssa_source_integer_expression_proof.c
  - tests/parser/test_ssa_source_integer_expression_faults.c
  - tests/parser/support/ssa_source_integer_expression_faults_fixture.c
  - tests/parser/support/ssa_source_integer_expression_faults_fixture.h
  - tests/parser/support/ssa_source_integer_expression_faults_producer_shim.h
  - tests/parser/support/ssa_source_integer_expression_faults_frame_shim.h
  - tests/parser/support/ssa_source_integer_expression_faults_graph_shim.h
  - tests/cmake/ssa-source-integer-expression-retention.cmake
  - tests/cmake/ssa-source-integer-expression-proof.cmake
  - tests/cmake/ssa-source-integer-expression-faults.cmake
  - tests/cmake/ssa-source-integer-expression-support.cmake
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_storage_internal.h
  - tests/parser/support/ssa_source_integer_expression_proof_shim.h
  - tests/parser/support/ssa_execir_builder_storage_cases.inc
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
  - tests/parser/test_ssa_source_integer_expression_retention.c
  - tests/parser/test_ssa_source_integer_expression_proof.c
  - tests/parser/test_ssa_source_integer_expression_faults.c
  - tests/parser/support/ssa_source_integer_expression_faults_fixture.c
  - tests/parser/support/ssa_source_integer_expression_faults_fixture.h
  - tests/parser/support/ssa_source_integer_expression_faults_producer_shim.h
  - tests/parser/support/ssa_source_integer_expression_faults_frame_shim.h
  - tests/parser/support/ssa_source_integer_expression_faults_graph_shim.h
  - tests/cmake/ssa-source-integer-expression-retention.cmake
  - tests/cmake/ssa-source-integer-expression-proof.cmake
  - tests/cmake/ssa-source-integer-expression-faults.cmake
  - tests/cmake/ssa-source-integer-expression-support.cmake
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_storage_internal.h
  - tests/parser/support/ssa_source_integer_expression_proof_shim.h
  - tests/parser/support/ssa_execir_builder_storage_cases.inc
plan_sources:
  - .codex/plans/20261009-ssa-source-returned-execir-retention-design-v1.md
  - .codex/plans/20261009-ssa-source-finalized-handoff-addendum-v1.md
  - .codex/plans/20261016-ssa-source-integer-expression-retention.md
tests:
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - tests/parser/test_ssa_source_returned_retention.c
  - tests/core/test_ssa_function_exec_ir_owner.c
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/parser/test_ssa_source_integer_expression_retention.c
  - tests/parser/test_ssa_source_integer_expression_proof.c
  - tests/parser/test_ssa_source_integer_expression_faults.c
doc_type: testing-guide
status: implemented-validated-finite
integer_expression_status: implemented-validated-finite
---

# Genuine returned-source finalized ExecIR tests

## Integer-expression checks: current finite acceptance

The integer-expression extension has independently accepted Source r10
functional/proof/CTest, lower r3 regression and finite MSVC r5 compile-only
evidence. The Source26/AOT25/CTest2 account below preserves the earlier
literal/child implementation. r8 separately reruns Source26/AOT25, both156-site
sweeps, two Frame faults and four child cases against its recorded current
inputs; it does not replace those historical receipts.

Actual expression RED evidence is under
`E:/cargo-targets/zr_vm/reports/ssa-source-integer-expression-01a0fe2b/`:

| Actual RED record | Observed result |
| --- | --- |
| `red-receipt.json` plus `red-validation.json` review | Configure/build/dependencies terminate naturally with exit0; direct test terminates naturally with exit11 |
| `red-direct-stdout.log` | 22 tests, 11 FAIL, 11 PASS, zero ignored; the eleven failures lack the retained expression owner |
| `red-direct-stderr.log` | Ten exact `SOURCE_I64_EXPRESSION_REFUSED stage=4 result=4 code=28` rows; the failing NULL-diagnostic case emits no such row |
| RED graph/build | 19 roles, 791 TUs, 405 compiled and 386 cached; 813 unique dependencies, 58 warnings, no recorded input drift |

The accepted feature RED is recorded by the later review; the original collection
receipt's awaiting-review flags are preserved. Failed-case teardown assertions
were interrupted by Unity's existing failure state, so the RED does not certify
their ledger health. This RED supplies no GREEN fault-sweep or mutation result. The actual r4
observations below remain insufficient for current acceptance.

The table describes the finite checks passed in r8 and Source r10. Historical r4/r7 outcomes
below remain bound to their own inputs:

| Target | Intended finite assertions |
| --- | --- |
| `zr_vm_ssa_source_integer_expression_retention_test` | 22 cases: actual I64 operator/operand graph, precedence, grouping, shared literal pool indices, source positions, independent owners, refusal and retry; NONE/NULL/zero and literal mode1 separation |
| `zr_vm_ssa_source_integer_expression_faults_test` | Two cases: measure a real successful trace, then fail each actual allocation attempt in a fresh lifecycle while preserving a separately returned keeper owner |
| `zr_vm_ssa_source_integer_expression_proof_test` | Four cases: unchanged22 mutations and proof-output/phase/scratch guards, independent measured span-growth OOM/cleanup/retry, and32 builder refusals/32 no-growth retries/5 numeric checks |

The fault fixture observes producer-other, bridge, compactor, Frame, finalizer,
candidate and owner windows. Each callback calls its real callee once and records
the innermost allocation phase. The sweep must compare actual kind/count/width/
bytes to the successful trace, observe exactly one failed allocation, preserve
the real lower diagnostic and correlate the Source stage. It requires failed
records freed once, keeper contents/address/records intact, a healthy unfaulted
retry, and both owner/CRT ledgers healthy before recovery. `INTEGER_EXPR_TRACE`,
`INTEGER_EXPR_FAULT`, `INTEGER_EXPR_SUMMARY` and `INTEGER_EXPR_FINAL` rows report
actual observations. The expression attempt count is dynamic and is not 156 by
assumption.

The mutation fixture borrows genuine Source-produced SemIR, AST, pools and both
ExecIR phases. It changes one fact, calls the real eleven-argument finalizer,
restores the exact borrowed storage before observation, and requires deterministic
refusal diagnostics, an unchanged existing owner and successful healthy retry.
The checked ORIGINAL/COMPACTED maps, independent `V`/`P` geometry and dynamic span
witness are described in the
[integer-expression guide](ssa-source-integer-expression-retention.md).

All new tests compile fixed local input using SourceCompile and CoreGlobal
metadata init/free with NULL loader/resolver callbacks. They do not execute the
returned function. Source r10 passes targeted CTest and current builder/proof
regression; lower r3 also passes. Finite changed-TU MSVC r5 compile-only
acceptance is recorded separately. The RED warnings are not W0; Linux, full MSVC, fullW0, execution/security
coverage and all47 acceptance remain open.

### Current producer and builder revision: finite functional evidence

The current scope also includes the two-file real identity producer
`compiler_script_callable_identity.c/.h` and the generic builder
`exec_ir_build.c` with `exec_ir_build_storage_internal.h`. The binary identity
candidate checks actual validated pure SemIR without allocating or fabricating
IDs. The existing literal branch, canonical interning, symbol registration,
root protection and final identity publication remain separate responsibilities;
the retention bridge still validates the actual entry token, AST and both pools.
The private builder header classifies invalid arguments, invalid storage/value
geometry, sealed state and capacity overflow before mutators. Only a genuine
allocation failure after valid growth is OOM; a false result is not blanket OOM.
Its `sourceId` identifies the semantic row. `instructionId` is zero before the
ExecIR instruction is published and becomes the actual returned ID afterward.

The earlier actual r4 run is recorded in `green-r4-receipt.json` and
`green-r4-runtime-observation.json` under
`E:/cargo-targets/zr_vm/reports/ssa-source-integer-expression-01a0fe2b/`.
Expression22, literalSource26 and AOT25 each naturally exit0 with every case
passing. The fault sweep measures/sweeps423 sites, refuses423 and retries423;
412 satisfy the full protocol and11 fail diagnostic checks. Cleanup, keeper,
retry and final ledger observations are healthy, but the fault target exits1
and the complete r4 attempt exits1. Proof and CTest did not run. These recorded
inputs precede the current builder/proof revision and do not validate it.

The earlier r7 receipt and runtime observation record 24 roles, 819 TUs,
822 dependencies and 1610 inputs: 5 compiles and 814 exact cached objects with
zero incremental warnings, not full-project W0. Expression22, literalSource26,
AOT25 and both fault tests naturally exit0. All 423 measured fault sites are
refused, retried and accepted; all 424 lifecycles close healthy with zero final
owner/VM/both CRT live/errors and VM diagnostics/dropped counts. Proof naturally exits1 with
3 PASS and 1 FAIL. Its healthy large case measures the unique 4064-byte span
growth at ordinal1 and 3840-byte scratch at ordinal30, both free1, with complete
owner/Frame/oracle and cleanup witnesses. These are actual observations, not
fixed ordinal requirements. The only failed assertion incorrectly required
scratch before span; that Unity failure prevents the third case's fault/retry.

The assertion-only correction is proof main SHA256
`047F3EABCE60BD815A8F22E3E049A409FC84C5A92524D7E1CDBAF0A203A41A50`
(61980 bytes). It uses the healthy measured order to decide whether a failed
span-growth prefix can reach scratch; it preserves unique geometry/free1,
complete nine-field diagnostics, inputs, owner checks, normal close and fresh
retry. At the r7 checkpoint this corrected case and CTest were unvalidated;
the subsequent accepted r8 result is recorded below.

### Final current Source r10 and lower r3 acceptance

`green-r10-runtime-review.json` accepts session65436, terminal `1b1d8d`, natural
exit0: nine phases, direct22/26/25/2/4 and CTest5. Current24roles/819TUs/822deps/
1610inputs use5 compiles+814 exact cached objects, incremental W0 only; current
1610input/819object/5EXE/18raw pins match. All423 fault sites/424 lifecycles and
FINAL records/frees1353/1353 are healthy. Ten direct raw files are byte-identical
to accepted r8, including literal156/156, Frame2/Child4 and all proof/storage/P0/
large-span protocols below. This CTest review uses complete nonempty-line
multisets: AOT70/literal383/expression25/fault3350/proof39, distinct from r8's
selected-protocol-row counts.

`lower-r3-runtime-review.json` accepts session74303, terminal `9d8c1b`, natural
exit0:24 PASS/89 faults/16 lifecycles;5roles/32TUs/257deps/1641inputs,2 compiles+
30 exact cached objects, incremental W0. Lower has no FINAL line: health comes
from each tearDown before Recover and AssertBalanced. Source r9/lower r2 failed
preflight receipts remain unchanged: VS imports ran, but no configure/build/run
phase was recorded. The guide gives the exact current receipt/review hashes.

### Actual r8 functional acceptance and lower regression

`green-r8-runtime-review.json` records independent ACCEPT of Root session60434,
terminal `b1b01e`, natural exit0: all nine phases, direct expression22,
literalSource26, AOT25, faults2 and proof4, plus targeted CTest5 pass. The graph
has 24 roles, 819 TUs, 822 dependencies and 1610 inputs; 3 compiles and 816 exact
cached objects have zero incremental warnings, not full-project W0. All 423
fault sites and 424 lifecycle cleanup/keeper/retry observations are healthy.
Literal156/156 sweeps, two Frame faults and four child-boundary retries pass.
The collection receipt retains its original awaiting-review fields; the
separate runtime review records the accepted raw-protocol result.

Proof main047F now passes all four cases: the unchanged22-mutation matrix,
direct proof/scratch guards, all three large-span healthy/fault/fresh-retry
lifecycles, and storage32/32/5 plus independent allocating P0. The healthy/retry
measurements remain grow4064 bytes at ordinal1 and scratch3840 bytes at
ordinal30, both free1; these are observations, not fixed test ordinals. The
failed grow has one failed event/no record, scratch is correctly unreachable,
and normal close precedes Reset and the successful fresh retry. Complete
nine-field diagnostics and full input/owner/Frame/oracle checks pass.

`lower-r1-runtime-review.json` separately accepts 24 lower metadata tests,
89 measured/completed fault sites and 16 lifecycles, natural exit0. Its five
roles/32 TUs use 257 dependencies and 1641 inputs, with 17 compiles and 15 cached
objects, zero finite-build warnings. Lower health comes from each tearDown's
pre-Recover ledger checks and AssertBalanced; the lower raw output has no
FINAL line. See the integer-expression guide for review hashes and exact
large-case diagnostics. Neither this lower result nor r8 executes a VM.

The proof target now registers four cases. Its original two cases and22
mutations remain unchanged. The independent third case compiles181 bytes with
44 literal leaves and43 ADDs, checks the actual87-value operand/source/pool/Frame
graph, and measures the unique real finalizer span-growth allocation. It uses
that observed relative ordinal for OOM injection; it never assumes ordinal2.
The fourth case implements32 storage-guard refusals,32 restored no-growth Core
retries and5 arithmetic-only growth checks. Its separate P0 positive calls the
real public `ZrParser_ExecIr_Build` with a genuine zero-place CFG and replaces a
tracked sealed old destination. It checks unchanged inputs, a cleared complete
diagnostic and one free per successful record. P0 uses real allocations and
accounts for them separately; it is outside the32/32/5 zero-allocation contract
and has its own `storage_zero_places` protocol row. This fourth case passes in
r7 and again in r8; the corrected third case also passes all three lifecycles
in r8. The third case alone uses four independent16384-word visitor buffers;
the original4096-word capture remains unchanged.

MSVC r5 has independent strict W0 acceptance:15 actual compiles/13 C/228deps/
64 outputs, session86909 natural0/`acd12c`;1822 before/after pins matched at
that run. It is compile-only, with actual ordered D/I/FI roles and dependency
admission. r1/r2 preserve zero-cl environment failures after VS imports; r3 runs
3 W0 cl phases then rejects8 unpinned headers; r4 runs5 cl phases with real
C4457 and strict CP936 decode failure. r5 genuinely fixes only three local
`capacity` identifiers to `spanCapacity`, without warning or encoding waiver.
Later foreign Core EOF comments are outside the owned36 paths and selected
15 C/228deps; the historical1822-pin match is not claimed globally current.
The guide preserves all failed attempts and the accepted r5 pins. See the
[integer-expression guide](ssa-source-integer-expression-retention.md)
for the finite evidence and remaining gates. Fixed local SourceCompile and NULL
loader/resolver callbacks remain the entry; SourceExecute, FunctionExecute, VM,
import/plugin, network/FFI and security execution remain outside scope. FullW0,
Linux, complete MSVC, release and the full47-leaf acceptance remain unproven.

## Current evidence boundary

Current status is implemented-validated-finite for the Source handoff and four
child AST boundaries. Production mode4 uses AttachPrimitiveSourceFrame,
VerifyFunction_ALL and the eleven-argument finalized helper. Actual direct
Source26 and AOT25 pass, and the exact two-test CTest selection passes; all three
phases exit0 naturally. The original twenty cases, two Frame OOM cases and both
156-site sweep protocols are preserved, followed by four child refusal/retry
cases. This is no release, whole-leaf or all47 acceptance. Earlier r1/r2 input-pin
and VerifyGlobs failures, r3/r4 helper failures and generic Source REDr3 remain
historical; no failed receipt is rewritten.

Current evidence is under
`E:/cargo-targets/zr_vm/reports/ssa-source-child-boundaries-01a0fe2b/`:

| Record | SHA256 | Finite evidence |
| --- | --- | --- |
| receipt.json | `2AFCA16CBE0C70459B490126BA00CC9418A4D5CE2734F3E0E137B09B3C47A0FF` | Eleven native phases naturally exit0; current build/dependency/runtime records |
| runtime-validation.json | `CB375DF69C3843ABE628B225C7EFCB2A47EC0BB5F05A4ABB7BD96136BC334872` | Source26/AOT25/CTest2, exact ordered protocols and healthy FINAL |
| symbols-check.json | `863BF1A1C230B2B67BED5920070238B2E0DAFA9F49EC7DEA41358D06007095FA` | Six metadata phases naturally exit0;15 changed objects,772 exact cached objects,27 providers |
| msvc-validation.json | `7FAEF779E20424BA1FB26E249A4B69C802C6A05AD5B9E2252713DE28E00A5D0F` | Two current TUs compile-only exit0; fixture W0, main has two unchanged prior-body warnings |

Source stdout is3470B/SHA
`9EA569CDEAA3A01EA046E23C6EFFB1A109BB1419D0F45B861C207B0AEA10C29D`;
stderr is25376B/SHA
`4705EC80CA0D6C56F7CAE089EBE5395AE7B5A0EF35ED9F154F837F456F8A25D2`.
The last build-r3 actually compiles10 objects, reuses777 cached objects, emits no
new warnings and has no measured input drift. This incremental result is not a
fresh full build or a whole-byte atomic snapshot of the shared checkout.

The preceding Source30 delivery's historical Source22 raw logs and validation are under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-current-direct/`.
`receipt.json` SHA is
`1AA01EE774524549B5D8C7160ACCDA311791A8F5F63D82BE68607E52165EBC55`;
`runtime-validation.json` SHA is
`DC730FF6F1FB4A5F93E31383BC7FF19DEE539C26242D6481BDC364CF400ECA52`.
Ninja dependency and archive-member relative-path handling, and the exact CTest
environment preamble were corrected in the metadata verifiers using the same
authenticated raw logs. Those native phases were not rerun. The two generated
AOT UBSAN_OPTIONS preamble rows are checked separately from child output.
Independent SourceSol review accepted those physical dependency/provider
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

### Historical Source22 protocol

The previous delivery's actual direct result was Source22 PASS/0FAIL/0ignored and child0; independent
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
The historical n9/n8 values were156/156; each sweep measured156 and completed156.
Thus that observed FEATURE count was341. Both Frame OOM rows reported failed=1 retry=1.
FINAL reports closed=1 support=1 with vm_live=0 vm_errors=0 owner_live=0
owner_errors=0 observer_errors=0. Direct and CTest child protocols agree.

### Current Source26 child-boundary protocol

Four Source cases now follow the historical22 without changing its case names,
order or observer prefix:

| Case | RUN_TEST name | Temporary mutation at the genuine finalizer call |
| --- | --- | --- |
| 23 | test_source_child_statements_null_cleanup_retry | SCRIPT statements header becomes NULL |
| 24 | test_source_child_statements_empty_cleanup_retry | statements count becomes0; real nodes allocation and capacity stay unchanged |
| 25 | test_source_child_statement_null_cleanup_retry | The sole RETURN child in nodes[0] becomes NULL |
| 26 | test_source_child_expression_null_cleanup_retry | The real RETURN expression child becomes NULL |

The typed `ZrTests_ReturnedExec_Finalized` wrapper in
`tests/parser/test_ssa_source_returned_execir_retention.c` calls the real
eleven-argument finalizer with its genuine arguments. The fixture's
`ZrTests_ReturnedExec_ChildBegin/End` in
`tests/parser/support/ssa_source_returned_execir_fixture.c` surround that call:
they inject only these four shapes, record the live SCRIPT/header/nodes/RETURN/
integer addresses, selected destination and actual entry token, poison the lower
diagnostic, check the armed input bytes, and restore the exact saved child field
before returning to compiler/AST cleanup. No Unity
assertion occurs while a mutated AST could escape to cleanup. The nodesNULL/count1
malformed-range case is a separate earlier guard, not the legal count0 shape.
Count0 is valid array geometry but unsupported by this sole-RETURN retention
profile; all four cases must refuse without publishing an owner.

The main records actual producer windows, input preservation and output-record
state while their borrows are live. It proves all candidate records released
before shutdown, then completes real Global shutdown and observes pre-recovery
health before assertions. Expected Source diagnostic is EXECIR_OWNER/UNSUPPORTED,
hasExecIr=true, with lower UNSUPPORTED and the actual nonzero entry functionToken;
the other seven lower fields and unrelated Source/range/AOT fields are zero.
All nine lower fields and the full Source diagnostic are compared. Saved numerical
witnesses survive cleanup; stale AST/owner pointers are not dereferenced afterward.
Each case then opens a fresh Global, performs a disarmed successful literal9
retry, observes real owner publication and exactly-once release, and closes it.
These are semantic refusals, not allocation failures; zero total allocations is
not a required assertion. The separate lower checks at
`963a81af3502d11361cab3dd64a9c0b9a5dc46a7` remain historical lower evidence rather
than a substitute for these actual Source injections.

Current stdout requires26 unique ordered PASS rows, footer
"26 Tests 0 Failures 0 Ignored ", OK and the exact healthy FINAL row.
After the historical22 stderr prefix, each child contributes one successful
retry FEATURE followed by its exact row, in this order:

    RETURNED_EXEC_CHILD_AST site=statements_null refused=1 restored=1 retry=1
    RETURNED_EXEC_CHILD_AST site=statements_empty refused=1 restored=1 retry=1
    RETURNED_EXEC_CHILD_AST site=statement_null refused=1 restored=1 retry=1
    RETURNED_EXEC_CHILD_AST site=expression_null refused=1 restored=1 retry=1

There are345 FEATURE rows (`33+n9+n8`), two completed156/156 sweeps, two Frame
OOM rows and four child rows. The exact stderr union is353 lines with no unknown,
missing or duplicated rows. FINAL has closed=1 support=1 and zero VM/owner/observer
errors/live counts. AOT25 retains its exact42 diagnostic rows. Direct Source,
direct AOT and the exact CTest2 stream unions agree and all exit0 naturally.

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

The current selection has18 roles/787 TUs/29 FI TUs and813 unique actual dependency
paths. It uses15 private Core owner
providers plus one private module-copy
TU,8 private Parser providers,2 private Frame providers and one observed producer.
The ten parser-side providers remain build, state_maps, dead_source_places,
source_frame, source_frame_validation, frame_layout, finalized_module,
finalized_module_validation, callable_query and canonical_type_lookup. Only the
two source_frame objects move from private_parser to private_frame. Historical
static784-TU and previous786-TU expectations are superseded by the actual787-TU graph and current
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
The preceding Source30 direct build performed775 compilations and two executable
links. After shared input changes, its actual refresh performs27 compilations
and two links, with759 exact cached objects and811 unique current dependency
paths checked VALID. That historical graph and symbol/member evidence covered all18
roles/786 TUs. No fresh full rebuild or atomic shared-checkout snapshot is claimed.
The initial build emits61 warnings and the refresh22 warnings. The complete
build warning audit retains three coordinate migrations and the checkpoint
size1208-to1232 message change. These are observed diagnostics, not W0; neither
the historical62-warning budget nor its fourteen coordinate migrations grants
fullW0 or release credit. The current child-boundary task adds actual incremental
build/dependency records and15-object symbol checks, current linked maps and four
archive inventories: ordinary/residual Core229→212 and Parser465→453, preserving
the exact17/12 exclusions and27 provider identities. The15 changed objects plus
772 exact cached objects account for all787 selected TUs; the final build-r3's
10 compilations plus777 cached objects is a different incremental phase, not a
claim that all15 symbol-checked objects compiled in that last phase.

Current MSVC validation compiles only the Source main and its Source ExecIR
fixture. Both exit0; the fixture has no warnings. The main retains two warnings
in unchanged prior bodies: C4389 at line260 and C4702 at line563. This supplies
neither an MSVC executable run nor fullMSVC/fullW0/Linux acceptance.

Current execution credit covers Source compilation and real Global metadata
initialization/shutdown. SourceExecute, returned Function execution, VM execution,
imports/plugins, network/FFI and security probes are not executed or credited.
The finite handoff is functionally GREEN; full strictW0, Linux and complete MSVC
validation remain required.

The lower [finalized-module gate](../acceptance/source-finalized-module.md)
proved24 cases/89 faults/16 declared-profile lifetimes at commit `d475d47494d5b7566e258dd379466ca099b3da6f`.
Those finite lower observations and independent AOT25/Function20/generic14 gates
do not supply the Source denominator or its functional credit; the current
Source26 run supplies its own evidence above. The four Source child NULL/zero
cases are now covered in this finite profile. Full Source/family migration, registry durability,
Linux/full MSVC, normal project, backend/runtime parity and all47 stay OPEN.

- [Finalized producer contract](../parser-and-semantics/ssa-source-returned-execir-retention.md)
- [Finite functional acceptance and remaining gates](../acceptance/ssa-source-returned-execir-retention.md)
