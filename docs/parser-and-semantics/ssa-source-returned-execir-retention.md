---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention_internal.h
  - zr_vm_core/src/zr_vm_core/function/function_exec_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - tests/parser/support/ssa_source_returned_execir_owner_shim.h
  - tests/parser/support/ssa_source_returned_execir_frame_shim.h
  - tests/parser/support/ssa_source_returned_execir_fixture.h
  - tests/parser/support/ssa_source_returned_execir_fixture.c
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
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention_internal.h
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
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - .codex/plans/20261009-ssa-source-finalized-handoff-addendum-v1.md
  - .codex/plans/20261016-ssa-source-integer-expression-retention.md
tests:
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - docs/acceptance/ssa-source-returned-execir-retention.md
  - tests/parser/test_ssa_source_integer_expression_retention.c
  - tests/parser/test_ssa_source_integer_expression_proof.c
  - tests/parser/test_ssa_source_integer_expression_faults.c
doc_type: module-detail
status: implemented-validated-finite
integer_expression_status: implemented-validated-finite
---

# Returned source ExecIR: finalized storage handoff

## Integer-expression extension: finite functional acceptance

The integer-expression extension has independently accepted current Source r10
functional/proof/CTest, lower r3 regression and finite MSVC r5 compile-only
evidence. Each record retains its exact inputs and scope. The earlier literal/child handoff and
its Source26/AOT25/CTest2 evidence remain recorded in the
[test guide](../testing-and-validation/ssa-source-returned-execir-retention.md);
the Source22 record below remains the historical handoff baseline. Current
expression acceptance comes from the new input-bound reviews below.

The new mode4 path retains one canonical I64 return expression built from
integer literals and binary `+`, `-` and `*`. It preserves the actual operator,
operand order, constant-pool indices and source coordinates; it does not evaluate
the expression. NONE/NULL/zero options and mode1 literal AOT remain separate
contracts whose current-input regressions pass in r8 and Source r10.

The bridge reads the genuine script-entry metadata token from the compiler's
validated module/member records. A SemIR symbol ID is not substituted for that
token. A temporary proof checks the canonical SemIR, the actual SCRIPT/RETURN
AST and both compiler/runtime constant pools before the existing producer builds
ExecIR. The proof checks every pool entry's INT64 tag before reading its payload;
equal literals can share one genuine pool index.

Proof geometry uses the actual counts `I` instructions, `V` semantic values and
`P` temporary places. It does not assume `P == V`. One aligned scratch block holds
the AST map, place IDs, two instruction maps and visits. The ORIGINAL function
has `V + 2*P` value rows; COMPACTED has `V`, with the removed place-base rows
represented by NOPs. Each phase derives its instruction map from actual source
IDs. The compactor proves SemIR/function facts without borrowing AST authority;
the finalizer separately checks the current AST, pools and both function phases.

The eleven-argument finalizer retains real Core verification and Frame checks.
Its span witness grows with checked allocation arithmetic instead of imposing a
fixed expression-size cap; the literal inline path remains available. Candidate
clone/validation precedes the final owned snapshot, and publication occurs only
after success. Every exit releases proof scratch, dynamic span storage and
temporary modules. Proof flags and borrowed AST pointers are temporary evidence,
not retained ownership or execution authority.

The actual feature RED contains **22 tests, 11 FAIL and 11 PASS**, direct exit11.
It is recorded under
`E:/cargo-targets/zr_vm/reports/ssa-source-integer-expression-01a0fe2b/` in
`red-receipt.json` and its subsequent `red-validation.json` review. The RED remains a historical feature reproduction.
The partial r4 and failed r7 observations below remain historical. Accepted
Earlier r8/lower1 and final Source r10/lower r3/MSVC r5 evidence are recorded separately.

The finite entry remains fixed local `SourceCompile` input with fresh CoreGlobal
metadata lifecycles and NULL loader/resolver callbacks. SourceExecute,
FunctionExecute, VM, import/plugin, network and FFI execution are outside this
task. The RED build's 58 warnings are not W0; current fullW0, full MSVC, Linux,
release and whole-leaf acceptance remain open. The detailed current contract and
remaining checks are in the
[integer-expression guide](../testing-and-validation/ssa-source-integer-expression-retention.md).

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

