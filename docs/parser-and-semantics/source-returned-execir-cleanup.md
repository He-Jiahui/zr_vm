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
  - docs/testing-and-validation/source-returned-execir-cleanup.md
  - docs/acceptance/source-returned-execir-cleanup.md
doc_type: module-detail
status: finite-support-accepted
---

# Returned ExecIR ledger cleanup

Root accepted this finite support gate on 2026-10-10: historical refusing-report
RED, explicit GREEN, an independently fresh default-GREEN runtime with UBSan,
and five-unit MSVC compilation. GREEN retains two deliberate Unity failures as
cleanup regression inputs; the exact wrapper protocol passes. See the
[finite acceptance record](../acceptance/source-returned-execir-cleanup.md).

## Defect and finite responsibility

Unity's assertion functions abort when `CurrentTestFailed` or `CurrentTestIgnored` is already set. An otherwise-correct assertion in teardown can therefore stop cleanup. The existing ReturnedExec fixture also uses assertion-bearing observations inside callbacks. Fixing callback and genuine Source lifecycle integration is separately leased work; this finite child tests the shared ledger cleanup worker first.

The worker owns recovery of the two accepted Returned CRT inventories and the generic ExecIR owner allocator inventory. It owns no Global, Function, GC root or opaque module. Its five-unit executable links the new main, worker, actual CRT ledger, actual generic owner allocator and actual Unity only. Header types are actual repository types. No Source compiler, Core/Parser archive, VM bootstrap, Function provider, network or FFI is selected.

## Caller-owned witness storage

The report records begun/closed state, completion and pre-recovery health, before/after named stats, query errors and per-record free witnesses. The caller tracks the lifecycle and supplies `SZrReturnedExecCleanupStorage` spans and capacities for both CRT kinds and the owner inventory. Storage is genuine writable independent memory whose lifetime includes the saved report checks. The finite tests provide one witness slot per ledger because each scenario acquires one real allocation per ledger; this supplies no arbitrary-profile inventory coverage.

`ZrTests_ReturnedExec_CleanupBegin(report, storage, priorWitnessesVerified)` requires an explicit independently checked prior-witness result of one, zero actual live allocations, valid storage and record counts fitting the supplied capacities. Refusal leaves the existing lifecycle/report unchanged. Beginning a new lifecycle invalidates the previous completed report only after its witnesses and drain have been checked. A repeated close within one lifecycle retains the first report, including an unhealthy one.

The implemented report reads actual named counters and record frees through
nonasserting observations. It saves public query results separately from the
internal stats copy, checks storage capacity and matching pre/post record
counts, and preserves unhealthy pre-recovery observations. Both actual
recovery functions precede completion and report assertions. Runtime verified
the one-slot, valid-query scenarios; invalid capacity, missing/refused queries,
missing report/begin and other guard branches have static-review credit only.
The historical RED worker recovered storage and set its once marker while
refusing completed/healthy report credit. Its independent drain evidence is
retained separately from the current implemented feature.

## Actual storage and lifetime

Descriptor requests use the real typed site1 shape: three `SZrAotIrInstruction` elements allocated through the actual descriptor callback. Returned owner and generic owner requests acquire small real CRT allocations and register each returned pointer immediately. Generic owner allocator storage is raw tracked storage; it is never cast to an opaque owner or VM object.

Ordinary tests clear registrations and free before the worker. The residual test explicitly transfers its registered pointers to the actual recovery inventory before recovery releases them. The runner saves independent before/after stats and pure record-free query statuses. Historical RED verified the real supporting drain despite report refusal; current GREEN additionally compares every saved report field and record witness with those independent observations.

CRT recovery adds one error for each kind with live storage. Generic owner recovery adds one error when it has residual live storage. Both retain the error and free history. After recovery, live zero proves cleanup, while saved failing pre-health and retained errors prevent healthy-shutdown credit. No test resets live storage or clears health to turn residual cleanup into success.

## Unity failure boundary

The runner uses actual top-level `RUN_TEST` calls and `UnityFail`. Resource
release, recovery and supporting observations finish before teardown assertions.
Teardown returns after cleanup when the prior failed/ignored flag is set;
otherwise it asserts support and completion, with the pre-health assertion
limited to cases1–4. The deliberate earlier failure remains set throughout
cleanup. Actual `UnityConcludeTest` counts and clears it before the next
top-level witness test. There is no nested AbortFrame, manual flag clearing or
substituted exit status.

In the accepted GREEN run, the residual scenario reports completion before its
saved pre-health assertion deliberately fails. Later cases inspect and repeat
that same report; an empty second close cannot replace the original failure.
An assertion-free final drain runs before process return and preserves the
earlier results. The module now defaults to GREEN; a separate fresh runtime
verified that default without a phase override. Historical explicit RED is an
old source proof, not the current default or an execution of current GREEN
code under a RED expectation.

Actual earlier-ignore cleanup is OPEN: this protocol has zero ignored tests and does not call `UnityIgnore`. Preserving the numerical ignored flag at zero is not evidence for teardown with a prior ignore.

## Later integration and OPEN boundaries

The genuine ReturnedExec coordinator must later release its known typed direct-owner slot, real registered Functions/names and actual Global through the existing APIs before invoking this shared worker. Pure TryArm/TryEvent/TryCapture callback changes require separate primitive and integration evidence. Numeric live-span containment cannot establish readable typed roots or VM authenticity.

The existing twenty Source cases, fault/retry Global lifetime, real finalizer publication after Attach/Verify, AOT consumer regression, Function/GC ownership and actual Source/VM shutdown remain separate gates. This child supplies no leak instrumentation, complete OOM, default/full-project, Linux, full MSVC runtime, platform parity or all47 acceptance.

See the [validation protocol](../testing-and-validation/source-returned-execir-cleanup.md) and [finite acceptance record](../acceptance/source-returned-execir-cleanup.md).
