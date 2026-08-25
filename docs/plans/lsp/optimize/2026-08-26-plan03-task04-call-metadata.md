---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/acceptance/2026-08-26-plan03-task04-call-metadata.md
doc_type: milestone-record
---

# Plan 03 Task 4.3: Canonical Call Metadata Projection

## Goal

Expose the metadata already persisted by the selected canonical call expression
fact without re-running overload resolution, inspecting the AST, or deriving
semantics from call text.

## Implementation

- `SZrParserSemanticCallQuery` now owns copied call-site and call-target
  ranges, argument count, named-argument state, and member-call state.
- `ZrParser_SemanticQuery_CallAt` copies all five values only after choosing
  the existing call expression fact and matching its canonical call reference.
- Reused output stays zeroed on failed queries. The value fields do not add a
  name fallback, receiver inference, parameter mapping, score, conversion, or
  external-call projection.

## Verification

- RED: the direct MSVC run of `zr_vm_semantic_query_calls_test` failed at the
  new `callSiteRange.source` assertion because `CallAt` returned a zero range.
- GREEN: the same direct executable run reported `3 Tests 0 Failures 0
  Ignored` with process exit zero after the fields were copied from the fact.

## 状态与产出记录

- 完成时间：2026-08-26 06:11 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 4 完成。
- 完成项目：call-site/target range、argument count、named-argument 与
  member-call metadata 的 canonical value projection，以及 no-recompute RED/GREEN 回归。
- 后续项目：argument-to-parameter mapping、compatibility score、conversion/
  exactness、receiver TypeId、external callable facts 和 LSP consumers。
