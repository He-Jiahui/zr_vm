---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task05-format-call-exactness.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_canonical_consumers.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 5.2 FormatCall Exactness Gate

## Scope

Accept only the fail-closed boundary for `FormatCall` value output. The
borrowed `CallAt` result remains the sole route for callers that need to
observe an inexact fact.

## Required Results

- An exact selected call can return its canonical signature display.
- An `UNKNOWN` or `APPROXIMATE` selected call returns false and leaves a usable
  caller buffer empty.
- A failed format operation cannot retain a previous signature in that buffer.
- No name, AST, wider-fact, or LSP fallback produces a display after failure.

## Evidence

The isolated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly ran
`zr_vm_semantic_query_calls_test`. Before the production gate, the approximate
selected-call assertion failed with `Expected FALSE Was TRUE`. The final direct
run reported `9 Tests 0 Failures 0 Ignored` and process exit zero. Direct
adjacent runs also reported semantic query `29/0`, query symbols `19/0`, query
contract `3/0`, canonical consumers `19/0`, semantic display `3/0`, and
compiler semantic-query diagnostics `46/0`, all with process exit zero.

This is isolated MSVC evidence only. It is neither a clean-baseline claim nor
a three-toolchain acceptance claim.

## Acceptance Decision

Accepted for the `FormatCall` exactness and buffer-clearing contract. Canonical
documentation facts, external display parity, and LSP consumer migration remain
unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 07:12 +08:00。
- 状态：已完成，待本子项精确提交。
- 完成项目：inexact call display fail-closed、stale-output clear，以及 isolated
  MSVC acceptance evidence。
- 后续项目：canonical documentation metadata、source/binary/native display
  parity 和 LSP signature/hover migration。
