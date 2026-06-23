# AOT M1.5 / 07-S5 i64 no/one-arg shape split

Status: support sub-slice complete on 2026-06-23 18:12:47 +08:00. 07-S5 remains partial; stages 08-12 remain not started.

## Scope

- Keep generated C behavior unchanged.
- Move i64 no-arg constant-return and one-arg shape recognition out of `backend_aot_c_typed_i64_thunks.c`.
- Let `backend_aot_c_typed_i64_thunk_shapes.{h,c}` own:
  - no-arg constant-return shape recognition;
  - one-arg identity, negate, bitwise-not, bitwise-constant, add-constant, subtract-constant, and multiply-constant shape recognition;
  - no/one-arg return type, parameter metadata, opcode, and constant-value checks.
- Keep `backend_aot_c_typed_i64_thunks.c` responsible for can-emit routing, forward declarations, and thunk definition emission.

## RED

- `zr_vm_aot_c_typed_call_contracts_test` failed because `backend_aot_c_typed_i64_thunk_shapes.c` did not contain `backend_aot_c_try_get_i64_constant_return(`.

## Implementation

- Added public no/one-arg i64 shape declarations to `backend_aot_c_typed_i64_thunk_shapes.h`.
- Moved the no/one-arg recognizers and private helper shape checks into `backend_aot_c_typed_i64_thunk_shapes.c`.
- Removed those recognizer definitions from `backend_aot_c_typed_i64_thunks.c`.
- Added `backend_aot_c_emitter.h` to the i64 shape source after the first focused smoke exposed an implicit declaration for `backend_aot_c_get_constant_value()`.
- Updated typed call contracts so shape-specific no/one-arg needles are checked against the shape source instead of the writer source.

## Validation

- Focused GREEN:
  - typed call contracts 4/0
  - typed direct-call i64 no/one/two-arg smoke 5/0
  - i64 three-arg smoke 6/0
  - arithmetic smoke 7/0
  - bitwise smoke 6/0
- Broader WSL GCC validation:
  - contracts: source 19/0, call 4/0, typed call 4/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0
  - shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, i64 three-arg 6/0, arithmetic 7/0, bitwise 6/0, bool 26/0, u64 23/0, f64 19/0, global 9/0, logical 4/0, power 1/0

## Size Notes

- `backend_aot_c_typed_i64_thunks.c`: 275 physical / 255 non-empty lines
- `backend_aot_c_typed_i64_thunk_shapes.c`: 805 physical / 702 non-empty lines
- `backend_aot_c_typed_i64_thunk_shapes.h`: 36 physical / 33 non-empty lines
- `tests/parser/test_aot_c_typed_call_contracts.c`: 911 physical / 890 non-empty lines

## Still Open

- general typed-return ABI
- inline structs
- `in`/`out` writeback
- deopt/dynamic bridges
- full 07-S5 acceptance
- stages 08-12
