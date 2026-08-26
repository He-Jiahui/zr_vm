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

# Acceptance: Plan 03 Task 6.3 Syntax Recovery No-Fix Producers

## Required Results

- Five direct syntax-recovery builders publish a defined no-fix reason.
- Missing semantic expressions use `REQUIRES_USER_DECISION`.
- A builder without a precise insertion range uses
  `INSUFFICIENT_CONTEXT` rather than the primary range as an edit fallback.
- Stable diagnostic text and parser call sites remain unchanged.
- The general builder shrinks while the focused module owns disposition.

## Evidence

GCC 11.4, Clang 14, and MSVC 19.44 directly execute
`zr_vm_semantic_query_diagnostics_test` at 4 Tests/0 Failures and
`zr_vm_compiler_semantic_query_diagnostics_test` at 46/0. Every sequence exits
zero. GCC and Clang consume one byte-matched fixed ext4 snapshot with separate
build directories.

## Acceptance Decision

Accepted for the five syntax-recovery no-fix producers and their module
boundary. Remaining producer categories and LSP parity stay outside this
record.

## 状态与产出记录

- 完成时间：2026-08-26 17:07 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：explicit recovery reasons、no request-position edit fallback、
  focused module 和 direct regression evidence。
