---
related_code:
  - tests/parser/support/ssa_source_returned_execir_cleanup.h
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
  - tests/parser/test_ssa_source_returned_execir_cleanup.c
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/cmake/ssa-source-returned-execir-cleanup.cmake
  - tests/cmake/ssa-source-returned-execir-cleanup-direct-validation/CMakeLists.txt
  - tests/cmake/ssa-source-returned-execir-cleanup-check.cmake
implementation_files:
  - tests/parser/support/ssa_source_returned_execir_cleanup.h
  - tests/parser/support/ssa_source_returned_execir_cleanup.c
plan_sources:
  - .codex/plans/20261010-ssa-returned-execir-cleanup.md
  - 'Root: 2026-10-10 finite cleanup helper gate; Source integration separately authorized'
tests:
  - tests/parser/test_ssa_source_returned_execir_cleanup.c
  - docs/parser-and-semantics/source-returned-execir-cleanup.md
  - docs/acceptance/source-returned-execir-cleanup.md
doc_type: testing-guide
status: finite-support-accepted
---

# Returned ExecIR cleanup validation

## Current evidence

Root accepted the finite support gate on 2026-10-10. Historical RED r2 produced
six exact expected failures and child exit6. Explicit GREEN r1 produced four
passes/two deliberate failures and child exit2. Independent fresh GREEN r2
then validated the actual module default without a configure phase override.
Each CTest wrapper exited0 after validating its exact raw protocol. MSVC r2
compiled the same five units with zero warnings only; it did not link or run.
The [acceptance record](../acceptance/source-returned-execir-cleanup.md)
identifies the actual receipts. This documentation update invokes no native
tool, CMake, child or Git command.

## Exact six-case order

1. `test_healthy_registered_ledger_close`
2. `test_prior_unity_failure_still_finishes_cleanup`
3. `test_prior_failure_saved_witnesses_are_complete`
4. `test_residual_cleanup_cannot_become_healthy`
5. `test_residual_saved_health_and_failure_remain_visible`
6. `test_repeated_close_preserves_original_failure_report`

| Case | Actual actions verified | Historical RED r2 | Accepted GREEN r1/r2 |
|---|---|---|---|
| 1 | Register/free one real allocation per ledger; worker and independent drain checks precede assertions | MISSING_COMPLETION failure | PASS; healthy pre/post, all record frees1 |
| 2 | Acquire registered resources then actual UnityFail; teardown cleans while prior failure remains set | PRIOR_FAILURE failure | Same deliberate failure; cleanup complete and prior flag preserved |
| 3 | Inspect case2 report after actual Conclude, before reset/new lifecycle | MISSING_COMPLETION failure | PASS; completed, healthy and supporting witnesses exact |
| 4 | Transfer live registrations to actual recovery; save live1/frees0 pre-state and errors1/live0/frees1 post-state per ledger | MISSING_COMPLETION failure | Deliberate PREHEALTH failure after completion |
| 5 | Inspect case4's unchanged report, including failing pre-health and retained errors | MISSING_COMPLETION failure | PASS |
| 6 | Repeat case4's close within its lifecycle; preserve every saved field and avoid ledger mutation | MISSING_COMPLETION failure | PASS |

Full markers are `RETURNED_EXEC_CLEANUP_MISSING_COMPLETION`, `RETURNED_EXEC_CLEANUP_PRIOR_FAILURE` and `RETURNED_EXEC_CLEANUP_PREHEALTH`. RED requires six tests/six failures/zero ignored and the actual Unity exit6. GREEN requires six tests/four passes/two deliberate failures/zero ignored and actual Unity exit2. These deliberate failures are cleanup regression inputs, not Source feature failures. Actual earlier-ignore handling remains OPEN.

The checker must refuse an extra failure, wrong reason, changed order, missing witness, unexpected diagnostic, crash or timeout. It must not use CTest `WILL_FAIL` or accept a nonzero exit alone. The runner preserves its actual `UNITY_END` result; the wrapper accepts only the exact reviewed protocol.

Each actual Unity result is followed, after Conclude, by one
`RETURNED_EXEC_CLEANUP_CASE` line with 15 numeric fields. The accepted raw
streams have case numbers1–6 and lifecycles1/2/2/3/3/3; prior_failed1 only for
cases2–3; prior_ignored0 and flags_preserved/closed/support1 throughout;
queries6 and record_frees3 throughout. Supporting before_live is0 for cases1–3
and3 for cases4–6; after_live0, before_errors0, and after_errors0/3 respectively.
Historical RED reports completed/healthy0/0; GREEN completed1 throughout with
healthy1 for cases1–3 and0 for cases4–6. Supporting counts are independent
actual ledger reads. In GREEN, assertions compare all saved report counters,
query results, counts, storage identities and three record witnesses with them.

The final line must be exactly `RETURNED_EXEC_CLEANUP_FINAL drain=1 live=0 record_frees=3 errors=3 flags_preserved=1`. Sole trimmed stderr must be `EXEC_OWNER_LEDGER errors=1 live=1`, from the actual residual recovery. Output streams are retained separately; this does not claim cross-stream event ordering. The matching saved case4 pre/post witness supplies the diagnostic's finite context.

## Resource and report discipline

Every actual allocation is registered immediately. Resource release, recovery
and supporting observation finish before teardown assertions; final drain has
no Unity assertions. After cleanup, teardown returns if failed/ignored is
already set; otherwise it checks support/completion and pre-health for cases1–4.
Ordinary cleanup clears registrations and frees; the residual case explicitly
transfers registrations before actual recovery. Independent pure stats/free
reads established supporting cleanup even while the historical RED report
refused completion.

