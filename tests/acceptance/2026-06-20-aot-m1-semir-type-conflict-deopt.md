# AOT M1 / 03-S2c SemIR Static Type Conflict Deopt Boundary

## Scope

- Slice: `03-S2` from `docs/plans/aot/03-instruction-set-refactor.md`, static type-conflict deopt sub-slice.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: ensure a typed scalar instruction with conflicting static type evidence for one slot does not stay in typed SemIR and instead becomes a dynamic runtime/deopt boundary.

## Completed Items

- Added `tests/parser/test_semir_type_conflict_deopt.c`.
- Registered the focused target and CTest entry `semir_type_conflict_deopt`.
- Added a conservative SemIR conflict check for typed scalar instruction operands and destination slots.
- When a slot has multiple typed-local bindings with non-matching annotated static C types, the mapper now emits `ZR_SEMIR_OPCODE_DYN_ARITHMETIC` instead of the typed scalar opcode.
- The conflict row is marked `ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME`, receives a generated `deoptId`, and preserves destination/operand slots.

## RED / GREEN Evidence

- RED: the new focused test failed with `Expected Non-NULL` because the conflicting typed scalar instruction still emitted typed `ADD` SemIR and had no dynamic deopt boundary.
- GREEN: after adding the conflict check, the same instruction emits `DYN_ARITHMETIC`, has a deopt-map entry, carries dynamic static-C type metadata, and no typed `ADD` row remains.

## Tests

The WSL CMake/Ninja path is still unreliable in this workspace because build graph verification stalls in `VerifyGlobs.cmake`. Focused validation directly compiled the test source with the current `compiler_semir.c` and linked against the existing static WSL libraries from:

```text
build/codex-aot-m1-deopt-wsl-gcc-debug/lib
```

Observed results:

- SemIR type conflict deopt: 1 test / 0 failures.
- SemIR typed opcode guardrails: 1 test / 0 failures.
- SemIR static C types: 1 test / 0 failures.
- SemIR dynamic arithmetic deopt: 1 test / 0 failures.
- SemIR dynamic index deopt: 1 test / 0 failures.

## Status

- Status: 03-S2c complete.
- 03-S2 remains partially complete: static C type annotation, binary roundtrip, dynamic arithmetic deopt, and a focused conflict-deopt boundary are present, but broad type-flow analysis and full typed/dynamic block splitting are still open.
- M1 remains partially complete. This slice does not implement deopt execution or AOT scalar pure-C lowering.
