# AOT M1 / 03-S4c SemIR Dynamic Iterator Deopt Boundary

## Scope

- Slice: `03-S4` from `docs/plans/aot/03-instruction-set-refactor.md`, dynamic iterator boundary sub-slice only.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: ensure generic iterator bytecode that is not lowered to a typed loop remains visible as a dynamic SemIR iterator boundary with a deopt entry.

## Completed Items

- Mapped generic `ITER_INIT` bytecode to `ZR_SEMIR_OPCODE_DYN_ITER_INIT`.
- Mapped generic `ITER_MOVE_NEXT` bytecode to `ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT`.
- Marked both rows as `ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME` with generated `deoptId` entries.
- Preserved result and iterator/source operands on the dynamic SemIR rows.
- Added `tests/parser/test_semir_dynamic_iter_deopt.c`.
- Registered the focused target and CTest entry `semir_dynamic_iter_deopt`.

## RED / GREEN Evidence

- RED: the new focused test failed with `Expected Non-NULL` because generic `ITER_INIT` and `ITER_MOVE_NEXT` produced no dynamic SemIR iterator boundary rows.
- GREEN: after adding the fallback mapping, both generic iterator instructions produce dynamic SemIR rows, deopt-map entries, dynamic static-C type metadata, and stable operands.

## Tests

The WSL CMake/Ninja path is still unreliable in this workspace because build graph verification stalls in `VerifyGlobs.cmake`. Focused validation directly compiled the test source with the current `compiler_semir.c` and linked against the existing static WSL libraries from:

```text
build/codex-aot-m1-deopt-wsl-gcc-debug/lib
```

Observed results:

- SemIR dynamic iterator deopt: 1 test / 0 failures.
- SemIR dynamic call deopt: 1 test / 0 failures.
- SemIR dynamic member deopt: 1 test / 0 failures.
- SemIR pipeline: 10 tests / 0 failures.

## Status

- Status: 03-S4c complete.
- 03-S4 remains partially complete: generic member, call, and iterator boundaries are now deopt-visible, but arrays and remaining complex instruction decomposition are not complete.
- M1 remains partially complete. Broader conflict/type-flow deopt analysis is still open before M2 scalar pure-C lowering.
- This slice does not remove `ZrCore_Stack_GetValue` / `ZR_VALUE_FAST_SET` from generated AOT C.
