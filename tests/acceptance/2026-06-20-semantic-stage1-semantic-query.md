# Semantic Stage 1 Semantic Query API

## Scope
- Add the Stage 1 `semantic_query.h` foundation from `docs/plans/lsp/01-semantic-inference-core.md`.
- Affected layers: parser public API, semantic query implementation, parser semantic test registration, and semantic documentation.

## Baseline
- `SZrSemanticContext` already stored expression, reference, numeric, reachability, logical, and ownership facts.
- Callers had to know the individual fact APIs directly; there was no shared parser-level query facade for LSP/debug/REPL.

## Test Inventory
- `tests/parser/test_semantic_query.c`
  - `TypeAt` returns the narrowest matching expression type.
  - `FactsAt` aggregates expression, numeric, reachability, logical, and ownership facts at a position.
  - `DefinitionOf` maps a resolved read reference to a matching declaration reference by `symbolId`.
  - `ReferencesOf` collects scoped references for a symbol and clears reused output arrays before a miss.
  - node scope filters out facts outside the root node range.
  - `Diagnostics` returns a valid empty foundation list.

## Tooling Evidence
- Windows MSVC focused build was used because the current WSL checkout still has unrelated stuck work.
- RED: `zr_vm_semantic_query_test` failed to compile because `zr_vm_parser/semantic_query.h` did not exist.
- RED: after adding `ReferencesOf`, reused output-array coverage first failed because a missing symbol query kept stale references from the previous query.
- GREEN: after adding output clearing in `ReferencesOf`, the focused target built and reported 7 PASS.
- WSL focused retry against `build/codex-semantic-wsl-gcc-debug` timed out during CMake regenerate/build with no test output; the remaining self-started CMake/Ninja process group was terminated, and a follow-up scan still showed unrelated AOT/core compile/link tasks in the shared checkout.

## Results
- Windows MSVC `zr_vm_semantic_query_test`: 7 PASS.
- Windows MSVC `zr_vm_reference_fact_emission_test`: 5 PASS.
- Windows MSVC `zr_vm_semantic_facts_test`: 4 PASS.
- WSL was not claimed GREEN because the focused retry timed out before tests ran and the latest shared-checkout scan still showed unrelated AOT/core compile/link work.

## Acceptance Decision
- Accepted for Stage 1 semantic query API foundation.
- Stage 1 remains open for local re-analysis, real structured diagnostics, concrete dataflow analyses, CFG expansion, and compiler/LSP diagnostic integration.
