# Semantic Stage 1 Dataflow Engine

## Scope
- Start the Stage 1 `dataflow.c/.h` slice from `docs/plans/lsp/01-semantic-inference-core.md`.
- Affected layers: parser CFG, generic dataflow engine, parser semantic test registration, and semantic documentation.

## Baseline
- CFG could emit reachability facts, but no generic work-queue engine existed for forward/backward analyses.
- Function-exiting terminators also lacked an explicit exit edge, which made backward analysis from exit unable to reach return/throw statements.

## Test Inventory
- `tests/parser/test_dataflow_engine.c`
  - forward dataflow visits reachable statements and skips a statement after `return`;
  - backward dataflow reaches `return` through the exit edge and still skips the entry-unreachable statement after it;
  - definite-assignment state propagation keeps a straight-line assignment `INIT` at CFG exit;
  - definite-assignment state propagation joins `if (condition) { assignment }` with no `else` as `MAYBE_INIT` at CFG exit.
- `tests/parser/test_cfg_reachability.c`
  - locks that `return` terminator blocks connect to CFG exit.

## Tooling Evidence
- Windows MSVC focused build was used because the current WSL checkout still has unrelated stuck work.
- RED 1: `zr_vm_dataflow_engine_test` failed to compile because `dataflow.h` did not exist.
- GREEN 1: after adding `dataflow.h/.c`, forward dataflow passed 1 focused test.
- RED 2: backward dataflow first visited both `return` and the unreachable statement after it, failing with visit count `2`.
- GREEN 2: after adding entry-reachability filtering, backward dataflow passed and the focused target reported 2 PASS.
- RED 3: CFG return-exit invariant failed because return blocks had zero successors.
- GREEN 3: after connecting `return`/`throw` terminators to exit, `zr_vm_cfg_reachability_test` reported 5 PASS.

## Results
- Windows MSVC `zr_vm_dataflow_engine_test`: 4 PASS after the definite-assignment supplement.
- Windows MSVC `zr_vm_cfg_reachability_test`: 5 PASS.
- Windows MSVC `zr_vm_semantic_facts_test`: 4 PASS.
- Windows MSVC `zr_vm_expression_fact_emission_test`: 28 PASS.
- Windows MSVC CMake target `zr_vm_semantic_facts_test`: 8 PASS after fixing CFG-backed reaching-definitions cloned-finally read aggregation.
- Windows MSVC CMake target `zr_vm_compiler_semantic_query_diagnostics_test`: 5 PASS after adding reaching-definitions loop regressions.
- Windows MSVC CMake target `zr_vm_semantic_facts_test`: 9 PASS after removing the fake zero-iteration path from constant-true loops for definite-assignment reads.
- Windows MSVC CMake target `zr_vm_compiler_semantic_query_diagnostics_test`: 6 PASS after suppressing the false `possibly_uninitialized_read` diagnostic for write-before-break constant-true loops.
- WSL was not claimed GREEN because the latest process scan still showed unrelated stuck shared-checkout `find` and AOT `cc1` tasks.

## Reaching Definitions Supplement
- Scope: first concrete reaching-definitions payload for straight-line reference facts, not full CFG join analysis.
- RED: `zr_vm_reference_fact_emission_test` first failed to compile because `ZrParser_SemanticFacts_ResolveLinearReachingDefinitions`, `SZrSemanticReferenceFact.hasDefinitionRange`, and `SZrSemanticReferenceFact.definitionRange` did not exist.
- GREEN: `SZrSemanticReferenceFact` now carries `definitionRange` / `hasDefinitionRange`; declarations initialize their own definition range, identifier writes initialize to the write token, and `ZrParser_SemanticFacts_ResolveLinearReachingDefinitions` annotates read facts with the latest preceding declaration/write for the same resolved symbol.
- Test: `Reference Facts Resolve Linear Reaching Definition To Latest Write` covers `var seed = 1; seed = 3; seed;`, preserving the read fact's `declarationRange` on the declaration token while resolving `definitionRange` to the assignment target.
- Consumer: `ZrParser_SemanticQuery_DefinitionOf` now reads that payload for resolved read facts and returns the matching write fact before declaration fallback.
- Evidence: Windows MSVC targets in `build\agent-msvc-tests` Debug report `zr_vm_reference_fact_emission_test` 6 PASS and `zr_vm_semantic_query_test` 9 PASS for the first consumer.

