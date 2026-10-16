---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention_internal.h
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - tests/parser/support/ssa_source_returned_execir_fixture.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - tests/parser/support/ssa_source_returned_execir_owner_shim.h
  - tests/parser/support/ssa_source_returned_execir_frame_shim.h
  - tests/parser/support/ssa_source_returned_execir_fixture.h
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/support/ssa_source_returned_execir_cleanup.h
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/core/support/ssa_exec_ir_owner_capture.h
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
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
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.h
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
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
doc_type: acceptance-record
status: implemented-validated-finite
integer_expression_status: implemented-validated-finite
---

# Returned-source finalized ExecIR: evidence and remaining gates

## Integer-expression increment: finite functional acceptance

The integer-expression increment has independently accepted current Source r10
functional/proof/CTest, lower r3 regression and finite MSVC r5 compile-only
evidence. Whole-leaf/release completion is not claimed. The Source22
evidence below remains historical; its subsequent Source26 child-boundary
extension is recorded in the
[test guide](../testing-and-validation/ssa-source-returned-execir-retention.md).
Current expression acceptance comes from the new input-bound reviews below.

The actual feature RED is stored under
`E:/cargo-targets/zr_vm/reports/ssa-source-integer-expression-01a0fe2b/`.
Its immutable `red-receipt.json` SHA256 is
`CB0545AC31D4C7EB8D0D4467A9AE7D0649D7DE0E9B6B357DDD080A740CF49FFF`.
The subsequent `red-validation.json` review accepts **22 tests, 11 FAIL and
11 PASS**, with direct natural exit11, as a feature reproduction. The failures
are missing retained expression owners after ordinary compilation prerequisites;
they are not a controller failure. The original receipt's collection-time
awaiting-review flags are retained as written.

The RED stdout is 3347 bytes, SHA256
`EAD91E1CA8D62B6D1008D43DCDFD7CAD30460AAB1F1C31A3F040D5F96038528E`;
stderr is 560 bytes, SHA256
`6595F5401B399D79219AAACD6E5C41488F322F394F52DD2E9CEB4C5029616764`.
Ten refusal rows contain `stage=4 result=4 code=28`; the failing NULL-diagnostic
case has no diagnostic row. The RED build has 58 warnings and supplies no W0
claim. Unity's prior failure state prevents the failed cases' teardown assertions
from supplying ledger PASS credit.

The implementation now uses the genuine metadata entry token and a temporary
canonical SemIR/actual AST/dual-pool proof. It checks independent `V`/`P` geometry,
ORIGINAL and COMPACTED function maps, real Core/Frame verification and a dynamic
span witness before candidate replacement and final owned snapshot publication.
The input-bound r8 native protocol and lower reviews below provide the actual
finite test evidence for this implementation.

Source r10 passes expression/fault/proof and Source26/AOT25 runs plus targeted
CTest5; lower r3 also passes. Independent raw
review accepts all423 actual expression fault sites, keeper preservation,
cleanup/retry and healthy pre-recovery FINAL fields. The literal sweeps are
separately measured156/156. Finite changed-TU MSVC r5 is also accepted.
Earlier r4/r7 failures remain historical and do not become successful receipts.

The finite scope is fixed local SourceCompile input, NULL loader/resolver
callbacks and CoreGlobal metadata lifecycle. SourceExecute, FunctionExecute,
VM, import/plugin, network and FFI execution are not exercised. Current fullW0,
full MSVC, Linux, security, release and all47 acceptance remain open. The
[integer-expression guide](../testing-and-validation/ssa-source-integer-expression-retention.md)
records the finite accepted protocols and remaining gates without closing a leaf plan.

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
[integer-expression guide](../testing-and-validation/ssa-source-integer-expression-retention.md)
for the finite evidence and remaining gates. Fixed local SourceCompile and NULL
loader/resolver callbacks remain the entry; SourceExecute, FunctionExecute, VM,
import/plugin, network/FFI and security execution remain outside scope. FullW0,
Linux, complete MSVC, release and the full47-leaf acceptance remain unproven.

## Current status and actual lower prerequisite

Current status is implemented-validated-finite for this Source handoff.
Production mode4 attaches the real Source frame, verifies the compacted function
with VerifyFunction_ALL and calls the eleven-argument finalized helper. The
original twenty cases and two appended Frame OOM cases pass. Actual direct
Source22, direct AOT25 and the exact two-test CTest selection terminate naturally
with exit0. This is finite functional evidence; release and whole-leaf acceptance
remain open.

