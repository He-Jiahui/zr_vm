---
plan_id: lsp-02-diagnostics
record_id: 2026-07-19-structured-diagnostic-baseline
status: completed
completed_at: 2026-07-19 03:41 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: historical-baseline
---

# Structured Diagnostic Baseline

## 可复用结论

- parser/language-server paths support structured diagnostic code/message/range, related semantic queries and selected fix payloads.
- focused tests exist for duplicate definitions, type mismatch, ownership diagnostics, reachability and local scope navigation.

## 证据入口

- `tests/parser/test_semantic_query.c`
- `tests/language_server/test_lsp_semantic_query_diagnostics.c`
- `tests/language_server/test_ownership_diagnostics.c`
- `tests/language_server/stdio_diagnostic_fix_smoke.js`

## 未完成边界

Target causal FactId chains, stable diagnostic registry, migration edits and Canonical Place/borrow diagnostics remain open until syntax 01/02/06 gates pass.
