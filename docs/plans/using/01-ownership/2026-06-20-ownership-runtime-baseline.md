---
plan_id: using-01-ownership
record_id: 2026-06-20-ownership-runtime-baseline
status: completed
completed_at: 2026-06-20 18:56 +08:00
source_plans:
  - docs/plans/using/01-ownership-as-generics.md
evidence_scope: historical-baseline
---

# Ownership Runtime Baseline

## 可复用结论

- runtime 已有 unique/shared/weak control blocks、move/detach/release/upgrade/borrow-like entry points and GC visibility hooks。
- parser semantic facts 已有 ownership kind、region/lifetime identity 与部分 move/use diagnostics载体。
- metadata signatures 已能携带部分 ownership wrapper identity。

## 证据入口

- `zr_vm_core/include/zr_vm_core/ownership.h`
- `tests/language_server/test_ownership_diagnostics.c`
- `tests/parser/test_dataflow_engine.c`

## 不继承的完成声明

旧 `%unique/%shared/%weak/%borrow/%loan` surface、运行时置 null move 和 concrete ownership type-name dispatch 全部被 syntax 01/02/04 替代。记录只证明可迁移的 runtime primitives 存在。

