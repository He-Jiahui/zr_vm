# AOT 07-S2/S4 Generic Numeric Result Stack-Copy Right DIV/MOD Guard Local

## Scope

- Plan slice: 07-S2/S4 value-copy migration for proven scalar locals.
- Focused shapes:
  - `GET_CONSTANT float -> GET_CONSTANT float -> ADD -> SET_STACK -> GET_CONSTANT float -> DIV(right copied result) -> RETURN`.
  - `GET_CONSTANT int -> GET_CONSTANT int -> ADD -> SET_STACK -> GET_CONSTANT int -> MOD(right copied result) -> RETURN`.
- Goal: copied generic numeric results must also feed the right-hand, zero-checked operand of downstream `DIV`/`MOD` as scalar locals without value-slot materialization or runtime fallback.

## Coverage Result

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_div_float_local` in `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`.
- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_mod_signed_int_local` in the same smoke file.
- Both coverage additions passed after increasing the focused WSL GCC command timeout; the first 120s command expired before reporting a test result and was not a test failure.
- No additional production-code change was required for this right-operand guard slice.

## Generated C Proof

- The f64 right-DIV shape keeps the initial ADD in `zr_aot_f2`, copies it through `zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2` and `zr_aot_f4 = zr_aot_f2;`, then emits `zr_aot_generic_numeric_f64_div_scalar_local`, `if (zr_aot_f4 == (TZrFloat64)0.0)`, and `zr_aot_f5 = zr_aot_f3 / zr_aot_f4;`.
- The i64 right-MOD shape keeps the initial ADD in `zr_aot_s2`, copies it through `zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2` and `zr_aot_s4 = zr_aot_s2;`, then emits `zr_aot_generic_numeric_i64_mod_scalar_local`, `if (zr_aot_s4 == (TZrInt64)0)`, and `zr_aot_s5 = zr_aot_s3 % zr_aot_s4;`.
- The generated C does not contain targeted `CopyStack`, `GenericNumericDiv`/`GenericNumericMod`, scalar sync boundaries, generic numeric binary boundaries, or copied slot 4 value materialization.

## Verification

- Initial WSL GCC command with a 120s timeout expired before producing a test result.
- WSL GCC rerun with a longer timeout: `46 Tests 0 Failures 0 Ignored`.
- WSL Clang focused generic numeric shared-library smoke: `46 Tests 0 Failures 0 Ignored`.
- MSVC Debug generic numeric shared-library smoke built successfully and reported expected Unix-only ignores with 0 failures: `46 Tests 0 Failures 46 Ignored`.

## Remaining

- This closes only straight-line copied generic numeric results feeding the right-hand guarded `DIV`/`MOD` operand in the covered f64 and i64 shapes.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies remain open 07 work.