## Current implementation and finite scope

The additive mode `ZR_SOURCE_RETENTION_REQUIRE_SCALAR_EXECIR=4` uses the real
`ZrParser_Source_CompileWithOptions` route. NONE0 and scalar AOT1 keep their
contracts; these are exact enum values. Unknown2, mixed1|4=5 and other unknown
modes refuse at OPTIONS/INVALID_ARGUMENT before producer preparation. NULL or
zero options remain NONE. Both REQUIRE modes reject submission context/output.

Current status is implemented-validated-finite for this handoff. After
genuine layout append, mode4 runs AttachPrimitiveSourceFrame, VerifyFunction_ALL
and the actual eleven-argument finalizer. Direct Source22, direct AOT25 and the
exact two-test CTest selection now pass with natural exit0. This supplies finite
functional evidence, with no release, fullW0 or whole-leaf acceptance. Earlier
diagnostic and generic-snapshot failures remain historical evidence in the
[acceptance record](../acceptance/ssa-source-returned-execir-retention.md).

The accepted run uses independently recorded shared-checkout inputs, including
external migrations. Finite commit scope excludes foreign changes; the run does
not establish a clean full build from the finite commit alone.

The finite inputs remain existing literal9/8 I64 SCRIPT programs, bool/float
refusals and existing invalid-input checks. No imports, captures, calls,
compile-time programs or returned Function execution are added. Existing AOT
retention has its own [contract](ssa-source-returned-retention.md).

## Genuine finalized handoff contract

The accepted design replaces the earlier preframe original-module snapshot.
The late compiler hook runs with the selected Function, CompilerState, AST,
SemIR, semantic context and both constant pools alive. The genuine bridge,
BuildModule, constant append, original Verify, module-contract bind, compaction
and layout append create an original module and a separate compacted function.
The implemented mode4 branch runs the existing AttachPrimitiveSourceFrame
and VerifyFunction_ALL on that compacted function first, then calls the private
`source_snapshot_finalized_primitive_module` once before the AOT projection path.

Its eleven typed arguments are the actual SemIR function, semantic context,
script AST, source callable identity, Compiler constant array, Function constant
pointer/count, original module, finalized compacted function, selected
`ownedExecIr` slot address and optional lower diagnostic. The test-only wrapper
passes every argument unchanged to the real helper. No Function is reconstructed
from the slot, no container-of calculation is used, and no later
FunctionSnapshotExecIr call performs a second snapshot.

The original supplies complete module constants, layouts, contract and identities.
The selected replacement supplies the genuine compacted instructions, maps and
attached frame. The helper validates original/finalized relations and source
facts, clones the complete module replacing the selected function, verifies the
candidate and frame, then creates independent owned module storage. Constant
pool indices remain the original indices; they are not renumbered through AOT
projection. Compiler and Function constant pools are separate readable inputs.
Published logical module content includes the replacement function; equality
with the whole uncompacted original function is therefore not the output contract.

The selected current Function must have no attached owner for this Source route.
The lower helper's destination transaction preserves existing output on refusal
and replaces it only on success. All temporary candidate storage is released
before returning. The Source route publishes ExecIR only, without AOT ownership.
Capacity normalization follows the existing Core copy/clone contract; spare
capacity, padding, registry ownership and executable admission are not promised.
Numeric canonical IDs retain identity without retaining the registry itself.

## Independent witnesses and ownership records

The current test records three separate pointer-free observations: original
module/full logical rows, finalized compacted function/frame, and published owned
module. Its expected logical source rows are derived from the genuine SemIR,
source identity and constant facts. Inputs are checked while their typed storage
is live, including observations before/after direct helper refusals.

Whole helper attempt records include temporary copies and final owner storage.
Successful publication classifies actual still-live records into an explicit
output-ID set; temporary records must already have one free. Live-span reads
bind genuine returned owner/view pointers to those records and check independence
from producer addresses. A numeric record or span alone is not Function/VM
provenance. After Function free or Global shutdown, only saved numerical facts
and independent witnesses are read.

`metadataTokenRecords` are checked on successful returned Function identity:
module and script-entry token relations and signature identity must agree with
the published module. The test does not snapshot every metadata record before
and after failed compilation; no universal failure-preservation claim is made.

