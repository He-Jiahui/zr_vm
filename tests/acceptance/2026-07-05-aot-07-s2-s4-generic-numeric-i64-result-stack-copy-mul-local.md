# AOT 07-S2/S4 Generic Numeric I64 Result Stack-Copy MUL Local

## Scope

- Plan slice: 07-S2/S4 value-copy migration for proven scalar locals.
- Focused shape:
  `GET_CONSTANT int -> GET_CONSTANT int -> ADD -> SET_STACK -> GET_CONSTANT int -> MUL -> RETURN`.
- Goal: copied i64 generic numeric results must remain i64 scalar locals and feed later generic numeric operations without value-slot materialization or runtime generic numeric fallback.

## RED

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_int_local` in `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`.
- First WSL GCC run built the focused target but failed the new smoke:
  `38 Tests 1 Failures 0 Ignored`.
- Generated C still emitted the runtime boundary for the initial ADD, then `ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)`, and finally `ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 5, 4, 3)`.
- The copied destination was also misclassified as `TZrFloat64 zr_aot_f4`, showing the stack-copy consumer proof could fall through to the f64 generic numeric consumer when the source result kind was not yet known.

## GREEN

- `backend_aot_c_scalar_locals.c` now has an integer value-kind proof that can walk prior i64/u64 constants, generic numeric binary results, mixed i64/u64 signed results, generic NEG-to-i64 results, conversions/results, and stack copies before a later instruction.
- i64/u64 generic numeric operand checks now accept copied proven integer results, not only constants and constant stack copies.
- stack-copy destination recording now recovers the source kind from integer/f64 value-kind proof when earlier declaration passes have not yet recorded the generic numeric result.
- stack-copy consumer classification now has an explicit i64 generic numeric binary branch, preventing copied proven i64 operands from falling through to the f64 consumer path.
- Generated C for the focused shape now contains `zr_aot_generic_numeric_i64_add_scalar_local`, `zr_aot_s2 = zr_aot_s0 + zr_aot_s1;`, `zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2`, `zr_aot_s4 = zr_aot_s2;`, `zr_aot_generic_numeric_i64_mul_scalar_local`, and `zr_aot_s5 = zr_aot_s4 * zr_aot_s3;`.
- The focused output no longer contains targeted `CopyStack`, `GenericNumericMul`, `SyncSignedIntLocal`, or copied slot value materialization for slot 4.

## Verification

- WSL GCC focused smoke: `38 Tests 0 Failures 0 Ignored`.
- WSL GCC adjacent matrix:
  generic numeric smoke `38/0`, generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, call-result stack-copy equality `1/0`.
- WSL Clang adjacent matrix:
  generic numeric smoke `38/0`, generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, call-result stack-copy equality `1/0`.
- MSVC Debug focused targets built successfully. Contracts passed `1/0`, `24/0`, `2/0`; Unix-only shared-library smoke/regression tests reported expected ignores with 0 failures: generic numeric `38`, power `1`, generic LOGICAL_NOT `8`, generic bool equality `5`, generic equality stack-copy `4`, generic not-equal stack-copy jump-if `1`, call-result stack-copy equality `1`.

## Remaining

- This closes the narrow straight-line i64 generic numeric result-copy feeding a later `MUL`.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies remain open 07 work.
