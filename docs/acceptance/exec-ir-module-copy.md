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
status: functional-red-observed-commit-pending
---

# ExecIR module-copy pending finite acceptance

## Authority and current implementation

Root has accepted the independent Core copy design and authorized RED source/
test drafting. The plan still labels itself DESIGN ONLY; this record reflects
Root's subsequent authorization without changing that plan.
Actual plan SHA is
`6A755C0B1E0764361D9435FC8C97292FDCF9DEECAB5CE93B614093BADE2A969C`.

Actual read public header SHA is
`950EAE336E6E24A469142A865C826B8980EBCFA1D92B75E2B4DE3588EB0B3B4E`;
implementation SHA is
`2AECC6A42DDA04D310190286098E7BC4946FB9EBA0ACACACA436AF8F895FE777`.
These are physical read observations, not a clean/staged/committed claim or a
Root acceptance receipt. Current source resets all nine fields, refuses NULL/
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

## Actual functional REDr1 observed

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
finite observed RED feature gate, with GREEN implementation/proof/commit pending.

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
and healthy teardown requirements remain future GREEN checks where RED exits
at the feature assertion.

| Remaining evidence | Current state |
| --- | --- |
| Finite MSVC seven-unit compile gate | REDr1 observed, compile-only7/7 natural0/zero warnings |
| New composite allocation denominator and completed fault/retry sweep | Not observed under RED |
| Independent GREEN implementation/full20 | Pending |
| Root current proof and RED commit | Pending |
| Normal/Parser integration and platform gates | OPEN |

Finite MSVC receipt `execir-module-copy-msvc-red-r1-receipt.json` in the same
report directory has SHA
`9BA515C0526F7E6D89FD0290EBDE780E0FFE5676F6AC33EB4B03E4495A7274D3`,
validated true/error null/compile-only. Actual seven units header25080,
module-copy38056,test29768,fixture40020,generic-fixture35484,allocator24960,
exec-ir41204 each terminate naturally0/zero warnings. All seven raw log and
object hashes independently match the physical receipt. This finite result
performs no link/runtime test, diagnostic execution or fault sweep; full MSVC
and the new composite denominator remain OPEN/pending respectively.

No new commit, GREEN or source runtime acceptance is inferred. Source documents
remain frozen under their separate gate; this record does not claim Source GREEN.
Any failed attempts and emitted diagnostics will remain historical evidence
when actual receipts are supplied.

## Future GREEN acceptance boundary

GREEN must independently implement CloneModule into initialized candidate,
complete ordered identity comparisons and CloneFunction into selected slot,
then free old destination and publish only after all fallible work. Failures
release only candidate and preserve source/replacement/old output. All nine
lower diagnostic fields must survive cleanup unchanged; wrapper identity
refusals obey complete reset and the frozen expected-source/actual-replacement
mapping. No VerifyModule/ExecutionContract_Check admission gate may be added.

Future default20 must establish full logical module/storage preservation,
replacement/old-source borrows, independent post-producer lifetimes, normalized
capacities and repeated balanced release. Measure actual composite fault sites
and each clone phase independently; do not infer a sum from lower61-site gates.
Every fault requires direct lower diagnostic comparison, exact candidate cleanup,
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
