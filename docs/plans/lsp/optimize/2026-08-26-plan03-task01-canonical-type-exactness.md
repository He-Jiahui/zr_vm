---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_contract.c
  - tests/acceptance/2026-08-26-plan03-task01-canonical-type-exactness.md
doc_type: milestone-record
---

# Plan 03 Task 1.3: CanonicalTypeAt Exactness Gate

## Goal

Close the remaining value-projection gap in `CanonicalTypeAt`: an approximate
expression fact must not expose a canonical `TypeId` where the result structure
cannot represent its exactness.

## Implementation

- The reference-first projection is unchanged and continues to consume the
  already-published reference identity.
- The expression fallback now requires `EXACT`, using the same public
  exactness predicate as `TypeAt`, call-edge values, candidate values, and
  call-display values.
- A failed expression fallback returns false with an invalid `typeId`; the
  borrowed expression/reference pointers remain observational context rather
  than a value projection.
- The query neither re-infers a type nor searches source, AST, spelling, or LSP
  state for a replacement.

## Verification

- RED: a fact-only approximate literal previously made `CanonicalTypeAt`
  return true with a `TypeId` (`Expected FALSE Was TRUE`).
- GREEN: after the one-condition gate, the direct isolated MSVC contract target
  reports `4 Tests 0 Failures 0 Ignored` with process exit zero.
- Regression: rebuilt direct isolated MSVC targets report calls `9/0`, semantic
  query `29/0`, query symbols `19/0`, canonical consumers `19/0`, semantic
  display `3/0`, and compiler semantic-query diagnostics `46/0`, each with
  process exit zero.

## 状态与产出记录

- 完成时间：2026-08-26 07:19 +08:00。
- 状态：已完成，待本子项精确代码、测试和记录提交；不重写既有 Plan 03 Task 1
  完成记录，也不声明 Task 5 完成。
- 完成项目：`CanonicalTypeAt` expression fallback exactness gate、invalid
  value result fail-closed，以及 approximate-fact RED/GREEN/回归。
- 后续项目：documentation metadata facts、完整 call value metadata producer、
  source/binary/native display parity 与 LSP consumer migration。
