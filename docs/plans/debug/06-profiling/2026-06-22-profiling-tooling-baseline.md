---
plan_id: debug-06-profiling
record_id: 2026-06-22-profiling-tooling-baseline
status: completed
completed_at: 2026-06-22 07:19 +08:00
source_plans:
  - docs/plans/debug/06-profiling-and-tooling.md
  - docs/plans/debug/07-testing-and-acceptance.md
evidence_scope: historical-baseline
---

# Profiling And Tooling Baseline

## 可复用结论

- execution/profile counters, debug protocol timing and CLI/tooling acceptance fixtures provide an observable baseline.
- separate trusted and sandboxed debug surfaces support capability-oriented redesign.

## 证据入口

- `zr_vm_core/include/zr_vm_core/profile.h`
- `tests/debug`
- `tests/cli/test_cli_debug_e2e.c`

## 未完成边界

Target profiling requires stable MethodId/TypeId/ModuleId, AOT native-PC attribution, sampling-safe metadata, bounded overhead and explicit security budgets.

