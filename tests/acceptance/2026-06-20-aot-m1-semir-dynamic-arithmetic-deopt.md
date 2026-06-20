# AOT M1 / 03-S2b SemIR Dynamic Arithmetic Deopt Boundary

## Scope

- Slice: `03-S2` from `docs/plans/aot/03-instruction-set-refactor.md`, dynamic/deopt sub-slice only.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: ensure generic dynamic arithmetic bytecode has an explicit SemIR runtime/deopt boundary instead of being absent from the SemIR model or confused with typed scalar operators.

## Completed Items

- Added `ZR_SEMIR_OPCODE_DYN_ARITHMETIC` as the explicit SemIR opcode for generic dynamic arithmetic/comparison bytecode.
- Mapped generic `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `LOGICAL_EQUAL`, and `LOGICAL_NOT_EQUAL` exec opcodes to `DYN_ARITHMETIC`.
- Marked those rows as `ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME` with generated `deoptId` entries.
- Preserved destination and operand slots on the dynamic SemIR row so later deopt/bridge work can recover the exact exec boundary.
- Updated the intermediate writer opcode name for `DYN_ARITHMETIC`.
- Added `tests/parser/test_semir_dynamic_arithmetic_deopt.c`.
- Registered the focused target and CTest entry `semir_dynamic_arithmetic_deopt`.

## RED / GREEN Evidence

- RED: a focused compile check failed because `ZR_SEMIR_OPCODE_DYN_ARITHMETIC` did not exist.
- GREEN: after adding the opcode and mapping, generic arithmetic/comparison bytecode builds one dynamic SemIR row, one deopt-map entry, a dynamic type-table index, and preserved destination/operand slots.

## Tests

Focused validation used an isolated WSL static build directory because another process was occupying `build/codex-wsl-gcc-debug`:

```text
build/codex-aot-m1-deopt-wsl-gcc-debug
```

Commands:

```text
zr_vm_semir_dynamic_arithmetic_deopt_test
zr_vm_semir_typed_opcode_guardrails_test
zr_vm_semir_static_c_types_test
zr_vm_semir_pipeline_test
ctest -R "semir_dynamic_arithmetic_deopt|semir_typed_opcode_guardrails|semir_static_c_types"
```

Observed results:

- SemIR dynamic arithmetic deopt: 1 test / 0 failures.
- SemIR typed opcode guardrails: 1 test / 0 failures.
- SemIR static C types: 1 test / 0 failures.
- SemIR pipeline: 10 tests / 0 failures.
- Focused CTest filter: 3 tests / 0 failures.

## Status

- Status: 03-S2b complete.
- 03-S2 remains partially complete: static C type annotations and dynamic arithmetic deopt boundaries are in place, but broader conflict/type-flow deopt analysis is not complete.
- M1 remains partially complete. Broader 03-S4 complex-instruction decomposition is still open before M2 scalar pure-C lowering.
- This slice does not remove `ZrCore_Stack_GetValue` / `ZR_VALUE_FAST_SET` from generated AOT C.
