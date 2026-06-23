# AOT M1.5 / 07-S5 static u64 three-arg modulo typed thunk

Status: sub-slice complete on 2026-06-23 19:45:07 +08:00. 07-S5 remains partial; stages 08-12 remain not started.

## Scope

- Extend the existing u64 three-argument typed direct-call family from add/subtract/multiply/bitwise/divide shapes to ordered unsigned modulo.
- Recognize only the narrow `arg0 % arg1 % arg2` shape: two ordered `MOD_UNSIGNED` instructions followed by `FUNCTION_RETURN`.
- Keep typed-to-typed calls on the scalar C ABI:
  `static TZrUInt64 zr_aot_typed_u64_fn_N(struct SZrState *state, TZrUInt64 arg0, TZrUInt64 arg1, TZrUInt64 arg2)`.
- Preserve the no-VM-call direct route for scalar-local destinations and reject unnecessary stack-slot sync in the smoke.

## RED

- `zr_vm_aot_c_typed_call_contracts_test` failed because `backend_aot_c_typed_u64_thunks.c` did not contain
  `generated AOT unsigned three-arg modulo by zero`.

## Implementation

- Added `backend_aot_c_try_read_u64_modulo_operands()` and
  `backend_aot_c_try_get_u64_arg0_arg1_arg2_modulo_return()` to
  `backend_aot_c_typed_u64_thunk_shapes.{h,c}`.
- Reused the three-argument binary-return helper with `preserveOperandOrder == ZR_TRUE` so modulo only accepts
  `(arg0 % arg1) % arg2`.
- Included the modulo recognizer in `backend_aot_c_can_emit_typed_u64_three_arg_thunk()`.
- Added `backend_aot_c_write_u64_three_arg_modulo_thunk_definition()` with `arg1`/`arg2` zero guards and normal
  `return (TZrUInt64)(zr_aot_arg0 % zr_aot_arg1 % zr_aot_arg2);` emission.
- Added a shared-library smoke case for `remainder3(92, 50, 43)` that verifies runtime result `42` and rejects
  `CallStaticDirect`, `CallStackValue`, and the three-argument sync marker.

## Validation

- Focused GREEN:
  - typed call contracts 4/0
  - u64 shared-library smoke 25/0
- Broader WSL GCC validation:
  - contract target build completed successfully
  - one-shot broad target build and smoke-target build batches hit tool timeouts; the final evidence is executable validation
  - contracts: source 19/0, call 4/0, typed call 4/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0
  - shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, i64 three-arg 8/0, arithmetic 7/0, bitwise 6/0, bool 26/0, u64 25/0, f64 19/0, global 9/0, logical 4/0, power 1/0

## Size Notes

- `backend_aot_c_typed_u64_thunks.c`: 912 physical / 824 non-empty lines
- `backend_aot_c_typed_u64_thunk_shapes.c`: 731 physical / 653 non-empty lines
- `backend_aot_c_typed_u64_thunk_shapes.h`: 23 physical / 20 non-empty lines
- `tests/parser/test_aot_c_typed_call_contracts.c`: 935 physical / 914 non-empty lines
- `tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c`: 634 physical / 583 non-empty lines

## Still Open

- general typed-return ABI
- inline structs
- `in`/`out` writeback
- deopt/dynamic bridges
- full 07-S5 acceptance
- stages 08-12
