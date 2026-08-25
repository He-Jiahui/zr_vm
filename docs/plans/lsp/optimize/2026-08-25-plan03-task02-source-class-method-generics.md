---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2g: Source Class Method Generic Scope Facts

## Goal

Publish source class-method type generic parameters through canonical
`VisibleSymbols` facts, with each generic owned by the exact compiler-registered
method symbol rather than a name-, text-, or query-side AST-derived fallback.

## Implementation

- `compiler_type_member_register_function_symbol` resolves the member's
  canonical owner `TypeId` and registers its exact declaration AST as a function
  semantic symbol. It fails closed when the canonical owner or declaration
  identity is unavailable.
- The class compiler invokes that helper before it appends a class method member
  record. The existing struct canonical-definition pass now invokes the same
  helper, keeping function member symbol identity consistent across both source
  type kinds.
- The scope-fact producer accepts exact `ZR_AST_STRUCT_METHOD` and
  `ZR_AST_CLASS_METHOD` nodes. It emits a child function scope only after the
  matching registered function symbol is found, then projects generic
  parameters, ordinary parameters, and body scopes beneath that scope.
- The class method scope remains below its class scope. An enclosing type
  generic is therefore visible through fact parentage, while the method generic
  is absent at the method name and becomes visible at its own declaration.

## Contract

For a source class method, the compiler publishes a stable function `SymbolId`
bound to the exact class-member declaration and its canonical owner `TypeId`.
A method generic carries that owner id, a canonical generic-parameter `TypeId`,
and its exact declaration/definition range. `VisibleSymbols` consumes only
these published scopes and symbols. It does not infer class methods or generic
parameters from spelling, range overlap, or language-server state.

## Verification

- RED: `class Crate<T> { fn echo<U>(value: U): U { return value; } }` compiled
  without a registered `ZR_AST_CLASS_METHOD` function symbol, so the canonical
  class method generic fixture failed closed at its exact symbol assertion.
- GCC static: `zr_vm_semantic_query_symbols_test` 11/11,
  `zr_vm_semantic_query_test` 29/29,
  `zr_vm_semantic_query_contract_test` 3/3,
  `zr_vm_compiler_semantic_query_diagnostics_test` 46/46, and compiler
  integration 127/127, all with zero failures and real process exit zero.
- MSVC static: the same targets passed 11/11, 29/29, 3/3, 46/46, and 127/127
  with zero failures and real process exit zero.
- Clang 14 WSL: `compiler_type_member.c`, `compiler_class.c`,
  `compiler_struct.c`, `semantic_scope_facts.c`, and the symbols test source
  compiled. The static executable link remains blocked by existing C11 inline
  unresolved references, including `ZrCore_Memory_RawFreeWithType` and
  `ZrCore_Array_Free`; no Clang executable pass is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 14:09:50 +08:00
- 状态：GCC/MSVC 子里程碑完成；Clang source 编译完成但既有 static link ABI
  失败阻断 executable gate，不宣称三工具链完成。
- 完成项目：class method canonical SymbolId、共享 struct/class member symbol
  registration、method generic scope projection、declaration-before-use、
  enclosing type-generic parentage、compiler integration regression、module
  文档和 acceptance evidence。
- 后续项目：const/interface method generic producer、type member、
  imports/aliases、receiver member、binary/native parity，以及 Task 2 的 LSP
  consumer 迁移。
