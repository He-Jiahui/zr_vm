---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - tests/parser/test_ssa_source_integer_expression_retention.c
  - tests/parser/test_ssa_source_integer_expression_proof.c
  - tests/parser/support/ssa_source_integer_expression_proof_shim.h
  - tests/parser/test_ssa_source_integer_expression_faults.c
  - tests/parser/support/ssa_source_integer_expression_faults_fixture.c
  - tests/parser/support/ssa_source_integer_expression_faults_fixture.h
  - tests/parser/support/ssa_source_integer_expression_faults_producer_shim.h
  - tests/parser/support/ssa_source_integer_expression_faults_frame_shim.h
  - tests/parser/support/ssa_source_integer_expression_faults_graph_shim.h
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owner_capture.c
  - tests/cmake/ssa-source-integer-expression-retention.cmake
  - tests/cmake/ssa-source-integer-expression-proof.cmake
  - tests/cmake/ssa-source-integer-expression-faults.cmake
  - tests/cmake/ssa-source-integer-expression-support.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_storage_internal.h
  - tests/parser/support/ssa_execir_builder_storage_cases.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_dead_source_places.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - tests/parser/test_ssa_source_integer_expression_retention.c
  - tests/parser/test_ssa_source_integer_expression_proof.c
  - tests/parser/test_ssa_source_integer_expression_faults.c
  - tests/parser/support/ssa_source_integer_expression_faults_fixture.c
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
  - .codex/plans/20261016-ssa-source-integer-expression-retention.md
  - docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_source_integer_expression_retention.c
  - tests/parser/test_ssa_source_integer_expression_faults.c
  - tests/parser/test_ssa_source_integer_expression_proof.c
  - tests/parser/test_ssa_source_returned_execir_retention.c
  - tests/parser/test_ssa_source_returned_retention.c
doc_type: testing-guide
status: implemented-validated-finite
---

# Source integer-expression retention: finite proof and test contract

## Current evidence

The extension has independently accepted **current finite evidence** from
Source r10, lower r3 and MSVC r5 compile-only validation. All direct targets,
targeted CTest, lower regression and the selected15 MSVC compile phases pass. These results do
not close a whole leaf or claim release/fullW0/Linux/VM execution. The literal/
child Source26/AOT25/CTest2 account in the
[returned-source test guide](ssa-source-returned-execir-retention.md) remains the
historical handoff; r8 has its own current-input regression evidence.

### Final current Source r10 acceptance

`green-r10-runtime-review.json`, SHA256
`EDDE7C19FEE94F352D5DE0FF67C51F1F5A174705D750B434DE6A9CB3885E6723`
(2843 bytes), records Root and independent strict ACCEPT of session65436,
natural exit0, terminal `1b1d8d`. The original collection receipt
`green-r10-receipt.json`, SHA256
`2B03448EF77ADDF561E34B95DBF00FB07700B7AECC28A022FE990EBAC79285A7`
(13714 bytes), retains its collection-time awaiting-review fields. Final
acceptance is recorded separately without rewriting the collection receipt.

| Current Source r10 evidence | Actual finite result |
| --- | --- |
| Native phases | All nine naturally exit0: configure, build, dependencies, expression, literal, AOT, faults, proof and targeted CTest |
| Graph/build | 24 roles,819 TUs,822 dependencies,1610 inputs;5 compiles and814 exact cached objects; zero incremental warnings, not full-project W0 |
| Direct targets | Expression22/literalSource26/AOT25/faults2/proof4, all PASS |
| Fault/cleanup |423 measured/swept/refused/retried/accepted sites;424 healthy lifecycles; FINAL owner records/frees1353/1353 and all live/errors/VM diagnostics/dropped zero |
| Literal regressions | Both156-site sweeps, two real Frame OOM cleanup/retries and four child refusal/restore/retries pass |
| Proof |22 mutations, storage32 refusals/32 no-growth retries/5 math, independent allocating P0 and genuine large-span healthy/fault/fresh-retry, all PASS |
| Direct/raw identity | All ten direct raw files are byte-identical to accepted r8; all18 raw pins match current artifacts |
| Targeted CTest |5 PASS; complete nonempty-line multisets equal the corresponding direct outputs: AOT70/literal383/expression25/faults3350/proof39 |
| Physical admission | Current1610 input pins,819 objects and5 executable pins independently match |

The current CTest70/383/25/3350/39 values count every nonempty line. They are not
the older58/381/23/3348/37 selected-protocol-row counts preserved in the r8
record below. Both comparisons retain their actual definitions.

The accepted current direct raw repeats the complete r8 large-span protocol
below byte for byte. Independent review checks the actual healthy/fault/fresh-
retry geometry, all nine diagnostics, complete graph oracle and normal cleanup
before Reset; this is current execution evidence, not a prediction from r8.

### Final current lower r3 regression

