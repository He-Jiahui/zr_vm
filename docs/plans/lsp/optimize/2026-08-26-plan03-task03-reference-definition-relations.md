---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: milestone-record
---

# Plan 03 Task 3.3: Declaration Definition Relations

## Goal

Publish declaration-to-definition relation facts from canonical resolved
reference facts. The producer must retain the resolved symbol identity and
exact definition range; it must not find declarations or definitions through
names, source text, or LSP state.

## Implementation

- Adds `ZrParser_SemanticRelations_PublishReferenceDefinitions`.
- Consumes only resolved `WRITE` reference facts with a valid `SymbolId`,
  canonical TypeId, and exact definition range.
- Obtains the declaration endpoint exclusively from the canonical symbol
  record selected by that SymbolId.
- Publishes a `ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION` edge whose source
  and target retain the same stable symbol identity and canonical type.
- Does not convert reads into definitions and fails closed by omitting facts
  that lack resolved identity or exact ranges.
- Is idempotent per symbol and definition range, while retaining separate
  definition sites for the same declaration.
- Runs in the compiler late-publication phase before property relation
  projection, so parser and future LSP consumers observe one snapshot-owned
  graph.

## Exclusions

This slice does not yet project CFG definition sets as per-read relation
targets, nor does it publish source inheritance, implementation, override,
alias, binary/native origin, or call graph edges. It does not migrate an LSP
consumer.

## Verification

- RED: the dedicated MSVC relation test failed to link because
  `ZrParser_SemanticRelations_PublishReferenceDefinitions` did not exist.
- GREEN: the dedicated MSVC static cache directly executed relation graph
  `7 Tests 0 Failures 0 Ignored`, including a `compile_script` source
  integration assertion, semantic query `29 Tests 0 Failures 0
  Ignored`, property consumer contracts `11 Tests 0 Failures 0 Ignored`, and
  compiler semantic diagnostics `46 Tests 0 Failures 0 Ignored`, each with
  process exit zero.

## 状态与产出记录

- 完成时间：2026-08-26 00:18 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 3 完成。
- 完成项目：resolved write fact 到 declaration-definition relation、SymbolId
  和 TypeId 严格投影、精确端点范围、每个定义位置幂等、compiler late
  publication，以及关系/查询/属性/诊断回归。
- 后续项目：CFG 多定义按读点关联、source base/interface/override/alias、
  binary/native origin、call graph 与 LSP relation consumers。
- 补充完成时间：2026-08-26 00:27 +08:00。
- 补充状态：已完成并将随补充测试提交；不改变本子项或 Plan 03 Task 3 的完成范围。
- 补充项目：`compile_script` source snapshot 的 declaration-definition
  relation 端到端断言，覆盖 compiler late-publication 顺序。
