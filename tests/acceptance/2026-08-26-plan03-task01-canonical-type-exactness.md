---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task01-canonical-type-exactness.md
tests:
  - tests/parser/test_semantic_query_contract.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 1.3 CanonicalTypeAt Exactness Gate

## Scope

Accept only the exactness boundary of the expression fallback in
`CanonicalTypeAt`. Reference-backed canonical type identity is outside this
slice and remains unchanged.

## Required Results

- An exact expression fallback can return its published canonical `TypeId`.
- An `UNKNOWN` or `APPROXIMATE` expression fallback returns false with an
  invalid `typeId`.
- A reference-backed result uses the existing published reference fact rather
  than re-inference or text recovery.
- The query does not use AST, source spelling, broader expression facts, or LSP
  state to replace a rejected expression fallback.

## Evidence

The isolated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly built and ran
`zr_vm_semantic_query_contract_test`. Before the gate, the approximate fallback
case failed with `Expected FALSE Was TRUE`. The final direct run reported
`4 Tests 0 Failures 0 Ignored` with process exit zero. Rebuilt adjacent direct
tests reported calls `9/0`, semantic query `29/0`, query symbols `19/0`,
canonical consumers `19/0`, semantic display `3/0`, and compiler diagnostics
`46/0`, all with process exit zero.

This is isolated MSVC evidence only. It is neither a clean-baseline nor a
three-toolchain acceptance claim.

## Acceptance Decision

Accepted for `CanonicalTypeAt` expression-fallback exactness. Remaining
reference exactness schema, documentation facts, complete call metadata, and
LSP consumer migration remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 07:19 +08:00。
- 状态：已完成，待本子项精确提交。
- 完成项目：canonical type expression value fail-closed 与 isolated MSVC
  acceptance evidence。
- 后续项目：semantic query exactness coverage audit、metadata producers 和 LSP
  migration。
