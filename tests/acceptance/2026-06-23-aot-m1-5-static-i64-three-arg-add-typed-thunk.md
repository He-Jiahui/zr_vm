# AOT M1.5 07-S5 static i64 three-arg add typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice broadens the static i64 typed-to-typed direct-call ABI from no/one/two-argument thunks to a narrow three-argument add-return route.

## Baseline

- Existing i64 typed thunks covered no-arg constants, one-arg identity/constant/unary arithmetic, and two-arg arithmetic/bitwise returns.
- Static typed direct-call routing proved scalar-local arguments only up to two i64 parameters.
- The existing i64 shared-library smoke file is near the large-file threshold, so this slice adds a separate i64 three-arg smoke target and support header instead of growing it.

## RED

- The typed call contract failed as expected because `backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function)` was absent.
- The new i64 three-arg shared-library smoke failed with `Expected Non-NULL` before generated C contained a three-argument thunk declaration, definition, return expression, or direct-call marker.

## Implementation

- `backend_aot_c_typed_i64_thunks.c` now recognizes three-int-parameter callees whose body is two signed add instructions followed by `FUNCTION_RETURN` of `arg0 + arg1 + arg2`.
- The i64 thunk emitter now declares and defines `static TZrInt64 zr_aot_typed_i64_fn_N(struct SZrState *state, TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1, TZrInt64 zr_aot_arg2)`.
- `backend_aot_c_typed_direct_calls.c` now proves the destination plus all three call-window argument slots are initialized i64 scalar locals before selecting the route.
- `backend_aot_c_lowering_calls.c` now emits `zr_aot_static_i64_three_arg_direct_call` and assigns `zr_aot_sD = zr_aot_typed_i64_fn_N(state, zr_aot_sA, zr_aot_sB, zr_aot_sC)` with the existing destination sync-elision rule.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused RED/GREEN command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test`
- Focused RED result:
  - typed call contracts 3/1, missing `backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function)`
  - i64 three-arg smoke 0/1, missing generated three-arg thunk/direct-call text
- Focused GREEN result:
  - typed call contracts 4/0
  - i64 three-arg smoke 1/0
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
  - float contracts 1/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call i64 smoke 5/0
  - i64 three-arg smoke 1/0
  - bool typed direct-call smoke 19/0
  - u64 typed direct-call smoke 14/0
  - f64 typed direct-call smoke 13/0
  - arithmetic typed direct-call smoke 5/0
  - bitwise typed direct-call smoke 6/0
  - typed scalar 1/0
  - value-type smoke 1/0
  - float smoke 1/0
  - generic numeric smoke 1/0
  - global smoke 9/0
  - logical smoke 4/0
  - power smoke 1/0

## File Scale

- `backend_aot_c_typed_i64_thunks.c`: 883 physical / 788 non-empty lines.
- `backend_aot_c_typed_direct_calls.c`: 845 physical / 758 non-empty lines.
- `backend_aot_c_lowering_calls.c`: 771 physical / 731 non-empty lines.
- `backend_aot_c_emitter.h`: 758 physical / 753 non-empty lines.
- `test_aot_c_typed_call_contracts.c`: 787 physical / 749 non-empty lines.
- `aot_c_typed_direct_call_i64_smoke_support.h`: 197 physical / 178 non-empty lines.
- `test_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke.c`: 42 physical / 38 non-empty lines.

## Acceptance Decision

Accepted for the narrow i64 three-parameter add-return typed direct-call slice. This does not close 07-S5: inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader multi-argument typed-return coverage, dynamic value access helpers, and stages 08-12 remain open.
