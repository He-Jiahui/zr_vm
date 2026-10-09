---
related_code:
  - tests/core/test_ssa_function_exec_ir_owner.c
  - tests/core/support/ssa_function_exec_ir_owner_ledger.h
  - tests/core/support/ssa_function_exec_ir_owner_ledger.c
  - tests/core/support/ssa_function_exec_ir_owner_aot_shim.h
  - tests/core/support/ssa_exec_ir_owned_module_fixture.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_function_aot_owner_fixture.c
  - zr_vm_core/src/zr_vm_core/function_exec_ir.c
implementation_files:
  - tests/core/test_ssa_function_exec_ir_owner.c
  - tests/core/support/ssa_function_exec_ir_owner_ledger.c
  - tests/core/support/ssa_function_exec_ir_owner_aot_shim.h
plan_sources:
  - .codex/plans/20261009-ssa-function-execir-owner-design-v1.md
tests:
  - tests/core/test_ssa_function_exec_ir_owner.c
  - tests/core/test_ssa_exec_ir_owned_module.c
  - tests/core/test_ssa_function_aot_owner.c
doc_type: testing-guide
status: functional-red-verified-commit-pending
---

# Finite Function ExecIR owner validation

## Planned twenty default cases

The approved design fixes20 default cases. Five RED source and six test/support
paths are frozen. RED r3 observes4PASS/16featureFAIL/0ignored. Successful
association and Function fault denominators remain uncredited; the rows below
describe the complete requirements.

| Case | Required observation |
| --- | --- |
| 1 | Genuine typed fixture and independent generic-owner prerequisite |
| 2 | Valid new Function has both slots NULL; NULL getter returns NULL |
| 3 | New(NULL) refusal and successful current-sized valid VM creation |
| 4 | Attached ExecIR view survives complete constructed source teardown |
| 5 | Replace seed9 with seed8; old clone records release once |
| 6 | Replace using the old borrowed ExecIR view |
| 7 | NULL Function resets poisoned diagnostic without allocation/mutation |
| 8 | NULL source preserves attached ExecIR and AOT |
| 9 | Optional NULL diagnostic preserves success/refusal behavior |
| 10 | Independently observed lower clone refusal is preserved completely |
| 11 | Every measured first-attachment allocation failure, cleanup and retry |
| 12 | Every measured replacement failure preserves both old owners |
| 13 | ExecIR attachment/replacement leaves existing AOT unchanged |
| 14 | AOT attachment/replacement leaves existing ExecIR unchanged |
| 15 | Full-record move/Detach transfers both owners without donor release |
| 16 | Two successive owned moves release once at final destination |
| 17 | Owned root/child/grandchild frees every independent owner once |
| 18 | Immediate borrowed shell clears both aliases, retains descendants |
| 19 | Proven-transfer tombstone idempotence and separate alias-helper scopes |
| 20 | Explicit Function_Free followed by actual Global shutdown |

The constructed ExecIR profile reuses existing Core fixtures and generic-owner
allocator support. It is not a Parser program or a semantic verifier proof.
AOT uses existing genuine producer/witness support. Generic-owner GREEN
`3496923f` establishes14 cases and two61-site constructed-profile sweeps; those
counts are prerequisites, not an assumed denominator for these Function loops.

## Three independent ledgers

The generic ExecIR fixed CRT ledger tracks its multi-allocation clone closure.
Separate fixed VM and AOT arena ledgers track actual VM pointer/size/type flags
and AOT allocation/releases. Allocator callbacks allocate no bookkeeping and
contain no assertion or I/O. Realloc preserves the old record on failure and
caches lookup/was-new facts before success invalidates the old pointer.
Only specifically selected owner objects are redirected; unrelated VM, Unity
and fixture TUs must not silently share the wrong ledger.

Fault loops must measure real attempts and ordered request geometry, then arm
only around the association call. Disarm before assertions, teardown or retry.
Candidate release is observed before fallback; old ExecIR/AOT/source witnesses
must stay unchanged. Lower diagnostics compare all nine fields; boolean failure
alone does not prove OOM. Numeric record IDs and saved typed contents survive
release, while stale Function/view pointers must not be read after GC shutdown.
Recovery records leaked storage as an error before freeing it.

## Real VM and child ownership

Bootstrap uses genuine Global/State resources. Save the GC stop flag after
initialization, temporarily protect known unrooted arrangements, and restore
before actual Global_Free. No raw-GcMalloc failure is injected; its GC retry and
exception behavior is outside this gate. VM ledger observations establish actual
current Function size and exact typed child-array allocation/free sizes.

Publish child storage/count and register cleanup before Unity assertions can
longjmp. Owned graph fixtures transfer full initialized records and detach
donors; borrowed fixtures clear only immediate alias slots and retain original
descendants. Teardown must free registered owners/producers and actual VM
resources even after feature assertions fail. No fake Function/Global or copied
production harness supplies the lifecycle evidence.

## Build selection and observed attempts

Native r1 stopped before native compilation on a null metadata pin. r2
configure15432 exited0; build8876 exited1 with174 baseline warnings and duplicate
ordinary/private providers. No20-case runtime result exists from either attempt.
The current test hash is
`D677C1CF8A6EA0398AEE4D6E80B74C4D805D40B5D9E7C0F269384784339D0D75`.
Its actual New OBJECT flag expectation and explicit AOT owner header include
were corrected before runtime; this does not supply a PASS result.

Root's registration repair selects private15 ExecIR providers plus one AOT owner
provider. For the dedicated direct Function binary, a derived residual compiled
Core archive excludes exactly those16 ordinary members; only that binary links
the residual archive. The normal route instead compiles the Core source list
excluding those16 providers. The helper verifies exact member inventory and
unchanged input archive. It copies compiled archive bytes, not repository source.
Accepted r3 verifies the repaired archive222→206 members, exactly16 removed,
the255-unit actual compile graph and18 private/residual provider symbols.

Fresh core-model support RED r1 exited0. Finite MSVC RED r2 compiled seven units
with0exits/0warnings: header probe, Function integration, association, test,
ledger, fixture and AOT owner. It is compile-only evidence and does not erase
the native r2 build failure or its warnings. Exact receipts are in the acceptance
record.

## Accepted RED and remaining gates

R3 configure41464/build15780/ordinary11960/residual12860/support37524 exited0;
Function CTest37664 exited8. All16 failures use the exact real attachment-success
assertion message. Cases1/2/3/7 pass; no ledger/UBSan diagnostic appears.
Actual sizeof(Function)=1208, AOT offset1192 and ExecIR offset1200, including
the real GC OBJECT allocation-tag assertion. Existing AOT12 and generic14
support pass; generic's two61-site sweeps remain lower support rather than
Function fault coverage. R3 is incremental relink/filter with no new diagnostics;
it does not erase r2's174 warnings or prove a clean full rebuild.

Root's r3 gate verifies actual registration, current Function layout and selected
providers against the raw output and input pins. The earlier MSVC7 and CoreModel
receipts remain valid separate support. RED is verified with commit pending.
No unobserved loop denominator is credited. Future GREEN requires all20 plus generic/CoreModel/AOT Function
regressions and current provenance; warnings and failed attempts stay visible.

These gates inspect metadata and ownership without source/native execution or
new provider/security callbacks. Function/source/default/full47 migration,
Linux/full MSVC and backend/native parity remain OPEN. Actual receipts will be
recorded in the [acceptance record](../acceptance/ssa-function-exec-ir-owner.md).
