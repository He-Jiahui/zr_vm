---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
  - tests/language_server/test_lsp_interface.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
plan_sources:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
  - tests/acceptance/2026-08-12-lsp-l8-canonical-native-construct-signature-expression-fact.md
doc_type: acceptance_record
plan_id: lsp-01-semantic-core
record_id: 2026-08-12-canonical-native-construct-signature-expression-fact
status: completed
completed_at: 2026-08-12 04:17 +08:00
---

# Canonical Native Construct Signature Expression Fact

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-08-12 04:17 +08:00 | 已完成 | L8 独立 native struct-init signature 合同：`init math.Vector3(...)` 的 `STRUCT_INIT_EXPRESSION` signature help 只消费同节点 exact expression fact、有效 canonical `TypeId` 与精确类型事实 | 本记录、[验收记录](../../../tests/acceptance/2026-08-12-lsp-l8-canonical-native-construct-signature-expression-fact.md) |

## Contract

signature context traversal 将 `ZR_AST_STRUCT_INIT_EXPRESSION` 视为构造调用上下文，
但它不能走 legacy construct 的局部 AST type inference。LSP 必须通过
`ZrParser_SemanticFacts_FindExpressionByNode` 找到同一 AST node 的 expression fact，
并同时要求 `exactness == ZR_SEMANTIC_FACT_EXACT`、非 invalid canonical `TypeId` 与
precise inferred type。

合同成立时，既有 canonical constructor contract 投影 `Vector3` 的 `x: float`、
`y: float`、`z: float` 参数。fact 为 unknown、TypeId invalid，或同节点 fact 缺失时，
signature help 直接不可用。它不调用 `ZrParser_ExpressionType_Infer`，也不以
constructor target、member name、source text 或 project specialized lookup 重建结果。

既有 `ZR_AST_CONSTRUCT_EXPRESSION` 路径不变；它保留原有 legacy constructor 行为。
本叶不修改 parser semantic-fact schema，也不表示 L8 整体完成。

## Evidence

- RED：native `init math.Vector3(4.0, 5.0, 6.0)` 被 parser 表示为
  `STRUCT_INIT_EXPRESSION`，旧 signature-context traversal 不识别该节点，因而无法
  投影构造器签名。
- GREEN：同一 fixture 的 exact fact 返回含 `x`、`y`、`z` 三个参数的签名。
- Fail closed：测试逐项把同节点 fact 置为 `UNKNOWN`、invalid TypeId，并移除同节点
  fact identity；三种状态均返回 no signature help。
- 边界：只有 struct-init 走此 exact-fact gate；legacy construct 不因本叶失去其原有
  fallback 兼容路径。

## Validation Matrix

每条记录的测试进程均以真实退出码 `0` 结束。GCC/Clang 使用隔离 WSL source/build
snapshot；MSVC 使用隔离静态 snapshot。

| Toolchain | Semantic facts | Local query | Interface | Project | expression/local hover | stdio/CLI |
|---|---:|---:|---:|---:|---:|---:|
| GCC | 13/13 | 32/32 | 107/107 | 58/58 | 9/9 and 12/12 | 2/2 |
| Clang | 13/13 | 32/32 | 107/107 | 58/58 | 9/9 and 12/12 | 2/2 |
| MSVC | 13/13 | 32/32 | 107/107 | 58/58 | 9/9 and 12/12 | 2/2 |

本叶未采集 L8 全量 workload 的 p50/p95/p99 或 process peak memory；这些属于仍开放的
L8 full project/protocol matrix，不将 microcase evidence 误记为 milestone 关闭。

## Open Scope

这只完成 L8 的第十一个独立 consumer 合同。其余 local fallback 删除、provider/project
覆盖、性能/内存预算与完整 protocol matrix 仍开放。
