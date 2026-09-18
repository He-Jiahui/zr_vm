---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/guides/A-execir-builder-verifier.md
tests:
  - tests/parser/test_ssa_builder_cfg.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-builder-cfg.md
doc_type: module-detail
---

# ExecIR builder CFG adjacency

## Responsibility and data flow

`ZrParser_ExecIr_Build` copies semantic values, blocks, edges, instructions and
source positions into a temporary ExecIR function, then computes dominators and
constructs SSA. It replaces the caller's function only after every stage
succeeds. CFG block indices in SemanticIR are zero-based; emitted ExecIR block
IDs are one-based. The producer uses `outgoingEdges` when that array is valid,
otherwise the block's bounded inline `successors` array.

For each source block, outgoing IDs occupy a contiguous segment of the
function's successor side pool. The builder records the segment start before
appending its edges and increments its count for each successful append. It
does not overwrite the range with the last edge. Incoming IDs cannot be
appended while visiting sources in arbitrary order: a first pass counts edges
per destination, a prefix sum reserves one contiguous predecessor row per
destination, and a second pass writes source IDs in source/edge order. A single
side-pool append publishes those rows before dominator analysis. Duplicate
source/destination edges remain distinct entries in both adjacency arrays;
edge identity for phi construction is a separate outstanding milestone.

## Validation, ownership and failure

The builder checks CFG block array shape, outgoing edge storage, inline edge
capacity and all destinations before publishing an edge. Bad storage reports
`INVALID_RANGE` with the source block; a missing destination reports
`INVALID_BLOCK` with source block, valid block count and offending one-based
destination. A 32-bit predecessor count or allocation-size overflow reports
`CAPACITY_OVERFLOW`; allocation failure reports `OUT_OF_MEMORY`. Scratch
counts, cursors and rows are freed on every path. The outer builder frees a
failed temporary function and leaves existing caller output unchanged.

This preflight covers adjacency bounds, not semantic correctness of the
terminator opcode, exceptional-result availability, pruned phi insertion or
source-program behavioral parity. The dominator analysis has its own
validation and is described in `execir-cfg-dominators.md`.

## Regression boundary

The independent `ssa_builder_cfg` target uses real builder, dominator, SSA and
core ExecIR implementations. It checks both arms of a diamond and both merge
predecessors, a two-edge inline successor array, invalid target diagnostics and
unchanged caller output, and malformed edge storage. See
`tests/acceptance/ssa-builder-cfg.md` for the commands actually executed and
the remaining 01.02 acceptance gaps.
