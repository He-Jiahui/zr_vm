---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2i: Source Interface Method Generic Scope Facts

## Goal

Publish source interface-method signature type generics through canonical
`VisibleSymbols` facts. Each candidate must be owned by the exact
compiler-registered signature symbol, not reconstructed from an interface
member name, source text, or range overlap.

## Implementation

- The interface compiler invokes the shared
  `compiler_type_member_register_function_symbol` helper before appending an
  interface method signature member. The helper resolves the canonical owner
  `TypeId` and records the exact signature declaration AST as a function
  semantic symbol.
- The scope-fact producer accepts
  `ZR_AST_INTERFACE_METHOD_SIGNATURE` together with source struct and class
  methods. A function scope is published only when the exact registered symbol
  exists; it projects the signature generic and ordinary parameters without
  inventing a body scope.
- The signature scope is below its interface type scope, preserving visible
  enclosing type generics through published parentage and declaration-before-use
  for the method generic.

## Contract

For `interface Readable<T> { fn echo<U>(value: U): U; }`, the compiler
publishes a function `SymbolId` tied to the exact method-signature AST and its
canonical interface owner `TypeId`. `U` carries that owner id, a canonical
generic-parameter `TypeId`, and its exact declaration range. The query reads
only those facts and fails closed when the matching canonical signature symbol
is absent.

## Verification

- RED: the interface signature was present in the type prototype but had no
  semantic function symbol, so the source fixture failed at the exact-symbol
  assertion before any scope publication could occur.
- GCC static: symbols 13/13, semantic query 29/29, query contract 3/3,
  compiler diagnostics 46/46, and compiler integration 127/127 with zero
  failures and real process exit zero.
- MSVC static: the same targets passed 13/13, 29/29, 3/3, 46/46, and 127/127
  with zero failures and real process exit zero.
- Clang 14 WSL: `compiler_interface.c`, `semantic_scope_facts.c`, and the
  symbols test source compiled. The static executable link remains blocked by
  existing C11 inline unresolved references, including
  `ZrCore_Memory_RawFreeWithType` and `ZrCore_Array_Free`; no Clang executable
  pass is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 14:36:40 +08:00
- 状态：GCC/MSVC 子里程碑完成；Clang source 编译完成但既有 static link ABI
  失败阻断 executable gate，不宣称三工具链完成。
- 完成项目：interface method canonical SymbolId、共享 type-member symbol
  registration、interface signature generic scope projection、
  declaration-before-use、enclosing type-generic parentage、compiler integration
  regression、module 文档和 acceptance evidence。
- 后续项目：source type member、imports/aliases、receiver member、binary/native
  parity，以及 Task 2 的 LSP consumer 迁移。
