---
related_code:
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.h
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - tests/cmake/ssa-execir-owned-module.cmake
  - tests/cmake/ssa-execir-owned-direct-validation/CMakeLists.txt
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owned_module.c
implementation_files:
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/cmake/ssa-execir-owned-module.cmake
plan_sources:
  - .codex/plans/20261009-ssa-execir-owned-module-tests-v1.md
  - .codex/plans/20261009-ssa-execir-owned-module-design-v1.md
tests:
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/parser/test_ssa_core_model.c
  - docs/acceptance/ssa-execir-owned-module.md
doc_type: testing-guide
status: finite-red-ready-to-submit-commit-pending
---

# Finite Core ExecIR owner tests

## Frozen 14-case inventory

The frozen actual test contains14 default Unity cases. Current RED r5 observed1PASS,
13featureFAIL and0ignored. Only case1's genuine lower prerequisite completed;
owner cases stop at the actual constructor refusal before their GREEN assertions.

| Case | Purpose |
| --- | --- |
| 1 | Existing full constructed CloneModule prerequisite |
| 2 | Empty owned module, NULL view and idempotent slot-clearing free |
| 3 | Two functions preserve their independent local ValueId1 and full storage |
| 4 | Owner remains readable after complete source teardown |
| 5 | Optional absent/present-empty GC, frame and logical-map headers |
| 6 | Two owners release independently |
| 7 | Successful replacement releases prior records once |
| 8 | Old borrowed view serves as replacement source |
| 9 | NULL arguments preserve an existing owner and exact diagnostic |
| 10 | Optional NULL diagnostic supports success and refusal |
| 11 | Bounded malformed live counts match direct CloneModule refusal |
| 12 | Every measured owner allocation fault preserves source and old owner |
| 13 | Every measured old-view source fault preserves its live borrow |
| 14 | Eight alternating clone/replace/free operations balance records |

Fixtures construct genuine Core typed storage, not Parser-produced programs.
Two functions have distinct payloads and independently scoped local IDs. Full
fixtures cover module pools, all function flat pools, valid binding rows, nested
GC maps, StateMap pools and frame storage. Comparisons follow actual clone
capacity policy rather than demanding source spare bytes or padding. Saved
numeric witnesses support reads after producer destruction; freed pointers are
not read or compared. Independent read-only review checked live-field capture,
registered teardown, disarming and bounded mutation restoration before assertions,
direct failed-clone cleanup before fallback, and old-view/realloc lifetimes.

## Strict allocator and fault boundary

The dedicated fixed CRT ledger records malloc/calloc/realloc/free only in the
private producer closure. It tracks request kind/count/width/bytes, record IDs,
attempts, live state and release counts without allocating bookkeeping. Fixture
construction uses the same tracked wrappers outside fault intervals. This is
a CRT ledger, not a VM size/flag ledger.

The tests must measure direct CloneModule and owner request sequences, establish
the initial envelope and shifted lower sites, then fault each actual request.
No fixed 60+1 denominator is assumed. Lower failures compare all nine diagnostic
fields with the direct same-source/site witness; envelope OOM has its own1/0
contract. Faults disarm before assertions, cleanup or retry. Candidate releases
are checked before fallback teardown; old/source records remain live. Recovery
must record leaks as errors before freeing them and cannot turn them into PASS.
Realloc bookkeeping saves record/was-new facts before realloc, preserving failed
records and avoiding reads of the old pointer after successful realloc.

## Actual registration scope and gates

The new registration selects 14 existing Core support TUs plus the owner TU as
15 private objects. Its executable has four TUs: test, fixture, allocator and
Unity. Forced allocator includes apply only to that private Core closure.
The focused registration also includes the existing 16-TU core-model executable,
giving35 compile units across the two tests. Root's r5 review verifies current
source/header/settings/artifact pins and seven provider symbols in the actual
maps, including owner, CloneModule/FreeModule and StateMap clone/free providers.

Current r5 configure18708/build39720/support25844/owner6612 exited0/0/0/8.
The existing core-model support passed, including its original28 Function and60
Module allocation fault loops. The new14 cases produced1PASS/13featureFAIL/
0ignored, with build0warnings and no ledger/UBSan diagnostic. Cases12/13 stop at
constructor refusal: their new measured denominator and fault assertions have
not run. The current r5 driver receipt is validatedtrue with a separate Root
current-pin/provider review. Historical r4's driver receipt remains validatedfalse because its parser
expected `Expected TRUE` while Unity emitted the test's custom assertion message.
Root accepted a separate read-only r4 raw/provider review without changing that
receipt. After a registration-only extra EOF blank was removed, r5 freshly
validated the normalized input; the parser accepts the exact custom message
without weakening the tests. Seven finite MSVC units exited0/0warnings; this is
compile-only evidence. Failed earlier gates remain in the acceptance record.

Future GREEN requires all14 PASS, every measured fault accounted,
lower core-model support, strict balance, assertions/UBSan and fresh finite MSVC
scope. Those future results and broader execution/platform coverage are not
claimed here. RED is ready to submit; commit is pending. Receipts are in the
[acceptance record](../acceptance/ssa-execir-owned-module.md).
