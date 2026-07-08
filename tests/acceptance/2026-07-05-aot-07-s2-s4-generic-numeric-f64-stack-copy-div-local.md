# 2026-07-05 AOT 07-S2/S4 generic numeric f64 stack-copy DIV local fold

## Scope

- Focused shape: `GET_CONSTANT float -> SET_STACK -> GET_CONSTANT float -> DIV -> RETURN`.
- Goal: prove the copied left f64 operand can remain in scalar locals and feed the generic numeric f64 DIV local fold.
- Non-goal: dynamic/unproven numeric operands still use the runtime generic numeric boundary.

## RED

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_div_float_stack_copy_left_local`.
- First WSL GCC focused run made generic numeric smoke 6 tests / 1 failure.
- Failure: generated C already emitted `zr_aot_scalar_stack_copy_f64 dstSlot=3 srcSlot=0` and `zr_aot_f3 = zr_aot_f0;`, but slot 1 and destination slot 2 were not declared as f64 locals, so DIV still emitted `zr_aot_arith_exec_generic_numeric_binary_boundary` and `ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 3, 1)`.

## GREEN

- `backend_aot_c_scalar_locals.c` now recognizes a generic numeric f64 binary operand when the other operand is a stack-copy destination whose source was a prior f64 constant.
- Stack-copy destination recording now runs before generic numeric destination recording, so copied f64 slots such as `slot 3` are known before `DIV` records destination `slot 2`.
- Generated C now declares `zr_aot_f0`, `zr_aot_f1`, `zr_aot_f2`, and `zr_aot_f3`, emits `zr_aot_f3 = zr_aot_f0;`, then emits `zr_aot_generic_numeric_f64_div_scalar_local` with zero guard and `zr_aot_f2 = zr_aot_f3 / zr_aot_f1;`.
- Source contract records the stack-copy constant proof helper and stack-copy-before-generic-numeric declaration pass.

## Verification

- WSL GCC `build-wsl-gcc`: generic numeric smoke 6/0, generic numeric contracts 1/0, source contracts 24/0, power smoke 1/0, power contracts 2/0, generic LOGICAL_NOT 8/0, generic equality stack-copy 4/0.
- WSL Clang `build-wsl-clang`: same matrix passed with 6/0, 1/0, 24/0, 1/0, 2/0, 8/0, and 4/0.
- Windows MSVC Debug `build-msvc-aot-stack-copy`: focused targets built; generic numeric contracts 1/0, source contracts 24/0, power contracts 2/0 passed. Unix-only shared-library smokes were expected ignored with 0 failures: generic numeric 6 ignored, power 1 ignored, generic LOGICAL_NOT 8 ignored, generic equality stack-copy 4 ignored.
- Scoped `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Remaining

- This closes only proven f64 generic numeric DIV through a left stack-copy operand.
- Right-side stack-copy, other arithmetic operators through copies, dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and full zero-frame typed bodies remain open 07 work.
