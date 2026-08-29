# Plan 03 Task 7.28 Unresolved Value Exactness Acceptance

## 验收基线

- 基线 HEAD：`a7d3a7634aa9c6344a156a4634c4aad477e77883`。
- code/test overlay：
  `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c`、
  `tests/language_server/test_lsp_semantic_query_parity.c`。
- RED：局部变量的 parser reference fact 被标记为 unresolved 后，LSP symbol table 仍有同名 symbol；
  旧 `ResolveAtPosition` 返回 local target，parity 为 13 Pass/1 Fail、真实 exit 1。
- GREEN：同一 non-TYPE reference 直接 fail closed，query kind 保持 NONE；parity 为 14/14。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44 semantic-query parity 均 14/14、真实 exit 0。
- 三工具链 LSP source-contract 均输出 `PASSED`、真实 exit 0；MSVC 使用
  `14.44.35207` compiler toolset。
- 三工具链 full interface 均为 111 Pass/2 个固定 marker，parent/overlay marker delta 为 0；
  既有 marker 是 class member navigation 和 reference-call diagnostic，不计 GREEN。
- 三工具链 project suite 均真实 exit 0，保留相同 51 Pass/9 个既有 marker；新增 marker 0。
- 过宽的 all-reference fail-closed overlay 令 closed-generic type display 新增 1 marker，已否决并撤销；
  unresolved TYPE producer 作为后续 support-first 边界保留。
- `git diff --check` 通过；GCC/Clang 使用固定 source snapshot，MSVC 使用独立 snapshot/build cache。

## 状态与产出记录

- 完成时间：2026-08-30 04:09 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：精确 RED、non-TYPE unresolved fail-closed、三工具链 focused 验证、interface/project
  marker delta 审计、TYPE producer 阻塞边界复核。
