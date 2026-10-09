---
related_code:
  - tests/core/test_ssa_exec_ir_module_copy.c
  - tests/core/support/ssa_exec_ir_module_copy_fixture.h
  - tests/core/support/ssa_exec_ir_module_copy_fixture.c
  - zr_vm_core/include/zr_vm_core/exec_ir_module_copy.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_module_copy.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_module_copy.c
plan_sources:
  - .codex/plans/20261009-ssa-module-final-function-copy-design-v1.md
tests:
  - tests/core/test_ssa_exec_ir_module_copy.c
  - tests/core/support/ssa_exec_ir_module_copy_fixture.h
  - tests/core/support/ssa_exec_ir_module_copy_fixture.c
  - tests/core/test_ssa_exec_ir_owned_module.c
  - docs/testing-and-validation/exec-ir-module-copy.md
doc_type: acceptance-record
status: green-observed-rootproof-commit-pending
---

# ExecIR module-copy finite GREEN observations

## Authority and current implementation

Root has accepted the independent Core copy design and authorized RED source/
test drafting. The plan still labels itself DESIGN ONLY; this record reflects
Root's subsequent authorization without changing that plan.
Actual plan SHA is
`6A755C0B1E0764361D9435FC8C97292FDCF9DEECAB5CE93B614093BADE2A969C`.

Historical RED public header SHA is
`950EAE336E6E24A469142A865C826B8980EBCFA1D92B75E2B4DE3588EB0B3B4E`;
implementation SHA is
`2AECC6A42DDA04D310190286098E7BC4946FB9EBA0ACACACA436AF8F895FE777`.
These are physical read observations, not a clean/staged/committed claim or a
Root acceptance receipt. Historical RED source resets all nine fields, refuses NULL/
exact source alias and selectedId range, then returns UNSUPPORTED without
allocation, clone, ownership release or destination publication.

## Lower prerequisites and separate source gate

Generic owner GREEN is committed
`3496923f58c6f4be2bb61c82c51a7743bef9fba4`; Function owner GREEN is committed
`112cbf4256a819d554659c7bdfd9d77eb8a04610`. Their accepted finite14/20 cases
and61-site sweeps are recorded in their separate acceptance documents. They
establish lower storage/lifetime support, not this new composite denominator.

Returned-source mode4 retains its separate uncompacted contract. Its fresh
current runtime acceptance is pending; this draft makes no Source GREEN claim.
Neither this Core API nor existing numeric registry identities finalize source
semantics or extend canonical registry ownership.

## Actual finite GREENr1 observed

Current public header SHA is
`34937E7E9DBD02D8D6904B9DA049F94BCB2BF69D998567066007567072C6D863`;
current implementation SHA is
`50BD7505BA9A017DADD6A19968C1DD8DF3484886A626CE49DED413E0C66D1A37`.
It implements initialized candidate CloneModule, ordered immutable identity
checks, CloneFunction replacement, candidate-only rollback and old-destination
release/publication last. Entry/success reset all nine fields; lower failures
survive candidate cleanup unchanged. This physical read is not a commit claim.

GREEN receipt `execir-module-copy-green-r1-receipt.json` in
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b` has SHA
`70A78108A3A1683D05EA6D60BE4E8BABFFF846417BD2EA2C1BFA163168B0D50D`.
It records validated true/error null,20PASS/0FAIL/0ignored. Raw module CTest log
`execir-module-copy-green-r1-module-copy-ctest.log` SHA is
`2F6800B7F0C8BDF1BC854D058C404BCC88233C94E6EA0CCD43FE69F547D7C46E`.

Actual raw marker is
`MODULE_COPY_FAULT_SWEEP module=60 replacement=28 composite=88 completed=88`.
All three allocation denominators are independently measured for the actual
two-function/one-row/equal-replacement profile; historical61 is not used.
The88 injected composite failures compare direct lower nine-field diagnostics
and allocation kind/count/width/bytes, preserve source/replacement/old output,
release candidate records and complete disarmed retry. This sweep is one Unity
case among20, not88 extra cases or arbitrary-geometry coverage.

Configure35888/build39404/CoreModel36516/generic37048/module30104 all terminate
naturally0. All five physical raw-log hashes match receipt and show no warning,
ledger-error or UBSan diagnostic. CoreModel and generic14 independently PASS.
Actual GREEN graph remains41 TUs/41 objects/6 providers in small-direct OBJECT
closure; no residual archive, Parser or vendor-body integration is credited.

MSVC GREENr1 receipt `execir-module-copy-msvc-green-r1-receipt.json` SHA is
`FBE8284B48DDB955FC1339636DD6E070E5DD72BE38EA9B2809FAAB9633500DF7`.
Actual header39956/module-copy40704/test30372/fixture38836/generic-fixture2628/
allocator33752/exec-ir11712 all seven terminate naturally0/zero warnings.
All seven raw-log/object hashes independently match receipt. This is compile-only
and supplies no runtime/semantic admission evidence. Root current proof and
GREEN commit remain pending; full MSVC/Linux/normal/platform gates remain OPEN.

## Preserved functional REDr1 history

Primary receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/execir-module-copy-red-r1-receipt.json`,
SHA `0111DEAE24B576C3EC4B9C8BA21AD6D8592A8E5D752A773F40ABC542F2240117`.
Its actual record is validated true/error null. Raw module CTest log
`execir-module-copy-red-r1-module-copy-ctest.log` SHA is
`FF69110E17798D64C5E7A0E2141F32437CAE810952887B7A6F3D655019A1A45B`.

