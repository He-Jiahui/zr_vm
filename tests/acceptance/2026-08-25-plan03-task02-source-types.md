---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_environment_types.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-source-types.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.2c Source Type Scope Facts

## Scope

Validate that source `struct`, `class`, and `interface` declarations reach
`ZrParser_SemanticQuery_VisibleSymbols` through canonical type-symbol facts,
without a language-server, text, range, or AST lookup fallback.

## Baseline

The regression starts from a compiler-generated semantic context containing
`Point`, `Meter`, `Readable`, and a later `probe` function. Before this child
milestone, type registration omitted the declaration AST identity and a module
visible-symbol query at `probe` returned zero candidates for all three types.

## Test Inventory

`tests/parser/test_semantic_query_symbols.c` adds
`test_visible_symbols_projects_source_type_declarations`. It compiles the
fixture, queries the source position of `probe`, and requires exactly one
visible candidate for each type declaration.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| GCC static | symbols, query, contract, compiler diagnostics | 7/7, 29/29, 3/3, 46/46; all exit 0 |
| MSVC static | symbols, query, contract, compiler diagnostics | 7/7, 29/29, 3/3, 46/46; all exit 0 |
| Clang 14 WSL static | parser and focused test source compilation | Compiled; executable link blocked by existing C11 inline ABI unresolved references beginning at `ZrCore_Memory_RawFree` |

## Results

The visible candidates now retain the canonical type `SymbolId` and exact
declaration range produced at type registration. Scope publication looks up that
record only by declaration-node identity and omits it when no exact record
exists. The query does not widen this to type-name or range matching.

## Acceptance Decision

Accepted for the GCC and MSVC source-type producer submilestone. Clang
executable acceptance is not claimed because the static-link ABI failure is
outside this child write set. This is not Task 2 completion: generic parameters,
type members, imports/aliases, receiver members, binary, native, and LSP
consumer migration remain open.

## 状态与产出记录

- 完成时间：2026-08-25 12:44:45 +08:00
- 状态：GCC/MSVC acceptance passed；Clang executable gate 被既有 static
  link ABI failure 阻断，未计入通过声明。
- 完成项目：source type canonical identity、module/type scope projection、
  focused compiler-backed regression、Task 2 acceptance evidence。
- 后续项目：补齐其余 source、artifact 与 native visible-symbol producer，
  然后迁移 LSP consumer 并执行 Task 2 最终门禁。
