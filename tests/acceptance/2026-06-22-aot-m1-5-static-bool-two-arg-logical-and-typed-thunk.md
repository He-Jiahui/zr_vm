# AOT M1.5 07-S5 static bool two-arg logical-and typed thunk

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This slice adds a narrow typed-to-typed direct-call route for static bool functions with two bool parameters returning `left && right`.

## RED

- The typed call contract first failed because the source did not expose `backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function)` or the matching two-argument bool direct-call writer.
- The first implementation then exposed the real compiler shape: source `left && right` lowers to short-circuit control flow rather than only a single `LOGICAL_AND` plus `FUNCTION_RETURN`.

## Implementation

- `backend_aot_c_typed_bool_thunks.c` recognizes both the compact `LOGICAL_AND`/`FUNCTION_RETURN` form and the current six-instruction short-circuit form using `GET_STACK`, `SET_STACK`, `JUMP_IF_BOOL_FALSE`, and `FUNCTION_RETURN`.
- The bool thunk writer emits `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrBool zr_aot_arg0, TZrBool zr_aot_arg1)` and returns `(TZrBool)(zr_aot_arg0 && zr_aot_arg1)`.
- `backend_aot_c_lowering_calls.c` emits the direct assignment route as `zr_aot_bD = zr_aot_typed_bool_fn_N(state, zr_aot_bA, zr_aot_bB)` with optional bool stack sync only when scalar-local proof cannot satisfy later consumers.
- `backend_aot_c_typed_direct_calls.c` proves the destination plus `functionSlot + 1u` and `functionSlot + 2u` are written bool scalar locals before selecting this path.
- The bool shared-library smoke executes `both(true, false)`, observes the expected `42` return path, and rejects `CallStaticDirect`, `CallStackValue`, and typed-destination stack sync for the scalar-only route.

## Validation

- Focused RED/GREEN:
  - RED: typed call contracts missing two-arg bool can-emit/writer source contracts.
  - Interim failure: bool smoke showed the generated callee uses the six-instruction short-circuit shape.
  - GREEN: typed call contracts 4/0 and bool typed direct-call smoke 3/0.
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
  - bool typed direct-call smoke 3/0
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

## Notes

The broad `ninja` rebuild reran configure and emitted unrelated parser/type-inference/module-init warnings from the shared dirty checkout; the changed bool thunk source did not emit a warning in the final focused build. This does not close 07-S5: inline structs, `in`/`out` writeback, deopt/dynamic bridges, broader typed ABI coverage, and stages 08-12 remain open.
