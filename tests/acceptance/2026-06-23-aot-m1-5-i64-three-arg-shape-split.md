# AOT M1.5 07-S5 i64 three-arg thunk shape split

Status: support slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This support slice keeps behavior unchanged while moving the i64 three-argument add-return shape recognizer out of the near-threshold i64 thunk writer file.

## Baseline

- The previous slice added `i64(i64, i64, i64)` add-return typed direct calls.
- `backend_aot_c_typed_i64_thunks.c` had grown to 883 physical / 788 non-empty lines.
- Continuing with more three-argument i64 shapes in that file would exceed the current modularization threshold.

## Implementation

- Added `backend_aot_c_typed_i64_thunk_shapes.{h,c}`.
- Moved `backend_aot_c_try_get_i64_arg0_arg1_arg2_add_return()` and its private add-operand reader/type-ref predicate into the new shape module.
- Kept i64 thunk can-emit routing, forward declarations, and thunk definitions in `backend_aot_c_typed_i64_thunks.c`.
- Updated `test_aot_c_typed_call_contracts.c` so shape-specific source needles are checked against the new shape file rather than the main thunk writer file.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Focused validation:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_call_contracts_test zr_vm_aot_c_typed_direct_call_i64_three_arg_shared_library_smoke_test -j 2`
  - typed call contracts 4/0
  - i64 three-arg smoke 1/0
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
  - float contracts 1/0
  - shared-library smoke 8/0
  - call smoke 5/0
  - typed direct-call i64 smoke 5/0
  - i64 three-arg smoke 1/0
  - bool typed direct-call smoke 19/0
  - u64 typed direct-call smoke 14/0
  - f64 typed direct-call smoke 13/0
  - arithmetic typed direct-call smoke 5/0
  - bitwise typed direct-call smoke 6/0
  - typed scalar 1/0
  - value-type smoke 1/0
  - float smoke 1/0
  - generic numeric smoke 1/0
  - global smoke 9/0
  - logical smoke 4/0
  - power smoke 1/0

## File Scale

- `backend_aot_c_typed_i64_thunks.c`: 818 physical / 730 non-empty lines.
- `backend_aot_c_typed_i64_thunk_shapes.c`: 76 physical / 67 non-empty lines.
- `backend_aot_c_typed_i64_thunk_shapes.h`: 8 physical / 5 non-empty lines.
- `test_aot_c_typed_call_contracts.c`: 800 physical / 762 non-empty lines.

## Acceptance Decision

Accepted as a behavior-preserving support split for the i64 three-argument thunk family. It does not close 07-S5 or add new runtime behavior; it prepares the next i64 three-argument expression slices while keeping the main thunk file below the large-file threshold.
