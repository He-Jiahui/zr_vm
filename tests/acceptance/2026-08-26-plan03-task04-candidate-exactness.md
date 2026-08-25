---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task04-candidate-exactness.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 4.7 Overload Candidate Exactness Gate

## Scope

Accept only the exactness boundary for value-only overload candidate results.
The borrowed `CallAt` exactness contract is retained rather than replaced.

## Required Results

- An exact selected call can return canonical overload candidates.
- An inexact selected call returns no candidate values.
- `CallAt` may still borrow the selected fact so callers can inspect
  exactness directly.
- Candidate selection never replaces an inexact call from a spelling, wider
  fact, AST, or LSP fallback.

## Evidence

The isolated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly ran
`zr_vm_semantic_query_calls_test`. Before the fix, its approximate selected-call
case failed with `Expected FALSE Was TRUE`. The final direct run returned
process exit zero and Unity reported `8 Tests 0 Failures 0 Ignored`. Adjacent
query regressions passed `29/0`, `19/0`, `3/0`, `19/0`, `3/0`, and `46/0`, all
with direct process exit zero. This is isolated MSVC evidence only, not a
clean-baseline or three-toolchain claim.

## Acceptance Decision

Accepted for exact overload candidate value projection. Remaining call
metadata, external producer parity, and LSP consumers remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 06:59 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：candidate exactness fail-closed、borrowed `CallAt` 与 value
  candidate boundary，以及 isolated MSVC acceptance evidence。
- 后续项目：canonical mapping/conversion/receiver facts 和 LSP call
  hierarchy migration。
