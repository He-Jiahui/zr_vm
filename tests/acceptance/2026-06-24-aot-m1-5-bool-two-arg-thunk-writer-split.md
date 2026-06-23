# AOT M1.5 07-S5 Bool Two-Arg Thunk Writer Split

Timestamp: 2026-06-24 00:08:20 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Move plain bool/bool two-argument typed thunk writer ownership out of the aggregate bool thunk source.
- Preserve existing generated C behavior for bool/bool equal, not-equal, logical AND, and logical OR thunks.

## RED

- Typed-call contracts were changed first to read `backend_aot_c_typed_bool_two_arg_thunks.{h,c}`.
- `zr_vm_aot_c_typed_call_contracts_test` failed 1/4 at `test_aot_c_source_lowers_static_no_arg_bool_calls_to_typed_thunks` with `Expected Non-NULL` because the new bool two-arg thunk module did not exist yet.

## Implementation

- Added `backend_aot_c_typed_bool_two_arg_thunks.{h,c}`.
- Moved plain bool/bool two-argument equal/not-equal/logical-and/logical-or recognizers into the new module.
- Moved `backend_aot_c_can_emit_typed_bool_two_arg_thunk()` into the new module.
- Moved the bool two-argument forward declaration writer into the new module.
- Moved plain bool/bool two-argument thunk definition writing into the new module.
- Kept `backend_aot_c_typed_bool_thunks.c` as the no/one-arg writer, three-arg delegate, and i64/u64/f64 comparison writer owner, delegating bool/bool two-arg definition writing through `backend_aot_c_try_write_bool_two_arg_thunk_definition()`.

## Validation

- Focused typed-call contracts: 4/0.
- Focused bool typed direct-call shared-library smoke: 28/0.
- Focused u64 typed direct-call shared-library smoke: 25/0.
- Before the final duplicate-guard cleanup, the broader AOT group passed: source 19/0, call 4/0, typed call 4/0, constant 5/0, generic numeric contracts 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, i64 three-arg 8/0, bool 28/0, u64 25/0, f64 19/0, typed direct-call arithmetic 7/0, typed direct-call bitwise 6/0, global 9/0, logical smoke 4/0, power smoke 1/0, value-type smoke 1/0, generic numeric smoke 1/0, float smoke 1/0.
- After deleting one duplicate `function->instructionsList == ZR_NULL` guard, focused typed-call contracts and bool smoke were rerun and passed again.
- A post-cleanup broad rerun exceeded the tool timeout while f64 generated shared-library compilation was still running, so it is not used as pass evidence; the leftover f64 smoke process later ended naturally.
- Touched-file whitespace checks passed; tracked `git diff --check` reported only existing LF/CRLF warnings.

## Size Notes

- `backend_aot_c_typed_bool_thunks.c`: 673 physical / 587 non-empty lines.
- `backend_aot_c_typed_bool_two_arg_thunks.c`: 298 / 257.
- `backend_aot_c_typed_bool_two_arg_thunks.h`: 12 / 8.
- `tests/parser/test_aot_c_typed_call_bool_contracts.c`: 347 / 341.
- Growth watch remains on `backend_aot_c_lowering_calls.c` 933 / 885 and `backend_aot_c_typed_u64_thunks.c` 832 / 748.

## Still Open

- 07-S5 full typed ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridge templates.
- General typed-return ABI.
- Dynamic value access hardening.
- Full 07-S5 acceptance and stages 08-12.
