---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/acceptance/2026-08-30-plan03-task07-canonical-source-hover.md
doc_type: milestone-detail
---

# Plan 03 Task 7.24 Canonical Source Hover

## 目标

- 让 source-local symbol hover 只消费 parser `SymbolAt` 的 stable SymbolId、TypeId、signature 与
  exact reference range。
- 删除 hover consumer 对 analyzer hover inference、token-name lookup 和 symbol-table semantic
  fallback 的依赖。
- 保持 leading comment 与 FFI decorator metadata 为非语义补充，不覆盖 canonical facts。

## 完成项目

- `SZrParserSemanticSymbolQuery` 新增 exact `referenceRange`；`SymbolAt` 与 `DeclaredSymbols` 从
  resolved reference/declaration fact 直接投影。
- 新增 cohesive `lsp_canonical_hover.c/.h` projector：callable 显示 canonical signature，普通
  symbol 格式化 exact TypeId，unavailable TypeId 明确 fail closed。
- local semantic query 先调用 `ZrParser_SemanticQuery_SymbolAt()`，复制 snapshot-borrowed query
  view，并使用 resolved reference fact range；不再调用 analyzer `GetHoverInfo` 或 token-name
  symbol-table lookup。
- detached parity 在移除 analyzer symbol table、reference tracker 与 AST 后仍构建 local hover；
  source-contract 阻止旧 semantic hover 与 identifier fallback 回流。
- extern delegate/struct/enum/interface kind 由 canonical declaration AST 投影；source extern 与
  pack/align/underlying/value 等 FFI metadata 继续来自结构化 AST/symbol metadata 补充层。

## 验证

- GCC 11.4、Clang 14、MSVC 19.44 的 semantic-query parity 均 11/11；parser SymbolAt query 均
  `21 Tests / 0 Failures / 0 Ignored`；GCC/Clang source contracts 输出 `PASSED`，MSVC Task724
  原缓存 source-contract 真实 exit 0。
- 三工具链 interface 均为 `111 Pass / 2 Fail`。仅保留既有 class-member navigation 与
  reference-call query diagnostic marker；extern type/layout hover 全部 PASS，marker delta 0。
- MSVC 工作树 cache 因并行 benchmark 会话暂列但未创建的 performance source 无法重生成；
  验收另用 `HEAD + 10 Task 7.24 code/test overlays` 隔离快照构建 interface/parser，避免消费
  其他会话脏文件。
- full stdio 的既有 generic `short_circuit_unreachable` blocker 未变化，本子里程碑不声明
  full stdio GREEN。

## 状态与产出记录

- 完成时间：2026-08-30 01:45 +08:00。
- 状态：已完成。
- 完成项目：SymbolAt exact reference range、canonical source-hover projector、detached analyzer
  parity、legacy hover/name fallback 删除、extern structured metadata parity、三工具链 focused 与
  interface marker 审计。
