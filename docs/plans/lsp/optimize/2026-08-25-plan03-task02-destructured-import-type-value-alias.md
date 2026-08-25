---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2l: Destructured Import and Type-Value Alias Facts

## Goal

Publish source destructured import bindings and source type-value aliases as
canonical visible-symbol facts. Each result must retain the compiler-produced
SymbolId, TypeId, and exact binding range; neither the parser query nor LSP may
recover the result from module, member, or alias spelling.

## Implementation

- `compile_statement_register_semantic_local` is shared with destructuring so
  a resolved destructured field binding uses the same type-environment,
  declaration-fact, and pre-Semantic-IR registration path as an ordinary local.
- For `var {Vec3: Vector3} = import("zr.math")`, the declaration node and
  source range are the local binding `Vec3`, not the imported member `Vector3`.
- Source scope facts enumerate object-destructuring binding nodes. Only an
  import-expression initializer marks those candidates `isImport` and
  `isAlias`.
- An identifier variable initialized by `ZR_AST_TYPE_LITERAL_EXPRESSION` is
  marked `isAlias` but not `isImport`.
- Existing `VisibleSymbols.includeImports` remains the sole filter for both
  imports and aliases. No query-side global scan or LSP fallback was added.

## Contract

The parser query projects `SymbolAt(Vec3)` and `VisibleSymbols` from the same
resolved declaration fact. With default options, both `Vec3` and `MatrixType`
are omitted because aliases are opt-in. With `includeImports`, both candidates
appear exactly once, and `Vec3` retains `isImport && isAlias` while
`MatrixType` retains `!isImport && isAlias`.

## Verification

- RED: no declaration symbol was registered for the destructuring binding.
- RED refinement: after registration, the declaration fact incorrectly used
  the imported member range, so `SymbolAt(Vec3)` failed closed.
- GREEN: binding-node registration and exact range publication make `SymbolAt`
  and `VisibleSymbols` agree without textual reconstruction.
- MSVC static: symbols 17/17, semantic query 29/29, query contract 3/3,
  compiler diagnostics 46/46, and compiler integration 127/127 passed with
  zero failures and real process exit zero.
- Windows GCC 4.8.3 did not build the target: the unchanged shared core fails
  before parser compilation because it does not support `_Thread_local` in
  `zr_vm_core/include/zr_vm_core/profile.h`. No GCC test result is claimed.
- WSL has no installed distribution and no local Clang executable is available;
  no Clang executable result is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 17:39:05 +08:00
- 状态：MSVC source-producer 子里程碑完成；GCC/Clang executable gate 因环境
  不可用或既有工具链限制保持未完成，未做跨工具链通过声明。
- 完成项目：destructured import binding canonical SymbolId/TypeId/range、
  type-value alias classification、SymbolAt/VisibleSymbols identity parity、
  import opt-in filtering、RED/GREEN、MSVC focused query/diagnostic/compiler-
  integration regression、模块文档与验收记录。
- 后续项目：binary/native import and alias producers、alias relation facts，
  以及 Task 2 LSP consumer migration。
