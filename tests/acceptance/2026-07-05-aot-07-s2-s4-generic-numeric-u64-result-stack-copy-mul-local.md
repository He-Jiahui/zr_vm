# AOT 07-S2/S4 Generic Numeric U64 Result Stack-Copy MUL Local

## Scope

- Plan slice: 07-S2/S4 value-copy migration for proven scalar locals.
- Focused shape:
  `GET_CONSTANT uint -> GET_CONSTANT uint -> ADD -> SET_STACK -> GET_CONSTANT uint -> MUL -> RETURN`.
- Goal: copied u64 generic numeric results must stay u64 scalar locals and feed a later generic numeric operation without value-slot materialization or runtime generic numeric fallback.

## Coverage Result

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_unsigned_int_local` in `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`.
- The new coverage passed on the first WSL GCC focused run after the integer result-copy proof added for the previous i64 RED/GREEN slice.
- No additional production-code change was required for this u64 slice.

## Generated C Proof

- The focused generated C contains `zr_aot_scalar_constant_u64_local`.
- The first ADD stays local through `zr_aot_generic_numeric_u64_add_scalar_local` and `zr_aot_u2 = zr_aot_u0 + zr_aot_u1;`.
- The copied result stays local through `zr_aot_scalar_stack_copy_u64 dstSlot=4 srcSlot=2` and `zr_aot_u4 = zr_aot_u2;`.
- The later MUL stays local through `zr_aot_generic_numeric_u64_mul_scalar_local` and `zr_aot_u5 = zr_aot_u4 * zr_aot_u3;`.
- The generated C does not contain targeted `CopyStack`, `GenericNumericMul`, `SyncUnsignedIntLocal`, `TZrFloat64 zr_aot_f4`, or copied slot value materialization for slot 4.

## Verification

- WSL GCC focused smoke: `39 Tests 0 Failures 0 Ignored`.
- WSL GCC adjacent matrix:
  generic numeric smoke `39/0`, generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, call-result stack-copy equality `1/0`.
- WSL Clang adjacent matrix:
  generic numeric smoke `39/0`, generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, call-result stack-copy equality `1/0`.
- MSVC Debug focused targets built successfully. Contracts passed `1/0`, `24/0`, `2/0`; Unix-only shared-library smoke/regression tests reported expected ignores with 0 failures: generic numeric `39`, power `1`, generic LOGICAL_NOT `8`, generic bool equality `5`, generic equality stack-copy `4`, generic not-equal stack-copy jump-if `1`, call-result stack-copy equality `1`.

## Remaining

- This closes the narrow straight-line u64 generic numeric result-copy feeding a later `MUL`.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies remain open 07 work.
