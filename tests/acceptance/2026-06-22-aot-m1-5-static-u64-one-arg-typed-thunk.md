# AOT M1.5 / 07-S5 static one-arg u64 typed thunk

- Timestamp: 2026-06-22 11:49:26 +08:00
- Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started
- Scope: one-parameter unsigned integer identity-return typed thunk direct-call route

## Completed

- Added a u64 identity-return recognizer and writer in `backend_aot_c_typed_u64_thunks.c` for `func pass(value: uint): uint { return value; }`.
- Added one-arg u64 static direct-call lowering that emits `zr_aot_uD = zr_aot_typed_u64_fn_N(state, zr_aot_uA)`.
- Extended typed direct-call route proof to require an initialized u64 scalar argument slot before selecting the route.
- Extended u64 shared-library smoke coverage to execute the one-arg route and verify runtime result 42.
- Fixed scalar stack-copy parameter staging so a u64 source local can materialize a value slot when the destination static type is not usable.

## RED / GREEN

- RED: call contracts required `backend_aot_c_can_emit_typed_u64_one_arg_thunk(const SZrFunction *function)` before production code existed.
- Intermediate failure: u64 smoke returned 5 instead of 42 because parameter staging copied from an unsynchronized frame slot and passed zero.
- GREEN: source contracts 19/0, call contracts 7/0, u64 typed direct-call smoke 2/0.

## Verification

- Focused WSL GCC validation passed:
  - source contracts 19/0
  - call contracts 7/0
  - typed direct-call smoke 5/0
  - bool smoke 2/0
  - u64 smoke 2/0
  - arithmetic smoke 5/0
  - bitwise smoke 6/0
  - call smoke 5/0
  - shared-library smoke 8/0
  - value-type smoke 1/0
  - power contracts 2/0 and power smoke 1/0
  - generic numeric contracts 1/0 and generic numeric smoke 1/0
  - global smoke 9/0
  - logical contracts 4/0 and logical smoke 4/0
  - typed scalar 1/0
  - return contracts 1/0
  - frame setup contracts 1/0

## Remaining

- u64 expression returns and multi-argument routes
- f64 typed direct-call routes
- inline struct typed-to-typed ABI
- in/out writeback
- deopt and dynamic bridge boxing
- 07-S5 complete acceptance
