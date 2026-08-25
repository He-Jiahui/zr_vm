---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2h: Source Const Generic Scope Facts

## Goal

Publish source `const` generic parameters through canonical `VisibleSymbols`
facts. A const generic must retain the exact owner and canonical generic
identity; it must not be synthesized from the `const` token, its name, or its
integer annotation by a query or LSP consumer.

## Implementation

- The source scope-fact publisher now accepts the parser-owned
  `ZR_GENERIC_PARAMETER_CONST_INT` kind in addition to ordinary type generic
  parameters.
- Both kinds continue through the same canonical owner/ordinal interning path,
  exact parameter `SymbolId` registration, declaration range publication, and
  declaration-before-use filtering.
- No query schema, LSP code, name matching, or type-text reconstruction was
  added. Unsupported generic kinds still fail closed.

## Contract

For `struct Matrix<const N: int>`, the candidate for `N` is published only in
the `Matrix` type scope after its declaration. It has `Matrix`'s exact
canonical owner `SymbolId` and a canonical generic-parameter `TypeId`. The
consumer observes that identity through the fact collection; the annotation
does not become a substitute symbol identity.

## Verification

- RED: the source fixture compiled but `VisibleSymbols` returned zero `N`
  candidates at the declaration because the scope producer accepted only
  `ZR_GENERIC_PARAMETER_TYPE`.
- GCC static: symbols 12/12, semantic query 29/29, query contract 3/3,
  compiler diagnostics 46/46, and compiler integration 127/127 with zero
  failures and real process exit zero.
- MSVC static: the same targets passed 12/12, 29/29, 3/3, 46/46, and 127/127
  with zero failures and real process exit zero.
- Clang 14 WSL: changed parser and test-source objects compiled. The static
  executable link remains blocked by existing C11 inline unresolved references,
  including `ZrCore_Memory_RawFreeWithType` and `ZrCore_Array_Free`; no Clang
  executable pass is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 14:23:25 +08:00
- 状态：GCC/MSVC 子里程碑完成；Clang source 编译完成但既有 static link ABI
  失败阻断 executable gate，不宣称三工具链完成。
- 完成项目：const generic canonical scope projection、exact owner/type
  identity、declaration-before-use、compiler integration regression、module
  文档和 acceptance evidence。
- 后续项目：interface method generic producer、type member、imports/aliases、
  receiver member、binary/native parity，以及 Task 2 的 LSP consumer 迁移。
