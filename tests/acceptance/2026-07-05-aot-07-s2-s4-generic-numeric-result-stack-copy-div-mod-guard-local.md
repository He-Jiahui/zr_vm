# AOT 07-S2/S4 Generic Numeric Result Stack-Copy DIV/MOD Guard Local

## Scope

- Plan slice: 07-S2/S4 value-copy migration for proven scalar locals.
- Focused shapes:
  - `GET_CONSTANT float -> GET_CONSTANT float -> ADD -> SET_STACK -> GET_CONSTANT float -> DIV -> RETURN`.
  - `GET_CONSTANT int -> GET_CONSTANT int -> ADD -> SET_STACK -> GET_CONSTANT int -> MOD -> RETURN`.
- Goal: copied generic numeric results must feed guarded downstream `DIV`/`MOD` operations as scalar locals without value-slot materialization or runtime fallback.

## Coverage Result

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_div_float_local` in `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`.
- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mod_signed_int_local` in the same smoke file.
- Both coverage additions passed on the first WSL GCC focused runs.
- No additional production-code change was required for this guard slice.

## Generated C Proof

- The f64 DIV shape keeps the initial ADD in `zr_aot_f2`, copies it through `zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2` and `zr_aot_f4 = zr_aot_f2;`, then emits `zr_aot_generic_numeric_f64_div_scalar_local`, `if (zr_aot_f3 == (TZrFloat64)0.0)`, and `zr_aot_f5 = zr_aot_f4 / zr_aot_f3;`.
- The i64 MOD shape keeps the initial ADD in `zr_aot_s2`, copies it through `zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2` and `zr_aot_s4 = zr_aot_s2;`, then emits `zr_aot_generic_numeric_i64_mod_scalar_local`, `if (zr_aot_s3 == (TZrInt64)0)`, and `zr_aot_s5 = zr_aot_s4 % zr_aot_s3;`.
- The generated C does not contain targeted `CopyStack`, `GenericNumericDiv`/`GenericNumericMod`, scalar sync boundaries, generic numeric binary boundaries, or copied slot 4 value materialization.

## Verification

- WSL GCC focused generic numeric shared-library smoke: `44 Tests 0 Failures 0 Ignored`.
- WSL Clang focused generic numeric shared-library smoke: `44 Tests 0 Failures 0 Ignored`.
- MSVC Debug generic numeric shared-library smoke built successfully and reported expected Unix-only ignores with 0 failures: `44 Tests 0 Failures 44 Ignored`.

## Remaining

- This closes only straight-line copied generic numeric results feeding later guarded `DIV`/`MOD` in the covered f64 and i64 shapes.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies remain open 07 work.
