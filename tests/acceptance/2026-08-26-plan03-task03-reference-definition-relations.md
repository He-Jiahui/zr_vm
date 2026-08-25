---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task03-reference-definition-relations.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_property_consumer_contracts.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.3 Declaration Definition Relations

## Scope

Accept only source declaration-to-definition relation production for resolved
write facts. The accepted producer uses canonical reference and symbol facts;
it does not reconstruct an identity, declaration, or definition from spelling
or LSP state.

## Required Results

- Each accepted edge is `ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION` from the
  declaration symbol to the same stable symbol identity at an exact write
  definition range.
- The symbol record supplies the source declaration range and canonical TypeId.
- Unresolved reads, unresolved writes, and facts without exact endpoint ranges
  are omitted rather than approximated.
- Repeating publication does not duplicate an existing definition edge, while
  different definition ranges remain distinct.

## Evidence

The dedicated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly returned process exit
zero for `zr_vm_semantic_query_relations_test` (`6 Tests 0 Failures 0
Ignored`), `zr_vm_semantic_query_test` (`29 Tests 0 Failures 0 Ignored`),
`zr_vm_property_consumer_contracts_test` (`11 Tests 0 Failures 0 Ignored`),
and `zr_vm_compiler_semantic_query_diagnostics_test` (`46 Tests 0 Failures 0
Ignored`). The shared working tree contains unrelated uncommitted work, so
this is not claimed as a clean-baseline or three-toolchain matrix result.

## Acceptance Decision

Accepted for source resolved-write declaration-definition relations only. CFG
multi-definition read associations, external origins, other relation
producers, and LSP consumers remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 00:18 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：canonical declaration-definition producer、精确 SymbolId/TypeId/
  range 投影、幂等性、关系图与既有查询/属性/诊断回归。
- 后续项目：CFG 多定义、外部 origin、其他 relation producer 和 LSP projection。
