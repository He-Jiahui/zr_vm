---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/include/zr_vm_core/function_exec_ir.h
  - zr_vm_core/src/zr_vm_core/function_exec_ir.c
  - zr_vm_core/src/zr_vm_core/function_ir_owners_internal.h
  - tests/core/test_ssa_function_exec_ir_owner.c
  - tests/core/support/ssa_function_exec_ir_owner_ledger.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/function_exec_ir.h
  - zr_vm_core/src/zr_vm_core/function_exec_ir.c
  - zr_vm_core/src/zr_vm_core/function_ir_owners_internal.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
plan_sources:
  - .codex/plans/20261009-ssa-function-execir-owner-design-v1.md
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
tests:
  - tests/core/test_ssa_function_exec_ir_owner.c
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/core/test_ssa_function_aot_owner.c
doc_type: acceptance-record
status: functional-red-verified-commit-pending
---

# Function ExecIR owner finite acceptance

## Established lower prerequisite

Generic independent ExecIR storage ownership is committed as
`3496923f58c6f4be2bb61c82c51a7743bef9fba4`. Its actual14 cases and two61-site
fault sweeps, lower CoreModel support and seven-unit MSVC compile-only evidence
are recorded in the [generic owner acceptance](ssa-execir-owned-module.md).
Existing AOT Function ownership has its own separate finite evidence. Neither
prerequisite proves this new association or mixed-layout Function consumers.

## Current RED design boundary

The source lease covers five additive integration paths. SnapshotExecIr refuses
non-NULL Function/source pairs with UNSUPPORTED, full reset and zero payload,
without allocation/publication/mutation. NULL arguments report INVALID_ARGUMENT.
New's NULL guard/dual-slot initialization, aggregate release and move-forget,
borrowed getter and immediate ExecIR-only alias clearing are real RED support.
No successful association is inferred from these declarations or lower GREEN.

The planned default inventory has20 cases. The five RED source paths and six
test/support paths are frozen. Actual record size/owner offsets, selected rebuild
graph and runtime evidence are now recorded below for r3. Function fault
denominators and successful association assertions remain uncredited. No
clean/staged claim or unobserved fault count is inferred. Root owns provenance
and any commit.

## Actual build history and lower checks

Native r1 failed before native compilation on a null metadata pin. Native r2
configure15432 exited0 and build8876 exited1; its build recorded174 baseline
warnings and duplicate ordinary/private allocation providers. Neither attempt
executed the20 Function cases, so no functional RED or loop denominator is
credited. Failed receipts and build logs remain historical evidence.

The frozen test SHA is
`D677C1CF8A6EA0398AEE4D6E80B74C4D805D40B5D9E7C0F269384784339D0D75`.
Before runtime, the actual Function_New allocation flag expectation was corrected
to OBJECT and the required AOT owner header was explicitly included.

Fresh core-model RED r1 support exited0. Finite MSVC RED r2 seven units all
exited0/0warnings: header probe, Function, association, test, ledger, fixture
and AOT owner. This is named compile-only scope, not20-case runtime acceptance.

Reports are under E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b:

| Actual receipt/log | SHA256 |
| --- | --- |
| function-execir-owner-red-r2-receipt.json (failed build) | `D779B14DB9407D6AAEF05DB259854FECE45FF400246874E9EEEFB03510B58384` |
| function-execir-owner-red-r2-build.log | `B73EE1FA30701D6326C9300112413FC29BF06762F282DF00351C290BB450F0B8` |
| function-execir-owner-msvc-red-r2-receipt.json | `3CC9963C8D3C950A21F97A4A7BB47CE185424E9F21EFEE0210F8BC1D6D3235AD` |
| function-execir-owner-core-model-red-r1-receipt.json | `5870ACD407CFA86BB6836B140DE6AB616085497E78C3671E8607BD68A4B69D46` |

Root's current build-selection repair excludes exactly15 ordinary ExecIR and
one AOT owner member from a derived compiled Core archive used only by the
dedicated Function test. The normal route excludes those16 source providers.
The new residual-archive CMake helper expands the repository scope to18 paths;
no production source is copied or reconstructed. Fresh runtime/map/provenance
evidence was subsequently supplied by r3 below; no normal-index claim is made.

## Accepted functional RED r3

Configure41464/build15780/ordinary archive11960/residual12860/support37524
all exited0; Function CTest37664 exited8. Actual20 cases produce4PASS/16FAIL/
0ignored. Every feature failure is exactly `actual Function ExecIR attachment
must succeed`, reached through the real UNSUPPORTED association stub. Passing
cases cover the typed/generic prerequisite, empty owner slots, New(NULL)/valid
current allocation and NULL Function diagnostic. No ledger or UBSan error is
reported. Successful association, dual-owner moves/trees and Function fault
loops are not credited by these early refusal failures.

The actual runtime/header probe reports Function size1208, AOT offset1192 and
ExecIR offset1200. The real GC allocation size and OBJECT type-tag assertion
passes. These numbers describe this selected build only.

R3 pins an actual255-unit compile graph, ordinary Core archive222 members and
residual206, exactly16 removed. Its map verifies18 private/residual provider
symbols, including real Function/Global lifecycle and private ExecIR/AOT owner
closure. Existing AOT12 and generic14 support pass. Generic's two61-site fault
sweeps are lower prerequisite evidence; no Function fault denominator completed
under RED. Finite MSVC7 and CoreModel receipts above remain valid.

This r3 incremental build only relinks/filters and emits no new diagnostics.
It does not erase the174 r2 baseline warnings or establish a warning-free full
rebuild. Earlier failures and warning logs remain preserved.

| Current accepted RED receipt/log | SHA256 |
| --- | --- |
| function-execir-owner-red-r3-receipt.json | `DD348580AD04139D16D20178436FE6C77677F8B594E7AB2290F64EAC7DD79334` |
| function-execir-owner-red-r3-function-ctest.log | `0D07C8A51B7DA8F07675F29C4D14326337F2AC36F8DB0C911BA1FCE22AAEAEE6` |

Functional RED is verified; its separate commit remains pending. GREEN is not
implemented or accepted by this record.

## Required RED and future GREEN evidence

Actual lower fixture/generic-owner and valid VM bootstrap prerequisites must
pass before constructor-refusal failures qualify as feature RED. New(NULL)
must exercise argument refusal; raw GC allocation retry/throw is not faked as
a simple NULL result. Current-sized consumers and real Global shutdown are
mandatory lifecycle evidence.

Future GREEN must preserve exact lower diagnostics and the other owner slot,
allow the old borrowed ExecIR view as source, and publish only after clone
success. All20 cases and actual measured attachment/replacement fault loops,
owned/borrowed graphs, two moves, explicit free and shutdown must pass with
exact independent ledger balance. Generic/CoreModel/AOT Function regressions,
selected compiler/header probes and finite MSVC provenance are separate gates.
Receipts and failed attempts will be recorded here after actual observations.

## Remaining acceptance

The runtime-only Function slot creates no canonical semantic proof, Verify_ALL
admission, new hash/schema/remapping, executable entry or one-arena ownership.
Top-level const does not enforce nested pointer immutability. Old smaller
records, concurrency and nonlocal VM exception recovery are outside scope.
Full Function migration, source/default canonical ownership, complete01.01,
Linux/full MSVC, native/backend parity and all47 SSA leaf gates remain OPEN.
No source, Oracle, generated native, network/FFI/provider/security execution is
authorized or credited by this finite work.

- [Module contract](../core-runtime/ssa-function-exec-ir-owner.md)
- [Twenty-case design and ledger scope](../testing-and-validation/ssa-function-exec-ir-owner.md)
