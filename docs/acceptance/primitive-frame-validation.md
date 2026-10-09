---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_source_frame.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_source_frame_validation.c
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
  - docs/testing-and-validation/primitive-frame-validation.md
doc_type: acceptance-record
status: functional-green-verified-commit-pending
---

# Primitive frame validation: finite GREEN verified

## Authorization and separate prerequisites

Root authorized independent RED drafting and a finite compile-only driver after
the design/test protocols. Design103 SHA is
`89C400256D39A5A813FE95022F9489B4F8F0B5DAF68BE2A2A98E93EFACA451E3`;
test design83 SHA is
`BE550E43003FF5448A16C9F947E5FBEA0704D325302A4AFBA51387302497D454`.
Their earlier design-only wording is historical authorization state; this record
does not edit those plans or expand source/test/CMake leases.

Core module copy `238fb35d160bad6142acee08da910742b0f1d2c9` and canonical lookup
`b24fa1b` are separate committed prerequisites. Core20/composite88 and lookup8
do not transfer allocation counts or acceptance to this new validator. No Core
copy TU is required by the frame-only candidate closure.

## Current accepted GREEN r2; final proof/commit pending

Runtime `primitive-frame-validation-green-r2-receipt.json` SHA
`3D01DCE7610B722CAD1185D8ED0B6D0930DE3411D30ADB45D813578A08BB925B`
is validated:true. Raw frame CTest SHA
`8802D6EBDA35FB729D4C5E8E7345DC103FBAD6B8516895DF7A0AAB83180E639A`
records18 PASS/0 FAIL/0 ignored. Actual marker is
`PRIMITIVE_FRAME_FAULT_SWEEP verify=12 request=1 packed=5 measured=18 completed=18`.
These independently measured18 validation fault/retry subobservations are within
18 Unity cases; they are unrelated to prior Core88/owner61 counts.

Actual Attach supplement reports
`PRIMITIVE_FRAME_ATTACH_FAULTS values=1 header=1 completed=2 attempts=31 values_site=13 header_site=19`.
The successful Attach trace has31 allocation attempts; two direct sites are
faulted/retried with full9 diagnostic, const failure preservation, owned success
publication and balanced temporary pairs. No legacy PackedFree callback ran.

| Runtime phase | Actual PID | Natural exit |
| --- | --- | --- |
| configure |20812|0|
| build |40964|0|
| CoreModel CTest |41800|0|
| generic owner CTest |37420|0|
| frame validation CTest |4364|0|

CoreModel/generic14 support PASS. Current5688 inputs/6 metadata/27 artifacts
(23 objects/map/3 executables) pin selected23/FI18/configured51 and9 unique private
providers. GREEN r2 build is main1+relink, exactly2 steps; previous objects are
hash-current. GREEN r1 built production2/test2/link,5 steps. Incremental output
and current diagnostic gates do not claim a full clean rebuild of normal scope.

MSVC `primitive-frame-msvc-green-r2-receipt.json` SHA
`E33B7D8BCD191AAD9BBBD71B2FD30916ABD8BC6D77AD4BCED7EE1193EFED5625`
is validated:true,4304 input pins. Original7 units plus legacy Attach/shared
units have actual PIDs28204/39700/25780/35916/31552/34936/31808/29072/38916,
all natural exit0/zero warnings. Original Parser3 generic FI is retained; the
last2 use the original Attach OOM shim. This is compile-only with no old callback,
Source CMake runtime, fault activation, linking or full MSVC runtime credit.
Driver v2 is152 lines SHA
`18D364BA01CC3248E49E321F3C7EB8A1ADA24FD4DB2D97576EDB817A244C4F5F`.

| Current physical file | Lines | SHA256 |
| --- | --- | --- |
| Public source-frame H |84|622C04D2E8094627E16818E17DF2F0BA1C86FAA132EACF3CACF238B399691A53|
| Attach C |72|A44A6DD949FBFE6C18CF589A93A4A9DC73C7F5DF08D94E8160A441A3758F1ED7|
| Private shared H |39|E0BC9631C1A13DCD8C91530FE47B0C867945CF14767CB6CEA3593CB7A302E10B|
| Shared validation C |411|22EEA41B82776C0B8789A68AB06FFE0EF7FC9813C48AF5F5C7A25A57E40F1CF1|
| Main test C |677|688F9BB601097A280924D5A835DCA74157B38D8A72138E04D26DAC32AF408659|
| Fixture H |35|8A4778FEDB02F47F0DA797E8BEAE0C37DE04C969B622AA59CBAF90F585DB177D|
| Fixture C |518|85D148A1614186F7AD643D62686AA31013A8453897F7E8B5A6D462383A655470|

