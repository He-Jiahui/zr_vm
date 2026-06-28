# 2026-06-28 AOT 07-S2/S4 Bool Call Result Branch Local

## Scope

This slice covers the 07/M1.5 register-model cleanup for a bool short-circuit branch whose condition is the direct result of a no-argument typed bool call.

It does not claim all 07-S2/S4 value-level stack-copy migration or full typed function-body zero-frame proof.

## Baseline

RED was added to `tests/parser/test_aot_c_logical_shared_library_smoke.c` for the bool short-circuit logical fixture.

The old generated C failed because slot 14 had been used as a callable stack-copy temporary before becoming the no-arg bool call result. The call result therefore lacked a bool local declaration, and the branch fell back to `frame.slotBase[14].value` instead of `if (!zr_aot_b14)`.

The focused RED failure was:

```text
test_aot_c_generated_shared_library_executes_bool_short_circuit_logical_expressions:FAIL: Expected Non-NULL
```

## Changes

- `backend_aot_c_scalar_locals.c` now records call-result destination slots during scalar-local declaration collection, using SemIR destination kind first and later bool branch/stack-copy consumers as proof when needed.
- `backend_aot_c_lowering_values.c` now emits direct stack-copy local sync only when both source and destination slots have the same scalar-local kind.
- `tests/parser/test_aot_c_logical_shared_library_smoke.c` now requires `TZrBool zr_aot_b14 = ZR_FALSE;`, the no-arg bool direct-call marker, and `if (!zr_aot_b14)`, while rejecting the slot 14 frame condition fallback and stale i64 local/sync forms.
- `tests/parser/test_aot_c_source_contracts.c` locks the call-result destination collector and source/destination-gated stack-copy sync contract.

## Generated C Evidence

The WSL GCC generated logical fixture now contains:

```c
TZrBool zr_aot_b14 = ZR_FALSE;
/* zr_aot_static_bool_no_arg_direct_call */
if (!zr_aot_b14) {
```

It no longer contains `zr_aot_condition = &frame.slotBase[14].value;`, `TZrInt64 zr_aot_s14`, or `ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 14, ...)`.

## Validation

- WSL GCC build: `zr_vm_aot_c_logical_shared_library_smoke_test` and `zr_vm_aot_c_source_contracts_test`.
- WSL GCC run: logical smoke 5 tests / 0 failures; source contracts 22 tests / 0 failures.
- WSL Clang build: same two targets.
- WSL Clang run: logical smoke 5 tests / 0 failures; source contracts 22 tests / 0 failures.
- Windows MSVC Debug build: same two targets.
- Windows MSVC Debug run: source contracts 22 tests / 0 failures; logical smoke 0 failures / 5 ignored because the runtime shared-library path is Unix-only.

## Acceptance Decision

Accepted for the focused 07-S2/S4 bool call-result branch-local slice. Remaining 07 work includes broader value-level stack-copy migration, dynamic/generic/string boundaries, GC roots/exports/frame cleanup, wider byte-frame narrowing, performance counters, and full typed function-body zero-frame proof.
