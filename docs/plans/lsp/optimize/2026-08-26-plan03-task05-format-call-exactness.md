---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_canonical_consumers.c
  - tests/acceptance/2026-08-26-plan03-task05-format-call-exactness.md
doc_type: milestone-record
---

# Plan 03 Task 5.2: FormatCall Exactness Gate

## Goal

Prevent the value-only `ZrParser_SemanticQuery_FormatCall` API from rendering a
signature for an inexact call fact or leaving an earlier caller buffer visible.

## Implementation

- A non-null, non-empty output buffer is cleared before input validation.
- The formatter requires the borrowed `CallAt` expression to be `EXACT` before
  it reads `signatureDisplay`, formats a canonical callable TypeId, or uses the
  already-published target name.
- `CallAt` remains a borrowed query, so callers that need to inspect exactness
  can do so without receiving a value-only signature.
- The formatter does not reconstruct a signature from source text, an AST,
  a member spelling, a broader fact, or language-server state.
- Fact-only canonical-consumer fixtures explicitly mark their manually created
  exact expression facts as `EXACT`; an omitted exactness value remains
  unavailable rather than silently treated as exact.

## Verification

- RED: a compiled overload call first returned true after its selected fact was
  changed to `APPROXIMATE`, leaving a stale display buffer available.
- GREEN: direct execution of the isolated MSVC static test binary reports
  semantic query calls `9 Tests 0 Failures 0 Ignored`, including the stale-buffer
  assertion, with process exit zero.
- Regression: direct isolated MSVC binaries report semantic query `29/0`, query
  symbols `19/0`, query contract `3/0`, canonical consumers `19/0`, semantic
  display `3/0`, and compiler semantic-query diagnostics `46/0`, each with
  process exit zero.

## 状态与产出记录

- 完成时间：2026-08-26 07:12 +08:00。
- 状态：已完成，待本子项的精确代码、测试与记录提交；不声明 Plan 03 Task 5 完成。
- 完成项目：`FormatCall` exactness gate、失败时清空可用输出缓冲区、手工语义
  fixture 的显式 exactness 前提，以及 approximate-call RED/GREEN/回归。
- 后续项目：documentation metadata fact、完整 canonical call display facade、
  external display parity 与 LSP consumer migration。
