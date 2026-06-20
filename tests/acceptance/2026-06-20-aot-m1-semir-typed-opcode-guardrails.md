# AOT M1 / 03-S1 SemIR Typed Opcode Guardrails

## Scope

- Slice: `03-S1` from `docs/plans/aot/03-instruction-set-refactor.md`.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: ensure typed numeric functions surface specialized scalar operations in SemIR instead of relying on generic dynamic arithmetic.

## Completed Items

- Added typed scalar SemIR opcodes for arithmetic and comparisons:
  `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `EQ`, `NE`, `LT`, `LE`, `GT`, and `GE`.
- Mapped existing specialized numeric and comparison bytecode instructions to the new SemIR scalar opcodes.
- Added explicit static C type propagation for scalar SemIR rows so temporary arithmetic result slots can still point at `I64`, `U64`, `F64`, or `BOOL` type-table entries.
- Updated intermediate writer opcode names for the new scalar SemIR rows.
- Added `tests/parser/test_semir_typed_opcode_guardrails.c`.
- Registered the focused target and CTest entry `semir_typed_opcode_guardrails`.

## RED / GREEN Evidence

- RED: `zr_vm_semir_typed_opcode_guardrails_test` first failed to build because the SemIR scalar opcode names did not exist.
- RED: after adding opcodes and mapping, the test failed because arithmetic temporary result rows still had dynamic type-table indices.
- GREEN: scalar SemIR rows now carry explicit static C type annotations and the typed numeric fixture produces `ADD/SUB/MUL/DIV/MOD/GT` rows without generic exec arithmetic opcodes.

## Tests

Focused validation:

```text
zr_vm_semir_typed_opcode_guardrails_test
zr_vm_semir_pipeline_test
zr_vm_semir_static_c_types_test
ctest -R "semir_typed_opcode_guardrails|semir_static_c_types"
```

Observed results:

- SemIR typed opcode guardrails: 1 test / 0 failures.
- SemIR pipeline: 10 tests / 0 failures.
- SemIR static C types: 1 test / 0 failures.
- Focused CTest filter: 2 tests / 0 failures.

## Status

- Status: 03-S1 complete.
- M1 remains partially complete. Full 03-S2 conflict-triggered deopt and 03-S4 broader complex-instruction decomposition are still open.
- This slice creates the SemIR input needed by later M2 scalar pure-C lowering; it does not yet remove `ZrCore_Stack_GetValue` / `ZR_VALUE_FAST_SET` from generated C.
