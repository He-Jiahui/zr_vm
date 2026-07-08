# 2026-07-05 AOT 07-S2/S4 generic numeric f64 result stack-copy ADD local fold

## Scope

- Focused shape: `GET_CONSTANT float -> GET_CONSTANT float -> DIV -> SET_STACK -> GET_CONSTANT float -> ADD -> RETURN`.
- Goal: prove a prior generic numeric f64 result can be stack-copied and then consumed by another generic numeric f64 binary operation without materializing the copy or the second operation through the runtime boundary.
- Non-goal: dynamic/unproven numeric operands, arbitrary recursive copies, and non-f64 generic numeric paths still use existing conservative fallbacks.

## RED

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_add_float_local`.
- First WSL GCC focused run made generic numeric smoke 7 tests / 1 failure.
- Failure: generated C already emitted the first local DIV and `zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2` / `zr_aot_f4 = zr_aot_f2;`, but the following ADD lacked `zr_aot_generic_numeric_f64_add_scalar_local` and still called `ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 5, 4, 3)`.

## GREEN

- `backend_aot_c_scalar_locals.c` now has a conservative f64-before-instruction proof for f64 constants, f64 generic numeric binary results, f64 conversions/results, and their stack copies.
- Generic numeric immediate-constant preservation now recognizes the other operand when it is a stack-copy destination sourced from a prior proven f64 generic numeric result.
- Generated C now declares `zr_aot_f0` through `zr_aot_f5`, emits `zr_aot_f2 = zr_aot_f0 / zr_aot_f1;`, `zr_aot_f4 = zr_aot_f2;`, `zr_aot_f3 = (TZrFloat64)1.5;`, and `zr_aot_f5 = zr_aot_f4 + zr_aot_f3;`.
- The focused generated C avoids targeted `CopyStack`, `GenericNumericAdd`, `SyncFloatLocal`, and copied-result value-slot materialization.

## Verification

- WSL GCC `build-wsl-gcc`: generic numeric smoke 7/0, generic numeric contracts 1/0, source contracts 24/0, power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy 4/0.
- WSL Clang `build-wsl-clang`: same matrix passed with 7/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug `build-msvc-aot-stack-copy`: focused targets built; generic numeric contracts 1/0, source contracts 24/0, and power contracts 2/0 passed. Unix-only shared-library smokes were expected ignored with 0 failures: generic numeric 7 ignored, power 1 ignored, generic LOGICAL_NOT 8 ignored, generic equality stack-copy 4 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Remaining

- This closes only copied f64 generic numeric results feeding a later proven f64 ADD in the focused straight-line shape.
- Right-side result copies, chained copies, other copied arithmetic variants, dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame typed bodies remain open 07 work.
