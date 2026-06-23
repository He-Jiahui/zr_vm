# AOT M1.5 07-S5 static bool two-arg inequality typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice adds a narrow typed-to-typed direct-call route for static bool functions with two bool parameters returning `left != right`.

## Baseline

- The previous bool two-argument comparison route recognized `left == right` only.
- Statically bool inequality already lowered to `LOGICAL_NOT_EQUAL_BOOL`, but the generated callee body still used the frame-backed `zr_aot_bool_compare_exec` path with `SZrTypeValue` access.
- The two-argument bool direct-call route already proved the destination and both call-window arguments were bool scalar locals; this slice reuses that proof and only broadens the eligible callee shape.

## RED

- The typed call contract first failed with missing source text `backend_aot_c_try_get_bool_arg0_arg1_not_equal_return(`.
- The bool shared-library smoke then failed because no generated typed thunk returned `return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);`; the generated callee still contained `zr_aot_bool_compare_exec` and a `SZrTypeValue` frame-slot path.

## Implementation

- `backend_aot_c_typed_bool_thunks.c` now shares a compact two-argument bool compare recognizer for `LOGICAL_EQUAL_BOOL` and `LOGICAL_NOT_EQUAL_BOOL`.
- The `!=` wrapper accepts bool-return, two-bool-parameter callees whose body is `LOGICAL_NOT_EQUAL_BOOL` followed by `FUNCTION_RETURN`, accepting the two parameter operands in either order.
- The two-argument bool can-emit gate now accepts equality, inequality, logical-and, and logical-or returns.
- The inequality thunk emits `return (TZrBool)(zr_aot_arg0 != zr_aot_arg1);`.
- The bool shared-library smoke executes `different(true, false)`, observes the expected `42` return path, and rejects `CallStaticDirect`, `CallStackValue`, and typed-destination stack sync for the scalar-only route.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused RED/GREEN command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Focused GREEN result:
  - typed call contracts 4/0
  - bool typed direct-call smoke 7/0
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
  - bool typed direct-call smoke 7/0
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

## Acceptance Decision

Accepted for the narrow bool `left != right` typed direct-call slice. This does not close 07-S5: inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader typed ABI coverage, and stages 08-12 remain open.