`lower-r3-runtime-review.json`, SHA256
`DF039154AA71F5EF5F380E613430D4BD3A82B2389C43D0B4AAE3CEF97283D9F4`
(1488 bytes), records Root and independent ACCEPT of session74303, natural
exit0, terminal `9d8c1b`; actual direct PID17024. The collection receipt is
`lower-r3-receipt.json`, SHA256
`2C8DF870B1D04040FDCA4F80DB25C888454B9995DC819C59B9D4C2DD8CBC4A82`
(6714 bytes). All four phases naturally exit0. There are24 unique PASS, zero
failed/ignored,89/89 measured/completed faults and16 declared/cleared lifecycles.
The five-role/32-TU graph has257 dependencies and1641 inputs;2 compiles and30
exact cached objects have zero incremental warnings. All input/object/
executable/generated-config/raw pins match. This is not full-project W0.

Lower has no FINAL text line. Each test's tearDown checks pre-Recover
errors/live/deferred zero, records equal frees, then AssertBalanced. This is
metadata/owner validation; no Source or VM execution is inferred. The earlier
lower1 receipt/review below remains unchanged.

### Retained Source r9 and lower r2 preflight failures

| Attempt | Original evidence and actual boundary |
| --- | --- |
| Source r9 | `green-r9-receipt.json`, SHA256 `16DF37A03D04F6D70C69E14CE00BE1D9C4C557606B5369E842742141551CA892`; controller build-flag preflight failure, empty recorded configure/build/run phases |
| Lower r2 | `lower-r2-receipt.json`, SHA256 `13CAEC28D83781CFB89B6728278A129FAF9D1B590B843BAAF704202CE46C7362`; foreign Core EOF-comment input change stops preflight, empty recorded configure/build/run phases |

VS environment imports did actually execute before both preflight failures;
empty project-phase arrays do not mean zero native activity. Original failures
and their evidence remain preserved. The separately reviewed foreign Core
change appends102433 bytes containing61 complete EOF block comments to the old51293-byte file. It
is outside the owned36-path scope and is not consumed by the MSVC15 C sources
or228 actual dependencies. Source r10/lower r3 pin and validate their actual
current checkout; the old r5 MSVC1822-input set is not claimed globally current.

### Accepted r8 functional evidence

`green-r8-runtime-review.json`, SHA256
`EDE6D8E43979A0684A9DAAB369D97D0141CC419779AA1E1344FDF9428CDAC642`
(1933 bytes), records Root and SourceSol independent ACCEPT. Root session60434
naturally exits0, terminal `b1b01e`. Its immutable collection receipt
`green-r8-receipt.json`, SHA256
`3EEBD7AD82E722C1BA2A29BEEB9E9A66A3476DB52D7F2EA41A1A51C2F5E67509`
(13690 bytes), retains its original awaiting-independent-review fields; the
separate review supplies the later protocol acceptance without rewriting it.

| r8 evidence | Actual finite result |
| --- | --- |
| Native phases | All nine naturally exit0: configure, build, dependencies, five direct targets and targeted CTest |
| Graph/build | 24 roles, 819 TUs, 822 dependencies, 1610 inputs; 3 compiles and 816 exact cached objects; zero incremental warnings, not full-project W0 |
| Direct targets | Expression22, literalSource26, AOT25, faults2 and proof4, all PASS |
| Literal regressions | Both156-site sweeps, two Frame OOM cleanup/retries and four child refusal/restore/retries pass |
| Expression faults | 423 measured/swept/refused/retried/accepted sites; 424 healthy closed lifecycles, all FINAL live/error/VM diagnostic/dropped fields zero |
| Proof | All four cases pass: original22 mutations, direct proof/output/phase/scratch guards, healthy/fault/fresh-retry dynamic span, storage32 refusals/32 no-growth retries/5 math plus independent genuine allocating P0 |
| Targeted CTest | All five tests PASS; raw protocol multisets equal their direct counterparts, with counts58/381/23/3348/37 |
| Input/output admission | All raw logs, products and current physical input pins independently checked; no SourceExecute/FunctionExecute/VM/network/FFI/security execution |

The third case uses actual proof main047F. Healthy and fresh retry each query
158 events and find one4064-byte MALLOC span grow at measured relative ordinal1
and one3840-byte scratch at ordinal30, each free1. Full original/finalized
captures contain7276/6590 words; input, owner, Frame and independent44-leaf/
43-ADD/RETURN oracle checks pass. Both healthy closes free786/786 records.

The fault lifecycle actually arms the measured grow ordinal, observes exactly
one4064-byte failed event with no record and leaves scratch unreachable:
matches0/ordinal0/free0. Window live storage is21 before and21 after. All nine
lower fields match OUT_OF_MEMORY code22, actual token50331649 and the other
seven fields0. Source reports EXECIR_OWNER stage14/result5/hasExecIr1, with
no AOT/range payload. Normal close frees516/516 records before Reset; the third
fresh Global then succeeds with the complete oracle. These numeric ordinals
are observed evidence; the test still measures them rather than hardcoding1/30.