Current evidence is stored under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-current-direct/`.
Its `receipt.json` has SHA
`1AA01EE774524549B5D8C7160ACCDA311791A8F5F63D82BE68607E52165EBC55`;
`runtime-validation.json` has SHA
`DC730FF6F1FB4A5F93E31383BC7FF19DEE539C26242D6481BDC364CF400ECA52`.
The latter admits Source22/AOT25/CTest2, the measured sweeps and the exact child
protocols. Independent SourceSol review accepts the current raw protocols,
dependency and provider evidence, with no extra native run. Both records retain
full_current_w0=false and release_accepted=false.

Validation uses the independently recorded physical inputs of the shared
checkout, including external migrations. Finite commit scope excludes foreign
changes; this validation does not establish a clean full build from the finite
commit alone. Current input/dependency and refresh records define the measured
scope, with commit evidence recorded separately.

The independently accepted lower helper is committed as
`d475d47494d5b7566e258dd379466ca099b3da6f`. Its [acceptance](source-finalized-module.md)
records24 PASS, one measured/completed89-site declared-profile sweep and16 lifetime
cycles. Its actual receipt `source-finalized-module-green-r2-receipt.json` has SHA
`9C476B61A4DD008501866C867CB188C91DD9340643522E4B7710F187DD0990F4`.
Those are lower finite observations, not genuine Source/Binder/bootstrap proof
or a returned Source allocation denominator. Function association commit
`112cbf4256a819d554659c7bdfd9d77eb8a04610` and generic owner commit
`3496923f58c6f4be2bb61c82c51a7743bef9fba4` remain separate prerequisites.

## Current finalized handoff contract

After the genuine bridge/Build/Bind/compaction/layout sequence, implemented mode4
runs AttachPrimitiveSourceFrame and VerifyFunction_ALL, then calls the real private
`source_snapshot_finalized_primitive_module` once. The helper receives actual
SemIR/context/AST/identity, Compiler and Function constant pools, original module,
selected finalized compacted function and the selected owner-slot address.
The typed observer passes all eleven arguments unchanged; no reconstructed
Function, original-only snapshot, fake graph or double snapshot is permitted.

Complete original module pools/contracts/identities combine with the genuine
finalized selected function through the real replacement copy and owned clone.
Original constant indices persist. Separate original/finalized/published logical
witnesses and output-record IDs prove their distinct roles while live. Numeric
canonical tokens do not establish registry ownership or runtime admission.
metadataTokenRecords checks apply to successful returned identity only; complete
failure-before/after record preservation is not measured by this fixture.

Failure must preserve the full lower9 and real mapped Source range; Attach/Verify
uses FRAME, finalizer refusal EXECIR_OWNER. The helper frees candidate storage
before producer descriptor/projection/compacted/original cleanup, then caller
Function/CompilerState/AST teardown. Nonasserting observers save errors. Close
saves actual VM and pre-recovery ledger health, finishes recovery and only then
asserts the first report; resetting or recovery cannot erase a failing lifecycle.

## Historical finalized diagnostic attempts

All report paths below use
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/`.

- Historical finalized REDr3 receipt `source-returned-finalized-red-r3-receipt.json`,
  SHA `177BE2AA3AA2EBF487EFAF0FD81B65C9A12E3BE772F9EAB5DCD3DD0AD9147819`,
  is false: actual771 compiles/two links finished naturally0, but strict build
  diagnostics stopped the gate before CTest. It does not prove functional RED.
- Diagnostic r1 receipt `source-returned-finalized-functional-red-diagnostic-r1-receipt.json`,
  SHA `BD5719C126A4E35B795C2BADB487EDD64C72513DE6E03942F283750EBEEB592C`,
  is false. Controller34828/session26141 naturally exited1/closed; configure and
  deps-before-build exited0, then actual utf8proc_data.c text include lacked an
  explicit repository pin. No diagnostic build, Source20 or AOT25 ran in r1.
