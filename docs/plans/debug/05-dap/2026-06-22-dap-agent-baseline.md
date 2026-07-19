---
plan_id: debug-05-dap
record_id: 2026-06-22-dap-agent-baseline
status: completed
completed_at: 2026-06-22 04:23 +08:00
source_plans:
  - docs/plans/debug/05-dap-agent-enhancements.md
evidence_scope: historical-baseline
---

# DAP Agent Baseline

## 可复用结论

- DAP agent/protocol, breakpoint, stack, scope, variable and expression-diagnostic paths have focused tests.
- snapshot pagination and protocol serialization remain reusable presentation layers.

## 证据入口

- `tests/debug/test_debug_agent.c`
- `tests/debug/test_debug_agent_protocol.c`
- `tests/debug/test_debug_expression_diagnostics.c`

## 未完成边界

Breakpoints and stepping must be rebound to revisioned source/DebugMap identities; Place availability and optimized AOT locations cannot be reconstructed by the DAP layer.

