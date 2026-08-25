---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task05-format-call-coherence.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 5.3 FormatCall Fact Coherence

## Scope

Accept only the formatter's coherence check between an exact call expression,
its canonical CALL reference, and the callable TypeId.

## Evidence

The isolated MSVC static cache directly ran the deliberate mismatch RED, which
failed with `Expected FALSE Was TRUE`. After the gate, direct calls report
`10 Tests 0 Failures 0 Ignored`; rebuilt adjacent targets report contract `4/0`,
query `29/0`, symbols `19/0`, canonical consumers `19/0`, display `3/0`, and
compiler diagnostics `46/0`, all with process exit zero. This is isolated MSVC
evidence only, not a clean-baseline or three-toolchain claim.

## 状态与产出记录

- 完成时间：2026-08-26 07:24 +08:00。
- 状态：已完成，待本子项精确提交。
- 完成项目：canonical call fact coherence 和 isolated MSVC acceptance evidence。
- 后续项目：documentation metadata、external parity 和 LSP migration。