Eight shared helpers, optional NULL diagnostic, exact lower9 preservation,
read-only storage and temporary cleanup are now implemented. Declared context
and spare values/frame3→clone4 remain typed finite witnesses, not Source history.
Normal/direct legacy OOM registrations select both real TUs with the old shim;
source-read and legacy MSVC compile compatibility do not credit old callback runtime.
Pending Source Parser6/exclusions8 registration adjustments do not accept or
stage the entire Source scope. Final current Root proof/12-path GREEN commit
remain pending; no completed commit is asserted here.

## Preserved GREEN r1 failure and compile-only history

Runtime GREEN r1 receipt SHA
`6F9AE78336718DC4F7EE2B635B179E26338094C2417CE597BE9EDA0D77258364`
is validated:false;17 PASS/1 case8 FAIL, `Expected 0 Was 28`.
Raw CTest SHA is `6D3502A8B086E065CC6A9A0C3AE78A41178319A07F571A4192D9D9503E9AD7CF`.
Core allows definition0/known flags1, while the wrapper correctly refuses that
admission with UNSUPPORTED. The corrected case8 uses id0, which actual Core
rejects as index+1, restores input, and checks non-NONE/full9 lower diagnostics.
Production was unchanged; no waiver or error normalization was introduced.
MSVC GREEN r1 receipt SHA
`6AB001145989B0137E0305B98B95721CE31A70843D0458FF34B4589680774788`
was accepted nine-unit compile-only history for the prior main, not current r2
runtime acceptance. Failed runtime/raw history remains preserved.

## Historical committed RED staging

The physically read validator resets all9 fields, handles NULL top-level guards,
then reports UNSUPPORTED/functionToken with other fields0. It allocates nothing
and does not yet execute the intended storage/Verify/request/packed comparison.
It publishes nothing. The additive public contract describes later GREEN;
existing Attach remains the genuine preparation producer.

## Actual RED r2 evidence

Reports live under `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/`.
Root runtime receipt `primitive-frame-validation-red-r2-receipt.json` SHA
`E9548E987717E82EF8EDFA5AC900DB389ED04F3FFBC29AF40960FD577907E0B2`
is validated:true. Raw `primitive-frame-validation-red-r2-frame-validation-ctest.log`
SHA `EFC2F5E98C38CAEDCDFD1FED334A37D80A6BB892E8D0BB3D8AD343569AC14E12`
records18 tests: case4 PASS,17 exact feature FAIL,0 ignored. Each refusal checks
all9 fields and emits `PRIMITIVE_FRAME_FEATURE code=28 attempts=0`; the exact
failure text is `actual const primitive frame validation must succeed`.

| Runtime phase | Actual PID | Natural exit |
| --- | --- | --- |
| configure |15940|0|
| build |36680|0|
| CoreModel CTest |34488|0|
| generic owner CTest |37420|0|
| frame validation CTest |25760|8, required RED outcome|

CoreModel and generic14 support PASS. Actual selected23/FI18/provider9 are in a
51-TU configuration, using Core15+Parser3+lookup1+main/fixture/allocator/Unity4.
Real provider pins cover validator, Attach, LayoutPackedFrame/Init/Free, Find,
VerifyFunction, CloneFunction and FreeFunction. r2 build log has exactly two test
TU compiles and relink; no new warning/UBSan/ledger diagnostic is credited as a
full clean rebuild or as new validator allocation coverage.

MSVC `primitive-frame-msvc-red-r2-receipt.json` SHA
`AA29CAA591E505C44B5BF7EDB5575B4A7334D5B03EC042AAAB8DF57EA8D49DD4`
is validated:true, compile-only. Units header/Attach/frame-layout/validator/main/
fixture/allocator have actual PIDs41924/12064/35284/41900/2060/38968/29052,
all natural exit0 and zero warnings. Parser3 uses generic FI; fixture has no FI.
No runtime/diagnostic/fault behavior is established by this MSVC compile gate.
Driver149 SHA `7334652A9401167B810A7A6B86AAC4DA269B3C2CF9AFC257C882E95BD1730BB2`
remains frozen; tool/header/source/script/log/object pins are in its receipt.