### Accepted lower metadata regression

`lower-r1-runtime-review.json`, SHA256
`23E6C63164CFCFF0A206B16BB4E1D3BD3B535005A35FEF9D1BD4389BBCCCF13D`
(1139 bytes), records Root and independent ACCEPT of session70792, natural
exit0, terminal `ecdabf`. Its receipt is `lower-r1-receipt.json`, SHA256
`789B86A2FB4E35E8CB4D11236FA47987E63E73E3610E51BDFBD7253F2B457620`.
All four native phases naturally exit0; 24 tests PASS with zero failed/ignored,
89/89 measured/completed fault sites and 16 declared/cleared lifecycles. The
five-role/32-TU graph has257 dependencies, 1641 inputs, 17 compiles and15 exact
cached objects; the finite build has zero warnings. All input/object/executable,
generated-config and raw pins are checked.

Lower has no FINAL output line. Its health evidence is each test's tearDown
assertions before Recover: errors/live/deferred all zero, records equal frees,
plus AssertBalanced. This is lower metadata/owner validation, not Source or VM
execution. It does not supply the expression423-site denominator or fullW0.

### Historical r4 partial results

`green-r4-runtime-observation.json` records the naturally completed r4 attempt
(controller terminal `6390c0`, overall exit1). Its collection receipt
`green-r4-receipt.json` has SHA256
`F8223618EAC54E4D40E5B3B4AC75B394C82FBC4AF0C9F8E1F3974A151B1E78F3`.
This is partial evidence for its recorded input pins, not an accepted GREEN of
the current builder/proof revision or an expression commit.

| r4 phase | Actual result |
| --- | --- |
| Expression | 22 tests,22 PASS; natural exit0 |
| Literal/child Source | 26 tests,26 PASS; natural exit0 |
| Literal AOT | 25 tests,25 PASS; natural exit0 |
| Fault tests | 2 tests,1 PASS/1 FAIL; natural exit1 |
| Actual fault sweep | 423 measured,423 swept,423 refused,423 retried,412 accepted;11 diagnostic-protocol mismatches |
| Cleanup and FINAL | Keeper/cleanup/retry healthy;424 lifecycles, closed/healthy1, unhealthy/observer0; owner/VM/CRT live/errors0, VM diagnostics/dropped0; pre-recovery owner records/frees1353/1353 |
| Proof and CTest | Not executed in r4 |

The r4 fault failure remains a failure even though its cleanup and retries
succeed. Later r7 results are recorded separately below; they do not rewrite
this historical receipt or grant complete expression acceptance.

### Historical r5/r6 build attempts

| Attempt | Recorded boundary |
| --- | --- |
| r5 | Configure/build/dependencies naturally complete; 24 roles, 819 TUs, 822 dependencies and 1610 inputs; 10 compiles and 809 exact cached objects. One owned missing-braces warning, no W0 and no runtime execution. `green-r5-build-review.json` retains the historical built input; the later designated initializer repair is distinct. |
| r6 | `green-r6-receipt.json` records a preflight AssertionError and no native phase. No product-test or GREEN credit is inferred. |

### Historical r7 result and assertion repair

`green-r7-receipt.json` and `green-r7-runtime-observation.json` record naturally
completed runtime session56578, terminal `8febf4`, overall exit1. No active owned
native job remains in that observation. The actual results apply to the r7
input pins. The later assertion-only proof-main repair is subsequently validated
by r8; the failed r7 receipt remains unchanged.

| r7 phase | Actual result |
| --- | --- |
| Configure/build/dependencies | Natural exit0; 24 roles, 819 TUs, 822 dependencies, 1610 inputs; 5 compiles and 814 exact cached objects; zero incremental warnings, not full-project W0 |
| Expression | 22 tests, 22 PASS; natural exit0 |
| Literal/child Source | 26 tests, 26 PASS; natural exit0 |
| Literal AOT | 25 tests, 25 PASS; natural exit0 |
| Fault target | 2 tests, 2 PASS; natural exit0; 423 measured/swept/refused/retried/accepted allocation sites |
| Fault cleanup and FINAL | 424 closed healthy lifecycles; unhealthy/observer0; owner/VM/both CRT live/errors0, VM diagnostics/dropped0; pre-recovery owner records/frees1353/1353 |
| Proof | 4 tests, 3 PASS/1 FAIL; natural exit1. The original22-mutation case, direct proof/scratch-lifetime case and storage32/32/5 plus genuine P0 public-Build case pass. |
| Large healthy observation | One 4064-byte span grow at relative ordinal1; one 3840-byte scratch at ordinal30; both free1; 158 queried events, full graph captures7276/6590 words, input/owner/Frame/oracle1; normal close records/frees786/786 and live/errors0 |
| Third-case fault/retry and CTest | Not executed: the incorrect healthy order assertion interrupts this case before its fault and fresh retry. CTest did not run. |

