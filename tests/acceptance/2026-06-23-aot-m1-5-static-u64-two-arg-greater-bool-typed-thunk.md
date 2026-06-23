# AOT M1.5 07-S5 static u64 two-arg greater bool typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice broadens the mixed `bool(u64, u64)` typed-to-typed direct-call route from unsigned inequality to unsigned greater-than.

## Baseline

- The previous u64-bool slices introduced the `TZrUInt64`-parameter bool thunk signature and caller-side `zr_aot_u*` argument route.
- The route already proved a bool destination local and two written u64 argument locals.
- A source function `greater(left: uint, right: uint): bool { return left > right; }` still needed a `LOGICAL_GREATER_UNSIGNED` recognizer and a `TZrUInt64`-parameter greater-than thunk writer.

## RED

- The typed call contract failed as expected because `backend_aot_c_try_get_bool_u64_arg0_arg1_greater_return(` was absent.
- The new bool shared-library smoke was added to prove generated C for `greater(50u, 8u)` needs the u64 bool thunk and must stay off the VM call/value fallback path.

## Implementation

- `backend_aot_c_typed_bool_thunks.c` reuses the shared u64 bool-return compare helper.
- The greater-than wrapper accepts `LOGICAL_GREATER_UNSIGNED` followed by `FUNCTION_RETURN`.
- `backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk()` now accepts `<`, `==`, `!=`, and `>` u64-parameter bool-return shapes.
- The greater-than thunk emits `return (TZrBool)(zr_aot_arg0 > zr_aot_arg1);`.
- The existing `zr_aot_static_u64_bool_two_arg_direct_call` writer and route proof are reused, so eligible call sites still assign `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_uA, zr_aot_uB)` with scalar-only destination sync elision.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused RED/GREEN command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Focused RED result:
  - typed call contracts 3/1, missing `backend_aot_c_try_get_bool_u64_arg0_arg1_greater_return(`
- Focused GREEN result:
  - typed call contracts 4/0
  - bool typed direct-call smoke 17/0
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
  - bool typed direct-call smoke 17/0
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

- `backend_aot_c_typed_bool_thunks.c`: 720 physical / 628 non-empty lines.
- `backend_aot_c_typed_direct_calls.c`: 782 physical / 700 non-empty lines.
- `backend_aot_c_lowering_calls.c`: 730 physical / 692 non-empty lines.
- `backend_aot_c_emitter.h`: 750 physical / 745 non-empty lines.
- `test_aot_c_typed_call_contracts.c`: 753 physical / 715 non-empty lines.
- `test_aot_c_typed_direct_call_bool_shared_library_smoke.c`: 478 physical / 443 non-empty lines.

## Acceptance Decision

Accepted for the narrow mixed u64-parameter bool-return greater-than typed direct-call slice. This does not close 07-S5: remaining unsigned comparisons, inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader ABI coverage, dynamic value access helpers, runtime-failure-capable division/modulo policy, and stages 08-12 remain open.
