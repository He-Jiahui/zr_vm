# Plan 03 Task 2.2a: Scope Facts and Visible Symbols

## Scope

This sub-milestone implements the parser-side fact model and read-only query
needed for `VisibleSymbols`. It deliberately does not add an LSP fallback and
does not claim that source, binary, or native analyzers already publish these
facts for every workspace.

## Delivered Contract

- `SZrSemanticScopeFact` records snapshot-scoped lexical parentage, kind,
  owner, range, and static context.
- `SZrSemanticVisibleSymbolFact` records the already-resolved candidate
  identity, visibility, ordering, ranges, owner, import/alias, receiver, and
  generic-parameter attributes.
- `ZrParser_SemanticQuery_VisibleSymbols` finds the narrowest containing
  parser scope and follows only its parent chain. It never scans an AST or
  reconstructs resolution from names.
- The query returns `SZrParserSemanticSymbolQuery` values in deterministic
  `scopeDistance`, `declarationOrder`, `SymbolId` order. Its display pointers
  are borrowed from the semantic snapshot.
- Shadowing preserves overload set members, separates the type namespace from
  the value namespace, honors hoisting, and applies explicit receiver,
  import/alias, inaccessible, and static-context filters.

## Boundaries

- The global symbol registry is consulted only after a visible-symbol fact has
  selected a `SymbolId`. It is not a visibility or name-resolution fallback.
- Empty, unscoped, invalid, or out-of-query-scope requests fail closed and
  clear a correctly typed reused output array.
- This slice does not modify `zr_vm_language_server`, LSP tests, or LSP plans.
  LSP migration is deferred until the parser producers cover source, `.zro`,
  and native descriptors with the same facts.

## Verification

- MinGW GCC static: `zr_vm_semantic_query_symbols_test` 4/4;
  `zr_vm_semantic_query_test` 29/29;
  `zr_vm_semantic_query_contract_test` 3/3; and
  `zr_vm_compiler_semantic_query_diagnostics_test` 46/46.
- Fresh MSVC static: the same four targets passed 4/4, 29/29, 3/3, and 46/46
  with real process exit code 0.
- WSL Clang 14 compiled `semantic.c` and `semantic_query_symbols.c` in both
  Debug and Debug plus `-O2` builds, but could not link the test executable.
  The pre-existing Clang C11 `ZR_FORCE_INLINE` branch emits non-static inline
  helpers without out-of-line definitions; the linker reports unresolved core
  array and memory helpers. This is an infrastructure blocker, not a passing
  Clang result, and is outside this exact path set.

## 状态与产出记录

- 完成时间: 2026-08-24 09:31:35 +08:00
- 状态: 子里程碑实现完成；GCC/MSVC 验收通过；Clang 工程链接门禁阻塞，未计为三工具链通过。
- 完成项目: parser-owned scope/candidate facts、`VisibleSymbols` 查询、shadowing/overload/static/filter TDD、模块 API 文档和本记录。
- 后续: 由 source、`.zro`、native descriptor producer 发布同构 scope/candidate facts，再迁移 LSP completion 等 consumer；Plan 03 Task 2 仍进行中。
