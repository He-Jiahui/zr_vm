---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_reaching_definitions.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_cfg_reachability.c
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_reaching_definitions.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
plan_sources:
  - user: 2026-06-20 参照 docs/plans/lsp 优化语义推断能力
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_cfg_reachability.c
  - tests/acceptance/2026-06-20-semantic-stage1-dataflow.md
doc_type: module-detail
---

# Dataflow Engine Foundation

## Purpose

The dataflow engine is the Stage 1 execution layer that runs analyses over `SZrParserCfg`. It is deliberately generic: concrete analyses such as definite assignment, reaching definitions, ownership flow, numeric intervals, and logical facts provide their own state shape and transfer rules.

The first reaching-definitions slice started with a straight-line resolver. `SZrSemanticReferenceFact` carries `definitionRange` / `hasDefinitionRange`, and `ZrParser_SemanticFacts_ResolveLinearReachingDefinitions` can annotate straight-line reference facts so a read points to the latest preceding declaration/write for the same resolved symbol. `ZrParser_SemanticQuery_DefinitionOf` consumes this payload for reads before falling back to declarations.

`semantic_reaching_definitions.c` now adds the first CFG-backed producer for that same payload. It maps resolved declaration/read/write facts to symbol ordinals, runs forward dataflow over script/function/test body CFGs, and tracks whether each symbol has one reaching declaration/write, no known definition, or multiple divergent definitions at a join. Reads keep a `definitionRange` only when the incoming state is a single known definition. Ambiguous states also preserve a bounded `definitionRanges` list, so query consumers that support multiple locations can show all known branch writes instead of incorrectly picking the last source-order write or falling back to the declaration.

Repeated source reads cloned into multiple `finally` CFG paths are accumulated by reference-fact index and applied after dataflow convergence. This prevents a normal path and a function-exit path from racing to write the same source read fact; if those paths see different declaration/write definitions, the read's `hasDefinitionRange` is cleared while `definitionRanges` retains the enumerable alternatives when they fit the current bounded payload. `semantic_reaching_definitions.c` also excludes structural parent statements such as `try`, `catch`, blocks, and `switch default` from whole-range fact matching, matching the definite-assignment producer so child-body facts only transfer in their real CFG blocks.

The first definite-assignment slice provides a reusable state lattice in `dataflow_definite_assignment.h/.c`. A state array is indexed by symbol ordinal and stores `UNINIT`, `INIT`, or `MAYBE_INIT`; joins keep equal values unchanged and collapse mismatches to `MAYBE_INIT`. `semantic_definite_assignment.c` now provides the first concrete CFG-backed producer: it maps resolved declaration/read/write reference facts to symbol ordinals, runs forward dataflow over each script/function/test body CFG, and annotates source read facts with the joined state seen at that read. Variable declarations with initializers keep the symbol `UNINIT` while the initializer expression is transferred, then mark the symbol `INIT` at statement end; this keeps `var seed = seed` from treating the initializer read as already assigned. Reads from a source `finally` body that is cloned into multiple CFG paths are accumulated by reference-fact index and written back after dataflow convergence, so normal and function-exit paths can join to `MAYBE_INIT` instead of whichever cloned block runs last. Structural parent statements such as `try`, `catch`, blocks, and `switch default` are excluded from whole-range fact matching so child-body reads and writes transfer only in their real CFG blocks. Constant-true `while`/traditional `for` loops and omitted-condition `for(;;)` loops no longer contribute a synthetic zero-iteration path to the loop join, so a write before an explicit `break` can stay definitely assigned at the following read.

The semantic fact layer also has a straight-line supplement: `ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments` walks reference facts in order, treats explicitly marked declarations as initial symbol state, treats writes as `INIT`, and annotates reads with the latest known state for diagnostics. Type inference and the compiler assignment lowering both pre-fill identifier write reference facts as `INIT`, so write facts carry a usable assignment-state payload before or during resolver passes. This mirrors the reaching-definitions bridge shape while the CFG-backed resolver starts covering branch joins.

## Behavior Model

`SZrParserDataflowAnalysis` describes one analysis:
- `direction` selects forward or backward propagation.
- `stateSize` defines the byte size of each per-block state value.
- `initEntry` initializes the boundary state: entry `inState` for forward, exit `outState` for backward.
- `join` merges a predecessor or successor state into the destination state and reports whether the destination changed.
- `transferStatement` applies one statement to the current block state.

`SZrParserDataflowResult` owns one `SZrParserDataflowBlockState` per CFG block. Each block state has `inState`, `outState`, and `isReachable`.

## Control Flow

Forward analysis starts at CFG entry. It copies `inState` to `outState`, applies the statement transfer for statement blocks, then propagates `outState` into each successor `inState`.

Backward analysis starts at CFG exit. It copies `outState` to `inState`, applies the statement transfer for statement blocks, then propagates `inState` into each predecessor `outState`.

Both directions use a work queue and re-enqueue blocks when propagation changes downstream state. The first reachable predecessor/successor copies its state into the destination; later reachable paths call the analysis `join` callback. The queue stores each block at most once at a time.