The r7 failure is the test's imposed `scratchOrdinal < growOrdinal`, not an
observed allocation, owner or graph failure. Proof main SHA256
`047F3EABCE60BD815A8F22E3E049A409FC84C5A92524D7E1CDBAF0A203A41A50`
(61980 bytes) changes only the third case's assertions. Healthy and retry still
require exactly one span grow and scratch, each freed once, with distinct
ordinals greater than0 and within the real event count. Retry binds scratch
bytes/ordinal, grow ordinal and event count to the healthy baseline. A fault
requires scratch only when the measured healthy scratch precedes span growth;
otherwise scratch matches/ordinal/free1 must all be0. Matching event-prefix
geometry, the unique failed grow/no record, every temporary release, all nine
lower diagnostic fields, full inputs/owner checks and normal close before
Reset/fresh retry remain required. No observer or real-callee window changed;
no ordinal1 or30 is hardcoded. Those changes are now exercised by accepted r8;
the separate lower regression also passes. The r7 failure remains historical,
and MSVC had no accepted result at that checkpoint. Final MSVC r5 acceptance
is recorded separately below.

The actual feature RED is recorded under
`E:/cargo-targets/zr_vm/reports/ssa-source-integer-expression-01a0fe2b/`:

| Record | Actual evidence |
| --- | --- |
| `red-receipt.json` | SHA256 `CB0545AC31D4C7EB8D0D4467A9AE7D0649D7DE0E9B6B357DDD080A740CF49FFF`; four naturally terminated native phases: configure/build/dependencies exit0, direct test exit11 |
| `red-validation.json` | Subsequent Root and independent review accepts feature RED: 22 tests, 11 FAIL, 11 PASS, zero ignored |
| `red-direct-stdout.log` | 3347 bytes, SHA256 `EAD91E1CA8D62B6D1008D43DCDFD7CAD30460AAB1F1C31A3F040D5F96038528E` |
| `red-direct-stderr.log` | 560 bytes, SHA256 `6595F5401B399D79219AAACD6E5C41488F322F394F52DD2E9CEB4C5029616764`; ten exact refusal rows |
| RED build/dependencies | 19 roles, 791 TUs, 405 compiled, 386 cached, 813 unique dependencies, 58 warnings; no recorded input drift |

The eleven failing cases reach ordinary compile prerequisites but lack the
required returned expression owner. Ten emit
`SOURCE_I64_EXPRESSION_REFUSED stage=4 result=4 code=28`; the NULL-diagnostic case
does not emit a diagnostic row. The original collection receipt retains its
awaiting-review flags; the later review records acceptance of the RED. Failed
Unity cases cannot supply teardown-ledger PASS credit because their existing
failure state interrupts those assertions. None of this is expression GREEN.

## Admitted Source input and ownership

Mode4 accepts a canonical I64 SCRIPT containing one RETURN whose expression is a
finite tree of integer literals and binary `+`, `-` or `*`. The actual graph,
left/right order, literal pool indices and source coordinates are retained.
Precedence and parentheses come from the real AST and SemIR. The producer does
not evaluate or fold the expression to manufacture an answer for these tests.

NONE, NULL options and zero options retain no new IR. Literal scalar AOT mode1
remains distinct: accepting a mode4 expression does not broaden mode1. Bool,
float, division, comparison, local reads and control flow remain outside this
finite profile. Ordinary compilation success is a prerequisite, not sufficient
proof that a retained expression was published.

The bridge `source_integer_constants_prepare` validates current compiler,
function and semantic context geometry. It finds the genuine script-entry
member token in the module's metadata-token records, checks its module owner and
signature identity, and passes that real token into proof construction. SemIR
symbol identity is checked separately and is never cast into a metadata token.

The retained owner is a real Core-owned module. The compiler's AST, semantic
arrays, scratch proof and current constant-pool storage are borrowed only during
the synchronous finalizer call. The result must survive Function/compiler/AST
cleanup independently. No proof object, cached validation flag or AST pointer
is transferred as ownership or permission to execute.

### Real identity producer

`compiler_script_callable_identity.c/.h` owns the optional ordinary compiler
identity path. Its existing literal proof remains intact; a separate binary
candidate admits actual single-character `+`, `-` and `*` roots only after the
live versionless SCRIPT, single nonreference RETURN, reachable terminated CFG,
pure canonical I64 SemIR, actual values/places/slots and source-map facts agree.
It requires the existing no-parameter/import/effect/closure/export profile. The
candidate adds no allocation, uses no synthetic IR, does not evaluate the
expression and does not impose `P == V`.