The caller supplies one witness slot for each of the two CRT ledgers and generic owner ledger. Begin requires independently verified prior witnesses, live zero and valid capacities. A new lifecycle may invalidate the previous report only at that boundary. Repeated close in one lifecycle preserves its first failing report. No successful completion assertion was required to reach historical RED's final drain. The finite run exercises valid admission and one-slot storage; capacity overflow, invalid/refused queries, invalid storage and missing-report/begin branches are statically reviewed, not runtime-tested here.

Post live zero is cleanup evidence only. Saved pre-health, exact error increments and every query/free witness remain visible. Resets occur only after prior reports have been checked and live storage is zero. Raw generic allocator pointers provide no opaque-owner, Function or VM identity evidence.

## Exact finite closure and route

Target `zr_vm_ssa_source_returned_execir_cleanup_test` selects exactly five C units:

- New six-case main.
- New shared cleanup worker.
- Actual Returned CRT ledger.
- Actual generic ExecIR owner allocator.
- Actual Unity C.

There is no Source/lifecycle fixture, Core/Parser/VM archive, Function provider, forced include, fabricated allocator/provider, source snapshot or generated oracle. Actual Core/common headers supply typed widths. The existing `_CRT_SECURE_NO_WARNINGS` target policy matches repository Windows CRT settings; `/W4` and raw warning review remain required.

The direct root is `tests/cmake/ssa-source-returned-execir-cleanup-direct-validation`. It confines configuration to actual `E:/Git/zr_vm` and a separate child under `E:/cargo-targets/zr_vm/build`; requires Windows/MSVC frontend/Ninja/Debug, C11 without extensions, dynamic MSVC CRT and assertions. UBSan defaults on with Clang and no recovery; clang-cl link-driver sanitizer flags precede `/link`. Actual executable ABI probes remain enabled.

CTest `ssa_source_returned_execir_cleanup` invokes the explicit checker, with
timeout180 and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
`ZR_SSA_RETURNED_EXEC_CLEANUP_EXPECTED_PHASE` now defaults to GREEN after Root's
accepted historical RED and authorized implementation. GREEN r1 used an
explicit configure override and supplies no default credit. GREEN r2 used a
different fresh build child, omitted `-DZR_SSA_RETURNED_EXEC_CLEANUP_EXPECTED_PHASE`
from configure, and emitted actual settings with expected_phase GREEN. The
checker invocation still receives the configured phase explicitly. Selecting
RED now changes the expectation only; it does not recreate historical RED code.

The checker bounds its child execution to120 seconds and saves `returned-execir-cleanup-<phase>-stdout.log` and `-stderr.log` in the selected E build child before interpreting results. Existing logs are refused to preserve the prior attempt. The wrapper emits the child's actual exit and the exact accepted-protocol marker only after every raw case/witness/summary/footer check succeeds. Root must retain these raw streams and the CTest/controller terminals; wrapper exit0 is not the raw Unity exit.

Configuration emits `compile_commands.json`, `returned-execir-cleanup-target-sources.tsv` and `returned-execir-cleanup-settings.tsv`. An optional `ZR_SSA_RETURNED_EXEC_CLEANUP_MAP_NAME` must be a basename ending in `.map`. Root reviews actual source/object edges, latest header dependencies, symbols/imports and map, tool/input/artifact pins and child natural terminals before execution/acceptance.

## Accepted runtime/compiler evidence

Default GREEN r2 used a fresh cache/map route with an actual executable compiler
ABI probe, five compilation steps and one link. Configure, build, latest
dependencies, defined symbols, undefined symbols and CTest each exited
naturally0. Raw warnings and UBSan diagnostics were zero. Latest read-header
records and primary source edges cover all five objects; map/nm prove ten sole
selected worker/ledger/Unity providers and actual `/MD` CRT imports. No forced
include, project archive or Source/VM provider was selected.

MSVC r2 used actual C11, `/experimental:c11atomics`, `/MD`, `/W4`, `/UNDEBUG`
and no forced include. Five compiler children exited naturally0 with zero
warnings and basename-only raw outputs. Actual read-header counts were
88/26/79/31/19. SourceDependencies JSON Version1.2 identifies each actual
Source/Includes, a present-empty ProvidedModule, and absent ImportedModules
and ImportedHeaderUnits. This is compile-only evidence with no ABI byte claim.

Before this documentation update, Root and this author verified the combined
441-entry runtime/MSVC evidence list with zero drift: runtime217 selected
inputs, eight metadata pins, seven artifacts, two raw child streams, 18 phase
stdout/stderr/combined logs, plus MSVC169 selected inputs and20 artifacts.
The three guides occur in both selected-input lists. Their original hashes in
the 18-file freezes are historical review/protocol metadata after this update;
the other code, headers, tools, metadata, artifacts and logs retain strict
current pins. Updated guide hashes are frozen separately for Root's transaction.
No runtime rerun or current all441-zero-drift claim follows from editing them.

## OPEN work

Support TryArm/TryEvent/TryCapture and real coordinator wiring are separate work. The unchanged twenty Source cases and genuine post-Attach/Verify finalizer require their own reviewed execution. This ledger-only gate supplies no real VM/Global shutdown, Function/GC ownership, complete OOM/leak, earlier-ignore, Linux, full MSVC runtime, default/full-project or all47 credit. Shared registration and Git transactions are Root-owned.

See the [module contract](../parser-and-semantics/source-returned-execir-cleanup.md) and [finite acceptance record](../acceptance/source-returned-execir-cleanup.md).
