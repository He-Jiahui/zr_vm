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

# Plan 03 Task 2.2d: Source Type Generic Scope Facts

## Goal

Publish source type generic parameters through the canonical `VisibleSymbols`
fact pipeline, without any query-side or language-server name, range, or AST
fallback.

## Implementation

- The source scope-fact producer now handles type generic parameters from
  `struct`, `class`, and `interface` declarations.
- Each type parameter receives a canonical generic-parameter `TypeId` derived
  from its owning type `SymbolId` and declaration ordinal. The producer then
  registers an exact parameter `SymbolId` from the parameter AST node and
  publishes a type-scope visible-symbol fact with the exact name range.
- The public query selects a descendant scope when its range ties the current
  innermost scope. It does not use publication order to prefer equal-width
  siblings.
- Const generic parameters are intentionally not projected by this child: they
  have value semantics and no existing canonical TypeId contract. The producer
  omits them rather than inventing a type-like identity.

## Contract

A visible source type generic candidate carries its canonical parameter
`SymbolId`, generic-parameter `TypeId`, owning type `SymbolId`, and exact
declaration/definition range. It becomes available at its declaration token,
is visible only through the owning type scope, and is absent before the
declaration or in sibling type scopes. `VisibleSymbols` only materializes the
published fact and the matching registry record; it does not discover generic
parameters by spelling, source range, or AST walking.

## Verification

- RED: the initial `struct Box<T>` fixture published the parameter fact but
  returned zero `T` candidates because equal-width module and type scopes chose
  the outer module.
- GCC static: `zr_vm_semantic_query_symbols_test` 8/8,
  `zr_vm_semantic_query_test` 29/29,
  `zr_vm_semantic_query_contract_test` 3/3, and
  `zr_vm_compiler_semantic_query_diagnostics_test` 46/46, all exit 0.
- MSVC static: the same four targets passed 8/8, 29/29, 3/3, and 46/46 with
  process exit 0.
- Clang 14 WSL: the changed parser and test-source objects compiled. The static
  test executable link remains blocked by existing C11 inline unresolved
  references, including `ZrCore_Memory_RawFreeWithType` and
  `ZrCore_Array_Free`; no Clang executable pass is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 13:19:39 +08:00
- 状态：GCC/MSVC 子里程碑完成；Clang source 编译完成但既有 static link ABI
  失败阻断 executable gate，不宣称三工具链完成。
- 完成项目：source struct/class/interface type generic parameter canonical
  identity、type-scope visible fact、等宽嵌套 scope 选择、declaration-before-use
  与 sibling isolation regression、module 文档和 acceptance evidence。
- 后续项目：const/function/method generic producer、type member、imports/aliases、
  receiver member、binary/native parity，以及 Task 2 的 LSP consumer 迁移。
