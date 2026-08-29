---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/acceptance/2026-08-30-plan03-task07-canonical-local-navigation-identity.md
doc_type: milestone-detail
---

# Plan 03 Task 7.29 Canonical Local Navigation Identity

## 目标

- 已成功解析的 canonical local query 必须能只凭 copied SymbolId 投影 definition、references 与
  document highlights，不要求继续持有 `SZrSymbol *`。
- 同一 projector 的 declaration/reference relation 查询共享一个 SymbolId，invalid identity 统一
  fail closed。
- project navigation 既有 `AppendReferencesForSymbol` 兼容入口与跨项目名称聚合保持不变，等待
  Task 7.25 producer identity 后再迁移。

## 执行

1. RED 扩展局部变量 parity case：解析后保存 `canonicalSymbol.symbolId`，置空 `query.symbol`，再请求
   definition/references/highlights。parser facts 仍为 3 条、2 条 use，但三个 projector 全部返回空，
   parity 为 13 Pass/1 Fail、真实 exit 1。
2. GREEN 为 reference projector 增加内部 SymbolId 入口；公共 project `ForSymbol` wrapper 仅提取
   既有 symbol id。definition fallback、references 与 highlights 都优先消费 copied canonical id。
3. version 1 返回 `1/3/3`，version 2 返回 `1/4/4`；置空 raw symbol 后结果保持不变。把 copied id
   改为 invalid 后 references 返回 false 且不产生部分输出。
4. fixed interface A/B 暴露三类 producer 差异：closed-generic type use 无 canonical view；extern
   function 为 canonical/raw `10/3`；web URI local 为 `2/1`。最终 consumer 只在 raw pointer 缺失或
   两个 id 一致时使用 copied id；identity 冲突继续保留旧路径并记录为 producer 阻塞。

## 状态与产出记录

- 完成时间：2026-08-30 04:34 +08:00。
- 状态：已完成。
- 完成项目：raw-symbol-detached RED/GREEN、definition/reference/highlight copied SymbolId projection、
  invalid-id fail-closed、三工具链 parity/source-contract/interface/project marker 审计、模块合同更新。
- 后续边界：extern/web URI SymbolId mismatch 与 closed-generic missing canonical view 必须由 parser
  producer 收口；Task 7.25 imported declaration identity、source-local rename 和 project name aggregation
  仍未完成。
