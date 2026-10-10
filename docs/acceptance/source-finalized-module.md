---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_callable_query.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
  - tests/parser/test_ssa_source_finalized_module.c
  - tests/parser/support/ssa_source_finalized_module_fixture.h
  - tests/parser/support/ssa_source_finalized_module_fixture.c
  - tests/parser/support/ssa_source_finalized_module_witness.c
  - tests/parser/support/ssa_source_finalized_module_faults.c
  - tests/cmake/ssa-source-finalized-module.cmake
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_finalized_module_validation.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - .codex/plans/20261009-ssa-source-finalized-module-design-v3.md
  - .codex/plans/20261009-ssa-source-finalized-module-test-protocol-v1.md
tests:
  - tests/parser/test_ssa_source_finalized_module.c
  - docs/testing-and-validation/source-finalized-module.md
doc_type: acceptance-record
status: functional-red-verified-commit-pending
---

# Finalized Source module: verified RED baseline

## Current scope and status

Status is functional-red-verified-commit-pending. Root accepted the actual RED r3
runtime and MSVC evidence below. Root proof, exact16-path RED commit and cleanup
are still unexecuted; there is no finalizer GREEN implementation or acceptance.
The16 paths comprise three private production files, five new test files, two
existing strict-allocator support files, three documents and three registrations.
The mixed normal registration has only its owned include hunk; pending returned
Source changes remain outside this scope.

The unchanged RED stub resets all nine fields, guards required top-level NULL
with INVALID_ARGUMENT/token0, and otherwise returns UNSUPPORTED with safely
bounded selected original Function token or0. No nested admission, allocation,
free, query, Verify, clone or publication is performed by this stub.
NULL functionConstants belongs to future pool admission, not the required NULL
list. The accepted v3 API/full9 table and94 input/33 candidate spans are intended
GREEN requirements; this RED result does not implement them.

## Actual current runtime RED r3

