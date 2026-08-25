---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: milestone-record
---

# Plan 03 Task 4.2: Resolved Overload Candidate Membership

## Goal

Expose the selected call target's canonical overload-set membership without
re-running overload resolution or treating a same-name function as a candidate.

## Implementation

- Adds `SZrParserSemanticCallCandidateQuery` and `CallCandidatesAt`.
- The query first consumes `CallAt`; an unresolved target returns no candidate
  result rather than searching a spelling.
- It reads the selected SymbolId's existing `overloadSetId`, projects only
  registered function SymbolIds from that set, deduplicates them, and orders
  the returned declaration candidates by SymbolId.
- Each candidate retains its declaration callable TypeId and declaration range.
  `isSelected` is true only for the exact CallAt target. The selected closed
  callable TypeId remains on `CallAt` and is not copied onto generic peers.

## Exclusions

This is declaration membership only. It does not persist compatibility scores,
generic inference outcomes, conversion exactness, argument-to-parameter maps,
receiver TypeId, lambda candidates, binary/native candidate facts, or LSP
overload UI.

## Verification

- RED: the call-edge test failed because `CallCandidatesAt` and its result type
  did not exist.
- GREEN: the dedicated MSVC static cache directly ran
  `zr_vm_semantic_query_calls_test` with `3 Tests 0 Failures 0 Ignored` and
  process exit zero. The overload fixture confirms the exact selected `int`
  declaration and its `string` peer are returned from one canonical set.

## 状态与产出记录

- 完成时间：2026-08-26 05:42 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 4 完成。
- 完成项目：resolved target overloadSet 投影、stable declaration candidates、
  selected marker、generic closed-call TypeId分离和 no-name-fallback 测试。
- 后续项目：candidate compatibility score、argument-to-parameter mapping、
  conversion/exactness、receiver/binary/native callable facts 与 LSP consumers。
