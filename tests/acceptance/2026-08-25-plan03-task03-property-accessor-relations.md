---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-25-plan03-task03-property-accessor-relations.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_property_consumer_contracts.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.2 Property Accessor Relations

## Scope

Accept only source property-accessor relation production from existing canonical
property contracts. The accepted producer may use only the contract's resolved
property/accessor SymbolIds, callable TypeIds, and canonical ranges.

## Required Results

- Getter, setter, and initializer relations use the visible property SymbolId
  as source and the corresponding resolved function SymbolId as target.
- An accessor TypeId mismatch, absent symbol, or non-function target fails
  closed without a property-name or hidden-accessor fallback.
- A relation retains the contract declaration range and accessor declaration
  range only when each canonical range is available.
- Repeated publication over an unchanged semantic context does not duplicate
  an edge.

## Evidence

The dedicated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` rebuilt and directly executed
`zr_vm_semantic_query_relations_test` with process exit zero and Unity summary
`4 Tests 0 Failures 0 Ignored`. The existing compiler-backed
`zr_vm_property_consumer_contracts_test` also directly returned process exit
zero with `11 Tests 0 Failures 0 Ignored`. The shared working tree contains
unrelated uncommitted work, so this is not claimed as a clean-baseline or
three-toolchain matrix result.

## Acceptance Decision

Accepted for source property accessor relation production only. Other relation
producers, external metadata projection, call graph facts, and LSP consumers
remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-25 23:58 +08:00。
- 状态：已完成并随本提交精确提交。
- 完成项目：property accessor canonical relation producer、严格 target identity、
  端点范围、幂等性与 compiler-backed consumer 验证。
- 后续项目：其余 source/binary/native relation producers、call graph、LSP projection。
