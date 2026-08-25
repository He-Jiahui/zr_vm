---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task04-call-edge-exactness.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 4.6 Call-Edge Exactness Gate

## Scope

Accept only fail-closed hierarchy publication for a selected inexact call
expression fact. This does not add a public mapping, conversion, receiver, or
exactness value field to `CallAt`.

## Required Results

- A matching exact call expression can continue to publish a call edge.
- A matching `UNKNOWN` or `APPROXIMATE` call expression publishes no edge.
- No wider fact, reference spelling, target name, or LSP logic replaces the
  inexact selected fact.
- The existing reference-only unresolved edge remains valid when no matching
  expression fact exists.

## Evidence

The isolated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly ran
`zr_vm_semantic_query_calls_test`. Before the fix, its approximate-fact case
failed with `Expected 0 Was 1`. The final direct run returned process exit zero
and Unity reported `7 Tests 0 Failures 0 Ignored`. Adjacent query regressions
passed `29/0`, `19/0`, `3/0`, `19/0`, `3/0`, and `46/0`, all with direct
process exit zero. This is isolated MSVC evidence only, not a clean-baseline
or three-toolchain claim.

## Acceptance Decision

Accepted for call-edge exactness containment. Remaining call metadata and
external producer parity remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 06:53 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：inexact call-edge suppression、no-fallback contract，以及
  isolated MSVC acceptance evidence。
- 后续项目：完整 CallAt fact projection、external callable parity 和 LSP
  hierarchy consumer migration。
