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
  - backward dataflow reaches `return` through the exit edge and still skips the entry-unreachable statement after it.
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
- Windows MSVC `zr_vm_dataflow_engine_test`: 2 PASS.
- Windows MSVC `zr_vm_cfg_reachability_test`: 5 PASS.
- Windows MSVC `zr_vm_semantic_facts_test`: 4 PASS.
- Windows MSVC `zr_vm_expression_fact_emission_test`: 28 PASS.
- WSL was not claimed GREEN because the latest process scan still showed unrelated stuck shared-checkout `find` and AOT `cc1` tasks.

## Acceptance Decision
- Accepted for Stage 1 generic dataflow engine skeleton.
- Stage 1 remains open for concrete dataflow analyses, CFG expansion, `semantic_query.h`, and compiler/LSP diagnostic integration.
