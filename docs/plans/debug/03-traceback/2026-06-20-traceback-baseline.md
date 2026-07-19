---
plan_id: debug-03-traceback
record_id: 2026-06-20-traceback-baseline
status: completed
completed_at: 2026-06-20 12:48 +08:00
source_plans:
  - docs/plans/debug/03-traceback-and-errors.md
evidence_scope: historical-baseline
---

# Traceback And Error Baseline

## 可复用结论

- formatted VM traceback, source/function naming and structured error propagation have core/debug tests.
- CLI debug error paths provide end-to-end baseline coverage.

## 证据入口

- `tests/debug`
- `tests/cli/test_cli_debug_e2e.c`

## 未完成边界

Async logical stacks, inlined/tail-call frames, native callbacks, Drop cleanup frames and trimmed AOT symbolization remain open.