## Failure diagnostics and cleanup

Finalizer refusals map to EXECIR_OWNER through the existing Source mapper. All
nine lower fields are preserved: code, functionToken, blockId, instructionId,
sourceId, expectedVersion, actualVersion, expectedHash and actualHash. OOM maps
to Source OUT_OF_MEMORY, Unsupported to UNSUPPORTED, and other lower errors to
PRODUCER_FAILED. Attach/Verify failures remain FRAME. A real live SemIR source
map supplies offsets and signed coordinates before cleanup; sourceId0 or an
unmapped refusal supplies no invented range. Success clears the complete Source
diagnostic; NULL diagnostic behavior uses the existing local normalization.

Producer cleanup remains descriptor, projection, compacted function, original
module, followed by caller Function/CompilerState and AST cleanup. Observers use
nonasserting TryCapture/TryArm/TryEvent/TryRecordFrees, save errors numerically
and disarm before release. Close performs real shutdown, saves VM statistics and
ledger health before recovery, then invokes accepted cleanup support. Assertions
consume the saved first report afterward; recovery cannot turn unhealthy prior
state into a passing observation, and Init checks prior health before Reset.
Existing asserting setup/Compile wrappers do not establish prior-failed Unity
Source entry or ignored-test execution.

## Prerequisites and remaining evidence

The lower [finalized-module acceptance](../acceptance/source-finalized-module.md)
is committed as `d475d47494d5b7566e258dd379466ca099b3da6f`: its declared-profile
24 cases, measured89 faults and16 lifetime cycles are finite lower evidence.
Those lower observations do not supply the genuine Source allocation denominator.
The current Source22 run preserves the existing twenty names/order and separately
measures/completes156/156 whole-helper faults for literal9 and156/156 for literal8.
It checks request traces, same-input direct refusal, candidate cleanup and a
disarmed retry in a fresh balanced Global. The exact protocol contains341 FEATURE
rows and healthy FINAL with closed=1 support=1 and zero saved VM/owner/observer
live or error counters.

Two appended cases now target the actual values calloc and frame-header malloc
inside AttachPrimitiveSourceFrame. The observer arms only that real call, saves
its lower diagnostic and pointer-free input witness, and records candidate frees
before Global shutdown. Values failure must stop before the header request;
header failure must release values and packed storage. Neither failure publishes
a frame or reaches the finalizer. A disarmed retry creates a fresh Global,
publishes the genuine frame/header/slots, calls the finalizer and proves release.
Both cases pass with exact values/header FRAME_OOM rows, each failed=1 retry=1.

The actual selected graph has eighteen roles,786 TUs and29 FI TUs. The original
private Parser10 is split into Parser8 and Frame2. Only Frame2 receives the frame
shim; the observed producer uses its typed AttachFrame wrapper. Initial775
compilations/two links are followed, after shared input changes, by an actual
27-compilation/two-link refresh with759 exact cached objects and811 unique VALID
dependency paths. Current command/provider/dependency evidence accompanies the
Source22/AOT25/CTest2 passes. The initial build emits61 warnings and refresh22;
full_current_w0 and release_accepted remain false.

The current receipt and runtime validator are under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/source-current-direct/`;
their exact hashes and retained verifier-failure history are in the
[acceptance record](../acceptance/ssa-source-returned-execir-retention.md).
Ninja dependency and archive-member relative-path handling, and the exact CTest
environment preamble were corrected as metadata interpretation of existing raw
evidence, without rerunning those native phases.

Four child NULL/zero cases injected through Source remain open. Separate lower
AST checks at `963a81af3502d11361cab3dd64a9c0b9a5dc46a7` do not supply this coverage.
Full strict warning-free validation, Function/source-family completion, default
canonical publication, registry lifetime, Linux/full MSVC, normal-project runtime,
backend/native parity and all47 remain OPEN. No SourceExecute, Oracle, returned
Function call, generated native code, network/FFI or security probe is credited.

- [Source22 protocol and allocation closure](../testing-and-validation/ssa-source-returned-execir-retention.md)
- [Finite functional evidence and remaining gates](../acceptance/ssa-source-returned-execir-retention.md)