- Diagnostic r2 receipt `source-returned-finalized-functional-red-diagnostic-r2-receipt.json`,
  SHA `11A12127C3061CAFA74424DFDA7CDFE13A32FB6DB1DA30E41D8844BC0EF114B7`,
  is false. Controller6152/session63744 naturally exited1/closed, terminal64c8d3.
  Configure24924/deps4480/plan19348 naturally exited0 with both streams drained.
  The guard rejected a standard forced VerifyGlobs dry-run preview; no build or
  runtime ran. Root read the stored Core223/Library40/Parser460 glob membership
  unchanged, restat1 and mismatch-only stamp behavior; metadata9 stayed unchanged.
  Those observations do not promote the false orchestration receipt. Its freeze
  `E:/cargo-targets/zr_vm/tmp/source-returned-finalized-functional-red-diagnostic-r2-freeze.json`
  SHA `EC925D56773346B07FD37F1932E63A991CB639A457B1273A26EA0D7EA96E9A21`
  admits806 repo/4755 headers/11 tools and the current17-target graph with Parser460.
  It explicitly pins the one included C dependency. The r2 helper hash is
  `3330A500A6594B17942C3DB9522AC732DAD467ACBB552AD2417072B6BBA19DF8`.
  Same-tree regeneration/actual incremental rebuild must be recorded without
  claiming a fresh full rebuild. Diagnostic warning recording never accepts
  releaseW0; the diagnostic helper always keeps release_accepted false.
- At that historical checkpoint diagnostic r3 was pending. Its narrow preview
  correction did not waive actual build regeneration/CXX/glob-mismatch refusal
  or14 required real compilations. This is not a current Source22 result.

The historical expected diagnostic protocol was Source20=11PASS/9 specified featureFAIL,
zero ignored, Source child9; AOT25 allPASS/child0; exact CTest selection8.
Cases5–7/15–20 require `RETURNED_EXEC_MISSING_FINALIZED_OWNER`, nine real
post-layout records append1/finalizer0 and healthy FINAL. This paragraph defines
historical expected gates; it does not claim r2 or that pending attempt reached them.
It supplies no current Source allocation denominator.

Later r3/r4 helper failures also remain failures in the historical record.
Verifier false rejections concerned Ninja dependency and archive-member
relative-path handling, and the exact CTest environment preamble. Corrected metadata
interpretation reuses the authenticated original raw logs; those native phases
were not rerun to replace their evidence. In particular, CTest's generated AOT
UBSAN_OPTIONS property accounts for its exact two environment-preamble rows.
These corrections do not erase prior orchestration failures or waive runtime
and physical-input checks.

## Historical generic-snapshot REDr3 (not finalized Source evidence)

This section and its warning history describe the superseded generic profile.
Its future-branch and provider requirements belong to that historical checkpoint.

