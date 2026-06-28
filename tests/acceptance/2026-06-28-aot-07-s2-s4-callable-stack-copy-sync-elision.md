# 2026-06-28 AOT 07-S2/S4 Callable Stack Copy Sync Elision

## Scope

This slice covers the 07/M1.5 register-model cleanup for frame-backed stack copies that only prepare the callable slot for the next call.

It does not claim callable/closure materialization removal, all value-level stack-copy migration, or full typed function-body zero-frame proof.

## Baseline

RED was added to `tests/parser/test_aot_c_logical_shared_library_smoke.c` for the bool short-circuit logical fixture.

The old generated C copied closure/callable values into upcoming call slots and then immediately synchronized the same slots as bool locals:

```c
ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CopyStack(state, &frame, 7, 1));
/* zr_aot_direct_stack_copy_sync_bool_local_boundary */
ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 7, &zr_aot_b7));
```

Those slots are callable inputs at that point. The actual bool result is written by the following typed direct call or synchronized by its fallback/deopt branch.

The focused RED failure was:

```text
test_aot_c_generated_shared_library_executes_bool_short_circuit_logical_expressions:FAIL: Expected NULL
```

## Changes

- `backend_aot_write_c_direct_stack_copy()` now accepts `skipScalarLocalSync`.
- `GET_STACK` / `SET_STACK` lowering passes `skipScalarLocalSync` when the destination slot is the upcoming call callable slot.
- The value `CopyStack` remains in generated C so the call boundary still receives the callable frame value.
- Scalar local synchronization remains enabled for non-callable value copies and for typed direct-call fallback/deopt result synchronization.
- Source contracts now lock the `skipScalarLocalSync` parameter and the callable-copy call path.

## Generated C Evidence

The WSL GCC generated logical fixture now contains:

```c
ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_CopyStack(state, &frame, 7, 1));
```

without a following `zr_aot_direct_stack_copy_sync_bool_local_boundary` in the same copy block.

The typed direct-call fallback still contains:

```c
ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 7, &zr_aot_b7));
```

## Validation

- WSL GCC build: `zr_vm_aot_c_logical_shared_library_smoke_test` and `zr_vm_aot_c_source_contracts_test`.
- WSL GCC run: logical smoke 5 tests / 0 failures; source contracts 22 tests / 0 failures.
- WSL Clang build: same two targets.
- WSL Clang run: logical smoke 5 tests / 0 failures; source contracts 22 tests / 0 failures.
- Windows MSVC Debug build: same two targets.
- Windows MSVC Debug run: logical smoke 0 failures / 5 ignored because the runtime shared-library path is Unix-only; source contracts 22 tests / 0 failures.

## Acceptance Decision

Accepted for the focused 07-S2/S4 callable stack-copy scalar-sync elision slice. Remaining 07 work includes callable/closure materialization, broader value-level stack-copy migration, dynamic/generic/string boundaries, GC roots/exports/frame cleanup, wider byte-frame narrowing, performance counters, and full typed function-body zero-frame proof.
