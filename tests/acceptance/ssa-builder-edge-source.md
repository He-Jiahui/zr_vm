# SSA 01.02: dynamic CFG edge source ownership

## Failure and acceptance fixtures

The independent `ssa_builder_cfg` fixture places an edge advertising source
block 0 in block 1's dynamic outgoing row. Before the check, the MSVC test
failed with `builder silently reattributed an edge from another semantic
block`. The builder now reports `INVALID_BLOCK` with function token 42,
one-based enclosing block 2 and expected/actual source IDs [2,1], without
changing caller-owned ExecIR output. A matching non-entry self-edge preserves
both the entry and loop-back predecessors. The existing diamond now uses
canonical `fromBlockId` fields on its non-entry edges.

## Observed validation

- MSVC (VSDevCmd, D:-backed build): the focused adjacent SSA CTest selection
  passed 8/8 after the fix; before it, `ssa_builder_cfg` failed as above.
- WSL GCC 11.4 directly compiled the focused real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed executable printed
  `ssa builder CFG PASS` (exit 0), without a sanitizer report.
- WSL Clang 14 compiled those sources without sanitizers; the D:-backed
  executable printed `ssa builder CFG PASS` (exit 0). Full GCC/Clang CTest,
  Clang sanitizers, and source-language differential parity are not claimed.

## Remaining gates

This enforces dynamic edge source identity, not exceptional/resume edge
lowering, pruned phi insertion, SSA renaming, or full 01.02 completion.
The user-modified `test_ssa_construction.c` remains untouched.
