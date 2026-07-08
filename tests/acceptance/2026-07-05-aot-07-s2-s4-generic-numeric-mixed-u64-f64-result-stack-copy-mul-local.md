# AOT 07-S2/S4 Generic Numeric Mixed U64/F64 Result Stack-Copy MUL Local

## Scope

- Plan slice: 07-S2/S4 value-copy migration for proven scalar locals.
- Focused shape:
  `GET_CONSTANT uint -> GET_CONSTANT float -> ADD -> SET_STACK -> GET_CONSTANT float -> MUL -> RETURN`.
- Goal: copied mixed u64/f64 generic numeric results must keep runtime float-promotion semantics as f64 scalar locals and feed later f64 operations without value-slot materialization or runtime fallback.

## Coverage Result

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_unsigned_float_local` in `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`.
- The new coverage passed on the first WSL GCC focused run after the mixed i64/f64 result-copy proof.
- No additional production-code change was required for this mixed u64/f64 slice.

## Generated C Proof

- The focused generated C contains both `zr_aot_scalar_constant_u64_local` and `zr_aot_scalar_constant_f64_local`.
- The first mixed ADD stays local through `zr_aot_generic_numeric_mixed_f64_add_scalar_local` and `zr_aot_f2 = (TZrFloat64)zr_aot_u0 + zr_aot_f1;`.
- The copied mixed result stays local through `zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2` and `zr_aot_f4 = zr_aot_f2;`.
- The later f64 constant and MUL stay local through `zr_aot_f3 = (TZrFloat64)1.5;`, `zr_aot_generic_numeric_f64_mul_scalar_local`, and `zr_aot_f5 = zr_aot_f4 * zr_aot_f3;`.
- The generated C does not contain targeted `CopyStack`, `GenericNumericMul`, `SyncFloatLocal`, or copied slot 4 value materialization.

## Verification

- WSL GCC focused smoke: `42 Tests 0 Failures 0 Ignored`.
- WSL Clang focused smoke: `42 Tests 0 Failures 0 Ignored`.
- MSVC Debug generic numeric shared-library smoke built successfully and reported expected Unix-only ignores with 0 failures: `42 Tests 0 Failures 42 Ignored`.
- Adjacent matrix from the production GREEN immediately before this coverage addition remains covered by WSL GCC/Clang generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, and call-result stack-copy equality `1/0`, plus MSVC Debug contracts `1/0`, `24/0`, `2/0` and Unix-only adjacent smokes as expected ignored with 0 failures.

## Remaining

- This closes the narrow straight-line mixed u64/f64 generic numeric result-copy feeding a later `MUL`.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies remain open 07 work.
