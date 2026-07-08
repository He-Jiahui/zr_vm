# AOT 07-S2/S4 Generic Numeric Mixed I64/F64 Result Stack-Copy MUL Local

## Scope

- Plan slice: 07-S2/S4 value-copy migration for proven scalar locals.
- Focused shape:
  `GET_CONSTANT int -> GET_CONSTANT float -> ADD -> SET_STACK -> GET_CONSTANT float -> MUL -> RETURN`.
- Goal: copied mixed i64/f64 generic numeric results must keep runtime float-promotion semantics as f64 scalar locals and feed later f64 operations without value-slot materialization or runtime fallback.

## Coverage Result

- Added `test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_float_local` in `tests/parser/test_aot_c_generic_numeric_shared_library_smoke.c`.
- RED: WSL GCC generic numeric smoke reported `41 Tests 1 Failures 0 Ignored`. The generated C already lowered the initial mixed ADD and the result stack-copy locally, but the later `MUL` still used `ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 5, 4, 3)`.
- GREEN: `backend_aot_c_scalar_locals_f64_value_kind_before_instruction()` now recognizes generic numeric binary results as f64 when one operand proves f64 and the other proves i64/u64 through the integer before-instruction value-kind proof. This makes copied mixed-f64 results provable for later f64 consumers.

## Generated C Proof

- The focused generated C contains `zr_aot_scalar_constant_i64_local` and `zr_aot_scalar_constant_f64_local`.
- The first mixed ADD stays local through `zr_aot_generic_numeric_mixed_f64_add_scalar_local` and `zr_aot_f2 = (TZrFloat64)zr_aot_s0 + zr_aot_f1;`.
- The copied mixed result stays local through `zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2` and `zr_aot_f4 = zr_aot_f2;`.
- The later f64 constant and MUL stay local through `zr_aot_f3 = (TZrFloat64)1.5;`, `zr_aot_generic_numeric_f64_mul_scalar_local`, and `zr_aot_f5 = zr_aot_f4 * zr_aot_f3;`.
- The generated C does not contain targeted `CopyStack`, `GenericNumericMul`, `SyncFloatLocal`, or copied slot 4 value materialization.

## Verification

- WSL GCC focused smoke: `41 Tests 0 Failures 0 Ignored`.
- WSL GCC adjacent matrix:
  generic numeric smoke `41/0`, generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, call-result stack-copy equality `1/0`.
- WSL Clang adjacent matrix:
  generic numeric smoke `41/0`, generic numeric contracts `1/0`, source contracts `24/0`, power smoke `1/0`, power contracts `2/0`, generic LOGICAL_NOT `8/0`, generic bool equality `5/0`, generic equality stack-copy `4/0`, generic not-equal stack-copy jump-if `1/0`, call-result stack-copy equality `1/0`.
- MSVC Debug focused targets built successfully. Contracts passed `1/0`, `24/0`, `2/0`; Unix-only shared-library smoke/regression tests reported expected ignores with 0 failures: generic numeric `41`, power `1`, generic LOGICAL_NOT `8`, generic bool equality `5`, generic equality stack-copy `4`, generic not-equal stack-copy jump-if `1`, call-result stack-copy equality `1`.

## Remaining

- This closes the narrow straight-line mixed i64/f64 generic numeric result-copy feeding a later `MUL`.
- Dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing, performance counters, and complete zero-frame typed bodies remain open 07 work.
