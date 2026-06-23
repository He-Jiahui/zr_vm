# AOT M1.5 07-S5 static i64 two-arg less-equal bool typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice broadens the mixed `bool(i64, i64)` typed-to-typed direct-call route from signed less-than/equality/inequality/greater-than to signed less-than-or-equal.

## Baseline

- The mixed bool-return route already handled `LOGICAL_LESS_SIGNED`, `LOGICAL_EQUAL_SIGNED`, `LOGICAL_NOT_EQUAL_SIGNED`, and `LOGICAL_GREATER_SIGNED`.
- The caller-side route already proved a bool destination local and two written i64 argument locals.
- A source function `at_most(left: int, right: int): bool { return left <= right; }` still generated a frame-backed callee body and fell back to `CallStaticDirect` plus `SyncBoolLocal`.

## RED

- The typed call contract failed as expected because `backend_aot_c_try_get_bool_i64_arg0_arg1_less_equal_return(` was absent.
- The new bool shared-library smoke was added to prove the generated C for `at_most(8, 50)` needs a `TZrInt64`-parameter bool thunk and direct typed call.

## Implementation

- `backend_aot_c_typed_bool_thunks.c` reuses the shared i64 bool-return compare recognizer.
- The new less-equal wrapper accepts `LOGICAL_LESS_EQUAL_SIGNED` followed by `FUNCTION_RETURN`.
- The mixed `bool(i64, i64)` can-emit gate now accepts signed less-than, equality, inequality, greater-than, and less-than-or-equal.
- The less-equal thunk emits `return (TZrBool)(zr_aot_arg0 <= zr_aot_arg1);`.
- The existing `zr_aot_static_i64_bool_two_arg_direct_call` writer and route proof are reused, so eligible call sites still assign `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_sA, zr_aot_sB)` with scalar-only destination sync elision.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused RED/GREEN command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Focused RED result:
  - typed call contracts 3/1, missing `backend_aot_c_try_get_bool_i64_arg0_arg1_less_equal_return(`
- Focused GREEN result:
  - typed call contracts 4/0
  - bool typed direct-call smoke 12/0
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
  - bool typed direct-call smoke 12/0
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

- `backend_aot_c_typed_bool_thunks.c`: 580 physical / 505 non-empty lines.
- `test_aot_c_typed_call_contracts.c`: 723 physical / 685 non-empty lines.
- `test_aot_c_typed_direct_call_bool_shared_library_smoke.c`: 338 physical / 313 non-empty lines.

## Acceptance Decision

Accepted for the narrow mixed i64-parameter bool-return less-than-or-equal typed direct-call slice. This does not close 07-S5: remaining signed greater-than-or-equal comparison, inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader ABI coverage, dynamic value access helpers, runtime-failure-capable division/modulo policy, and stages 08-12 remain open.
