---
plan_id: debug-02-introspection
record_id: 2026-06-20-introspection-baseline
status: completed
completed_at: 2026-06-20 10:53 +08:00
source_plans:
  - docs/plans/debug/02-introspection-api.md
evidence_scope: historical-baseline
---

# Introspection API Baseline

## 可复用结论

- stack/local/upvalue inspection APIs and DAP snapshot consumers have representative tests.
- variable child-shape and closure identity support can be adapted to Canonical Place/value locations.

## 证据入口

- `tests/debug/test_debug_metadata.c`
- `tests/debug/test_debug_variable_child_shape.c`
- `tests/debug/test_debug_agent.c`

## 未完成边界

Inline struct/ref struct/Span, moved/uninitialized Place state, ownership wrappers, pool guards and optimized-out locations require the new inspection model.

