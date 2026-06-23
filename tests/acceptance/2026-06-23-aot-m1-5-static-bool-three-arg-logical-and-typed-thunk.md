# AOT M1.5 / 07-S5 static bool three-arg logical-and typed thunk

Status: sub-slice complete on 2026-06-23 21:11:23 +08:00. 07-S5 remains partial; stages 08-12 remain not started.

## Scope

- Extend the bool typed direct-call family to the narrow three-argument `arg0 && arg1 && arg2` shape.
- Keep the generated callee on the scalar C ABI:
  `static TZrBool zr_aot_typed_bool_fn_N(struct SZrState *state, TZrBool arg0, TZrBool arg1, TZrBool arg2)`.
- Preserve the no-VM-call direct route for scalar-local destinations and reject unnecessary three-argument stack sync in the smoke.
- Keep 07-S3 frame setup isolation intact: generated frames still do not write `frame.recordHandle`.

## RED

- `zr_vm_aot_c_typed_call_contracts_test` failed because the bool three-argument can-emit/source contract was missing
  `backend_aot_c_can_emit_typed_bool_three_arg_thunk(`.
- The first runtime smoke exposed closure materialization still assuming `frame->recordHandle` existed after 07-S3 removed it from generated frame setup.

## Implementation

- Added `backend_aot_c_typed_bool_three_arg_thunks.{h,c}` for the three-bool-parameter logical-and recognizer and thunk writer.
- Delegated bool three-argument emission from `backend_aot_c_typed_bool_thunks.c` to the focused module.
- Extended typed direct-call proof and lowering so generated callers emit `zr_aot_static_bool_three_arg_direct_call` and assign the `TZrBool` result to a bool scalar local.
- Added `GET_CLOSURE` / `GETUPVAL` callable provenance needed by resolvable captured closure materialization.
- Added `backend_aot_write_c_create_closure()` and routed resolvable `CREATE_CLOSURE` cases through `ZrLibrary_AotRuntime_CreateClosure()`.
- Updated `ZrLibrary_AotRuntime_CopyConstant()` and `ZrLibrary_AotRuntime_CreateClosure()` to fall back to `aot_runtime_find_record_for_function(runtimeState, function)` when `frame->recordHandle` is null.
- Added a shared-library smoke case for `all(true, true, true)` returning `42`, while rejecting `CallStaticDirect`, `CallStackValue`, and the bool three-arg sync marker.

## Validation

- Focused GREEN:
  - constant contracts 5/0
  - frame setup contracts 1/0
  - typed call contracts 4/0
  - bool shared-library smoke 27/0
- Generated text check:
  - typed bool three-arg declaration and `zr_aot_typed_bool_fn_1(state, ...)` direct call present
  - no `frame.recordHandle`
  - no `ZrLibrary_AotRuntime_CallStaticDirect`
  - no `ZrLibrary_AotRuntime_CallStackValue`
- Broader WSL GCC validation:
  - contracts: source 19/0, call 4/0, typed call 4/0, constant 5/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0
  - shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, i64 three-arg 8/0, arithmetic 7/0, bitwise 6/0, bool 27/0, u64 25/0, f64 19/0, global 9/0, logical 4/0, power 1/0, value-type 1/0
  - one-shot smoke target build exceeded the tool timeout; the final evidence is split target builds plus all listed smoke executable runs.

## Size Notes

- `backend_aot_c_typed_bool_three_arg_thunks.c`: 199 physical / 173 non-empty lines
- `backend_aot_c_typed_bool_three_arg_thunks.h`: 12 physical / 8 non-empty lines
- `backend_aot_c_typed_bool_thunks.c`: 928 physical / 812 non-empty lines
- `backend_aot_c_typed_direct_calls.c`: 932 physical / 844 non-empty lines
- `backend_aot_c_lowering_calls.c`: 933 physical / 885 non-empty lines
- `backend_aot_callable_provenance.c`: 257 physical / 224 non-empty lines
- `tests/parser/test_aot_c_typed_call_contracts.c`: 994 physical / 973 non-empty lines
- `tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c`: 761 physical / 706 non-empty lines
- `tests/parser/test_aot_c_constant_contracts.c`: 404 physical / 359 non-empty lines
- `zr_vm_library/src/zr_vm_library/aot_runtime.c`: 8619 physical / 7566 non-empty lines

## Still Open

- general typed-return ABI
- inline structs
- `in`/`out` writeback
- deopt/dynamic bridges
- full 07-S5 acceptance
- stages 08-12
