---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_canonical_symbol_display.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_canonical_symbol_display.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_canonical_symbol_display.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
plan_sources:
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_native_construct_receiver_fact_cases.h
doc_type: acceptance_record
plan_id: lsp-01-semantic-core
record_id: 2026-08-11-canonical-native-construct-receiver-expression-fact
status: completed
completed_at: 2026-08-11 15:09 +08:00
---

# Canonical Native Construct Receiver Expression Fact

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-08-11 15:09 +08:00 | 已完成 | L8 独立 native source construct receiver 合同：native member projection 只消费 receiver-prefix AST node 的精确 expression fact 与 canonical TypeId；缺失、unknown 或 invalid TypeId 一律 fail closed | 本记录、[验收记录](../../../tests/acceptance/2026-08-11-lsp-l8-canonical-native-construct-receiver-expression-fact.md) |

## Contract

`init math.Vector3(...).y` 的 cursor 先定位 receiver-prefix AST node，再以
`ZrParser_SemanticFacts_FindExpressionByNode` 读取该节点的事实。仅
`ZR_SEMANTIC_FACT_EXACT` 和有效 canonical `TypeId` 可以进入
`ZrParser_CanonicalType_Format` 并驱动 native descriptor member lookup。

不得按 range 选择事实：构造调用范围还会覆盖外层 member 的 `double` fact。
不得调用 `ExpressionType_Infer`、按 member name、文本或 AST 重推类型。构造后的
member chain 以 `memberIndex > 0` 识别为派生 receiver，同样必须拥有精确事实；
只有直接普通标识符 receiver 保留既有解析路径。project receiver resolver 未变更，
因为它有独立的 canonical property 合同。

## Evidence

- RED: 在基线 AST fallback 中，使构造 receiver 的 exact fact unavailable 后，
  native `.y` 仍可解析，证明 AST fallback 不满足合同。
- GREEN: 接口测试分别覆盖 `UNKNOWN + valid TypeId` 与 `EXACT + invalid TypeId`。
- GREEN: 构造派生 member chain 缺失 receiver-prefix expression fact 时关闭解析。
- 复验快照为 `3333d4a + 本合同五个 LSP overlay paths`；根工作树 HEAD 为
  `38bac74`。没有修改 parser query schema 或 semantic fact 数据结构。

## Validation Matrix

每个命令均以真实进程退出码 `0` 结束，日志 failure marker 为零。

| Toolchain | Semantic facts | Local hover | Local query | Interface | Project | stdio/CLI |
|---|---:|---:|---:|---:|---:|---:|
| GCC | 13/13 | 12/12 | 32/32 | 105/105 | 58/58 | 2/2 |
| Clang | 13/13 | 12/12 | 32/32 | 105/105 | 58/58 | 2/2 |
| MSVC | 13/13 | 12/12 | 32/32 | 105/105 | 58/58 | 2/2 |

## Open Scope

这只完成 L8 的一个独立 native construct receiver consumer，不表示 L8 完成。
project AST inference、其余 provider fallback 删除和完整 project/protocol matrix 仍开放。
`language_server_stdio_inline_value_semantic_smoke` 的 computed-member payload 失败已在
旧验证 binary 上复现，是本合同之外的既有基线缺口；本记录不把 33-test CTest
集合宣称为通过。
