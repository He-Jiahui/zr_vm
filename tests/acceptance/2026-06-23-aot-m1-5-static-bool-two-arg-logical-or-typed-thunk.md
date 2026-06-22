# AOT M1.5 07-S5 static bool two-arg logical-or typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice adds a narrow typed-to-typed direct-call route for static bool functions with two bool parameters returning `left || right`.

## Baseline

- The previous bool two-argument route only recognized `left && right`.
- The two-argument direct-call route already proved the destination and both call-window arguments were bool scalar locals; this slice reuses that proof and only broadens the eligible callee shape.

## RED

- The typed call contract first failed with missing source text `backend_aot_c_try_get_bool_arg0_arg1_logical_or_return(`.
- The first implementation attempt assumed a `JUMP_IF_BOOL_TRUE` opcode; the WSL GCC build failed because the instruction set only exposes `JUMP_IF_BOOL_FALSE` for bool short-circuit branches.
- After switching to a generic `%s` operator writer, the contract failed again because the exact generated `&&` return expression was no longer present as source text. The final implementation keeps explicit AND and OR writer templates.

## Implementation

- `backend_aot_c_typed_bool_thunks.c` now recognizes compact `LOGICAL_OR` plus `FUNCTION_RETURN` and the current seven-instruction source-level `left || right` short-circuit shape:
  - left parameter copy to a temp
  - temp copy to the result
  - `JUMP_IF_BOOL_FALSE` to the right operand
  - unconditional `JUMP` to the return path when the left operand is true
  - right parameter copy to the result
  - `FUNCTION_RETURN`
- The OR thunk emits `return (TZrBool)(zr_aot_arg0 || zr_aot_arg1);`.
- The existing two-argument bool direct-call writer and route proof are reused, so eligible calls still emit `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)` and omit typed-destination stack sync when scalar-local proof permits it.
- The bool shared-library smoke executes `either(false, true)`, observes the expected `42` return path, and rejects `CallStaticDirect`, `CallStackValue`, and typed-destination stack sync for the scalar-only route.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused RED/GREEN command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_call_contracts_test`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Focused GREEN result:
  - typed call contracts 4/0
  - bool typed direct-call smoke 4/0
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
  - bool typed direct-call smoke 4/0
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

Accepted for the narrow bool `left || right` typed direct-call slice. This does not close 07-S5: inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader typed ABI coverage, and stages 08-12 remain open.
