# AOT M1.5 07-S5 U64 Typed-Direct Route Split

Timestamp: 2026-06-23 22:51:07 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Move u64 typed-direct route proof out of the top-level typed direct-call router.
- Preserve existing generated C behavior.

## RED

- Typed-call contracts were changed first to read `backend_aot_c_typed_direct_u64_calls.c`.
- `zr_vm_aot_c_typed_call_contracts_test` failed 1/4 at `test_aot_c_source_lowers_static_no_arg_u64_calls_to_typed_thunks` with `Expected Non-NULL` because the new u64 route module did not exist yet.

## Implementation

- Added `backend_aot_c_typed_direct_u64_calls.{h,c}`.
- Moved u64 no/one/two/three-arg typed-direct can-write proofs into the new module.
- Moved the u64-to-bool two-arg route proof into the new module.
- Kept `backend_aot_c_typed_direct_calls.c` as the top-level dispatch and writer-call orchestrator.
- Updated u64 and bool typed-call contract files to lock route-proof ownership to the new u64 direct-route module.

## Validation

- Focused typed-call contracts: 4/0.
- Focused u64 typed direct-call shared-library smoke: 25/0.
- Focused bool typed direct-call shared-library smoke: 28/0.
- Broader AOT group: source 19/0, call 4/0, typed call 4/0, constant 5/0, generic numeric contracts 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, i64 three-arg 8/0, bool 28/0, u64 25/0, f64 19/0, arithmetic 7/0, bitwise 6/0, global 9/0, logical smoke 4/0, power smoke 1/0, value-type smoke 1/0, generic numeric smoke 1/0 after rebuilding its stale target, float smoke 1/0.
- Touched-file whitespace checks passed; tracked `git diff --check` reported only existing LF/CRLF warnings.

## Size Notes

- `backend_aot_c_typed_direct_calls.c`: 771 physical / 702 non-empty lines.
- `backend_aot_c_typed_direct_u64_calls.c`: 189 / 165.
- `backend_aot_c_typed_direct_u64_calls.h`: 53 / 50.
- Growth watch remains on `backend_aot_c_lowering_calls.c` 933 / 885, `backend_aot_c_typed_bool_thunks.c` 928 / 812, and `backend_aot_c_typed_u64_thunks.c` 912 / 824.

## Still Open

- 07-S5 full typed ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridge templates.
- General typed-return ABI.
- Dynamic value access hardening.
- Full 07-S5 acceptance and stages 08-12.
