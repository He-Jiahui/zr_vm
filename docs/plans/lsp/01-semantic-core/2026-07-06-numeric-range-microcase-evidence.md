---
plan_id: lsp-01-semantic-core
record_id: 2026-07-06-numeric-range-microcase-evidence
status: completed
completed_at: 2026-07-06 06:34 +08:00
source_plans:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: consolidated-historical-tests
---

# Numeric Range Microcase Evidence

## 可复用结论

The old plan accumulated extensive parser/LSP paired tests for arithmetic, bitwise, branch, loop and wrapper range facts across GCC, Clang and MSVC focused runs. The reusable result is the paired-test pattern: every semantic fact producer has a parser query test and an LSP consumer query test.

## 证据入口

- `tests/parser/test_numeric_range_inference.c`
- `tests/parser/test_numeric_branch_refinement.c`
- `tests/parser/test_dataflow_engine.c`
- `tests/language_server/test_lsp_numeric_semantic_query.c`
- `tests/acceptance/2026-06-20-semantic-stage1-semantic-query.md`

## 清除内容

Hundreds of expression-shape-specific progress rows are intentionally not migrated. They are regression tests, not a future inference architecture. Target plans require general transfer functions and fixed-point CFG facts.