The primary receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-returned-execir-red-r3-receipt.json`,
SHA `F229C8F2F7361B3A076E373C4B0401F5AEAE9C58684E5DA609E2825D00B47526`.
Its physical receipt is validated true. Raw source CTest log SHA is
`E033C603EDE1D1F93C8AE3488E20AD7B6CF360E7787EE138B0DD0A5FEEACA296`.
At that historical checkpoint Root's provenance proof and RED commit were pending.

Historical default20 was11PASS/9FAIL/0ignored. Every failure has the exact message
`actual source wrapper must return retained ExecIR Function`. Cases1–4 and8–14
PASS; cases5–7 and15–20 FAIL. Nine observations show actual successful layout
append followed by refusal: eight stage14/result4, one NULL-diagnostic
stage0/result0 observation. Every record has append1/snapshot0. Thus actual
owner Snapshot calls are0 and no source-derived owner-fault denominator is
measured. Actual source/support logs contain no ledger-error or UBSan diagnostics.
This was the older post-layout generic-snapshot refusal. It is not current finalized Source RED/GREEN evidence.

Eight native phases terminate naturally: configure36236/build40020, four
archive phases11088/2716/32900/2804 and support29624 exit0; source30152 exits8.
PID2804 is read from the physical receipt. Separate support output establishes
returned AOT25, Function20, generic14 and FunctionAOT12 all PASS. Function
replacement0/1 each measured61/completed61 and generic self-view0/1 each
measured61/completed61/direct-clone60 remain lower support observations; they
are unrelated to the unmeasured source snapshot window.

The actual compile graph has752 entries. Selected allocator FI applies to20
Core15/Parser5 objects; retention, descriptor and owner are separately selected.
Residual Core ordinary222 removes exactly16 to206. Residual Parser ordinary437
removes exactly7 to430. Both receipts report rebuilt indices and exact full
member-path matches. The RED provider proof checks actual used CloneFunction;
future GREEN must additionally check CloneModule after the snapshot becomes used.

Finite MSVC REDr1 compiles10 units, all natural exits0/zero warnings, compile-only.
Receipt `source-returned-execir-msvc-red-r1-receipt.json` SHA is
`7CB400EA5E379425BAEBA9CCAAD78F02C93FF2367D81F23FBF89873EF2352FD5`.
It does not establish full MSVC or runtime behavior. Fresh CoreModel REDr2
receipt `function-execir-owner-core-model-red-r2-receipt.json` SHA is
`1DFBCB98E7B41087EA2ACEC6D18244648348FB739A5C4C9854BDB2C316722FB6`;
original28 Function/60 Module fault loops PASS as separate lower support.
All receipt/log paths in this section share the report directory above.

## Historical generic failures and warning limits

REDr1 build actually exits0, but59 warnings across18 files prevent acceptance.
Its failed receipt `source-returned-execir-red-r1-receipt.json` SHA is
`59369F18E8FE5936BA4CF5ED37AC889571BC6BA98C1AD20773152C7D63F7E61C`.
Warning audit `source-returned-execir-red-r1-warning-audit.json` SHA is
`0C8FBD40E21E7FAAF78F6ADA2D76697E7B7E410EA60EAF54717B79E70AAA0EF6`.
No newly leased source-warning headers were changed. These persisted59 warnings
are not erased by r3's incremental zero new diagnostics; a full clean build
with zero warnings remains unproved.

REDr2 wrongly required CloneModule, which is stripped from the RED route before
an owner snapshot call exists. Failed receipt
`source-returned-execir-red-r2-receipt.json` SHA is
`B6C405FAB6C66C981183E6FE848DA1728C457A5CCBA049A0DE51304048D69897`.
The provider check was corrected to actual used CloneFunction for RED without
changing the C source or tests. Both failed attempts remain historical failures.

## Current finite functional GREEN evidence

The twenty original names/order remain unchanged and all pass, including genuine
original/source facts, independent finalized owner storage, AOT coexistence and
replacement, actual free/shutdown and16 alternating lifecycles. Literal9 and
literal8 each independently measure156 full-helper allocation sites and complete
156 injected refusals. The checks compare real request geometry and same-input
direct lower refusals, release candidate/output records and retry disarmed in
fresh balanced Globals. Full lower9 and real Source ranges remain exact. These
Source denominators are measured here; lower89/88/61 counts do not supply them.

The actual graph selects eighteen roles,786 TUs and29 FI TUs, including Core
private15+copy1, Parser private8 and Frame2+producer1. The initial build performs
775 real compilations and two links. After shared input changes, the actual
refresh performs27 compilations and two links, retains759 exact cached objects
and validates811 unique current dependency paths. Current command, dependency,
symbol and archive evidence binds this graph. Residual inventories remain
Core17/Parser12; Function16 is configure-only. Ordinary Library and genuine Global
metadata initialization/shutdown remain real dependencies. Linked provider
presence alone does not establish callback execution.

Both appended values/header cases pass with one genuine allocation failure each
inside AttachPrimitiveSourceFrame. FRAME/OUT_OF_MEMORY, all lower diagnostic
fields, preserved prepublication input and cleanup before Global shutdown are
checked. Each disarmed fresh-Global retry publishes the real frame, reaches the
finalizer and releases its owner exactly once. Direct Source reports22 PASS,
0 failures and0 ignored; AOT reports25 PASS; CTest passes exactly those two tests.
The unchanged feature/sweep prefix plus the two Frame retries yields341 FEATURE
rows. Exact values/header FRAME_OOM rows each report failed=1 retry=1. FINAL
reports closed=1 support=1 and zero VM/owner live counts and observer/ledger errors.

The initial build emits61 warnings and the refresh22 warnings. Neither is W0.
Four child NULL/zero cases injected through this Source route, full strictW0
and Linux/full MSVC remain open. The four
lower AST empty-child checks are separately validated at
`963a81af3502d11361cab3dd64a9c0b9a5dc46a7`; they do not close the Source injection
gate.

## Open scope

The current finite Source finalized handoff passes its measured functional gate.
Semantic/default canonical adoption, full Function or Source family completion,
registry durability, runtime admission and VM execution remain unproved here.
Capacity normalization is allowed; spare bytes/padding are not logical content.
Linux/full MSVC, normal-project runtime, backend/native parity, GC/unwind and all47
SSA leaves remain OPEN. No new imports, compile-time/macro programs, SourceExecute,
Oracle, returned Function call, network/FFI, security or generated native execution
is authorized or credited here. Existing asserting Compile wrappers do not prove
prior-failed Unity Source entry or ignored-test behavior.

- [Finalized producer contract](../parser-and-semantics/ssa-source-returned-execir-retention.md)
- [Preserved twenty plus Frame cases and closure](../testing-and-validation/ssa-source-returned-execir-retention.md)
- [Independent lower finalizer acceptance](source-finalized-module.md)