Historical RED main676 SHA `77A34EC747B4F099033560A079DF853E889B7A46DD979A443D6E39A40D2DBC3D`;
fixtureC328 SHA `B3C7B03F3C8A9AE9FE5FEF0ADF75702A9532AF36CCCB74AF8097C78204263D7E`;
fixtureH34 SHA `9FECF75A7BA5AE67054954F0D1EF54538FC6884084F31090DC2F611E7F3CCE9F`.
Validator RED C24 SHA `24CF233E9079D724BF373F7212C15047F0A3C125C29457B8124F5197E9061DB4`.
Source values/frame capacity3 clones to real Core reserve4; initialized spare witnesses and
nonzero SourceMap IDs501/502 are finite typed storage observations, not Source
provenance. No new allocation denominator is measured under this RED stub.

Accepted extraction addendum SHA
`8400EA21254376AE79B761C34B0059D8AF32FC3D7C935FC0ABCAAF086ABE8217`
splits nonallocating admission from request preparation, eight private helpers,
so missing-frame refusal precedes allocation without changing Attach order.
At RED commit this authorized later GREEN mechanics; current GREEN now implements them.
RED was committed as `d68281c`; its proof SHA is
`3B9EBE22488A0C4FDBEF56CEC366603A013331C0D9D8F8C22A54061A954D6E54`
and commit receipt SHA is
`E2F09F048B89EEB2CD1E225497BBA461EE591B206EA617951BED9FB0163883B3`.
Current GREEN evidence is recorded above; final GREEN Root proof/commit remain pending.

## Preserved failed r1 history

Runtime RED r1 receipt SHA
`301BB356AB28A9225E708CE3B1213CE3F70CF00893892ACE5AB47B4F26E5B371`
is validated:false: configure/build natural0 but a wrong ID pointer warning fails
the strict diagnostic gate before any CTest. MSVC RED r1 receipt SHA
`576F23E2900A6372DD044E763E3472D84EE6E8EB1F358BB6608F8C9228D21042`
is validated:false: first five units compile exit0, main has one warning, and the
gate stops before fixture/allocator. r2 test correction resolves that warning;
failed receipts remain historical and are not relabeled successful.

## Implemented finite GREEN boundary

GREEN preserves Attach orchestration/refusal/publication while sharing actual
storage/body/request/geometry preparation. New const validator accepts sealed/
unsealed input, regenerates the real packed result, compares logical scalars,
hashes/slots and borrowed-view geometry, then frees only its own temporaries.
All9 wrapper/lower diagnostics and first-error order match the frozen table;
no repair, persistent header, owner publication or borrowed-view Free occurs.

Real Attach+Verify+Clone prerequisites use declared primitive field/context and
explicit layout owners with two differing geometries. They prove typed predicates,
not interner hash authenticity, genuine SCRIPT Binder or target ABI provenance.
Clone equality excludes capacity/padding/spare bytes; current spans remain valid.

Measure Verify/request/packed and full successful validator allocation phases
independently. Every measured injected fault must use a direct lower witness,
exact9 fields/failed event geometry, const producer preservation, temporary balance
and disarmed retry. Four eager layout mallocs mean failed ordinal and total
attempts can differ. Ledger recovery must not hide leaks; no completed sweep
marker may appear before all actual iterations finish. Core88 and historical61
loops remain unrelated lower support evidence.

## Open acceptance

Finite MSVC is compile-only. The current gate proves no Binder/source wrapper
GREEN, canonical interner/registry retention, target ABI, whole-module finalizer,
mode4/default migration, complete04.01/04.02/04.04, normal/Linux/full MSVC/native
backend parity or all47 completion. Those gates remain OPEN. Foreign Source
warning blockers, active jobs and shared CMake state are preserved.

- [Contract and complete diagnostic order](../parser-and-semantics/primitive-frame-validation.md)
- [Eighteen-case/fault protocol](../testing-and-validation/primitive-frame-validation.md)
- [Canonical lookup prerequisite](canonical-type-lookup.md)
- [Core module-copy prerequisite](exec-ir-module-copy.md)