## Reachability Filter

Before running the work queue, the engine computes CFG reachability from entry. Propagation ignores blocks that are not entry-reachable. This is important for backward analyses: a syntactically unreachable statement may still have an edge to exit, but it must not be treated as a real path from program entry.

CFG therefore connects `return` and `throw` terminators to exit for backward analysis while keeping post-terminator statements entry-unreachable.

## Test Coverage

`tests/parser/test_dataflow_engine.c` covers:
- forward propagation visits a reachable `return` statement and skips the statement after it;
- backward propagation reaches `return` through the exit edge and still skips the entry-unreachable statement after it;
- definite-assignment state propagation keeps a straight-line assignment as `INIT` at CFG exit;
- definite-assignment branch join marks `if (condition) { assignment }` with no `else` as `MAYBE_INIT` at CFG exit.

`tests/parser/test_reference_fact_emission.c` now also covers the first concrete reaching-definitions payload:
- `var seed = 1; seed = 3; seed;` keeps the read fact's `declarationRange` on the declaration token while `definitionRange` resolves to the latest assignment target after `ZrParser_SemanticFacts_ResolveLinearReachingDefinitions`.

`tests/parser/test_compiler_semantic_query_diagnostics.c` covers the first CFG-backed reaching-definitions branch join:
- `var seed; if (flag) { seed = 1; } else { seed = 2; } return seed;` first shows the linear resolver would resolve the final read to the last source-order write, then verifies `ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions` clears that read's single-definition payload because both branch writes reach the join.
- `var seed = 0; while (flag) { seed; seed = 1; } return seed;` verifies loop-carried paths clear single-definition payloads for reads that can see both the initializer and the prior iteration write.
- `var seed = 0; while (true) { seed = 1; break; } return seed;` verifies a constant-true loop with a write-before-break preserves the single reaching write for the final read.

`tests/parser/test_semantic_query.c` covers the first consumer:
- `DefinitionOf` on the read after `seed = 3` returns the matching write fact through `definitionRange`, while the returned fact still exposes the original declaration range.
- `DefinitionsOf` returns both write facts in source order when a read carries two reaching definition ranges, even if the payload was stored in reverse order.
- `Diagnostics` consumes linear definite-assignment resolver output, reporting the read before a write while ignoring the read after the write.

`tests/parser/test_semantic_facts.c` covers the straight-line definite-assignment fact producer:
- a declaration marked `UNINIT` makes the first read `UNINIT`, a later write becomes `INIT`, and the later read stays `INIT`;
- a declaration with no known state leaves the read unannotated as `UNKNOWN` / no-state.
- the CFG-backed resolver marks the read inside `var seed = seed` as `UNINIT` because the declaration initializer has not completed before that read.
- the CFG-backed resolver joins normal and function-exit paths through cloned `finally` blocks, so `try { if (flag) { seed = 1; return seed; } } finally { return seed; }` marks the final read as `MAYBE_INIT`.
- the CFG-backed resolver marks the final read in `var seed; while (true) { seed = 1; break; } return seed;` as `INIT`, proving the constant-true loop no longer contributes a fake uninitialized zero-iteration path.
- the CFG-backed reaching-definitions resolver joins normal and function-exit paths through cloned `finally` blocks, so the same final read clears `hasDefinitionRange` instead of falsely pointing at the guarded `seed = 1` write.
- LSP reaching-definition navigation now requires divergent `if/else` branch writes to return both branch write locations through `DefinitionsOf`.

`tests/parser/test_reference_fact_emission.c` covers the first production write-state payload:
- identifier assignment write facts are emitted with `hasDefiniteAssignmentState` and `INIT`.

`tests/parser/test_compiler_semantic_query_diagnostics.c` also covers the first CFG-backed compiler diagnostic consumer:
- `var seed; if (flag) { seed = 1; } return seed;` publishes `possibly_uninitialized_read` after CFG branch join state is written back to the return read fact.
- `var seed; while (true) { seed = 1; break; } return seed;` publishes no `uninitialized_read` or `possibly_uninitialized_read` diagnostic after the CFG removes the fake zero-iteration path.

`tests/parser/test_cfg_reachability.c` also locks the CFG invariant that `return` terminator blocks connect to exit.

## Limits And Next Steps

The engine does not yet enforce iteration caps or expose diagnostics for analysis bailout. CFG-backed definite assignment now has a first production integration for function/test/script body branch joins, declaration-initializer read ordering, cloned-finally read joins, and constant-true write-before-break loop precision, but broader loop fixed points, remaining finally edge cases, local reanalysis, and richer source-to-statement matching remain open. CFG-backed reaching definitions now handle conservative branch joins, representative loop regressions, cloned-finally read aggregation, a first multi-definition result surface, and deterministic same-source source-order query output, but broader loop fixed points, overload/member-aware ranking, and local recomputation remain pending. Ownership, numeric intervals, and logical facts remain future analyses.
