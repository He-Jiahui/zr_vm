# Plan 03 Task 7.10 Canonical Inlay Declarations

## 目标

- 为 snapshot-local declaration consumer 发布稳定、精确、有序的 parser
  declaration query。
- 让 inlay hint 只消费 canonical SymbolId、TypeId、declaration fact 与 exact
  declaration range。
- 删除 request-time LSP symbol-table enumeration 与 inference materialization。

## 完成项目

- `SZrParserSemanticSymbolQuery` 发布 snapshot-borrowed `declarationNode`；
  `SymbolAt`、`VisibleSymbols` 与新 `DeclaredSymbols` 保持同一记录身份。
- `ZrParser_SemanticQuery_DeclaredSymbols` 只投影 resolved declaration fact，
  要求 SymbolId、TypeId、AST identity 与 symbol record 精确一致，按 range 与
  SymbolId 稳定排序并去重。
- `ZrLanguageServer_Lsp_FormatCanonicalDeclarationType` 重新核对
  `DeclarationOf` 后格式化 canonical TypeId。
- inlay consumer 不再遍历 `symbolTable->allScopes`，也不在请求期间调用
  `InferExactExpressionType` 补 facts；unresolved/inconsistent declaration
  fail closed。
- parser、LSP、source-contract 和 stdio 测试覆盖无 symbol table 枚举、显式
  annotation 过滤、exact hint range 和 clean shutdown。

## 验证

- GCC/Clang/MSVC parser declared-symbol query：`20 Tests / 0 Failures`，真实
  exit 0。
- GCC/Clang/MSVC focused inlay：`11 Tests / 0 Failures`，真实 exit 0。
- GCC/Clang/MSVC LSP source contracts：`61 Pass / 0 Fail`，真实 exit 0。
- GCC/Clang/MSVC focused stdio inlay smoke：真实 exit 0，clean shutdown。
- 三工具链 full interface 保持 `109 Pass / 4 Fail`，四个既有 marker 完全
  相同、delta 0，因此不计 GREEN。
- GCC full stdio 在进入本 inlay 场景前被既有 generic fixture
  `short_circuit_unreachable` diagnostic 缺失阻断，因此不计 GREEN。

## 状态与产出记录

- 完成时间：2026-08-29 00:40 +08:00。
- 状态：已完成。
- 完成项目：canonical declaration enumeration、exact TypeId formatter、inlay
  consumer migration、三工具链 focused query/inlay/source-contract/stdio 验证。
- 后续边界：full interface 四个既有 marker、full stdio generic short-circuit
  diagnostic、cross-project/binary/native declaration consumers 不属于本项
  GREEN 证据，继续留在 Plan 03 后续任务。
