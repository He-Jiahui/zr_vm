# SSA 01.02: canonical CFG fact identity checks

## Failure fixtures

With two semantic blocks, the second array element falsely declares block
ID 0. Before the change the MSVC builder fixture failed with `builder
silently renumbered a malformed canonical CFG block`. It now reports
`INVALID_BLOCK`, function token 42, source block 2 and expected/actual
one-based IDs [2,1]. A separate fixture assigns `EDGE_ENUM_MAX` to a
dynamic outgoing edge with a valid source/destination; it now reports
`INVALID_RANGE` with source block 1 and the highest valid/actual edge-kind
values. Both fail before publishing any output. Existing correctly numbered
block, diamond, and non-entry self-edge fixtures still build.

## Observed validation

- MSVC (VSDevCmd, D:-backed build): the focused adjacent SSA CTest selection
  passed 8/8 after the change; the mismatched block fixture failed before it.
- WSL GCC 11.4 compiled real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed executable printed
  `ssa builder CFG PASS` (exit 0), without a sanitizer report.
- WSL Clang 14 compiled the same sources without sanitizers; the D:-backed
  executable printed `ssa builder CFG PASS` (exit 0). Full cross-backend
  parity and Clang sanitizers are not claimed.

## Remaining gates

The checks validate input identity and enum bounds. Exception/cleanup/resume
edge-kind semantics, pruned phis, SSA renaming and full 01.02 remain open.
The independent dirty `test_ssa_construction.c` was not changed.
