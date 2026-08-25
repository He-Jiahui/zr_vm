---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/acceptance/2026-08-26-plan03-task04-call-edge-exactness.md
doc_type: milestone-record
---

# Plan 03 Task 4.6: Call-Edge Exactness Gate

## Goal

Keep call hierarchy facts fail-closed when a matching call expression fact is
not exact and the public call-edge value row has no exactness output field.

## Implementation

- `semantic_calls` retains its existing narrowest matching call expression.
- If that selected expression has `UNKNOWN` or `APPROXIMATE` exactness, the
  producer emits no call edge.
- The producer does not fall back to a wider expression, the reference name,
  or a target symbol. A missing expression fact retains the existing
  reference-only unresolved-edge behavior.

## Verification

- RED: a fact-only resolved call with matching `APPROXIMATE` expression fact
  produced one outgoing edge in the direct MSVC call-edge runner.
- GREEN: the same executable reported `7 Tests 0 Failures 0 Ignored` with
  process exit zero after the exactness gate; its outgoing array is empty.
- Regression: direct MSVC runs also passed semantic query `29/0`, query
  symbols `19/0`, query contract `3/0`, canonical consumers `19/0`, semantic
  display `3/0`, and compiler semantic-query diagnostics `46/0`.

## 状态与产出记录

- 完成时间：2026-08-26 06:53 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 4 完成。
- 完成项目：call-edge exactness fail-closed gate、approximate fact RED/GREEN，
  以及 query/consumer 回归。
- 后续项目：receiver TypeId、argument mapping、conversion/exactness value
  projection、source/binary/native external callable producers 和 LSP consumers。