## CFG-Backed Reaching Definitions Supplement
- Scope: first CFG-backed reaching-definitions producer for branch joins, focused on avoiding false single-definition results after divergent writes. This is not yet loop fixed-point precision or a multi-definition result surface.
- RED: `zr_vm_compiler_semantic_query_diagnostics_test` first failed to link because `ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions` did not exist.
- GREEN: `semantic_reaching_definitions.c` now maps resolved declaration/read/write facts to symbol slots, runs forward dataflow over script/function/test body CFGs, keeps a read's `definitionRange` when a single declaration/write reaches it, and clears `hasDefinitionRange` when different branch writes reach the same read.
- Test: `test_compile_script_cfg_reaching_definitions_rejects_divergent_branch_writes` compiles an `if/else` pair that writes `seed` in both branches. It verifies the linear resolver would point the final read to the else write, then verifies the CFG-backed resolver clears the read's single-definition payload.
- Consumer: LSP local-symbol definition queries now run the CFG-backed resolver when analyzer AST is available, so divergent branch writes fall back to the declaration instead of one branch write.
- Evidence: Windows MSVC targets in `build\agent-msvc-tests` Debug report `zr_vm_compiler_semantic_query_diagnostics_test` 3 PASS and `zr_vm_language_server_reaching_definition_navigation_test` 2 PASS. The focused adjacent regression also reports `zr_vm_semantic_query_test` 11 PASS, `zr_vm_semantic_facts_test` 5 PASS, `zr_vm_reference_fact_emission_test` 6 PASS, `zr_vm_dataflow_engine_test` 4 PASS, and `zr_vm_language_server_semantic_query_diagnostics_test` 1 PASS.

## CFG-Backed Reaching Definitions Finally And Loop Supplement
- Scope: cloned-finally read aggregation and representative loop regressions for the CFG-backed reaching-definitions producer. This is not yet a multi-definition result surface or refined ranking model.
- RED: `zr_vm_semantic_facts_test` added an AST+fact-level `try { if (flag) { seed = 1; return seed; } } finally { return seed; }` reaching-definitions fixture and failed because the final read still carried the guarded write as a single `definitionRange`.
- Diagnosis: structural parent statements could match child facts by whole source range, and the same source read cloned into multiple finally CFG paths used last-write-wins mutation instead of joining all seen read definitions.
- GREEN: `semantic_reaching_definitions.c` now excludes structural parent statements (`block`, `catch`, `switch default`, `try`) from whole-range fact matching, accumulates read definition slots by reference fact index during dataflow, and applies the joined read slot after CFG convergence.
- Tests: `test_cfg_reaching_definitions_clears_finally_read_from_normal_and_return_paths` requires the final `return seed` read to clear `hasDefinitionRange`; compiler diagnostics coverage also now includes loop-carried reads and a constant-true loop with write-before-break.
- Evidence: Windows MSVC targets in `build\agent-msvc-tests` Debug report `zr_vm_semantic_facts_test` 8 PASS and `zr_vm_compiler_semantic_query_diagnostics_test` 5 PASS. Focused adjacent regression also reports `zr_vm_semantic_query_test` 11 PASS, `zr_vm_dataflow_engine_test` 4 PASS, `zr_vm_reference_fact_emission_test` 6 PASS, `zr_vm_language_server_reaching_definition_navigation_test` 2 PASS, and `zr_vm_language_server_semantic_query_diagnostics_test` 1 PASS.

