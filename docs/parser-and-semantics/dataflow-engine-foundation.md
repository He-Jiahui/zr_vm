---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_cfg_reachability.c
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
plan_sources:
  - user: 2026-06-20 参照 docs/plans/lsp 优化语义推断能力
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_cfg_reachability.c
  - tests/acceptance/2026-06-20-semantic-stage1-dataflow.md
doc_type: module-detail
---

# Dataflow Engine Foundation

## Purpose

The dataflow engine is the Stage 1 execution layer that runs analyses over `SZrParserCfg`. It is deliberately generic: concrete analyses such as definite assignment, reaching definitions, ownership flow, numeric intervals, and logical facts provide their own state shape and transfer rules.

## Behavior Model

`SZrParserDataflowAnalysis` describes one analysis:
- `direction` selects forward or backward propagation.
- `stateSize` defines the byte size of each per-block state value.
- `initEntry` initializes the boundary state: entry `inState` for forward, exit `outState` for backward.
- `join` merges a predecessor or successor state into the destination state and reports whether the destination changed.
- `transferStatement` applies one statement to the current block state.

`SZrParserDataflowResult` owns one `SZrParserDataflowBlockState` per CFG block. Each block state has `inState`, `outState`, and `isReachable`.

## Control Flow

Forward analysis starts at CFG entry. It copies `inState` to `outState`, applies the statement transfer for statement blocks, then joins `outState` into each successor `inState`.

Backward analysis starts at CFG exit. It copies `outState` to `inState`, applies the statement transfer for statement blocks, then joins `inState` into each predecessor `outState`.

Both directions use a work queue and re-enqueue blocks when a join changes downstream state. The queue stores each block at most once at a time.

## Reachability Filter

Before running the work queue, the engine computes CFG reachability from entry. Propagation ignores blocks that are not entry-reachable. This is important for backward analyses: a syntactically unreachable statement may still have an edge to exit, but it must not be treated as a real path from program entry.

CFG therefore connects `return` and `throw` terminators to exit for backward analysis while keeping post-terminator statements entry-unreachable.

## Test Coverage

`tests/parser/test_dataflow_engine.c` covers:
- forward propagation visits a reachable `return` statement and skips the statement after it;
- backward propagation reaches `return` through the exit edge and still skips the entry-unreachable statement after it.

`tests/parser/test_cfg_reachability.c` also locks the CFG invariant that `return` terminator blocks connect to exit.

## Limits And Next Steps

The engine does not yet enforce iteration caps, expose diagnostics for analysis bailout, or provide helpers for common lattice shapes. Concrete analyses are still pending: definite assignment, reaching definitions, ownership, numeric intervals, and logical facts.
