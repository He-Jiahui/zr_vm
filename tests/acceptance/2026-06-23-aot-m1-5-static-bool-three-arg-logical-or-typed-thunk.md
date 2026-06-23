# AOT M1.5 / 07-S5 static bool three-arg logical-or typed thunk

Status: sub-slice complete on 2026-06-23 21:45:12 +08:00. 07-S5 remains partial; stages 08-12 remain not started.

## Scope

- Extend the bool typed direct-call family to the narrow three-argument `arg0 || arg1 || arg2` shape.
- Keep the generated callee on the scalar C ABI:
  `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrBool arg0, TZrBool arg1, TZrBool arg2)`.
- Preserve the existing no-VM-call three-argument bool direct route and reject unnecessary stack sync in the smoke.
- Leave typed direct-call route proof/lowering unchanged; only the three-arg bool thunk recognizer/writer grows.

## RED

- `zr_vm_aot_c_typed_call_contracts_test` failed because the bool three-argument source contract had no
  `backend_aot_c_try_get_bool_arg0_arg1_arg2_logical_or_return(`.
- The new bool shared-library smoke for `either3(false, false, true)` failed before the generated C contained the
  three-argument OR thunk declaration/definition.

## Implementation

- Extended `backend_aot_c_typed_bool_three_arg_thunks.c` to accept compact `LOGICAL_OR -> LOGICAL_OR -> FUNCTION_RETURN`.
- Added recognition for the current source-level 12-instruction short-circuit OR shape:
  `GET_STACK`, `SET_STACK`, `JUMP_IF_BOOL_FALSE`, `JUMP`, middle copy/result, second short-circuit block, right copy/result, and return.
- Included OR in `backend_aot_c_can_emit_typed_bool_three_arg_thunk()`.
- Added the OR writer that emits `return (TZrBool)(zr_aot_arg0 || zr_aot_arg1 || zr_aot_arg2);`.
- Added contract needles and a bool shared-library smoke case that rejects `CallStaticDirect`, `CallStackValue`, and the bool three-arg sync marker.

## Validation

- Focused GREEN:
  - typed call contracts 4/0
  - bool shared-library smoke 28/0
- Broader WSL GCC validation:
  - contracts: source 19/0, call 4/0, typed call 4/0, constant 5/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0
  - shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, i64 three-arg 8/0, arithmetic 7/0, bitwise 6/0, bool 28/0, u64 25/0, f64 19/0, global 9/0, logical 4/0, power 1/0, value-type 1/0
- `git diff --check` on the scoped implementation/test files passed with only existing LF/CRLF warnings for the touched test files.

## Size Notes

- `backend_aot_c_typed_bool_three_arg_thunks.c`: 277 physical / 243 non-empty lines
- `tests/parser/test_aot_c_typed_call_contracts.c`: 1000 physical / 979 non-empty lines
- `tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c`: 790 physical / 733 non-empty lines
- The typed call contract is now at the 1000-line boundary; the next expansion of that contract surface should split or migrate needles before adding another responsibility.

## Still Open

- general typed-return ABI
- inline structs
- `in`/`out` writeback
- deopt/dynamic bridges
- full 07-S5 acceptance
- stages 08-12
