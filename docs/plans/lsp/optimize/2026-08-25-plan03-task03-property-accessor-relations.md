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
  - tests/parser/test_property_consumer_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 3.2: Property Accessor Relations

## Goal

Publish source property-to-accessor relation facts from the canonical property
contract after all property accessor symbols have been resolved. The relation
producer must not pair hidden accessors by name, AST shape, or LSP state.

## Implementation

- Adds `ZrParser_SemanticRelations_PublishPropertyContracts`.
- Projects getter, setter, and initializer edges as
  `ZR_SEMANTIC_RELATION_PROPERTY_ACCESSOR` from the visible property SymbolId.
- Requires each published target to be an exact registered function SymbolId
  with the callable TypeId recorded by the property contract.
- Copies source declaration and target symbol ranges from the canonical facts.
- Validates the complete contract set before appending, so an invalid accessor
  cannot leave a partial property relation graph behind.
- Runs once during compiler late publication and remains idempotent for callers
  that explicitly publish the same snapshot again.

## Exclusions

This slice does not publish inheritance, implementation, constructor, alias,
binary/native origin, or call graph relations. It does not migrate an LSP
consumer and does not add a name-based recovery path.

## Verification

- RED: `zr_vm_semantic_query_relations_test` failed to link because the property
  relation producer did not exist.
- GREEN: the dedicated MSVC build cache directly executes relation graph `5 Tests 0
  Failures 0 Ignored` and property consumer contracts `11 Tests 0 Failures 0
  Ignored`, both with process exit zero.

## 状态与产出记录

- 完成时间：2026-08-26 00:05 +08:00。
- 状态：已完成并随本补充提交精确提交；不声明 Plan 03 Task 3 完成。
- 完成项目：canonical property contract 到 getter/setter/init relation edges、
  SymbolId/Callable TypeId 严格匹配、精确端点范围、原子 fail-closed、幂等发布和
  真实属性消费者回归。
- 后续项目：source base/interface/override/alias producers、binary/native origin、
  call graph 与 LSP relation consumers。
