---
plan_id: lsp-03-robustness
record_id: 2026-06-20-position-robustness-baseline
status: completed
completed_at: 2026-06-20 16:01 +08:00
source_plans:
  - docs/plans/lsp/03-lsp-robustness-and-position.md
evidence_scope: historical-baseline
---

# Position And Robustness Baseline

## 可复用结论

- LSP has UTF position conversion, document discovery, partial semantic query and malformed request handling baselines.
- project fixtures cover nested/ambiguous discovery and source/binary language-feature matrices.

## 证据入口

- `tests/fixtures/projects/lsp_discovery_ambiguous`
- `tests/fixtures/projects/lsp_discovery_nested`
- `tests/fixtures/projects/lsp_language_feature_matrix`
- `tests/language_server`

## 未完成边界

Revisioned immutable snapshots, cancellation, dependency invalidation by Canonical ModuleId, cache budgets and p50/p95/p99 gates remain target work.
