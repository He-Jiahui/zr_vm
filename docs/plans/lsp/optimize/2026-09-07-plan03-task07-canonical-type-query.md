---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
plan_sources:
  - docs/plans/lsp/optimize/index.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/astra/lsp/review.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_exact_type_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
doc_type: milestone-record
---

# Plan 03 Task 7.63: Canonical Type Query Fail-Closed

## Goal

让 `ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition` 只投影 parser
semantic snapshot 中已经发布的 canonical type facts。请求期不再通过 AST
遍历、compiler type inference、符号表或 declared-type builder 补造类型。

## Contract

- 查询必须持有当前 `SZrSemanticContext`，并调用
  `ZrParser_SemanticQuery_CanonicalTypeAt`。
- canonical `TypeId` 必须存在于同一 snapshot 的 canonical type graph。
- 精确 expression fact 必须携带相同的 `TypeId`、`EXACT` exactness 和 precise
  `SZrInferredType`，才能复制到输出。
- type reference 必须是 `ZR_SEMANTIC_REFERENCE_TYPE` 且 `isResolved`，然后按同一
  `TypeId` 取得 precise semantic type record；缺失或过期记录直接失败。
- query 返回的 fact、reference 和 canonical node 都是 snapshot 借用视图；函数只把
  inferred type 复制给调用方，不保存这些指针。调用方负责释放输出类型。
- approximate、unknown、invalid、stale 或 identity-mismatched facts 均 fail closed。
  `semantic_find_type_node_at_position`、`FindExpressionNodeAtPosition`、
  `InferExactExpressionType`、`GetSymbolAt` 和 `BuildDeclaredTypeInferredType` 不属于
  request-time type query。

## Implementation

- `ResolveTypeAtPosition` 改为 canonical query、canonical graph existence 和
  exactness/resolution checks 的单一路径。
- expression facts 直接复制 parser 已发布的 precise type；type references 通过
  canonical `TypeId` 对应的 semantic type record 复制，保留 ownership qualifier 和
  structural generic fields。
- 删除只服务于旧 type-position symbol fallback 的私有名字查找 helper。
- 新增 semantic analyzer regression：把纯字面量表达式 fact 标为 approximate，并在
  AST/compiler state 仍可用时断开 analyzer semantic context；旧实现会返回 `int`，
  新实现必须失败。
- 新增 source-contract，锁定 canonical query、canonical node 和 TypeId record bridge，
  并禁止 AST/inference/symbol/builder fallback。

## Verification

- GCC WSL build：
  `cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --target zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_lsp_source_contracts_test --parallel 4`
  完成并链接成功。
- GCC semantic analyzer runner 真实 exit 1，新增
  `Semantic Analyzer Type Resolution Rejects Approximate Expression Fact` 真实 PASS；
  其余失败与既有 analyzer baseline 一致。
- GCC LSP interface runner 真实 exit 1，receiver canonical expression 和 ownership
  type annotation 的相关用例真实 PASS；其余失败保持既有 baseline。
- GCC LSP source-contract runner 真实 exit 0，包含新的 `ResolveTypeAtPosition` 契约。
- Clang ASan/UBSan 同三目标完成重编译；semantic analyzer 新用例真实 PASS，source
  contract 真实 exit 0，完整 analyzer/interface runner 的既有失败仍按 baseline 记录。

## 状态与产出记录

- 开始时间：2026-09-07 05:10 +08:00。
- 完成时间：2026-09-07。
- 状态：Plan 03 Task 7.63 focused 子里程碑完成；Plan 03 Task 7、Task 3、Task 8
  及完整跨工具链矩阵仍进行中。
- 完成项目：移除 `ResolveTypeAtPosition` 的 request-time AST/type-inference fallback；
  建立 canonical expression/type-reference projection；补充 RED/GREEN 回归和 source
  contract。
- 源码版本：基于 `313df917` 的当前工作树及本子里程碑提交。
- 产出路径：`semantic_analyzer.c`、semantic analyzer exact-type regression、LSP
  source-contract、类型查询模块文档与本记录。
- 未完成项目：其余 Task 7 consumer、Task 3 sourceless relation/generation matrix、
  source/binary/native/stale/unresolved 完整矩阵、Task 8 的 16-target 与三套
  stdio/CLI smoke。
