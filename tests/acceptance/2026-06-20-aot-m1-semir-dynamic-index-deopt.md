# AOT M1 / 03-S4d SemIR Dynamic Index Deopt Boundary

## Scope

- Slice: `03-S4` from `docs/plans/aot/03-instruction-set-refactor.md`, dynamic index/array boundary sub-slice only.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: ensure generic index bytecode that is not lowered to typed array access remains visible as a dynamic SemIR index boundary with a deopt entry.

## Completed Items

- Added `ZR_SEMIR_OPCODE_DYN_INDEX_GET`.
- Added `ZR_SEMIR_OPCODE_DYN_INDEX_SET`.
- Mapped generic `GET_BY_INDEX` bytecode to `ZR_SEMIR_OPCODE_DYN_INDEX_GET`.
- Mapped generic `SET_BY_INDEX` bytecode to `ZR_SEMIR_OPCODE_DYN_INDEX_SET`.
- Marked both rows as `ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME` with generated `deoptId` entries.
- Preserved destination/value, receiver, and index operands on the dynamic SemIR rows.
- Updated the intermediate writer opcode names for `.zri` output.
- Added `tests/parser/test_semir_dynamic_index_deopt.c`.
- Registered the focused target and CTest entry `semir_dynamic_index_deopt`.

## RED / GREEN Evidence

- RED: the new focused test failed to compile because `ZR_SEMIR_OPCODE_DYN_INDEX_GET` and `ZR_SEMIR_OPCODE_DYN_INDEX_SET` did not exist.
- GREEN: after adding the opcodes, writer names, and fallback mapping, both generic index instructions produce dynamic SemIR rows, deopt-map entries, dynamic static-C type metadata, and stable operands.

## Tests

The WSL CMake/Ninja path is still unreliable in this workspace because build graph verification stalls in `VerifyGlobs.cmake`. Focused validation directly compiled the test source with the current `compiler_semir.c` and linked against the existing static WSL libraries from:

```text
build/codex-aot-m1-deopt-wsl-gcc-debug/lib
```

Observed results:

- SemIR dynamic index deopt: 1 test / 0 failures.
- SemIR dynamic iterator deopt: 1 test / 0 failures.
- SemIR dynamic call deopt: 1 test / 0 failures.
- SemIR dynamic member deopt: 1 test / 0 failures.
- SemIR dynamic arithmetic deopt: 1 test / 0 failures.
- SemIR typed opcode guardrails: 1 test / 0 failures.
- SemIR static C types: 1 test / 0 failures.
- SemIR pipeline: 10 tests / 0 failures.
- Intermediate writer syntax check: passed with the new opcode names.

## Status

- Status: 03-S4d complete.
- 03-S4 remains partially complete: generic member, call, iterator, and index boundaries are now deopt-visible, but typed array bounds checks and remaining complex instruction decomposition are not complete.
- M1 remains partially complete. Broader conflict/type-flow deopt analysis is still open before M2 scalar pure-C lowering.
- This slice does not remove `ZrCore_Stack_GetValue` / `ZR_VALUE_FAST_SET` from generated AOT C.
