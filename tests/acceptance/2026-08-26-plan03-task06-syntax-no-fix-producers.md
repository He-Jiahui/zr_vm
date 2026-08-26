---
related_code:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fix_disposition.c
tests:
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.2 Syntax No-Fix Producers

## Required Results

- Existing syntax no-fix producers publish a defined reason rather than only
  an empty edit array.
- Missing user-authored expressions use `REQUIRES_USER_DECISION`.
- Conditional colon insertion remains a typed fix when structurally complete
  and uses `INSUFFICIENT_CONTEXT` when the alternate expression is absent.
- Producer logic does not depend on code/message/name matching in a consumer.
- The large general builder shrinks and no parser call site changes.

## Evidence

GCC 11.4, Clang 14, and MSVC 19.44 directly execute
`zr_vm_semantic_query_diagnostics_test` at 3 Tests/0 Failures and
`zr_vm_compiler_semantic_query_diagnostics_test` at 46/0. Every sequence exits
zero. GCC and Clang consume one byte-matched fixed ext4 snapshot with separate
build directories.

## Acceptance Decision

Accepted for the four classified syntax no-fix branches and their focused
module boundary. Remaining diagnostic producers and LSP parity are outside
this acceptance record.

## 状态与产出记录

- 完成时间：2026-08-26 16:59 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：typed producer reasons、conditional fix/no-fix split、focused
  module 和 direct regression evidence。
