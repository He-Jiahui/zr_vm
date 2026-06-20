# AOT M1 / 03-S4b SemIR Dynamic Call Deopt Boundary

## Scope

- Slice: `03-S4` from `docs/plans/aot/03-instruction-set-refactor.md`, dynamic call boundary sub-slice only.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: ensure generic call bytecode that is not proven typed is still visible as a dynamic SemIR call boundary with a deopt entry.

## Completed Items

- Mapped generic `FUNCTION_CALL` bytecode to `ZR_SEMIR_OPCODE_DYN_CALL` when value-type `CALL_TYPED` lowering does not apply.
- Mapped generic `FUNCTION_TAIL_CALL` bytecode to `ZR_SEMIR_OPCODE_DYN_TAIL_CALL`.
- Marked both rows as `ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME` with generated `deoptId` entries.
- Preserved result, callee, and argument-count operands on the dynamic SemIR rows.
- Added `tests/parser/test_semir_dynamic_call_deopt.c`.
- Registered the focused target and CTest entry `semir_dynamic_call_deopt`.

## RED / GREEN Evidence

- RED: the new focused test failed with `Expected Non-NULL` because generic `FUNCTION_CALL` and `FUNCTION_TAIL_CALL` produced no dynamic SemIR call boundary rows.
- GREEN: after adding the fallback mapping, both generic call instructions produce dynamic SemIR rows, deopt-map entries, dynamic static-C type metadata, and stable call-shape operands.

## Tests

The WSL CMake/Ninja path in the new call-focused build directory stalled in `VerifyGlobs.cmake` during build graph verification. To keep this slice evidence-driven without waiting on that unrelated build-system scan, focused validation directly compiled the test source with the current `compiler_semir.c` and linked against the existing static WSL libraries from:

```text
build/codex-aot-m1-deopt-wsl-gcc-debug/lib
```

Observed results:

- SemIR dynamic call deopt: 1 test / 0 failures.
- SemIR dynamic member deopt: 1 test / 0 failures.
- SemIR dynamic arithmetic deopt: 1 test / 0 failures.
- SemIR typed opcode guardrails: 1 test / 0 failures.
- SemIR static C types: 1 test / 0 failures.
- SemIR pipeline: 10 tests / 0 failures.

## Status

- Status: 03-S4b complete.
- 03-S4 remains partially complete: generic member and call boundaries are now deopt-visible, but arrays, iterators, and remaining complex instruction decomposition are not complete.
- M1 remains partially complete. Broader conflict/type-flow deopt analysis is still open before M2 scalar pure-C lowering.
- This slice does not remove `ZrCore_Stack_GetValue` / `ZR_VALUE_FAST_SET` from generated AOT C.
