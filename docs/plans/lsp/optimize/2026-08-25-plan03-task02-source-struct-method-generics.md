---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2f: Source Struct Method Generic Scope Facts

## Goal

Publish source struct-method type generic parameters through canonical
`VisibleSymbols` facts, with the method owner identity emitted by the compiler
rather than reconstructed from member spelling or AST scanning in the query.

## Implementation

- The struct canonical-definition pass now registers each exact
  `ZR_AST_STRUCT_METHOD` declaration as a semantic function symbol before the
  scope-fact producer runs.
- The source scope-fact builder creates a method-owned function scope only
  when that canonical symbol exists, then publishes the method generic
  parameters, ordinary parameters, and body scopes beneath it.
- The method scope remains a child of its struct scope, so an enclosing type
  generic is available through published parentage while the method generic is
  unavailable before its own declaration.
- The child deliberately does not register class or interface methods. Those
  compilers do not yet publish an equivalent canonical member SymbolId, so
  inventing one in the query producer would violate the Task 2 contract.

## Contract

For an exact struct method declaration, the compiler creates a stable function
`SymbolId` associated with that declaration AST identity. A type generic owned
by the method carries this owner id, a canonical generic-parameter `TypeId`,
and its exact declaration/definition range. The query sees the candidate only
through the method scope and its published parent chain; it does not discover
methods or generic parameters from names, text, range overlap, or LSP state.

## Verification

- RED: a struct method was absent from `SZrSemanticContext.symbols`, so the
  scope producer had no valid generic owner and the `VisibleSymbols` fixture
  failed closed.
- GCC static: `zr_vm_semantic_query_symbols_test` 10/10,
  `zr_vm_semantic_query_test` 29/29,
  `zr_vm_semantic_query_contract_test` 3/3,
  `zr_vm_compiler_semantic_query_diagnostics_test` 46/46, and compiler
  integration 127/127 with zero failures.
- MSVC static: the same targets passed 10/10, 29/29, 3/3, 46/46, and 127/127
  with zero failures.
- Clang 14 WSL: changed parser and test-source objects compiled. The static
  executable link remains blocked by existing C11 inline unresolved references,
  including `ZrCore_Memory_RawFreeWithType` and `ZrCore_Array_Free`; no Clang
  executable pass is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 13:52:15 +08:00
- 状态：GCC/MSVC 子里程碑完成；Clang source 编译完成但既有 static link ABI
  失败阻断 executable gate，不宣称三工具链完成。
- 完成项目：struct method canonical SymbolId、method-scope generic fact、
  declaration-before-use、enclosing type-generic parentage、compiler
  integration regression、module 文档和 acceptance evidence。
- 后续项目：const/class/interface method generic producer、type member、
  imports/aliases、receiver member、binary/native parity，以及 Task 2 的 LSP
  consumer 迁移。
