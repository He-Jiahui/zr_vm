# AOT M1.5 07-S5 static i64 two-arg less bool typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice adds a narrow mixed-signature typed-to-typed direct-call route for static functions with two `int`/i64 parameters returning `left < right` as bool.

## Baseline

- Typed direct-call routes already covered same-kind scalar signatures such as `i64 -> i64`, `i64,i64 -> i64`, and `bool,bool -> bool`.
- The `int,int -> bool` callee still generated a frame-backed `LOGICAL_LESS_SIGNED` body with `SZrTypeValue` reads/writes.
- The caller therefore fell back to `ZrLibrary_AotRuntime_CallStaticDirect` plus bool local sync instead of passing i64 locals directly and receiving a bool local.

## RED

- The new bool shared-library smoke case failed as expected with `Expected Non-NULL`.
- The generated C for `smaller(left: int, right: int): bool` had no `TZrInt64`-parameter bool typed thunk declaration/definition.
- The call site still contained `ZrLibrary_AotRuntime_CallStaticDirect(state, ...)` and `ZrLibrary_AotRuntime_SyncBoolLocal(...)`.

## Implementation

- `backend_aot_c_typed_bool_thunks.c` now recognizes bool-return, two-i64-parameter callees whose body is `LOGICAL_LESS_SIGNED` followed by `FUNCTION_RETURN`.
- The generated thunk signature is `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1)` and returns `return (TZrBool)(zr_aot_arg0 < zr_aot_arg1);`.
- `backend_aot_c_lowering_calls.c` adds an explicit `zr_aot_static_i64_bool_two_arg_direct_call` writer that assigns the bool local from `zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)`.
- `backend_aot_c_typed_direct_calls.c` proves the caller shape with a bool destination local, two written i64 argument locals, and the new callee recognizer before selecting the direct typed route.
- The bool shared-library smoke now executes `smaller(7, 9)`, returns `42`, and rejects `CallStaticDirect`, `CallStackValue`, and typed-destination stack sync for the scalar-only route.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused RED/GREEN command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Focused GREEN result:
  - typed call contracts 4/0
  - bool typed direct-call smoke 8/0
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
  - bool typed direct-call smoke 8/0
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

- `backend_aot_c_typed_bool_thunks.c`: 506 physical / 440 non-empty lines.
- `backend_aot_c_typed_direct_calls.c`: 726 physical / 649 non-empty lines.
- `backend_aot_c_lowering_calls.c`: 691 physical / 655 non-empty lines.
- `test_aot_c_typed_direct_call_bool_shared_library_smoke.c`: 226 physical / 209 non-empty lines.

## Acceptance Decision

Accepted for the narrow mixed i64-parameter bool-return typed direct-call slice. This does not close 07-S5: inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader ABI coverage, dynamic value access helpers, runtime-failure-capable division/modulo policy, and stages 08-12 remain open.