## Definite Assignment Supplement
- Scope: first definite-assignment state lattice helper and dataflow first-arrival propagation semantics; this is not yet full symbol mapping, read diagnostics, or compiler/LSP publication.
- RED 1: `zr_vm_dataflow_engine_test` first failed to compile because `dataflow_definite_assignment.h` did not exist.
- GREEN 1: `dataflow_definite_assignment.h/.c` now provide `UNINIT`, `INIT`, and `MAYBE_INIT` state arrays plus init/get/set/join helpers; branch joins collapse mixed states to `MAYBE_INIT`.
- RED 2: the straight-line assignment test failed with `Expected 1 Was 2`, showing that the generic engine joined the first predecessor state against zero-initialized destination state and produced `MAYBE_INIT`.
- GREEN 2: forward and backward propagation now copy the state on first reachability and only call `join` for later paths.
- Evidence: Windows MSVC targets in `build\agent-msvc-tests` Debug report `zr_vm_dataflow_engine_test` 4 PASS, `zr_vm_semantic_query_test` 9 PASS, `zr_vm_reference_fact_emission_test` 6 PASS, `zr_vm_semantic_facts_test` 4 PASS, and `zr_vm_expression_fact_emission_test` 28 PASS.

## CFG-Backed Definite Assignment Initializer Order Supplement
- Scope: first declaration-initializer transfer ordering fix for the CFG-backed definite-assignment producer, focused on reads inside a variable's own initializer. This is not yet compiler diagnostic publication for self-initializer source because the current compiler path already reports a compile error before query diagnostic publication.
- RED: `zr_vm_semantic_facts_test` added an AST+fact-level `var seed: int = seed` fixture and failed with the initializer read annotated as `INIT` instead of `UNINIT`.
- GREEN: `semantic_definite_assignment.c` now treats variable declaration facts as `UNINIT` while the initializer expression transfers, and only sets the declaration slot to `INIT` after the initializer has completed.
- Test: `test_cfg_definite_assignment_marks_self_initializer_read_uninit` parses a function body, appends declaration/read reference facts for the declaration pattern and initializer identifier, runs `ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments`, and requires the initializer read fact to be `UNINIT`.
- Evidence: Windows MSVC targets in `build\agent-msvc-tests` Debug report `zr_vm_semantic_facts_test` 6 PASS and adjacent `zr_vm_compiler_semantic_query_diagnostics_test` 3 PASS.

## CFG-Backed Definite Assignment Finally Join Supplement
- Scope: first cloned-finally read-state join for the CFG-backed definite-assignment producer. This covers duplicated source reads created when CFG construction builds separate normal/function-exit finally paths; it is not yet loop fixed-point precision, local recomputation, or exhaustive finally diagnostics.
- RED: `zr_vm_semantic_facts_test` added an AST+fact-level `try { if (flag) { seed = 1; return seed; } } finally { return seed; }` fixture and failed because the final read was annotated as `INIT` instead of `MAYBE_INIT`.
- Diagnosis: the source `try` statement's whole range matched child body write/read facts before CFG entered the cloned finally blocks, so the normal path was polluted by the guarded write. Repeated cloned finally reads also needed per-fact accumulation rather than last-write-wins fact mutation.
- GREEN: `semantic_definite_assignment.c` now excludes structural parent statements (`block`, `catch`, `switch default`, `try`) from whole-range fact matching, accumulates read states by reference fact index during dataflow, and applies the joined read state after the CFG run converges.
- Test: `test_cfg_definite_assignment_joins_finally_read_from_normal_and_return_paths` parses the function body, appends synthetic declaration/write/finally-read reference facts, runs `ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments`, and requires the final `return seed` read to be `MAYBE_INIT`.
- Evidence: Windows MSVC target in `build\agent-msvc-tests` Debug reports `zr_vm_semantic_facts_test` 7 PASS for the focused GREEN run.

