---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_external_member_reference_identity_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_project_features.c
  - tests/acceptance/2026-08-30-plan03-task07-semantic-query-document-version.md
doc_type: milestone-detail
---

# Plan 03 Task 7.27 Semantic Query Document Version

## 目标

- 已解析的 source semantic query 只能由产生它的文档版本消费。
- hover、definition、references 与 document highlights 共享同一个版本门禁。
- stale query 必须 fail closed，不得继续读取借用的 analyzer、canonical fact 或 metadata view。
- binary/plugin declaration URI 没有 source document version 时维持既有 provider consumer 合同，
  不从虚拟文档或名称重建版本身份。

## 执行

1. RED 在 version 1 的 `LinkedList<int>.addLast` 上解析 external type-member query，再把同一 URI
   更新为 version 2。旧实现仍产生 hover、1 个 definition、2 个 references 和 2 个 highlights，
   parity 新增第 13 项精确失败、进程 exit 1。
2. GREEN 在每个成功解析出口完成 provider/project 读取后捕获 `SZrFileVersion.version`，避免请求内
   惰性 metadata 初始化污染 identity。
3. 四个公共 consumer 在生成任何结果前比较当前版本；版本不一致统一返回 false，输出保持为空。
4. 审计拒绝了两种过宽实现：完整 semantic snapshot validation 会误伤既有 project dependency
   marker；全局 provider generation 会误伤 provider reload consumer。本片最终只约束 query URI
   自身的 source document version。
5. 三工具链 project suite 对照固定 marker 集；新增的 binary/plugin navigation marker 必须为零，
   不通过扩大 marker 白名单取得 GREEN。

## 状态与产出记录

- 完成时间：2026-08-30 03:43 +08:00。
- 状态：已完成。
- 完成项目：stale-query RED/GREEN、source document version capture、四 consumer 统一 fail-closed、
  binary/plugin no-version 边界、三工具链 parity/source-contract/project marker 审计、模块合同更新。
- 后续边界：Task 7.25 imported source function identity 仍等待 parser producer；本阶段未修改
  Syntax05 property/interface 路径，也未增加 imported module/member 名称回退。
