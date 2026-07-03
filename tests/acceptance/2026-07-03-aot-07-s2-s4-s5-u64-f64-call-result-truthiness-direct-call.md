# AOT 07-S2/S4/S5 U64/F64 Call-Result Truthiness Direct Call

## Scope

This acceptance record covers one narrow M1.5 / AOT 07 slice: no-argument typed `uint` and `float` call results can stay in scalar locals when consumed by generic truthiness.

Affected layers:

- AOT scalar-local declaration and per-exec written-before proof in `backend_aot_c_scalar_locals.c`.
- F64 no-argument thunk shape recognition in `backend_aot_c_typed_f64_thunk_shapes.c`.
- Source-level logical shared-library smoke coverage in `tests/parser/test_aot_c_logical_shared_library_smoke.c`.

Accepted generated shapes:

- `if (unsignedZero())` calls the typed u64 no-arg thunk into `zr_aot_u17` and branches with `zr_aot_generic_jump_if_u64_scalar_local`.
- `var unsignedInverted = !unsignedZero();` writes a bool local from `zr_aot_u* == 0`.
- `if (floatZero())` calls the typed f64 no-arg thunk into `zr_aot_f21` and branches with `zr_aot_generic_jump_if_f64_scalar_local`.
- `var floatInverted = !floatZero();` writes a bool local from `zr_aot_f* == 0.0`.
- Generated C avoids typed-destination `SZrTypeValue` materialization and avoids static u64/f64 no-arg direct-call stack-slot sync markers for the covered call-result truthiness paths.

## Baseline

Before this slice, call-result destination declarations could discover the typed callee kind, but the per-exec written-before query could still recover a stale kind from earlier occupants of the same numeric slot. That allowed generated truthiness to read a u64 direct-call result through an i64 local. The f64 fixture also exposed that constant-return functions with a reset-tail instruction did not match the f64 no-arg thunk shape, so `floatZero()` and `floatOne()` fell back to the generic static direct-call path.

Initial RED:

```text
zr_vm_aot_c_logical_shared_library_smoke_test
test_aot_c_generated_shared_library_executes_generic_truthiness_boundary_helpers:FAIL: Expected Non-NULL
```

The missing marker was:

```text
/* zr_aot_static_f64_no_arg_direct_call */
```

Generated C inspection also showed `unsignedZero()` branching through stale `zr_aot_s17`, and the f64 truthiness path falling through to U64 sync/read instead of `zr_aot_f*`.

## Test Inventory

- `tests/parser/test_aot_c_logical_shared_library_smoke.c`
  - Adds `unsignedZero/unsignedOne/floatZero/floatOne`.
  - Covers `!call()`, `if (call())`, and saved inverted bool locals.
  - Requires u64/f64 no-arg direct-call markers and generic u64/f64 truthiness scalar-local markers.
  - Forbids typed-destination `SZrTypeValue` materialization and static u64/f64 no-arg direct-call stack-slot sync markers.
- Existing adjacent tests:
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`
  - `zr_vm_aot_c_source_contracts_test`
  - `zr_vm_aot_c_guardrail_contracts_test`
  - `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`
  - `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`

## Results

- Focused WSL GCC logical shared-library smoke: 6 tests, 0 failures.
- WSL GCC:
  - logical shared-library smoke 6/0
  - generic LOGICAL_NOT numeric local smoke 1/0
  - generic JUMP_IF bool/numeric local smoke 3/0
  - source contracts 24/0
  - guardrail contracts 6/0
  - typed direct-call f64 shared-library smoke 19/0
  - typed direct-call u64 shared-library smoke 25/0
- WSL Clang: same listed targets and counts passed.
- Windows MSVC Debug:
  - source contracts 24/0
  - guardrail contracts 6/0
  - the same shared-library/direct-call targets built and reported expected Unix-only ignored cases with 0 failures.

## Acceptance Decision

Accepted as a completed 07-S2/S4/S5 sub-slice.

This does not complete 07-S2, 07-S4, 07-S5, M1.5, or the 07~12 goal. Remaining work includes dynamic/string/object truthiness, value-copy migration, GC roots/exports/frame cleanup, wider byte-frame narrowing, performance counters, and full typed function-body zero-frame proof.