## CFG-Backed Definite Assignment Loop Exit Supplement
- Scope: first constant-true loop-exit precision fix for the CFG-backed definite-assignment producer. This covers fake zero-iteration exits from `while(true)`, `for(...; true; ...)`, and omitted-condition `for(;;)`; it is not yet a full loop fixed-point or nontermination proof.
- RED: `zr_vm_semantic_facts_test` added an AST+fact-level `var seed; while (true) { seed = 1; break; } return seed;` fixture and failed because the final read was annotated as `MAYBE_INIT` instead of `INIT`.
- RED: `zr_vm_compiler_semantic_query_diagnostics_test` added the same source through `compile_script` and failed because a `possibly_uninitialized_read` diagnostic was still published.
- GREEN: `cfg_loops.c` now adds the loop-header-to-join zero-iteration edge only when a condition is unknown or can be false. Known-true `while`/traditional `for` loops and omitted-condition `for` loops exit through explicit loop-control edges such as `break`.
- Tests: `test_cfg_definite_assignment_preserves_true_loop_break_write` requires the final read to be `INIT`; `test_compile_script_suppresses_true_loop_break_definite_assignment_diagnostic` requires no uninitialized-read diagnostics; `test_cfg_connects_unconditional_for_break_to_loop_join` locks the omitted-condition `for` break path.
- Evidence: Windows MSVC targets in `build\agent-msvc-tests` Debug report `zr_vm_cfg_reachability_test` 29 PASS, `zr_vm_semantic_facts_test` 9 PASS, and `zr_vm_compiler_semantic_query_diagnostics_test` 6 PASS.

## Linear Definite Assignment Fact Resolution Supplement
- Scope: first straight-line reference-fact producer for definite-assignment read states, not full CFG branch/loop analysis.
- RED: `zr_vm_semantic_facts_test` first failed to compile because `ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments` did not exist.
- GREEN: `ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments` now walks resolved reference facts in order, treats explicitly marked declarations as initial state, treats writes as `INIT`, and annotates reads with the latest known same-symbol state. Unknown declarations keep reads as `UNKNOWN` / no-state.
- RED: `zr_vm_reference_fact_emission_test` then failed with `Expected TRUE Was FALSE` when asserting emitted identifier write facts already carry definite-assignment state.
- GREEN: identifier write reference fact emission now pre-fills `INIT`, so write facts carry usable assignment-state payload before resolver output is consumed.
- Consumer: `ZrParser_SemanticQuery_Diagnostics` consumes those resolved read states through the existing definite-assignment diagnostics bridge.
- Evidence: Windows MSVC targets in `build\agent-msvc-tests` Debug report `zr_vm_semantic_facts_test` 5 PASS, `zr_vm_semantic_query_test` 11 PASS, and `zr_vm_reference_fact_emission_test` 6 PASS for the first producer/consumer pair.

## Acceptance Decision
- Accepted for Stage 1 generic dataflow engine skeleton.
- Accepted for the first linear reaching-definitions fact payload.
- Accepted for the first CFG-backed reaching-definitions branch-join producer and LSP declaration fallback for divergent writes.
- Accepted for CFG-backed reaching-definitions cloned-finally read aggregation and structural parent statement matching fix.
- Accepted as regression coverage for representative CFG-backed reaching-definitions loop cases.
- Accepted for the first definite-assignment lattice helper and dataflow first-arrival copy semantics.
- Accepted for the first CFG-backed definite-assignment declaration-initializer ordering fix.
- Accepted for the first CFG-backed definite-assignment cloned-finally read join and structural parent statement matching fix.
- Accepted for the first CFG-backed definite-assignment constant-true loop write-before-break precision fix.
- Accepted for the first straight-line definite-assignment fact resolver and diagnostic consumer.
- Stage 1 remains open for broader CFG-backed definite-assignment loop fixed points, remaining finally edge cases and local recomputation, broader CFG-backed reaching-definition loop fixed points, multi-definition result surfaces, CFG expansion, and compiler/LSP diagnostic integration beyond the current semantic query bridges.