The existing canonical function interning, real semantic symbol registration,
Function/name root protection and canonical reacquisition still perform actual
identity creation. Publication happens after those fallible gates: SemIR gets
its real symbol/callable type, and the Function gets the pointer-free identity
and presence flag. Unsupported/incomplete state publishes nothing while ordinary
compilation retains its optional-identity behavior. The void producer API does
not gain a new precise identity-allocation OOM contract. Complete AST, pool bits,
ORIGINAL/COMPACTED and Frame certification remains the later per-call retention
proof; this early producer is not a replacement for it.

## Proof geometry and production flow

The proof is initialized empty, proves current canonical SemIR, checks the
actual AST and checks the compiler and runtime pools. Every pool entry has its
INT64 tag checked before union payload access; all entries must be referenced
consistently. Repeated equal literals may share a pool slot. The actual constant
count `C` is therefore independent of the number of CONSTANT instructions.

| Quantity | Meaning and checked relation |
| --- | --- |
| `I` | Actual SemIR instruction count; each phase's map is derived from genuine ExecIR source IDs |
| `V` | Actual semantic value count, with a unique definition for each admitted value |
| `P` | Actual temporary-place count, checked independently of `V` |
| ORIGINAL values | `V + 2*P`: semantic values, then `P` address rows and `P` provenance rows |
| COMPACTED values | `V`; removed place-base instructions become NOPs while source correspondence is retained |
| Proof workspace | One aligned allocation for `V` AST pointers, `P` place IDs, two `I`-entry maps and `V` visits |

All slice sizes, alignment, count/capacity/width and pointer spans are checked
before access. AST traversal is iterative and bounded by actual values; it
rejects NULL children, cycles, shared node reuse, missing values and extra or
different operators. Source ranges must agree with actual AST/SemIR facts.

The existing producer builds the original module, installs real constants,
verifies and binds its source contract, then compacts source places. The
compactor proves canonical SemIR/function correspondence in ORIGINAL or
COMPACTED form, clones/rebuilds and verifies the actual result. Its proof does
not dereference AST nodes or replace the finalizer's AST checks. Frame attachment
and VerifyFunction_ALL still run on the real compacted function.

The eleven-argument finalizer receives genuine SemIR, context, SCRIPT, identity,
compiler/runtime pools, original module and compacted function. It separately
checks current AST and pools, ORIGINAL and COMPACTED maps, real frame geometry
and Core verification. Its span witness retains an inline literal path and
grows dynamically using checked allocation arithmetic for expression inputs.
There is no fixed expression instruction-count cap substituted for that proof.

Candidate-module replacement, candidate validation and frame verification occur
before the final owned snapshot. Successful publication occurs last. Every exit
releases temporary modules, proof workspace and dynamically grown span storage.
The literal fast path is intended to avoid new proof-workspace allocation;
post-change literal sweep evidence is still required to verify that contract.

### Generic builder storage and diagnostic coordinates

`exec_ir_build.c` orchestrates real SemIR-to-ExecIR construction. Its private
`exec_ir_build_storage_internal.h` wraps the existing Core value/block/result/
operand/instruction mutators and local source-map growth. It checks mutability,
result definitions, enums, count/capacity/pointer/alignment geometry and checked
addition/requested/grown byte sizes before a mutator can publish a result.
The growth calculation follows Core's real reserve policy; result/operand
ReserveEx failures retain their diagnostic category. Existing Core owns the
allocation and graph mutation. The header does not create an alternate IR or
turn every false boolean into OOM.

Invalid arguments, invalid ranges/values, sealed state and capacity overflow
remain distinct refusals. A valid growth path whose actual allocator fails
reports OUT_OF_MEMORY; source-map realloc has the same distinction after its
capacity checks. Guard refusals preserve the tested function/backing bytes and
output sentinels. This finite guarantee does not claim that all partially built
modules are unchanged after every later failure; the real caller still cleans
failed construction.

`SExecIrBuildStorageSite.sourceId` is the original semantic instruction ID.
Before successful `FunctionAppendInstruction`, its ExecIR `instructionId` is0.
Only the actual returned ExecIR ID is installed in the site for the subsequent
source-map append. A semantic source row is never passed off as a published
ExecIR instruction ID. Lower diagnostics reset all nine fields and preserve the
actual token and available block/source/published-instruction coordinates;
Source diagnostics use those real source facts before producer cleanup.

## Three native test targets and verified finite protocols

These CMake targets and their current test code exist; the table gives their
assertions. r8 passes this revision; r4/r7 remain separate historical outcomes. The direct validation project is
`tests/cmake/ssa-source-direct-validation`; the reusable build directory is
`E:/cargo-targets/zr_vm/build/ssa-20261011-01a0fe2b-source-returned-finalized-red-r2`.
Root records actual invocations and inputs in the task report before execution.

