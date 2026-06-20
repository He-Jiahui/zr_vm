---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - tests/parser/test_semantic_query.c
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
plan_sources:
  - user: 2026-06-20 参照 docs/plans/lsp 优化语义推断能力
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_semantic_query.c
  - tests/acceptance/2026-06-20-semantic-stage1-semantic-query.md
doc_type: module-detail
---

# Semantic Query API Foundation

## Purpose

`semantic_query.h` is the Stage 1 public parser query surface for shared semantic facts. It gives compiler/LSP/debug/REPL callers a stable entrypoint before concrete dataflow analyses are wired in.

## Query Scope

`SZrParserSemanticQueryScope` currently supports:
- module scope, which accepts all facts in the `SZrSemanticContext`;
- node scope, which filters facts and query positions by the root node source range.

Node scope is a range filter only. It does not yet trigger local analysis, rebuild facts, or understand lexical parentage beyond the AST node's `location`.

## Query Functions

`ZrParser_SemanticQuery_TypeAt` finds the narrowest expression fact at a position and copies its `SZrInferredType` into the caller-owned output value. Missing facts, invalid context, invalid output, or an out-of-scope position return `ZR_FALSE`.

`ZrParser_SemanticQuery_ReferencesOf` scans reference facts by `symbolId`, applies the optional query scope, and appends borrowed `const SZrSemanticReferenceFact *` entries to the caller-owned `SZrArray`. If the output array is already initialized, the query clears its length before appending so reused arrays do not keep stale references.

`ZrParser_SemanticQuery_DefinitionOf` finds the reference fact at a position. A declaration reference returns itself; a resolved read/write/call/member reference scans declaration facts by `symbolId`, then falls back to matching `declarationRange`.

`ZrParser_SemanticQuery_FactsAt` aggregates facts available at a position into `SZrParserSemanticQueryFacts`: expression, reference, numeric, reachability, logical, and ownership. Numeric facts are found by position because the lower-level fact API only exposed by-node lookup.

`ZrParser_SemanticQuery_Diagnostics` currently returns a valid empty diagnostic list. It exists to stabilize the API shape required by the LSP plan; real structured diagnostics will be connected once parser semantic diagnostics are represented as queryable context data.

## Test Coverage

`tests/parser/test_semantic_query.c` covers:
- `TypeAt` picks the narrowest expression fact and copies the inferred type;
- `FactsAt` aggregates expression, numeric, reachability, logical, and ownership facts;
- `DefinitionOf` resolves a read reference to a declaration fact with the same `symbolId`;
- `ReferencesOf` collects declaration/read/write facts for a symbol inside scope and clears reused output arrays before a miss;
- node scope accepts in-range facts and rejects facts outside the root range;
- `Diagnostics` returns an empty foundation list instead of crashing.

The test target is `zr_vm_semantic_query_test`, registered in `tests/CMakeLists.txt` and included in `language_pipeline`.

## Limits And Next Steps

The API does not yet expose local re-analysis, real structured diagnostics, overload/member-specific definition ranking, or lexical scope ancestry. Those should be added with definite assignment, reaching definitions, and LSP incremental inference slices.
