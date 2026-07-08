# AOT 07-S2/S4 Generic Numeric Mixed I64/U64 Result Stack-Copy MUL Local

## Scope

- Plan slice: 07-S2/S4 value-copy migration for proven scalar locals.
- Focused shape:
  `GET_CONSTANT int -> GET_CONSTANT uint -> ADD -> SET_STACK -> GET_CONSTANT uint -> MUL -> RETURN`.
- Goal: copied mixed i64/u64 generic numeric results must keep runtime signed-result semantics as i64 scalar locals and feed later mixed integer operations without value-slot materialization or runtime fallback.

## Coverage Result

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_unsigned_int_local` in `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`.
- The new coverage passed on the first WSL GCC focused run after the integer result-copy proof added for the previous i64 RED/GREEN slice.
- No additional production-code change was required for this mixed i64/u64 slice.

## Generated C Proof

- The focused generated C contains both `zr_aot_scalar_constant_i64_local` and `zr_aot_scalar_constant_u64_local`.
- The first mixed ADD stays local through `zr_aot_generic_numeric_mixed_i64_u64_add_scalar_local` and `zr_aot_s2 = zr_aot_s0 + (TZrInt64)zr_aot_u1;`.
- The copied signed result stays local through `zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2` and `zr_aot_s4 = zr_aot_s2;`.
- The later mixed MUL stays local through `zr_aot_generic_numeric_mixed_i64_u64_mul_scalar_local` and `zr_aot_s5 = zr_aot_s4 * (TZrInt64)zr_aot_u3;`.
- The generated C does not contain targeted `CopyStack`, `GenericNumericMul`, `SyncSignedIntLocal`, `TZrFloat64 zr_aot_f4`, or copied slot value materialization for slot 4.

## Verification

- WSL GCC focused smoke: `40 Tests 0 Failures 0 Ignored`.
- WSL GCC adjacent matrix:
  generic numeric smoke `40/0`, generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, call-result stack-copy equality `1/0`.
- WSL Clang adjacent matrix:
  generic numeric smoke `40/0`, generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, call-result stack-copy equality `1/0`.
- MSVC Debug focused targets built successfully. Contracts passed `1/0`, `24/0`, `2/0`; Unix-only shared-library smoke/regression tests reported expected ignores with 0 failures: generic numeric `40`, power `1`, generic LOGICAL_NOT `8`, generic bool equality `5`, generic equality stack-copy `4`, generic not-equal stack-copy jump-if `1`, call-result stack-copy equality `1`.

## Remaining

- This closes the narrow straight-line mixed i64/u64 generic numeric result-copy feeding a later `MUL`.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies remain open 07 work.
