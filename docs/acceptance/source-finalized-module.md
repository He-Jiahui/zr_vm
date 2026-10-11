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
status: functional-green-verified-committed
---

# Finalized Source module: finite GREEN evidence

## Current finite GREEN scope and status

Status is functional-green-verified-committed. Actual runtime GREEN r2 and
MSVC GREEN r2 are validated; the finite GREEN implementation was committed as
`d475d47494d5b7566e258dd379466ca099b3da6f`. The implemented private helper validates declared typed consistency
and owns complete copied module storage. It does not establish genuine Source/
Binder history, Compiler/VM bootstrap behavior, registry durability or admission.

### Runtime GREEN r2

Report directory: `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/`.
`source-finalized-module-green-r2-receipt.json` SHA256:
`9C476B61A4DD008501866C867CB188C91DD9340643522E4B7710F187DD0990F4`.
Primary `source-finalized-module-green-r2-finalized-module-ctest.log` SHA256:
`9AF3832F2EC47E39B4A1CB4AF9E73A0BF76B25AE50C0660188AEA05AA8D04C94`.
Controller25804/session4449 naturally exited0 and closed; all17 phases exited0:
configure23056/build15080; finalizer U/D29116/21252; validation U/D2712/25744;
query U/D12476/32616; lookup U/D31996/37284; lower CoreModel33528/owner33296/
modulecopy5936/frame18172/query33160/lookup36884; primary25700.

Actual24 protocol names/order PASS/0 FAIL/0 ignored. Raw case23 marker:
`SOURCE_FINALIZED_LIFETIME cycles=16 declared_storage_cleared=16`.
Raw case24 marker:
`SOURCE_FINALIZED_FAULT_SWEEP measured=89 completed=89 original_verify=12 finalized_frame=18 whole_copy=17 candidate_verify=12 candidate_frame=18 snapshot=12`.
This is one89-site measured sweep with full direct-phase request geometry/lower9,
owner/input preservation, candidate cleanup and disarmed retries;24 Unity cases
remain24. Declared inline AST/context/pool clearing is not interner destruction.
No ledger/UBSan errors or failed Setup cascade occurred.

Selected30/FI23 plus support36 gives66 configured current commands/objects. Actual
r2 build had three incremental steps: two production C compilations plus one link,
zero warnings. It is not a clean66-TU rebuild. Runtime current selected pins320,
metadata7 and artifacts74 cover66 objects/map/seven executables; unselected broad
header inventory is historical. U/defined/map evidence identifies actual providers;
query/lookup heap/callback/lifecycle callee checks are object-specific. Accepted
SDK inline stdio T symbols/read-only R literals do not promise CRT internals or
whole-executable allocation freedom. Six lower gates passed separately: CoreModel
28/60 loops, owner14/two61, modulecopy20/88, frame18/18, query18/zeroalloc, lookup8.

Eight-record manifest `E:/cargo-targets/zr_vm/tmp/source-finalized-module-green-r2-freeze.json`
SHA256 `C58506A8971917CA9C23719108A867844EE1B8E77B8F75CA5D4C20748BFE8E19`.
Current production physical pins:

| File | Lines | SHA256 |
| --- | ---: | --- |
| finalizer C |212|`E655E8C70120BEB65A98499CF0727BE7F357F1DE6D4C2AF6F180CEF8AB71DFC7`|
| validation C |578|`5C7E2834934FD5EC7A1EE90251CEA87BE3ED9F9C0EB6EDCBF805E9EBBACDDE2E`|
| private H |89|`288FCCA924D2F0B0D7A8A9AB8040BA48C3178195622210C833C84478098589DD`|

Input94 plus owned candidate33 uses bounded127 spans. The transaction calls six
real allocating lower phases in order and publishes generic owned output last;
all candidate storage is freed on success/refusal. It preserves full9 diagnostics,
old borrowed output until success, typed logical rows/present-empty owners and
Core capacity normalization. No repository source copy, Function double snapshot, new arena,
ID mapping, registry owner or default output is introduced.

