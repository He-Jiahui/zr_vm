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
- 任一侧缺少完整 declaration identity 时均 fail closed；LSP 不得以 module/type/member
  display name 推断同一成员。

## 执行

1. RED 使用 `LinkedList<int>.addLast` 的两个 native receiver usage，先证明原始 query 能得到两个
   references，再只篡改 query declaration range。旧 matcher 仍按名称返回 usage，parity 第 12 项
   精确失败。
2. GREEN 要求双方都有非空 declaration URI 且 URI/range 完全相同；缺失任一 identity
   时直接返回 unavailable。
3. `includeDeclaration=false` 隔离 usage matching，避免 query 自身声明位置掩盖 fail-closed 结果。
4. 追加 query identity 清空回归，证明 references/highlights 不再通过同名、模块名或 owner
   type 名匹配；project suite 验证已有 exact declaration 的 binary/native references 与
   highlights 继续通过。

## 状态与产出记录

- 完成时间：2026-08-30 02:53 +08:00。
- 状态：已完成。
- 完成项目：external type-member exact declaration matcher、mismatched/missing identity
  fail-closed RED/GREEN、native repeated-usage parity、三工具链 parity/source-contract
  focused 验证、模块合同更新。
- 后续边界：Task 7.25 imported source function 的 parser `SymbolAt` declaration range 仍为零，等待
  Syntax05 释放 producer 路径；本阶段未增加 imported module/member 名称兼容。

- 补充完成时间：2026-08-30 10:26 +08:00。
- 补充状态：已完成 declaration identity fail-closed 收口；缺失 identity 不再进入任何
  name/type/module matcher。GCC/Clang/MSVC parity 与 source-contract 真实 exit 0；整体
  Plan 03 Task 7/Task 8 仍因 producer/fixture 与 semantic-token ownership 未通过。
- 补充完成项目：新增 missing declaration identity references/highlights regression，删除
  external type-member matcher 的 member/module/owner-type name fallback，并重放三工具链
  focused consumer gate。
