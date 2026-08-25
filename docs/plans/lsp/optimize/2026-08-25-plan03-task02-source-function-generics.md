---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2e: Source Function Generic Scope Facts

## Goal

Publish source free-function generic parameters through the canonical
`VisibleSymbols` fact pipeline, without query-side or language-server name,
range, or AST fallback.

## Implementation

- The source scope-fact producer now publishes free-function generic parameters
  before ordinary value parameters.
- Each type parameter receives a canonical generic-parameter `TypeId` from the
  owning function `SymbolId` and declaration ordinal. Its visible fact carries
  the exact parameter `SymbolId`, name range, and function scope identity.
- The public query now selects a descendant containing scope ahead of any
  ancestor, regardless of malformed or overlapping parser-range widths. Range
  width remains the deterministic tie-breaker only for unrelated candidates.
- Const generic parameters remain omitted because their value identity has no
  canonical visible-symbol contract in this task.

## Contract

A free-function type generic is unavailable before its declaration token. At
and after that token it is materialized only from the function scope and has a
stable canonical parameter `SymbolId`, generic-parameter `TypeId`, exact owner
function `SymbolId`, and exact declaration/definition range. The query walks
published parentage and facts only; it does not infer generic scope by name,
source text, AST traversal, or range coincidence.

## Verification

- RED: `fn identity<T>(value: T): T` initially returned no `T` at the generic
  declaration because the overlapping module scope had a numerically narrower
  range than the descendant function scope.
- GCC static: `zr_vm_semantic_query_symbols_test` 9/9,
  `zr_vm_semantic_query_test` 29/29,
  `zr_vm_semantic_query_contract_test` 3/3, and
  `zr_vm_compiler_semantic_query_diagnostics_test` 46/46, all exit 0.
- MSVC static: the same four targets passed 9/9, 29/29, 3/3, and 46/46 with
  process exit 0.
- Clang 14 WSL: changed parser and test-source objects compiled. The static
  executable link remains blocked by existing C11 inline unresolved references,
  including `ZrCore_Memory_RawFreeWithType` and `ZrCore_Array_Free`; no Clang
  executable pass is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 13:32:38 +08:00
- 状态：GCC/MSVC 子里程碑完成；Clang source 编译完成但既有 static link ABI
  失败阻断 executable gate，不宣称三工具链完成。
- 完成项目：source free-function generic parameter canonical identity、
  function-scope visible fact、ancestor/descendant scope 选择、
  declaration-before-use regression、module 文档和 acceptance evidence。
- 后续项目：const/method generic producer、type member、imports/aliases、
  receiver member、binary/native parity，以及 Task 2 的 LSP consumer 迁移。