### Finite MSVC GREEN r2

`source-finalized-module-msvc-GREEN-r2-receipt.json` SHA256:
`28BC3AFCDCD95A8FC0B7326FDBB8C7BDE0FA0EA5F131E1E8DE24204B45111201`.
Controller27444/session4383 naturally exited0 and closed. All31 ordered compile
phases naturally exited0 with zero warnings/raw basename profiles:23 generic FI
providers and eight unFI test/support/probe units. PIDs19720/25444/10524/37920/
36084/40552/37792/22196/8576/41128/21276/23072/28448/5636/10832/36940/
16628/39164/17844/23032/35252/40348/6560/5084/36768/10380/40188/9924/
39444/10708/34972. Selected current pins240; actual /TC C11 /c /UNDEBUG /MD /W4.
Each immutable C Version1.2 JSON records Source/ProvidedModule(empty)/Includes;
optional ImportedModules/HeaderUnits are absent and explicitly recorded as absent.
Actual selected dependencies/raw/object/JSON/tool/self checks give compile-only
credit: no MSVC link/runtime, fault execution, full-project or full MSVC claim.

## Historical RED status at the r3 checkpoint

The RED checkpoint below was functional-red-verified-commit-pending at that time.
Its stub guarded required NULL and otherwise refused UNSUPPORTED with bounded
original token without allocation/query/Verify/publication. Later actual RED r4
proof/recovery commit/cleanup completed as recorded below; those original receipts
remain immutable. The16 RED paths included production3/test5/allocator2/docs3/
registration3, with only the owned hunk in mixed normal registration and pending
returned Source excluded. GREEN current implementation is the separate evidence above.

## Historical runtime RED r3

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

Six independent lower CTests passed in that historical r3 run:

| Lower gate | Actual finite evidence |
| --- | --- |
| CoreModel | PASS;28 Function and60 Module clone fault loops |
| Generic owner |14 PASS; two independent61-site sweeps |
| Complete modulecopy |20 PASS; composite88 sites |
| Primitive frame |18 PASS;18-site frame sweep |
| Callable query |18 PASS; zero allocator-call observation |
| Canonical lookup |8 PASS |

At this RED checkpoint these were lower prerequisites, not finalizer cases or
a measured finalizer denominator.

The eight-file runtime manifest
`E:/cargo-targets/zr_vm/tmp/source-finalized-module-red-r3-freeze.json` SHA256 is
`269D1A9D5A3405F0B93B2962D50E6FE79781DF01A235DDF58805E7E9523B76A9`.
The r3 revised support source pins were:

| File | Lines | SHA256 |
| --- | ---: | --- |
| fixture C |492|`A3535F5D92CE8395CC72AD11C8E19265BA1C9A95F8B7187317A6D3E8895A8CB8`|
| witness C |347|`32DF9F766FF1891A36E249262647C1D3E8BA188FBE90047C1B6754834D19223C`|
| generic allocator C |112|`57DA2A307904ABE98C3C0E044CEAA6E703A2CB7ADDCE22DED842C81B91C39055`|
| generic allocator H |22|`775411298AC440A5FF68319FD4EB835F6C513C78DDC7D81B52DC17085010C548`|

At this historical checkpoint main498, fixture H81 and faults C158 were frozen;
production3 was RED.
TryRecordFrees is an additive pure nonasserting read: invalid record/output
returns false without changing the output. Fixture/witness numeric observers use
it, defer errors and finish teardown before assertions after intentional Unity
failure. Registered cleanup and original-health evidence precede recovery.

## Historical finite MSVC RED r3

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
At the r3 gate, selected input pins240 matched with zero drift. Each structured C
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

## Historical r3 authoring and pending transactions

The following descriptions record authoring-time status before the later accepted
r4 recovery/cleanup; they are not current pending actions.

