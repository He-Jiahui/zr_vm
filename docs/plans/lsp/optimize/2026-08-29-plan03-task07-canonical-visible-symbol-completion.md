---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/acceptance/2026-08-29-plan03-task07-canonical-visible-symbol-completion.md
doc_type: milestone-detail
---

# Plan 03 Task 7.23 Canonical Visible Symbol Completion

## 目标

- 让 source lexical completion 只消费 `ZrParser_SemanticQuery_VisibleSymbols()`。
- 让 completion item kind、detail 和 callable signature 来自 canonical symbol facts。
- 删除 lexical completion 对 LSP symbol table、request-time inference 与 AST/name reconstruction
  的依赖；receiver/import completion 保持既有 structured 边界。

## 完成项目

- `SZrParserSemanticSymbolQuery` 新增 structured `kind`，`SymbolAt`、`DeclaredSymbols` 与
  `VisibleSymbols` 从同一 symbol record 投影。
- scope-fact producer 发布 extern function/delegate/struct/class/interface/enum，并保持 lexical
  shadowing、generic parameter、import/alias、receiver/static 和稳定排序合同。
- parser semantic display 从 exact SymbolId、closed function TypeId 与 declaration AST identity
  生成 named callable signature；参数 passing/type/return 不由 LSP 拼接。
- 新增 cohesive canonical completion projector。候选缺少 exact SymbolId/declaration identity 时
  省略；TypeId unavailable 时显示 `cannot infer exact type`，不伪造 `object`。
- parity test 在同时脱离 analyzer symbol table 与 document AST 后，仍从 snapshot visible facts
  返回 local、unknown-type local 与 source callable completion。
- source contract 阻止 lexical path 重新调用 legacy analyzer completion/symbol-table helper。

## 验证

- 固定源为 `b730b40 + 14 exact code/test overlays`；WSL 与 Windows snapshot overlay 均通过
  SHA-256 `14/14` 审计。
- GCC 11.4、Clang 14、MSVC 19.44 均通过 parser symbols `21/21`、semantic-query parity
  `10/10`、source contracts、expression hover `11/11` 与 inlay facts `13/13`，真实 exit 0。
- 三工具链 interface 均保持 `111 Pass / 2 Fail`，两个固定 marker 为 class-member navigation
  与 reference-call query diagnostic；本任务四项 completion 回归全部 PASS，marker delta 0。
- 三工具链 project 均保持 `56 Pass / 4 Fail`，marker 集合与计划已接受基线一致，process exit 0。
- 三工具链 full stdio 均在既有 generic fixture `short_circuit_unreachable` 缺失处退出 1，未计
  GREEN；三套 CLI `--version` 均真实 exit 0。

## 状态与产出记录

- 完成时间：2026-08-29 23:35 +08:00。
- 状态：已完成。
- 完成项目：VisibleSymbols lexical completion consumer、structured symbol kind、extern declaration
  scope facts、canonical callable display、detached symbol-table/AST parity、exact-type fail-closed、
  source-contract gate 与三工具链 fixed-snapshot marker 审计。