Reports are under
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/`.
Runtime receipt `source-finalized-module-red-r3-receipt.json` SHA256:
`E0B4164D768E8816D6ED499353C6F4C10AD4E649BA803651D9725CEFA457E951`.
Primary raw `source-finalized-module-red-r3-finalized-module-ctest.log` SHA256:
`9E439990CBFA3E60D40A14CBFE586F3CA562E00D424D04BF7F57AEA4D34DCAE8`.
Controller18080/session21050 naturally exited0 and closed. Primary CTest36640
naturally exited8 for the genuine expected RED result.

Actual24 names/order match the protocol: case1 guard PASS, cases2-24 exact
feature FAIL, zero ignored. All23 reached feature refusals emitted exactly
`SOURCE_FINALIZED_FEATURE code=28 attempts=0 frees=0` with complete9 reset/
UNSUPPORTED witness and unchanged live inputs/old output. No ledger/UBSan error
or Setup cascade occurred. Every later mutation, lower precedence variant,
complete-copy success, lifetime cycle and sweep is behind that baseline refusal;
none receives GREEN predicate credit from this inventory.

All17 native phases reached natural terminals. Configure32728/build32428;
finalizer U/D39312/35200; validation U/D37556/26808; query U/D41688/16824;
lookup U/D34364/39000; lower CoreModel37328/owner7736/modulecopy30184/
frame41032/query34640/lookup26928 all exited0; primary36640 exited8.
U/defined raw records and map evidence identify the four actual private providers,
including accepted SDK wrapper/literal symbols; query/lookup import checks do not
claim anything about internal CRT implementations or the whole executable heap.

The configured closure is selected30/FI23 plus support36, total66 actual compile
commands/objects. Selected groups are15 Core owner objects,1 modulecopy,3 frame,
4 finalizer/query/lookup and7 test/support. Build r3 had39 compiles and4 links,
43 incremental steps. It did not compile all66 afresh; retained objects are
admitted through actual selected dependencies/object hashes. Broad unselected
header inventory is gate-time history, not global-current acceptance.

Six independent lower CTests passed in this current run:

| Lower gate | Actual finite evidence |
| --- | --- |
| CoreModel | PASS;28 Function and60 Module clone fault loops |
| Generic owner |14 PASS; two independent61-site sweeps |
| Complete modulecopy |20 PASS; composite88 sites |
| Primitive frame |18 PASS;18-site frame sweep |
| Callable query |18 PASS; zero allocator-call observation |
| Canonical lookup |8 PASS |

These are lower prerequisites, not finalizer cases or its unmeasured denominator.

The eight-file runtime manifest
`E:/cargo-targets/zr_vm/tmp/source-finalized-module-red-r3-freeze.json` SHA256 is
`269D1A9D5A3405F0B93B2962D50E6FE79781DF01A235DDF58805E7E9523B76A9`.
Current revised support source pins include:

| File | Lines | SHA256 |
| --- | ---: | --- |
| fixture C |492|`A3535F5D92CE8395CC72AD11C8E19265BA1C9A95F8B7187317A6D3E8895A8CB8`|
| witness C |347|`32DF9F766FF1891A36E249262647C1D3E8BA188FBE90047C1B6754834D19223C`|
| generic allocator C |112|`57DA2A307904ABE98C3C0E044CEAA6E703A2CB7ADDCE22DED842C81B91C39055`|
| generic allocator H |22|`775411298AC440A5FF68319FD4EB835F6C513C78DDC7D81B52DC17085010C548`|

Main498, fixture H81 and faults C158 remain frozen; production3 remains RED.
TryRecordFrees is an additive pure nonasserting read: invalid record/output
returns false without changing the output. Fixture/witness numeric observers use
it, defer errors and finish teardown before assertions after intentional Unity
failure. Registered cleanup and original-health evidence precede recovery.

## Actual current finite MSVC RED r3

Receipt `source-finalized-module-msvc-RED-r3-receipt.json` SHA256:
`AEA16D77B057CB10DB674807776C2B2E93FBB3A326AA68EEF34CC0F8F122824A`.
Controller39304/session4753 naturally exited0 and closed. All31 ordered native
compiler phases naturally exited0 with zero warnings and raw basename profiles
matched:23 real provider units use generic FI;7 test/support plus declaration
probe are unFI. Actual MSVC14.44 /TC C11 /c /UNDEBUG /MD /W4 is compile-only.
E outputs/TEMP and readonly SDK/tool inputs remain the finite platform boundary.

Ordered compiler PIDs are41180/15240/30884/33280/41736/30692/17164/39092/
40184/41024/31828/33524/14208/18040/27972/12140/27828/32676/37896/39648/
23032/27212/40948/33868/20944/37336/21396/24440/27256/9036/3148.
Actual current selected input pins240 have zero drift. Each structured C
Version1.2 metadata file contains Source, ProvidedModule(empty string) and
Includes. Optional import fields are absent and explicitly recorded as absent;
absence is not an observed empty-array claim. JSON/raw/object/source/tool/script
pins and selected paths are retained. No source snapshots, MSVC link/runtime,
full-project compile, provider runtime or fault-sweep credit is granted.

## Preserved failed and partial attempts

- Runtime r1 controller31924/session72741 naturally exited1 and closed after
  configure/build0: prebaseline omitted the actually read
  `tests/parser/exec_ir_clone_test_allocator_cases.inc`. No CTest credit.
  Receipt SHA256
  `1D79349CA51FBA06B6D4878800AED9D302DEFAC8F118E02578EA0B6549CFC326`.
- MSVC r1 controller25200/session5265 naturally exited1 and closed.24 compiler
  phases exited0; first23 had zero warnings, main33764 had C4127 warning1 and
  failed its strict raw profile. No31-unit credit. Receipt SHA256
  `BF1784DFB05881C7A7FC36ABBCBA5F0CC4E4855D5F8C9AF562F51063FA3CE885`.
  The main constant branch was changed to preprocessor selection; runtime gate
  v3 explicitly prepins the actual .inc dependency. Neither failed receipt flips.
- Runtime r2 controller40340/session53283 naturally exited1 and closed. Guard
  passed and case2 reached one genuine feature marker. Later22 failed through
  Setup/teardown cascade: an assertion-backed numeric RecordFrees getter aborted
  while UnityCurrentTestFailed was already set, leaving31 live records.
  Receipt SHA256
  `95C7BFF9153F7818289ECA43F1C2C478178D4AD5213E4057BB63A76A3AD9CB6F`.
  This is not an accepted RED24 result; those22 are not feature observations.
- MSVC r2 controller30388/session35634 naturally exited0 and closed, actual31
  zero-warning compile-only phases. Receipt SHA256
  `5F4B97681FA84C0BC7EA78754144FD130E1578DD32EB8D9C757EF1BE009C7FF0`.
  This valid-at-gate compile result remains historical after numeric-support
  revisions; it does not accept the failed runtime or current source pins.

## Current authoring and future transactions

Runtime uses gate v3; MSVC uses immutable v2 (261 lines SHA256
`B50547C21F1B2B30FB1E48768ED73C9B4C6843D02E2A98E0DAD3F36246909420`).
Earlier unexecuted MSVC v1 and all executed attempts are preserved. The read-only
Root review v1 is370 lines SHA256
`B8295BC87ADAE3831309D9C2FC23AED91455C602FFDE61AF66631919C1DB7513`.
Proof has not run yet and no accepted proof hash is invented.

Commit v2 is121 lines SHA256
`1916FDB0D0F72765963AEED0936BD90F91440E9F51EAE7BE848373D00ED0667F`.
It requires actual proof pins for all16 paths, stages15 whole paths plus the
single mixed owned include, and preserves foreign pending Source state. Both
Git identities must be He-Jiahui <814111601@qq.com>. This transaction has not run.
Commit v1 remains immutable historical with its superseded14-path scope.

Cleanup v2 is112 lines SHA256
`A2DF41BAE0D30AFE6EA4A1AD0DE78A2787DB401F9BCDE6565EB3EAE436AC5876`.
Its exact55 obsolete objects are r1 24+r2 31, observed778242 bytes. It requires
future accepted RED r3 proof/exact16 commit/current31 counterparts and seven
explicit naturally terminal/closed Root controller records. The tiny r2/r3 probe
uses literal E paths and equal content hashes; repository sources are never
copied. Removal would be literal, nonrecursive, one object at a time after whole
set and per-path ancestry/regular/hash/size checks. No deletion has occurred.
Raw logs, receipts, metadata, maps and current objects remain protected.
Cleanup v1 is immutable historical and will not execute with r2/14 assumptions.

## Intended GREEN acceptance and open limits

Later authorized implementation must pass actual24/0/0 and publish complete
independent ownership with precise lower9 preservation. It must measure the six
phase counts, full finalizer N, every-site direct lower geometry/diagnostic and
disarmed retry independently. Historical88/18/61 cannot supply N. Temporary
candidate cleanup must precede fallback; publication frees old output only last.
Source capacity normalization is allowed; spare bytes are not semantic identity.

Current RED has no deep-copy success, post-source-owned lifetime,16-cycle result,
paired-order mutation credit or finalizer fault denominator. Actual readable
compiler storage beyondUINT32_MAX remains unexecuted. Declared inline clearing
is not real context/interner destruction. Original/finalized source-map promises
are declared field consistency, not genuine Binder/compactor producer history.
The width/real Source preparation/registry lifetime/target ABI/default adoption/
normal/Linux/full MSVC/backend parity and all47 remain OPEN. mode4 returned
Source documentation keeps its separate frozen history and is not made GREEN.
No VM/Compiler/Semantic/interner lifecycle, FFI/network/security execution,
SourceExecute, Oracle or native backend admission belongs to this lower closure.

- [Private contract](../parser-and-semantics/source-finalized-module.md)
- [Exact24 protocol](../testing-and-validation/source-finalized-module.md)
- [Callable query](source-callable-query.md)
- [Frame validation](primitive-frame-validation.md)
- [Core copy](exec-ir-module-copy.md)
- [Generic owner](ssa-execir-owned-module.md)
