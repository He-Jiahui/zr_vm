---
related_code:
  - zr_vm_parser/include/zr_vm_parser/place.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/src/zr_vm_parser/place.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_graph.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_cleanup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/place.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/src/zr_vm_parser/place.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_graph.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_cleanup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
plan_sources:
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
tests:
  - tests/parser/test_place_cfg_graph.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_cfg_union_exhaustiveness.c
  - tests/parser/test_cfg_reachability.c
  - tests/parser/test_cfg_try_catch_edges.c
  - tests/parser/test_cfg_typed_catch_flow.c
  - tests/parser/test_cfg_typed_catch_loop_flow.c
  - tests/parser/test_dataflow_engine.c
  - tests/acceptance/2026-07-19-syntax-01-m2-place-cfg.md
doc_type: module-detail
---

# Place And General CFG

## Purpose

M2 establishes stable in-session identities for addressable storage and removes the semantic two-successor ceiling from the parser CFG. Later Semantic IR and borrow analysis can refer to `PlaceId`, typed edges, and block instruction ranges without recovering storage or control-flow meaning from AST shape.

## Place Identity

`SZrParserPlaceGraph` owns append-only `SZrParserPlace` nodes. IDs start at one; zero is invalid. A base place records its base kind, stable identity, canonical `TypeId`, and source range. A projection records its parent `PlaceId`, inherits the base, stores the complete flattened projection path, and keeps the source range for that use site.

The base model covers local, parameter, `this`, static, temporary, return slot, and external handle storage. The projection model covers field symbol, dynamic index `ValueId`, constant index, dereference, union variant symbol, and tuple element.

Each projected node keeps both a parent link and a flattened path. The parent link supports source-oriented diagnostics; the flattened path makes structural comparison independent of graph construction history. Place graph storage is session-local and is not an artifact identity.

## Overlap Query

`ZrParser_PlaceGraph_Overlap` returns one of four conservative states:

- `equal`: bases and every projection are structurally equal.
- `disjoint`: distinct non-aliasing bases, fields, tuple elements, or constant indexes.
- `overlap`: one path is a prefix of the other, or distinct variants share union storage.
- `unknown`: dynamic indexes, dereference aliases, unrelated external handles, or incomparable projection shapes.

The query never converts `unknown` into `disjoint`. That rule is the safety boundary required by M3 loan and move facts.

## CFG Storage

Each CFG block owns a dynamic `outgoingEdges` array. An edge records source block, target block, edge kind, and optional source AST node. The public accessor APIs are the semantic traversal surface. The two-entry `successors` array remains only as a compatibility prefix for existing binary-branch tests; `successorCount` reflects the complete dynamic array and is not capped at two.

Current edge kinds are normal, true branch, false branch, switch case, switch default, exception, cleanup, return, suspend, and resume. Blocks also carry a unique terminator kind plus the `firstInstructionIndex` and `instructionCount` fields that M3 will populate from pre-execution Semantic IR.

Graph initialization, append/connect operations, dynamic storage cleanup, and edge retagging live in `cfg_graph.c`. Rebuilding a graph frees every prior edge array before clearing blocks, so repeated semantic queries do not leak per-block storage.

## Control-Flow Rules

Builders label `if`, loop, switch, catch dispatch, return, exception, and finally edges directly. Switch dispatch can represent any number of outgoing edges; union compilation publishes an exhaustiveness fact on the switch AST so CFG construction can mark a redundant default unreachable with the dedicated exhaustive-branch cause.

Return and throw do not receive a direct exit edge when a required cleanup edge exists. `cfg_cleanup.c` traces each cleanup subgraph and attaches the pending return or exception only at its terminal blocks. A terminal shared by multiple completion kinds becomes a cleanup-dispatch terminator. Break and continue already route through cloned finally paths to their loop targets.

Suspension blocks and suspend/resume edge kinds are explicit even though async/generator lowering is deferred. This keeps suspension visible before M3/M5 borrow analysis instead of hiding it in execution bytecode.

## Consumer Boundary

Reachability and forward/backward dataflow traverse `ZrParser_Cfg_BlockSuccessorIdAt`, so they see every dynamic successor. Compiler optimizer and quickening graphs are separate execution-level structures and are not parser CFG consumers.

M2 does not yet make compile lowering emit load/store/move/borrow instructions. It supplies the Place and CFG substrate; M3 owns pre-execution Semantic IR instructions, per-block instruction ranges, and definite-assignment/move/loan/escape joins.

## Verification

`test_place_cfg_graph.c` directly covers every base and projection, all four overlap states, five outgoing switch edges, every current edge kind, suspension blocks, and builder-produced branch/return terminators. Existing CFG suites cover reachability, typed catches, loops, and dataflow. `test_cfg_finally_abrupt.c` directly proves that a try-return reaches the exit only after its finally block.
