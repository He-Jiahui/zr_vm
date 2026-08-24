# Acceptance: Plan 03 Task 2.2a Visible Symbols

## Acceptance Surface

The parser query accepts a semantic snapshot, a position, an optional query
scope, explicit inclusion options, and an output array of
`SZrParserSemanticSymbolQuery` values. The snapshot must already contain
canonical scope and visible-symbol facts; neither this query nor an LSP
consumer may reconstruct them from source text.

## Required Evidence

- A nested local shadows its same-namespace outer local.
- A later non-hoisted declaration is absent before its declaration position.
- Generic parameters remain visible, and members of one overload set remain
  together.
- Receiver members, imports/aliases, and inaccessible candidates are opt-in.
- Static context rejects instance receiver members even when receiver members
  are requested.
- Returned ordering is scope distance, declaration order, then `SymbolId`.

`tests/parser/test_semantic_query_symbols.c` covers all of these conditions.
It creates `SZrSemanticScopeFact` and `SZrSemanticVisibleSymbolFact` rows
directly and asserts stable `SymbolId` results. The test therefore cannot pass
through a language-server symbol table, AST scan, or member-name fallback.

## Execution Evidence

- MinGW GCC static: symbols 4/4, semantic query 29/29, contract 3/3, and
  compiler semantic diagnostics 46/46, all exit 0.
- Fresh MSVC static: symbols 4/4, semantic query 29/29, contract 3/3, and
  compiler semantic diagnostics 46/46, all exit 0.
- WSL Clang 14 compiled the changed parser units, but existing C11 inline ABI
  behavior prevented the test target from linking. It is recorded as a
  toolchain blocker and is not counted as a green acceptance result.

## 状态与产出记录

- 完成时间: 2026-08-24 09:31:35 +08:00
- 状态: GCC/MSVC 验收通过；Clang 链接阻塞保留，未将本子里程碑标记为三工具链全绿。
- 完成项目: scope ancestry、visible candidate filtering、lexical shadowing、overload/static context、稳定排序和 fail-closed 输出复用。
- 未完成项目: source/binary/native producer 接线与 LSP consumer migration，均属于 Plan 03 Task 2 后续阶段。
