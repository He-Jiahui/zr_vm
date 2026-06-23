# AOT M1.5 07-S5 No-Arg Local-Constant Typed Direct Call Acceptance

Date: 2026-06-23 16:30:13 +08:00
Status: Completed sub-slice; 07-S5/M1.5/07 remain partial; 08-12 not started.

## Scope

This slice tightens the i64, bool, u64, and f64 no-argument typed direct-call path for functions that return a local
initialized from a constant, such as `var result = constant; return result;`. It covers scalar no-arg local-constant
return shapes only and does not claim the broader typed-return ABI.

## RED

- The bool return-boundary shared-library smoke first failed because `answer(): bool { var result: bool = true;
  return result; }` still generated the old runtime static-call fallback instead of a typed bool thunk declaration and
  direct call.
- The i64 typed direct-call shared-library smoke then failed after its no-arg case was tightened to
  `answer(): int { var result: int = 42; return result; }`, proving the signed recognizer had the same local-copy gap.

## Implementation

- `backend_aot_c_try_get_i64_constant_return()` now accepts
  `GET_CONSTANT -> GET_STACK/SET_STACK copy -> FUNCTION_RETURN` local-return shapes.
- `backend_aot_c_try_get_bool_constant_return()` and `backend_aot_c_try_get_f64_constant_return()` now accept
  `GET_CONSTANT -> GET_STACK/SET_STACK copy -> FUNCTION_RETURN` local-return shapes.
- `backend_aot_c_try_get_u64_constant_return()` now accepts the same local-copy shape and the current unsigned
  conversion shape, `GET_CONSTANT -> GET_STACK/SET_STACK copy -> TO_UINT/TO_UINT_SIGNED -> FUNCTION_RETURN`.
- The i64/bool/u64/f64 no-arg smokes now reject `ZrLibrary_AotRuntime_CallStaticDirect()` and
  `ZrLibrary_AotRuntime_CallStackValue()` for those proven no-arg cases.
- The static numeric call shared-library smoke now expects u64/f64 no-arg typed thunk calls rather than the old runtime
  fallback call marker.

## GREEN

- Focused WSL GCC validation:
  - `zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`: 5/0.
  - `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`: 26/0.
  - `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 23/0.
  - `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`: 19/0.
  - `zr_vm_aot_c_call_shared_library_smoke_test`: 5/0.
- Broader WSL GCC AOT validation:
  - Contracts: source 19/0, call 4/0, typed call 4/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0,
    frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0.
  - Shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, bool 26/0, u64 23/0, f64 19/0, global 9/0,
    logical 4/0, power 1/0.

## Open

- General typed-return ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridges.
- Full 07-S5 acceptance and stages 08-12.
