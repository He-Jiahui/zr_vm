---
plan_id: aot-02-type-layout
record_id: 2026-06-24-typed-layout-baseline
status: completed
completed_at: 2026-06-24 23:59 +08:00
source_plans:
  - docs/plans/aot/02-typed-value-and-layout.md
  - docs/plans/aot/08-generic-sharing.md
evidence_scope: historical-baseline
---

# Typed Layout Baseline

## 可复用结论

- 运行时已有 `SZrTypeLayout`、size/alignment、field offset、GC/ownership offset 与 copy/drop 分类载体。
- AOT 已能为代表性 closed inline struct 和泛型值实例发布 layout，并在解释器/AOT 之间验证代表性结果一致。
- layout identity 已进入 metadata token/runtime 链，可作为 Canonical TypeLayout 的迁移输入。

## 证据入口

- `tests/parser/test_aot_c_type_layout_contracts.c`
- `tests/parser/test_value_type_runtime.c`
- `tests/module/test_metadata_runtime_type_layout.c`
- `tests/module/test_metadata_runtime_typespec_layout.c`

## 不继承的完成声明

这些证据不证明新的 Canonical TypeId、`ref struct`、Span、destination-first `init`、部分构造 cleanup 或完整 copy/move/drop matrix 已完成。新计划必须按 syntax 01/03/04 重新验收。

