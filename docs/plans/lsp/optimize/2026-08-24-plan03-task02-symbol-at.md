# Plan 03 Task 2.1: Canonical SymbolAt

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-24 03:41 +08:00 | completed (Task 2.1) | 在 `semantic_query.h` 发布 `SZrParserSemanticSymbolQuery` 与 `ZrParser_SemanticQuery_SymbolAt`；新查询仅投影已解析的 canonical `SZrSemanticReferenceFact`，返回 SymbolId、TypeId、role、声明/定义范围与借用的 display/signature。未解析、缺失或无有效 SymbolId 的 fact 会清零输出并返回 false，禁止名称、AST 或 token fallback。当前 fact schema 未发布 owner identity，成功结果明确返回 invalid `ownerSymbolId`。新增 parser Unity 测试和 CMake target，并在 GCC 4.8.3、Clang 19.1.5、MSVC 19.44.35228 上验证新增 `2/2`、semantic query `29/29`、query contract `3/3`、compiler semantic diagnostics `46/46`，全部真实进程 exit 0。 |

## 完成边界

- 本次实现 Plan 03 Task 2 的 `SymbolAt` 子里程碑，不关闭整个 Task 2。
- `VisibleSymbols` 继续等待 canonical producer 发布 lexical scope/shadowing、owner/access/static context、imports/aliases、generic parameters 与 declaration order facts。
- 在上述事实可用前，LSP 和 query 层均不得从 symbol table 名称、token 扫描或 AST pairing 重建 visible-symbol 结果。

## 产物

- Parser API and implementation:
  `zr_vm_parser/include/zr_vm_parser/semantic_query.h` and
  `zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c`.
- Regression test and registration:
  `tests/parser/test_semantic_query_symbols.c` and `tests/CMakeLists.txt`.
- Module contract and acceptance evidence:
  `docs/parser-and-semantics/semantic-query-api-foundation.md` and
  `tests/acceptance/2026-08-24-plan03-task02-symbol-at.md`.