Actual20 is3 guard PASS/17 feature FAIL/0ignored. NULL argument, selected range
and exact source/destination alias guards PASS. All17 remaining cases fail with
`actual whole-module selected-function copy must succeed`; each records
`MODULE_COPY_FEATURE code=28 attempts=0`. Real direct lower clones construct
expected storage and healthy old output before these assertions. RED does not
reach the new composite sweep and supplies no measured composite denominator.

Configure34972/build34788/CoreModel19396/generic6976 terminate naturally0;
module41520 terminates naturally8. Independent generic14 and CoreModel support
PASS. All five physical raw-log hashes match receipt; none contains warning,
ledger-error or UBSan diagnostics. Assertions and UBSan are enabled. This is a
finite historical RED feature gate. RED committed
`fcc84729099e5bbb4de0a16dd3980c3a545be139`; commit receipt
`execir-module-copy-red-commit-r1.json` SHA is
`AF41B8730CB91E25241184D1D2615AD51CBCE269C509BDBB2F9B182401BCFC06`.

Actual small-direct configuration has41 TUs/41 objects and6 unique providers:
FreeModule, CloneModuleReplacingFunction, CloneFunction, CloneModule,
StateMapFree and StateMapClone. Target link settings contain only private Core
and module-copy OBJECT target expressions. This finite gate has no residual
archives, Parser or vendor-body compilation. Later normal-project integration,
Parser consumers and any corresponding residual exclusions remain independent
OPEN gates, rather than prerequisites falsely credited by this small build.

The actual test profile contains at most two fixed one-row functions with equal
source/replacement geometry; witness4096 words and256 pointer-free address/
record slots bound observation. Present-empty and normalized-capacity assertions
cover the actual GC/state/frame header presence/counts, StateMap count/capacity,
and value/instruction capacity>=live count. No all-spare-byte or arbitrary-shape
coverage follows. Nine-field diagnostic/borrow-before-free/direct fault geometry
and healthy teardown requirements were not exercised beyond the RED feature
assertion; the current GREEN observations above cover this finite profile.

Finite MSVC receipt `execir-module-copy-msvc-red-r1-receipt.json` in the same
report directory has SHA
`9BA515C0526F7E6D89FD0290EBDE780E0FFE5676F6AC33EB4B03E4495A7274D3`,
validated true/error null/compile-only. Actual seven units header25080,
module-copy38056,test29768,fixture40020,generic-fixture35484,allocator24960,
exec-ir41204 each terminate naturally0/zero warnings. All seven raw log and
object hashes independently match the physical receipt. This finite result
performs no link/runtime test, diagnostic execution or fault sweep; full MSVC
remains OPEN. At this historical RED point the composite denominator was
unmeasured; current GREEN independently measures88 as recorded above.

No GREEN commit or source runtime acceptance is inferred. Root current proof
and GREEN commit remain pending. Source documents
remain frozen under their separate gate; this record does not claim Source GREEN.
Any failed attempts and emitted diagnostics will remain historical evidence
when actual receipts are supplied.

## Implemented finite contract and open acceptance

Current GREEN implements CloneModule into initialized candidate,
complete ordered identity comparisons and CloneFunction into selected slot,
then free old destination and publish only after all fallible work. Failures
release only candidate and preserve source/replacement/old output. All nine
lower diagnostic fields must survive cleanup unchanged; wrapper identity
refusals obey complete reset and the frozen expected-source/actual-replacement
mapping. No VerifyModule/ExecutionContract_Check admission gate may be added.

Actual finite20 establishes logical module/storage preservation within the
fixed two-function/one-row profile,
replacement/old-source borrows, independent post-producer lifetimes, normalized
capacities and repeated balanced release. Actual composite fault sites and each clone phase are measured independently;
no historical lower61-site count is used.
Every measured fault checks direct lower diagnostic comparison, exact candidate cleanup,
old-output preservation and disarmed clean retry before recovery assertions.

Finite compile checks remain compile-only. Core storage success does not prove
canonical/source/frame compatibility, default publication, arbitrary graphs/maps,
owned registry lifetime, runtime/backend parity, Linux/full MSVC or all47.
Those broader gates remain OPEN; the later Parser consumer requires its own
genuine semantic/canonical/frame checks and verified candidate publication.

- [Detailed storage/diagnostic contract](../core-runtime/exec-ir-module-copy.md)
- [Twenty-case and ledger design](../testing-and-validation/exec-ir-module-copy.md)
- [Generic owner prerequisite](ssa-execir-owned-module.md)
- [Function owner prerequisite](ssa-function-exec-ir-owner.md)
