# AOT M1.5 07-S5 generic numeric local sync boundary helper

## Status

- Completed: 2026-06-22 03:40:06 +08:00
- Slice: M1.5 / 07-S5 generic numeric i64/u64/f64 scalar-local sync boundary helper
- State: sub-slice complete; 07-S5 and M1.5/07 remain partially complete; 08-12 not started.

## Scope

- Generic `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, and `NEG` emitters now receive the current `SZrAotExecIrFunction`.
- The generic numeric boundary writer still calls `ZrLibrary_AotRuntime_GenericNumeric*()` for the VM-frame result.
- After the helper returns, generated C conditionally synchronizes matching i64, u64, or f64 scalar locals through `ZrLibrary_AotRuntime_SyncSignedIntLocal()`, `ZrLibrary_AotRuntime_SyncUnsignedIntLocal()`, or `ZrLibrary_AotRuntime_SyncFloatLocal()`.
- Generated C emits `zr_aot_generic_numeric_sync_i64_local_boundary`, `zr_aot_generic_numeric_sync_u64_local_boundary`, and `zr_aot_generic_numeric_sync_f64_local_boundary` markers only when the destination slot has matching scalar-local coverage.

## Evidence

- RED: generic numeric contracts failed before implementation because generic numeric emitters did not receive `functionIr` and had no i64/u64/f64 sync markers or helper calls.
- Follow-up contract repair: the first broad pass exposed a stale source-contract expectation for concrete `GenericNumeric*` helper-call strings after the lowering was collapsed through a shared `%s` helper.
- Fix: source contracts now lock the shared helperized call shape, function-body `functionIr` routing, and all three local-sync helper boundaries.
- GREEN: focused generic numeric contracts 1/0, generic numeric shared-library smoke 1/0, and source contracts 19/0.
- Broad focused group passed: call contracts 4/0, call shared-library smoke 5/0, shared-library smoke 8/0, value-type shared-library smoke 1/0, power contracts 2/0, power smoke 1/0, source contracts 19/0, generic numeric contracts/smoke 1/0 + 1/0, global smoke 9/0, logical contracts/smoke 4/0 + 4/0, typed scalar 1/0, return contracts 1/0, frame setup contracts 1/0.

## Generated C Checks

- Source contracts lock i64/u64/f64 generic numeric sync markers and helper calls.
- The existing float `MOD` smoke still compiles generated C into a shared library and verifies the generic numeric helper boundary remains linkable.
- This slice does not claim executable i64/u64/f64 generic numeric local-sync coverage; the current smoke path does not infer scalar locals for hand-built generic numeric bytecode.

## Follow-up

- Add executable local-sync coverage when a source-level or SemIR fixture can prove generic numeric helper destinations as i64, u64, or f64 scalar locals without changing dynamic numeric semantics.
