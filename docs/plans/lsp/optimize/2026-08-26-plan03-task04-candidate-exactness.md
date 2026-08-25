---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/acceptance/2026-08-26-plan03-task04-candidate-exactness.md
doc_type: milestone-record
---

# Plan 03 Task 4.7: Overload Candidate Exactness Gate

## Goal

Prevent `CallCandidatesAt` from exposing value-only overload candidates when
its selected call expression is inexact.

## Implementation

- `CallAt` remains a borrowed view and continues to expose the selected
  expression fact, including its exactness.
- `CallCandidatesAt` checks that borrowed fact before it projects candidate
  value rows. A missing or non-EXACT expression clears the reusable output and
  returns false.
- The query does not recover candidates from a name, a wider expression, an
  AST traversal, or LSP state.

## Verification

- RED: an overload source fixture first proves `CallAt` succeeds, then marks
  its selected expression `APPROXIMATE`; the old `CallCandidatesAt` returned
  true with candidates despite that inexactness.
- GREEN: the direct MSVC executable reported `8 Tests 0 Failures 0 Ignored`
  with process exit zero. `CallAt` still exposes the approximate borrowed fact,
  while candidates return false and an empty array.
- Regression: direct MSVC runs also passed semantic query `29/0`, query
  symbols `19/0`, query contract `3/0`, canonical consumers `19/0`, semantic
  display `3/0`, and compiler semantic-query diagnostics `46/0`.

## 状态与产出记录

- 完成时间：2026-08-26 06:59 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 4 完成。
- 完成项目：overload candidate exactness gate、borrowed/value query boundary，
  以及 approximate selected-call RED/GREEN/回归。
- 后续项目：receiver TypeId、argument mapping、conversion/exactness value
  projection、source/binary/native external callable producers 和 LSP consumers。
