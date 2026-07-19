---
plan_id: using-05-migration
record_id: 2026-06-18-legacy-plugin-guard-baseline
status: completed
completed_at: 2026-06-18 16:00 +08:00
source_plans:
  - docs/plans/using/02-using-scopes-and-plugin-guards.md
  - docs/plans/using/05-migration-and-phasing.md
evidence_scope: migration-baseline
---

# Legacy Plugin Guard Baseline

## 可复用结论

- existing analysis tracks legacy plugin guard aliases through several expression forms and rejects selected uses across await boundaries.
- project import/version/provider metadata and runtime plugin-loading primitives provide inputs for a future `loadPlugin` result API.

## 证据入口

- `tests/library/test_project_import_resolver.c`
- `tests/parser/test_project_import_canonicalization.c`
- `tests/task/test_task_runtime.c`

## 迁移限定

This is not a target-language completion record. Plugin/import guards leave `using`; the migration tool may reuse alias/escape evidence to propose `loadPlugin(...)` plus result/union control flow, which requires manual review when CFG changes.
