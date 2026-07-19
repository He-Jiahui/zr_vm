---
plan_id: lsp-01-semantic-core
record_id: 2026-06-20-semantic-fact-query-baseline
status: completed
completed_at: 2026-06-20 23:59 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/parser-and-semantics/semantic-fact-layer.md
evidence_scope: historical-baseline
---

# Semantic Fact Query Baseline

## 可复用结论

- parser exports position/range indexed semantic facts, SymbolId/TypeId-like identities, definition/reference links, numeric ranges and CFG/dataflow query entry points.
- language server already consumes shared parser queries for hover, inlay, diagnostics and navigation in selected paths.
- revision-aware semantic query tests and source/binary metadata paths provide a base for the target snapshot model.

## 证据入口

- `tests/parser/test_semantic_query.c`
- `tests/parser/test_semantic_facts.c`
- `tests/language_server/test_lsp_expression_fact_hover.c`
- `tests/language_server/test_lsp_inlay_semantic_facts.c`

## 未完成边界

Current identities are not yet the Canonical TypeId/PlaceId/BlockId graph from syntax 01. LSP-specific inference and AST-pointer identity must be removed rather than treated as completed architecture.
