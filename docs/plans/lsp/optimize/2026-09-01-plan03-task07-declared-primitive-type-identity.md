---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_declared_primitive_type_identity_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/index.md
doc_type: milestone-record
---

# Plan 03 Task 7.59: Declared Primitive Type Identity

## Goal

Close the declared primitive type identity regression exposed after Task 5 removed the LSP
name-to-type mapper. Function parameter and const-generic bound type uses must publish parser-backed
canonical TypeId/reference facts before a hover or signature consumer asks for display text.

## Contract

- `ZrParser_AstTypeToInferredType_Convert` remains the only primitive type inference authority used
  by the LSP prototype bootstrap. No LSP helper maps `int`, aliases, or other source spelling back to
  `EZrValueType`.
- A successfully converted primitive may have no stored `SZrInferredType.typeName`. Canonical
  registration uses the structural inferred type and must not reject that valid state.
- Type-prototype, method-signature, and free-function generic metadata collection publishes a const
  parameter's declared bound through
  `ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType` with its exact owner/callable
  context.
- `ZrParser_SemanticQuery_CanonicalTypeAt` is valid at both the ordinary parameter type use and the
  const-generic bound type use before any hover request. Both facts are resolved type references and
  share the canonical primitive TypeId.
- Missing or unresolved declared types remain fail-closed. Display text never creates semantic
  identity.

## TDD

The existing full semantic-analyzer RED showed:

- `make(seed: int)` rendered `seed: cannot infer exact type` while its inferred return stayed `int`;
- `class Derived<T, const N: int>` rendered the const bound as `cannot infer exact type`.

Removing the stored-name gate made both existing hover regressions pass. A new focused parity case
then exposed the producer timing precisely: the ordinary parameter already returned TypeId 1 with a
resolved type reference, while the class const bound initially returned no fact. After that producer
fix, an expanded RED showed the same missing fact for a free-function const bound. The prototype and
function symbol collectors now publish both declared bounds during snapshot construction. The
focused case passes only when all three positions share one valid TypeId and
`ZrParser_CanonicalType_Format` returns `int`.

## Verification

- GCC: semantic-query parity, complete LSP interface, and source-contract targets all real exit 0.
- Clang: semantic-query parity, complete LSP interface, and source-contract targets all real exit 0.
- MSVC static Debug (`VSCMD_VER=17.14.38`): the same three targets all real exit 0.
- GCC, Clang, and MSVC full semantic-analyzer targets each still return exit 1 with the same 14
  historical Task 7 failures. In all three logs, `Semantic Analyzer Unannotated Function Surfaces
  Exact Return Signature Detail` and `Semantic Analyzer Generic Type Symbols Surface Signature
  Detail` are PASS.

This submilestone does not claim the full 16-target matrix, source/binary/native consumer parity,
stale/unresolved exactness coverage, or the three stdio/CLI smokes.

## 状态与产出记录

- 完成时间：2026-09-01 22:00 +08:00。
- 状态：Task 7.59 source declared-type producer support GREEN；Plan 03 Task 7 与 Task 8 继续进行。
- 完成项目：nameless primitive canonical registration；const-generic bound fact publication；
  owner/callable context preservation；direct `CanonicalTypeAt`/TypeId/formatter regression；
  GCC/Clang/MSVC focused verification；主计划、module docs与session note更新。
- 未完成项目：Task 7 consumer总迁移；source/binary/native、stale snapshot与unresolved exactness
  完整矩阵；删除剩余LSP typecheck/reference/scope语义；完整16-target与三套stdio/CLI smoke。