| Target / CTest name | Intended checks |
| --- | --- |
| `zr_vm_ssa_source_integer_expression_retention_test` / `ssa_source_integer_expression_retention` | 22 tests: addition/subtraction/multiplication, associativity, precedence, grouping, repeated literals and pool indices, multiline coordinates, owner independence, repeated compile/free, NULL diagnostic, refusals and retry, NONE and mode1 separation |
| `zr_vm_ssa_source_integer_expression_faults_test` / `ssa_source_integer_expression_faults` | Two tests: successful real trace measurement, then one fresh lifecycle per observed allocation attempt with keeper preservation, failure cleanup and retry |
| `zr_vm_ssa_source_integer_expression_proof_test` / `ssa_source_integer_expression_proof` | Four tests: original22 mutations, original output/phase/workspace guards, measured large span-growth OOM/cleanup/retry, and32 builder negatives/32 restored Core retries/5 growth-math checks |

The functional assertions inspect actual operator and ordered operand rows,
canonical I64 types, frame value/layout metadata, pool payloads and real source
coordinates. Expected tables are an oracle for retained data; they neither
construct replacement IR nor execute an expression to obtain its result.

### Allocation windows and failure protocol

The fault fixture first returns keeper slot0 from `return 6 + 7;`, then subject
slot1 from `return 2 + 3 * 4;` within one fresh Global. It records the successful
subject's actual allocation trace. Seven observed phases are producer-other,
bridge, compactor, Frame, finalizer, candidate clone and owner clone. Nested
windows attribute each allocation to its innermost phase. Callbacks are
nonasserting, invoke their real callee once, retain actual results/diagnostics
and allocate no replacement IR. Trace/record/call capacity overflow is a failed
observation, not silent truncation.

Each observed allocation site is then faulted in its own fresh lifecycle. The
test recreates and captures the keeper's real owner, topology, logical contents
and live allocation records before arming one site. It requires exactly one
actual failed event with the same kind/count/width/bytes and phase as the
successful trace. Source stage and all lower diagnostic fields are recorded;
an unclassified false result is not silently reinterpreted as OOM.

The failed subject must be NULL. New successful allocations in the failed
attempt must be freed exactly once and live storage must return to the keeper
baseline. The keeper's owner address, full logical content and allocation
records must remain unchanged and unfreed. An unfaulted subject retry must then
publish a healthy owner passing real module and VerifyFunction_ALL checks.
Finally both functions and Global are freed. Owner and both CRT ledgers must be
healthy before recovery; the next lifecycle refuses to Reset unhealthy evidence.

`INTEGER_EXPR_BASELINE`, `INTEGER_EXPR_PHASE` and `INTEGER_EXPR_TRACE` expose the
actual successful trace. `INTEGER_EXPR_WINDOW` records actual window diagnostics
and stats. Each `INTEGER_EXPR_FAULT` records the failed site, geometry, Source
stage/result, lower diagnostic, cleanup, keeper preservation and retry outcome.
`INTEGER_EXPR_SUMMARY` and `INTEGER_EXPR_FINAL` must agree with actual swept sites
and closed healthy lifecycles. FINAL retains pre-recovery live/error evidence and
health failure must not be hidden by Unity failure state. The new attempt count
is measured; the historical 156-site literal sweeps do not predict it.

### Genuine finalizer mutation protocol

The proof fixture uses the real Source-produced SemIR, AST, pools, ORIGINAL and
COMPACTED functions. Twenty-two mutation kinds cover semantic opcode/operands/
result type/constant index/range/source map, array width/capacity and auxiliary
facts; compiler/runtime constant tags and bits; AST division/NULL/cycle/shared
children; original/compacted opcode and operands; and ExecIR source maps.

Each mutation changes borrowed storage around a real eleven-argument finalizer
call. Restoration occurs before observations and Unity assertions. Repeated
refusal must produce deterministic diagnostics across all nine lower fields,
leave the seeded existing owner and its records intact, and release every new
allocation. Unmodified retry must publish the genuine finalized owner and free
the replaced owner once. Input fingerprints and restored borrowed bytes must
remain intact. A separate case requires malformed input to preserve empty proof
output, invalid phase to refuse, and proof scratch to be released only once.
The original22-mutation case and the separate direct proof/scratch-lifetime
case pass in r7 and again in r8. r8 also passes the corrected third case,
completing all four proof cases and their checked health protocols.

### Independent large span-growth case: r8 healthy/fault/retry PASS

`test_real_source_dynamic_span_growth_oom_cleanup_retry` is the third proof
case. It constructs the fixed181-byte `return` source with44 literal `1` leaves
joined by43 `+` operators. Its independent syntax oracle checks44 CONSTANTs,
43 ADDs and one RETURN, actual left-associated operand/result IDs, deduplicated
constant pool bits/tags/index, all instruction/source maps and actual Frame
slots. It inspects retained metadata; it never computes or executes the answer.
Semantic `V` is87 while `P` is observed independently: ORIGINAL must have
`V + 2*P` values and COMPACTED must have87. No assertion equates `P` and `V`.
The complete tree crosses the inline127-span limit; the test does not assert an
invented exact total span count.

