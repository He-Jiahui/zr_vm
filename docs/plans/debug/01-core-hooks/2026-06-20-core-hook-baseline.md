---
plan_id: debug-01-core-hooks
record_id: 2026-06-20-core-hook-baseline
status: completed
completed_at: 2026-06-20 04:38 +08:00
source_plans:
  - docs/plans/debug/01-core-hook-fixes.md
evidence_scope: historical-baseline
---

# Core Hook Baseline

## 可复用结论

- COUNT/LINE hook dispatch, hook setters/getters, stack activation and masked debug info queries have focused coverage.
- hook trap propagation and observer coexistence were verified on representative VM paths.

## 证据入口

- `tests/debug/test_debug_hook_core.c`
- `tests/debug/test_debug_trace.c`
- `tests/acceptance/2026-06-20-debug-phase1-core-hooks.md`

## 未完成边界

Target hooks must bind to stable DebugMap/ExecIR locations and AOT native-PC maps; bytecode-only completion is not inferred.

