# Plan 03 Task 7.29 Canonical Local Navigation Identity Acceptance

## 验收基线

- 基线 HEAD：`8a8908c1916c9c44ca82db6b98a311003d83694a`。
- code/test overlay：
  `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c`、
  `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c`、
  `tests/language_server/test_lsp_semantic_query_parity.c`。
- RED：query 已有 copied SymbolId=3；置空 raw `query.symbol` 后 definition/references/highlights 均为空，
  parity 为 13 Pass/1 Fail、真实 exit 1。
- GREEN：同一 detached query 返回 definition 1、references 3、highlights 3；version 2 分别为
  1/4/4，parity 14/14。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44 semantic-query parity 均 14/14、真实 exit 0。
- 三工具链 LSP source-contract 均输出 `PASSED`、真实 exit 0；MSVC 使用
  `14.44.35207` compiler toolset。
- 三工具链 full interface 均为 111 Pass/2 个固定 marker；project suite 均真实 exit 0，保留
  51 Pass/9 个相同既有 marker；两组 marker delta 都为 0。
- GDB support-first 审计确认 extern function canonical/raw SymbolId 为 `10/3`、web URI local 为
  `2/1`；过宽的 canonical-only draft分别产生 2 个新 marker，已否决，不计 GREEN。
- closed-generic type use 没有 canonical view，仍使用旧边界；本片没有新增 name、token 或 type-text
  fallback，也没有修改 marker whitelist。
- `git diff --check` 通过；GCC/Clang 使用固定 source snapshot，MSVC 使用独立 snapshot/build cache。

## 状态与产出记录

- 完成时间：2026-08-30 04:34 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：detached raw symbol RED/GREEN、copied SymbolId navigation、invalid-id negative case、
  producer mismatch GDB 审计、三工具链 focused 与 marker delta 验证。
