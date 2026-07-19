---
plan_id: aot-07-codegen
record_id: 2026-07-19-typed-register-codegen-baseline
status: completed
completed_at: 2026-07-19 03:40 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
evidence_scope: historical-baseline
---

# Typed Register Codegen Baseline

## 可复用结论

- 代表性 typed scalar local、constant、arithmetic、bitwise、control-flow 和 typed direct-call 路径已有纯 C lowering 与 shared-library smoke 覆盖。
- AOT 已具备 method metadata、typed helper、GC root frame、reference-local 与边界 marshaling 的分层实现入口。
- 已有 construction/profile guardrail 能检测部分 `SZrValue` 回退与值构造退化。

## 证据入口

- `tests/parser/test_aot_c_typed_scalar.c`
- `tests/parser/test_aot_c_typed_call_contracts.c`
- `tests/parser/test_aot_c_control_contracts.c`
- `tests/parser/test_aot_c_guardrail_contracts.c`
- `tests/acceptance/2026-07-18-aot-07-s7-typed-loop-performance-stage-acceptance.md`

## 不继承的完成声明

旧 `07-S*` 状态不等于新 AOT codegen 完成。新路径必须从 Semantic IR/Place/CFG lowering，不能从 bytecode 或 AST shape 继续扩张模板特例。