The healthy lifecycle brackets only the real eleven-argument finalizer with
allocator Stats. It queries every event and requires exactly one MALLOC with
count1 and width/bytes `2 * SOURCE_FINALIZED_INLINE_SPANS *
sizeof(SSourceFinalizedSpan)` (4064 bytes on the current x64 target). Its relative
ordinal is measured. Prior Core verification may allocate, so ordinal2 is not a
contract. Ambiguous geometry, missing events or bounded trace overflow fail the
observation rather than selecting an unrelated malloc. Healthy span storage and
the genuine finalizer proof scratch must each have exactly one free. Their
measured ordinals must be distinct and valid within the actual event count;
no ordering is prescribed. r7 observes span ordinal1 and scratch ordinal30,
with both free1 and the complete healthy oracle, as recorded above.

A second fresh Global arms that measured ordinal immediately before the one
real finalizer call, then Disarms immediately on return. The failed event must
match the healthy prefix and geometry, have failed1 and no successful record.
If healthy scratch precedes healthy span growth, that scratch must have the
same ordinal/geometry and free1 in the failed prefix. Otherwise scratch must
not be reached: matches0, ordinal0 and free0. An unfaulted retry must preserve
the healthy scratch bytes/ordinal as well as grow ordinal and event count.
Source must return NULL at EXECIR_OWNER/OUT_OF_MEMORY with hasExecIr1, no AOT or
range payload, and the actual lower OUT_OF_MEMORY/entry token with the other
seven fields0. No owner is published; every new successful temporary record is
freed once and live storage returns to its pre-call baseline. A third fresh
Global performs the unfaulted healthy compile again with the complete oracle.

Four static independent16384-word buffers use the existing VisitContents
visitor after genuine VerifyModule/VerifyFunction_ALL; their emitter rejects
overflow and compares every emitted word. Only this case uses the larger sink.
Original TryCapture4096, both earlier cases, all22 mutations and common teardown
remain unchanged. Observer proof/capture work occurs outside the measured/armed
window. Callbacks and visitors contain no Unity assertions and never substitute
for the real callee result or diagnostic.

Each lifecycle normally frees Function/Global, then checks every successful
record for free1 before any Recover or Reset. It saves and prints aggregate
owner/VM/both CRT stats and the checked `released`/`crt_released` witnesses,
not individual record rows. The intentional failure count1 survives in the saved fault observation;
Reset is allowed only after health assertions. Retry leaves failures0 for the
unchanged common teardown. Three `INTEGER_PROOF_SPAN_GROW` rows, with modes
healthy/fault/retry, must expose real ordinals/geometry/diagnostics/cleanup.
r7 emitted the healthy row and closed it normally, then the erroneous order
assertion stopped this case before its fault/retry. r8 subsequently exercised
the 047F repair and all three lifecycles successfully, including the failed-grow
prefix with unreachable scratch and the fresh retry.

### Independent builder guards: r8 case PASS

The fourth proof case includes
`support/ssa_execir_builder_storage_cases.inc` and calls
`test_builder_storage_guards_preserve_outputs_without_allocation`. The reviewed
inc SHA256 is
`808426919E2C717CE394C5019988EE9DFF3E0C19DB3B8947C35DFDA77DC40C59`.
Its32 negative rows cover sealed mutators, invalid result IDs/definitions/
external flags/ranges, ownership/nullability/block flags, NULL input/storage,
count/capacity/alignment, zero width/alignment and count/request/grown overflow.
Each guard must preserve full sample/backing bytes and output sentinels, return
the exact complete diagnostic and leave every allocator statistic unchanged.

After each refusal the borrowed sample is restored and a real Core instruction
append succeeds within existing capacity, checking returned ID and value
definition without allocation. All stack-backed pointers are unbound before
Unity assertions. The5 additional math rows use requests0,1,4,5,UINT32_MAX with
width1; they call only the numeric growth predicate, preserve the poisoned
diagnostic and ledger, and do not advertise giant live pools or allocate them.
The protocol row is
`INTEGER_PROOF storage refusals=32 retries=32 math=5 allocations=0`.

The independent P0 positive then calls the genuine public
`ZrParser_ExecIr_Build` on the canonical zero-place CFG shape. It first creates
and tracks a real owning block allocation in an old destination, seals that
destination, then requires successful transactional replacement by the public
build path. The new output must be unsealed with the actual token/signature,
entry block1 and zero values/instructions/source maps. The old block address
must be replaced and its record freed once; borrowed semantic/CFG inputs remain
byte-identical and the complete diagnostic is cleared. Normal output release
must free every new successful record once and restore live/errors/failures to
the initial ledger values before Unity assertions.

