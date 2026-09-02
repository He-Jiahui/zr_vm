---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - tests/language_server/test_semantic_analyzer.c
tests:
  - tests/language_server/test_semantic_analyzer.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/index.md
doc_type: milestone-record
---

# Plan 03 Task 7.61: Structured Generic Member Returns

## Goal

Publish source method return types as structured canonical member facts so closed generic receiver calls
can substitute owner and method generic parameters without reconstructing types from names or text.

## Contract

- Class, struct, and interface methods build `SZrTypeMemberInfo.structuredReturnType` in the declaration's
  owner and callable generic context.
- `hasStructuredReturnType` is true only when the complete inferred return tree is available.
- Type parameters and const parameters retain their distinct `genericArgumentKind` values until canonical
  call substitution closes them.
- A missing structured return fact fails closed; LSP consumers do not parse `returnTypeName`, display text,
  member names, or request-time AST to recover the result type.

## TDD

After Task 7.60 corrected source-aware query fixtures, the complete semantic-analyzer target exposed two
producer failures. `Box<int>.shape<4>(...)` reported that `N` had the wrong generic kind, and an owner plus
method generic signature failed for the same reason. Parameter types were already structured, while the
method return existed only as `returnTypeName`.

The producer now captures the return with the same declaration-context inferred-type builder used for
member parameters. The analyzer test directly inspects `Box.shape` before hover projection and requires
the second `Matrix<T, N>` argument to remain a named const parameter.

## Verification

- GCC, Clang, and MSVC complete semantic-analyzer executables: real exit 0; both prior generic failures and
  the direct structured-return assertion pass.
- On the same production baseline, GCC, Clang, and MSVC semantic-query parity: 16/16, real exit 0.
- GCC, Clang, and MSVC LSP source-contract and complete interface executables: real exit 0.
- The current full 16-target matrix and three stdio/CLI smokes remain pending for Task 8.

## 状态与产出记录

- 完成时间：2026-09-02 20:47 +08:00。
- 状态：Task 7.61 structured generic member return producer GREEN；Task 7 与 Task 8 继续进行。
- 完成项目：class/struct/interface method结构化return fact；owner/method generic kind保持；
  closed generic receiver call回归；三工具链analyzer/parity/source-contract/interface验证；主计划与
  semantic-query foundation文档更新。
- 未完成项目：Task 7 consumer总迁移；source/binary/native、stale/unresolved完整consumer矩阵；
  Task 8完整16-target与三套stdio/CLI smoke。
