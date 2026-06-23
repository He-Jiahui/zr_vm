# AOT M1.5 07-S5 static i64 two-arg equal bool typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice broadens the mixed `bool(i64, i64)` typed-to-typed direct-call route from signed less-than to signed equality.

## Baseline

- The previous mixed bool-return route recognized only `LOGICAL_LESS_SIGNED`.
- The caller-side route already proved a bool destination local and two written i64 argument locals.
- A source function `same(left: int, right: int): bool { return left == right; }` still generated a frame-backed callee body and fell back to `CallStaticDirect` plus `SyncBoolLocal`.

## RED

- The bool shared-library smoke failed as expected with `Expected Non-NULL`.
- The generated C for `same(21, 21)` had no `TZrInt64`-parameter bool thunk declaration/definition.
- The call site still used `ZrLibrary_AotRuntime_CallStaticDirect(state, ...)` and `ZrLibrary_AotRuntime_SyncBoolLocal(...)`.

## Implementation

- `backend_aot_c_typed_bool_thunks.c` now uses a shared i64 compare recognizer for bool-return, two-i64-parameter callees.
- The new equality wrapper accepts `LOGICAL_EQUAL_SIGNED` followed by `FUNCTION_RETURN`.
- The mixed `bool(i64, i64)` can-emit gate accepts signed less-than and signed equality.
- The equality thunk emits `return (TZrBool)(zr_aot_arg0 == zr_aot_arg1);`.
- The existing `zr_aot_static_i64_bool_two_arg_direct_call` writer and route proof are reused, so eligible call sites still assign `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)` with scalar-only destination sync elision.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused RED/GREEN command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Focused GREEN result:
  - typed call contracts 4/0
  - bool typed direct-call smoke 9/0
- Broader focused WSL GCC AOT validation:
  - source contracts 19/0
  - call contracts 4/0
  - typed call contracts 4/0
  - generic numeric contracts 1/0
  - global contracts 7/0
  - logical contracts 4/0
  - power contracts 2/0
  - frame setup contracts 1/0
  - return contracts 1/0
  - value SemIR contracts 4/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call i64 smoke 5/0
  - bool typed direct-call smoke 9/0
  - u64 typed direct-call smoke 14/0
  - f64 typed direct-call smoke 11/0
  - arithmetic typed direct-call smoke 5/0
  - bitwise typed direct-call smoke 6/0
  - typed scalar 1/0
  - value-type smoke 1/0
  - generic numeric smoke 1/0
  - global smoke 9/0
  - logical smoke 4/0
  - power smoke 1/0

## File Scale

- `backend_aot_c_typed_bool_thunks.c`: 529 physical / 460 non-empty lines.
- `backend_aot_c_typed_direct_calls.c`: 726 physical / 649 non-empty lines.
- `test_aot_c_typed_call_contracts.c`: 717 physical / 679 non-empty lines.
- `test_aot_c_typed_direct_call_bool_shared_library_smoke.c`: 254 physical / 235 non-empty lines.

## Acceptance Decision

Accepted for the narrow mixed i64-parameter bool-return equality typed direct-call slice. This does not close 07-S5: additional signed comparisons, inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader ABI coverage, dynamic value access helpers, runtime-failure-capable division/modulo policy, and stages 08-12 remain open.