At that checkpoint runtime used gate v3; MSVC used immutable v2 (261 lines SHA256
`B50547C21F1B2B30FB1E48768ED73C9B4C6843D02E2A98E0DAD3F36246909420`).
Earlier unexecuted MSVC v1 and all executed attempts are preserved. The read-only
Root review v1 is370 lines SHA256
`B8295BC87ADAE3831309D9C2FC23AED91455C602FFDE61AF66631919C1DB7513`.
At that checkpoint proof had not run and no accepted proof hash was recorded.

Commit v2 is121 lines SHA256
`1916FDB0D0F72765963AEED0936BD90F91440E9F51EAE7BE848373D00ED0667F`.
It requires actual proof pins for all16 paths, stages15 whole paths plus the
single mixed owned include, and preserves foreign pending Source state. Both
Git identities must be He-Jiahui <814111601@qq.com>. At that checkpoint this transaction had not run.
Commit v1 remains immutable historical with its superseded14-path scope.

Cleanup v2 is112 lines SHA256
`A2DF41BAE0D30AFE6EA4A1AD0DE78A2787DB401F9BCDE6565EB3EAE436AC5876`.
Its exact55 obsolete objects are r1 24+r2 31, observed778242 bytes. It requires
future accepted RED r3 proof/exact16 commit/current31 counterparts and seven
explicit naturally terminal/closed Root controller records. The tiny r2/r3 probe
uses literal E paths and equal content hashes; repository sources are never
copied. Removal would be literal, nonrecursive, one object at a time after whole
set and per-path ancestry/regular/hash/size checks. That v2 cleanup never executed; later v4 cleanup is recorded below.
Raw logs, receipts, metadata, maps and current objects remain protected.
Cleanup v1 is immutable historical and will not execute with r2/14 assumptions.

## Historical intended GREEN acceptance

The following RED-era requirements and missing observations are preserved as
history; actual r2 GREEN evidence above supersedes their pending predicates.

Later authorized implementation must pass actual24/0/0 and publish complete
independent ownership with precise lower9 preservation. It must measure the six
phase counts, full finalizer N, every-site direct lower geometry/diagnostic and
disarmed retry independently. Historical88/18/61 cannot supply N. Temporary
candidate cleanup must precede fallback; publication frees old output only last.
Source capacity normalization is allowed; spare bytes are not semantic identity.

The historical RED result had no deep-copy success, post-source-owned lifetime,16-cycle result,
paired-order mutation credit or finalizer fault denominator. Actual readable
compiler storage beyondUINT32_MAX remains unexecuted. Declared inline clearing
is not real context/interner destruction. Original/finalized source-map promises
are declared field consistency, not genuine Binder/compactor producer history.
The width/real Source preparation/registry lifetime/target ABI/default adoption/
normal/Linux/full MSVC/backend parity and all47 remain OPEN. mode4 returned
Source documentation keeps its separate frozen history and is not made GREEN.
No VM/Compiler/Semantic/interner lifecycle, FFI/network/security execution,
SourceExecute, Oracle or native backend admission belongs to this lower closure.

## Accepted RED r4, recovery and obsolete-object cleanup

After a consumed foreign type_system.h change invalidated the attempted r3
Doc-correction proof, fresh RED r4 reran actual dependencies. Runtime receipt
`source-finalized-module-red-r4-receipt.json` SHA256
`DB0ADFBAEA88439CA0E0D4D264CC7E9F2ED84236C7402AF1F0CE1F27DFE331AC`;
primary raw SHA256 `F11CC7B945E7D58A086DEAFDCD0166FDB4A7800128A62E518B2F2DCDD364382E`.
It preserved1 guard PASS/23 precise feature FAIL/0 ignored with healthy cleanup;
MSVC r4 receipt `4EA552A77F6CA239CE225FCF6F03E23AB2B091C1900E72FD9273EB401E796F58`
validated31 compile-only units. Accepted RED r4 Root proof SHA256
`FB134DA0136E1C7EC3E238F6E796D4B2717F9583DCCD44741496644D6F628097`.

