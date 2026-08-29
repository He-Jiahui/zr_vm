---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_external_member_reference_identity_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_project_features.c
  - tests/acceptance/2026-08-30-plan03-task07-external-member-reference-identity.md
doc_type: milestone-detail
---

# Plan 03 Task 7.26 External Member Reference Identity

## 目标

- 外部 type-member query 与候选都拥有 declaration identity 时，只按 declaration URI/range
  判断是否属于同一成员。
- identity 不一致时 fail closed，禁止继续按 module/type/member display name 聚合引用。
- 保留双方都没有 declaration identity 时的 structured metadata contract 边界，避免破坏
  native/binary declaration-unavailable consumer。

## 执行

1. RED 使用 `LinkedList<int>.addLast` 的两个 native receiver usage，先证明原始 query 能得到两个
   references，再只篡改 query declaration range。旧 matcher 仍按名称返回 usage，parity 第 12 项
   精确失败。
2. GREEN 将 declaration identity 判断置于 spelling 判断之前。任一侧 `hasDeclaration` 时，必须
   双方都有非空 declaration URI 且 URI/range 完全相同。
3. `includeDeclaration=false` 隔离 usage matching，避免 query 自身声明位置掩盖 fail-closed 结果。
4. project suite 验证 binary/native references 与 highlights 继续通过；固定 marker 做三工具链集合
   对照，不把 marker 计为 GREEN。

## 状态与产出记录

- 完成时间：2026-08-30 02:53 +08:00。
- 状态：已完成。
- 完成项目：external type-member exact declaration matcher、mismatched identity fail-closed RED/GREEN、
  native repeated-usage parity、三工具链 parity/source-contract/project marker 审计、模块合同更新。
- 后续边界：Task 7.25 imported source function 的 parser `SymbolAt` declaration range 仍为零，等待
  Syntax05 释放 producer 路径；本阶段未增加 imported module/member 名称兼容。
