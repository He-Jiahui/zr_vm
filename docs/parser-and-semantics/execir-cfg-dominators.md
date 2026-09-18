---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/guides/A-execir-builder-verifier.md
  - user: 2026-09-17 staged SSA plan execution
tests:
  - tests/parser/test_ssa_dominator_cfg.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-dominator-cfg.md
doc_type: module-detail
---

# ExecIR CFG dominators

## Purpose and ownership

The parser's `ZrParser_ExecIr_ComputeDominators` analyzes a completed
function-level ExecIR adjacency graph before SSA construction or analysis
consumers use `immediateDominator`. It depends on the core ExecIR model and
side-pool ownership but does not inspect AST nodes, emitted ExecBC instructions,
or runtime pointers. Both the builder and optimization pass manager call this
entry point; it does not infer missing edges on their behalf.

## Validation and failure behavior

The function rejects a missing/invalid entry, null or inconsistent block/edge
storage, out-of-range adjacency spans, nonsequential block IDs, and invalid
successor/predecessor IDs before following an edge. It also compares sorted
`(source, destination)` edge multisets in both adjacency tables: missing,
invented, or collapsed parallel edges fail with
`PHI_PREDECESSOR_MISMATCH`, including discrepant counts or endpoints.
Diagnostics carry the function token, owning block ID, error code, and
expected/actual edge ID for invalid endpoints. Scratch allocation failures
report `OUT_OF_MEMORY`; on a 32-bit size type, impossible stack or edge
capacities report `CAPACITY_OVERFLOW`.

All traversal and dominator work stays in temporary arrays. The function
updates `immediateDominator` only after the complete analysis succeeds.
Malformed edges, allocation failure, and a disconnected reachable block with
no usable incoming definition leave the prior cached idoms untouched.
Repeated successful calls replace all idoms, including resetting unreachable
blocks to the invalid ID. Neither `FreeFunction` nor the caller has to release
analysis scratch storage.

## Algorithm and edge cases

An iterative depth-first search marks each reachable block when it is pushed,
then records blocks on exit. Reversing that postorder yields a genuine reverse
postorder, unlike a discovery-order walk. Each block is pushed at most once;
even a diamond with multiple incoming edges cannot overflow the block-sized
DFS stack. The Cooper–Harvey–Kennedy fixed-point walk starts the entry at its
own ID, intersects defined reachable predecessors, and climbs the idom chain
from the *higher* reverse-postorder rank. A bounded intersection fails closed
instead of spinning on a malformed idom chain.

An unreachable predecessor does not define an idom for a reachable join; an
unreachable block keeps its idom invalid. A loop backedge is revisited during
the fixed-point iteration without recursion. No phi insertion, live-variable
pruning, critical-edge split, or exceptional-result availability is proved by
this analysis alone; these remain the larger 01.02/01.03 milestones.

## Verification and follow-up

`ssa_dominator_cfg` uses a standalone focused target to test invalid ID
diagnostics and failure atomicity, a valid diamond and repeated analysis, a
loop backedge and unreachable block, and mismatched or matching parallel edge
occurrences. The executable is compiled with the real parser CFG implementation
and core ExecIR sources. The separate 01.03 structural verifier still owns
the broader IR contract; this analysis validates edge symmetry before relying
on its own cached predecessor information.

An existing uncommitted `ssa_construction` fixture still needs its edge-range
construction corrected before it can serve as an integration acceptance gate.
The builder's production edge emission and the full M1 four-backend parity
remain open; see `tests/acceptance/ssa-dominator-cfg.md` for actual results.
