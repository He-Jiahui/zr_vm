# AOT M1.5 07-S5 U64 Three-Arg Thunk Writer Split

Timestamp: 2026-06-23 23:32:28 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Move u64 three-argument typed thunk writer ownership out of the aggregate u64 thunk source.
- Preserve existing generated C behavior for u64 three-argument add, multiply, subtract, divide, modulo, bitwise AND, bitwise OR, and bitwise XOR thunks.

## RED

- Typed-call contracts were changed first to read `backend_aot_c_typed_u64_three_arg_thunks.{h,c}`.
- `zr_vm_aot_c_typed_call_contracts_test` failed 1/4 at `test_aot_c_source_lowers_static_no_arg_u64_calls_to_typed_thunks` with `Expected Non-NULL` because the new u64 three-arg thunk module did not exist yet.

## Implementation

- Added `backend_aot_c_typed_u64_three_arg_thunks.{h,c}`.
- Moved `backend_aot_c_can_emit_typed_u64_three_arg_thunk()` into the new module.
- Moved the u64 three-argument forward declaration writer into the new module.
- Moved u64 three-argument thunk definition writing for arithmetic, guarded divide/modulo, and bitwise routes into the new module.
- Kept `backend_aot_c_typed_u64_thunks.c` as the no/one/two-arg writer plus aggregate table loop owner, delegating three-arg definition writing through `backend_aot_c_try_write_u64_three_arg_thunk_definition()`.

## Validation

- Focused typed-call contracts: 4/0.
- Focused u64 typed direct-call shared-library smoke: 25/0.
- Focused bool typed direct-call shared-library smoke: 28/0.
- Broader AOT group: source 19/0, call 4/0, typed call 4/0, constant 5/0, generic numeric contracts 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, i64 three-arg 8/0, bool 28/0, u64 25/0, f64 19/0, typed direct-call arithmetic 7/0, typed direct-call bitwise 6/0, global 9/0, logical smoke 4/0, power smoke 1/0, value-type smoke 1/0, generic numeric smoke 1/0, float smoke 1/0.
- The first focused verification attempt timed out after two builds ran concurrently against the same WSL build directory; serial reruns passed.
- The first broad loop stopped on stale arithmetic/bitwise target names; rerunning the actual `typed_direct_call_arithmetic` and `typed_direct_call_bitwise` targets passed.
- Touched-file whitespace checks passed; tracked `git diff --check` reported only existing LF/CRLF warnings.

## Size Notes

- `backend_aot_c_typed_u64_thunks.c`: 832 physical / 748 non-empty lines.
- `backend_aot_c_typed_u64_three_arg_thunks.c`: 126 / 109.
- `backend_aot_c_typed_u64_three_arg_thunks.h`: 12 / 8.
- Growth watch remains on `backend_aot_c_lowering_calls.c` 933 / 885 and `backend_aot_c_typed_bool_thunks.c` 928 / 812.

## Still Open

- 07-S5 full typed ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridge templates.
- General typed-return ABI.
- Dynamic value access hardening.
- Full 07-S5 acceptance and stages 08-12.
