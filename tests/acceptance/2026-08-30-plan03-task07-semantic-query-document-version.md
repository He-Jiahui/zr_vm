# Plan 03 Task 7.27 Semantic Query Document Version Acceptance

## 验收基线

- 基线 HEAD：`f00d4c5cfa83ba9e114b255adbf0c15123a27fa0`。
- code/test overlay：
  `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.h`、
  `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c`、
  `tests/language_server/test_lsp_semantic_query_parity.c`、
  `tests/language_server/test_lsp_external_member_reference_identity_cases.h`。
- RED：version 1 query 在同一 URI 更新到 version 2 后仍产生 hover、definition、references 与
  highlights，输出分别为 `1/1/2/2`，parity 进程 exit 1。
- GREEN：source query 捕获 version 1；更新到 version 2 后四个 consumer 全部返回 false，结果为空。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44 semantic-query parity 均 13/13、真实 exit 0。
- 三工具链 LSP source-contract 均输出 `PASSED`、真实 exit 0；MSVC
  `VSCMD_VER=17.14.38`。
- GCC/Clang/MSVC project feature suite 均真实 exit 0，三套只保留同一 9 条既有 marker；相对
  Task 7.26 基线新增 marker 0。binary metadata definition/references/highlights 与 descriptor-plugin
  member definition/references/type-member navigation 均恢复 PASS。
- 中间的 full semantic-snapshot 与 provider-generation 两种实现都因新增 6 个 metadata/project
  marker 被否决，不计入 GREEN，也未写入 marker 白名单。
- `git diff --check` 通过；三工具链 9 个 focused executable 均以独立命令直接返回真实 exit 0。

## 状态与产出记录

- 完成时间：2026-08-30 03:43 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：RED/GREEN、document-version exactness、四 consumer stale fail-closed、三工具链
  focused 验证、project marker delta 审计、binary/plugin navigation regression closure。