Failed r3 commit receipt `50F11C0CE94741369862FDA3E943A17561B53EC486C33107B36B483E714DE6E8`
stopped at cached diff-check2 after staging exactly16 owned paths; no commit/reset.
The two Doc EOF blank bytes were removed without source/native/index mutation.
Doc-correction attempt `1BAE82CF662D437273FBE28497A0B173645201ED07D60E57EADB7D48D9517394`
remained validated:false because consumed type_system.h had changed, not because
Doc pins were waived. The existing staged transaction was preserved until fresh r4.
Recovery r2 receipt `779EB347D327528AC0C3BBA9DE6B8C2F83344D296F948E91AB1274016778141D`
committed exact16 RED paths as `1f9488b9c4065c54980e3841bc1a41279bdebb3a`,
parent0887b2bda1ebc0fd8b23d4a4c42bacaaf6baf847, both identities He-Jiahui
<814111601@qq.com>, with empty cache/no lock and pending Source preserved.

Actual cleanup r1 receipt `C159B421CE2561889F23C8203226768C31699FD8A380939FF61D51D4A64F4424`
validated literal nonrecursive deletion of86 obsolete MSVC RED objects
(r1 24+r2 31+r3 31), reclaiming1,245,238 bytes after accepted RED commit/current
counterparts/natural closed-controller proof. Logs/JSON/receipts/maps/current31
objects remained. Superseded unexecuted cleanup v1-v3 and all failed attempts
remain history; this completed RED cleanup grants no future GREEN deletion credit.

## Preserved first GREEN build failure

Runtime GREEN r1 receipt `0FCDF3A8AF0C510A45630ACECAF9B47708F195EA00533D673448D274B132ACC3`
is validated:false. Configure34620/build34256 naturally exited0, but strict gate
stopped on17 production diagnostics:16 typed SZrArray head pointer assignments
and one signed enum/UInt32 comparison. No primary/lower CTest credit from r1.
Narrow explicit typed casts and enum-width correction in the two owned C files
resolved those diagnostics; r2 independently compiled both and linked with zero
warnings. The failed r1 receipt/raw logs stay unchanged.

MSVC GREEN r1 also failed strict acceptance. Receipt
`source-finalized-module-msvc-GREEN-r1-receipt.json` SHA256
`9EE0808322EFD5E5C19FDC70146748FA78EE4DAAAE4CCD68F105E46B94A8792D`;
controller5640/session30217 naturally exited1 and closed. All21 recorded compiler
children naturally exited0; first20 had zero warnings, then finalizer16968 emitted
two C4133 typed-pointer warnings and failed the strict raw basename profile.
Finalizer raw SHA256 `CB535FFDF526FA4E343F657B85D346C61D18697400F48A27C76D4A8B53BA44C8`.
There is no31-unit/zero-warning acceptance from this partial attempt. The two
explicit finalizer C casts are part of the16 typed-pointer corrections accepted
by fresh runtime/MSVC r2; earlier receipts and warning logs remain unchanged.

## Current limits and remaining delivery

The actual24/89/16 observations establish this declared finite storage contract.
They do not establish genuine Source/Binder/compactor history, Compiler/Global
bootstrap, context/interner destruction, durable registry ownership, target ABI,
Source wrapper mode4 GREEN or executable admission. Actual readable compiler
storage>UINT32_MAX remains unexecuted despite bounded malformed-header tests.
Ordinary/default adoption, broader Source, normal-project runtime, Linux, full
MSVC, backend/native parity and all47 remain OPEN. No SourceExecute, Oracle,
FFI/network/security or generated native execution occurred in this lower gate.

The exact finite GREEN commit is `d475d47494d5b7566e258dd379466ca099b3da6f`.
This documentation synchronization does not change the finite validation scope.
Separate returned Source docs remain frozen and retain their own build-only/failed history.

- [Private contract](../parser-and-semantics/source-finalized-module.md)
- [Exact24 protocol](../testing-and-validation/source-finalized-module.md)
- [Callable query](source-callable-query.md)
- [Frame validation](primitive-frame-validation.md)
- [Core copy](exec-ir-module-copy.md)
- [Generic owner](ssa-execir-owned-module.md)
