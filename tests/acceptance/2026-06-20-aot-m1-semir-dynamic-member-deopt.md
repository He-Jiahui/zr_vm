# AOT M1 / 03-S4a SemIR Dynamic Member Deopt Boundary

## Scope

- Slice: `03-S4` from `docs/plans/aot/03-instruction-set-refactor.md`, dynamic member-access boundary sub-slice only.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: ensure generic member access bytecode has explicit SemIR runtime/deopt boundaries instead of being absent from the SemIR model.

## Completed Items

- Mapped generic `GET_MEMBER` bytecode to `ZR_SEMIR_OPCODE_META_GET`.
- Mapped generic `SET_MEMBER` bytecode to `ZR_SEMIR_OPCODE_META_SET`.
- Marked both rows as `ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME` with generated `deoptId` entries.
- Preserved destination/value, receiver, and member-entry operands on the dynamic SemIR rows.
- Added `tests/parser/test_semir_dynamic_member_deopt.c`.
- Registered the focused target and CTest entry `semir_dynamic_member_deopt`.

## RED / GREEN Evidence

- RED: the new focused test failed with `Expected Non-NULL` because `GET_MEMBER` and `SET_MEMBER` produced no SemIR dynamic boundary rows.
- GREEN: after adding the mapping, both generic member-access instructions produce dynamic SemIR rows, deopt-map entries, and stable operands.

## Tests

Focused validation used the isolated WSL static build directory:

```text
build/codex-aot-m1-deopt-wsl-gcc-debug
```

Commands:

```text
zr_vm_semir_dynamic_member_deopt_test
zr_vm_semir_dynamic_arithmetic_deopt_test
zr_vm_semir_typed_opcode_guardrails_test
zr_vm_semir_static_c_types_test
zr_vm_semir_pipeline_test
ctest -R "semir_dynamic_member_deopt|semir_dynamic_arithmetic_deopt|semir_typed_opcode_guardrails|semir_static_c_types"
```

Observed results:

- SemIR dynamic member deopt: 1 test / 0 failures.
- SemIR dynamic arithmetic deopt: 1 test / 0 failures.
- SemIR typed opcode guardrails: 1 test / 0 failures.
- SemIR static C types: 1 test / 0 failures.
- SemIR pipeline: 10 tests / 0 failures.
- Focused CTest filter: 4 tests / 0 failures.

## Status

- Status: 03-S4a complete.
- 03-S4 remains partially complete: generic member access now has deopt-visible SemIR boundaries, but broader complex-instruction decomposition for calls, arrays, iterators, and remaining metadata/runtime boundaries is not complete.
- M1 remains partially complete. Broader conflict/type-flow deopt analysis is still open before M2 scalar pure-C lowering.
- This slice does not remove `ZrCore_Stack_GetValue` / `ZR_VALUE_FAST_SET` from generated AOT C.
