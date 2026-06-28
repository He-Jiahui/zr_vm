# AOT 07-S2/S4 Signed Load-Const Scalar Frame Elision

- Completion time: 2026-06-28 08:12:21 +08:00
- Status: Completed sub-slice; 07-S2/S4 and 07/M1.5 remain partially complete.
- Scope: signed fused load-const/load-stack-const/load-stack-load-const scalar-local frame elision.

## Completed Items

- Classified signed fused constant arithmetic opcodes as i64 scalar-local results and consumers:
  `ADD/SUB/MUL/DIV/MOD_SIGNED_LOAD_CONST`,
  `ADD/SUB/MUL/DIV/MOD_SIGNED_LOAD_STACK_CONST`, and signed load-stack/load-stack-load-const variants.
- Restricted signed fused scalar consumers to their real source slots, so constant-pool indexes and
  materialized constant operands are not treated as ordinary frame reads.
- Declared immediate `GET_CONSTANT` destinations as scalar locals when later scalar consumers,
  power operands, or fused signed constant materialization slots require them.
- Added `zr_vm_aot_c_load_const_scalar_test` for a direct IR chain:
  `GET_CONSTANT -> ADD_SIGNED_LOAD_CONST -> GET_STACK -> SUB_SIGNED_LOAD_STACK_CONST ->
  GET_CONSTANT -> ADD_SIGNED_LOAD_STACK_LOAD_CONST -> FUNCTION_RETURN`.
- Locked generated-C contracts for no `zr_aot_generated_frame_setup`, no `ZrAotGeneratedFrame frame`,
  no `frame.slotBase`, no `ZrCore_Stack_GetValue`, and no `ZR_VALUE_FAST_SET`.

## Verification

- WSL gcc: built and ran `zr_vm_aot_c_load_const_scalar_test`,
  `zr_vm_aot_c_source_contracts_test`, and `zr_vm_aot_c_frame_setup_contracts_test`.
  Result: load-const scalar smoke 1/0, source contracts 21/0, frame setup contracts 1/0.
- WSL clang: built and ran the same focused target set.
  Result: load-const scalar smoke 1/0, source contracts 21/0, frame setup contracts 1/0.
- Windows MSVC Debug: built the same target set and ran the executables.
  Result: source contracts 21/0, frame setup contracts 1/0, load-const scalar smoke
  0 failures / 1 ignored Unix-only.
- `git diff --check` passed for the touched implementation and focused test files, with only LF/CRLF warnings.

## Remaining Work

This does not claim 07-S2/S4 completion. Dynamic/generic/string boundaries, GC roots/exports/frame cleanup,
wider byte-frame narrowing, performance counters, and complete typed function bodies with zero `SZrValue`/frame
write remain later slices.