P0 performs real allocations. Their actual ledger accounting is separate from
the preceding32/32/5 zero-allocation contract; no fixed P0 allocation count is
assumed. Its distinct protocol row is
`INTEGER_PROOF storage_zero_places build=1 sealed_destination=1 released=1`.
The reviewed inc is15573 bytes with271 LF lines. This fourth case, including
32/32/5 and the independent allocating P0, passes in r7 and r8. The r7 full
proof target still exits1 historically; r8 passes all four proof cases after
the third case's assertion-only repair.

### Accepted finite MSVC r5 compile-only evidence

`msvc-r5-native-review.json`, SHA256
`136F6F95744924BA8F0DA090F63CB03B5F9B1707C0EDE8E1A3107CCB69C4DDD8`
(1327 bytes), records Root and two independent ACCEPT reviews. Root session86909
naturally exits0, terminal `acd12c`. The collection receipt is
`msvc-r5/receipt.json`, SHA256
`F498DDCB671ED619B861EE1668F87F91320FC9A02D74A546A27857D90BE44A65`
(386131 bytes); its original pending-review fields are preserved.

All15 actual compile phases over13 unique C files naturally exit0, with streams
fully drained and strict W0. The selected role graph preserves ordered defines,
includes and forced includes. All228 actual dependencies pass strict
sourceDependencies schema/before admission. At r5,1822 inputs match before and
after;63 product pins plus the receipt account for64 registered outputs.
TEMP is empty. No link, executable probe or runtime phase is present.

The15 phases cover five source proof/compaction/finalization units, Frame
validation, three actual retention producer roles, the ordinary identity
producer, four test/fixture units and the generic builder. Each compile has an
object, sourceDependencies JSON and two raw streams; the other four outputs
are unit inventory, inputs-before, inputs-after and receipt metadata. The
compiler label uses PE FileVersion19.44.35229.0; ProductVersion14.44.35229.0 is
the toolset resource. Strict CP936 decoding remains in force.

| Retained MSVC attempt | Actual failure boundary |
| --- | --- |
| r1 | `msvc-r1-controller-failure.json`; VS import ran, then environment-key case lookup yields an empty absolute-drive path. Zero cl calls, no compile receipt or registered products; controller failure, no product RED. |
| r2 | `msvc-r2-controller-failure.json`; VS import ran, then unexpected INCLUDE directory/SDK override fails admission. Zero cl calls, no compile receipt or registered products. |
| r3 | `msvc-r3/receipt.json`, SHA256 `E504F41EEFBF83C4933920208BEED8BA58CE894FA97907B103314646464203B2`;3 actual cl phases with W0, then8 new atomic/standard headers absent from the before baseline are rejected. No late before-pinning or GREEN. |
| r4 | `msvc-r4/receipt.json`, SHA256 `89793CC77CE79F85CA04D3502B24F37BBD8D17E7B4809A180EE785570AAE8DD3`;5 actual cl phases expose real C4457 shadowing. Its UTF-8 diagnostic fails strict CP936 decoding; this receipt remains failed, not W0 or GREEN. |

The genuine C4457 repair renames only three local identifier occurrences from
`capacity` to `spanCapacity` in
`zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c`.
The actually compiled repaired file is40017 bytes, SHA256
`0883C55D9E072DE53E093199A2346D15643DD75AE68D8120347512B9ABC49648`.
It does not suppress a warning or change/waive diagnostic decoding. r5 performs
the actual successful15-phase compilation after that repair.

The later reviewed foreign Core EOF comments are outside the selected15 C
sources and228 dependencies. The r5-time1822 before/after match remains its
historical observation, not a new assertion that every broad pin still matches
the later checkout. Final Source r10/lower r3 separately prove the updated
inputs. Finite MSVC W0 does not establish complete MSVC, full-project W0,
Linux or VM/runtime execution.

## Remaining gates and finite safety scope

Final Source r10 functional/fault/proof and Source26/AOT25 regressions,
targeted CTest5, lower r3 metadata regression and finite15-phase MSVC r5
compile-only validation have independent acceptance. Every result proves its
recorded inputs and finite scope; the historical collection receipts retain
their original state fields. Validation uses the shared
checkout, including separately recorded external migrations; a future finite
commit must not claim that its own hunks alone reproduce a clean full build.

Inputs are fixed local integer source strings. CoreGlobal is initialized and
freed for metadata compilation; loader/resolver callbacks are NULL. No returned
function is executed. SourceExecute, FunctionExecute, VM, import/plugin, FFI,
network and security execution are outside this finite validation. Generic GC
callback machinery does not establish coverage of those execution routes.

The actual RED build has 58 warnings, so it is not W0. FullW0, complete MSVC,
Linux, broader language/execution support, release and all47 leaf acceptance
remain open. This task does not close core-model, SSA-construction or contract
freeze plans, and no finite-subtask/leaf-count completion percentage is implied.
