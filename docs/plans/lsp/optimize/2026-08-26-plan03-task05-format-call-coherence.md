---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/acceptance/2026-08-26-plan03-task05-format-call-coherence.md
doc_type: milestone-record
---

# Plan 03 Task 5.3: FormatCall Fact Coherence

## Goal

Require `FormatCall` to project only one coherent canonical call fact, even
though its public query structure is mutable by its caller.

## Implementation

- The formatter requires a non-null `CALL` reference.
- Its reference callable TypeId must equal the query callable TypeId, and its
  range must be inside the selected expression's call-target range.
- A mismatch clears a usable output buffer and returns false; no signature from
  another reference is reused.

## Verification

- RED: swapping two resolved call references made the first query display the
  second call's signature (`Expected FALSE Was TRUE`).
- GREEN: direct isolated MSVC calls report `10 Tests 0 Failures 0 Ignored` and
  process exit zero. Rebuilt adjacent query/display/diagnostic targets report
  contract `4/0`, query `29/0`, symbols `19/0`, consumers `19/0`, display
  `3/0`, and diagnostics `46/0` with process exit zero.

## 状态与产出记录

- 完成时间：2026-08-26 07:24 +08:00。
- 状态：已完成，待本子项精确提交；不声明 Plan 03 Task 5 完成。
- 完成项目：`FormatCall` expression/reference/type coherence gate、mismatched
  reference RED/GREEN 与 fail-closed stale-buffer contract。
- 后续项目：documentation metadata facts、external display parity 和 LSP
  consumer migration。
