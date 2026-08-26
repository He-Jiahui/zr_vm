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

# Acceptance: Plan 03 Task 6.4 Pattern And Import No-Fix Producers

## Required Results

- Six using/import/pattern builders publish a defined no-fix reason.
- Every producer uses `REQUIRES_USER_DECISION` for a semantic choice the
  compiler cannot make safely.
- Dynamic names remain display data and never become policy or edit fallbacks.
- Stable diagnostics and parser call sites remain unchanged.
- The unclassified direct-builder inventory decreases from 13 to 7.

## Evidence

GCC 11.4, Clang 14, and MSVC 19.44 directly execute
`zr_vm_semantic_query_diagnostics_test` at 5 Tests/0 Failures and
`zr_vm_compiler_semantic_query_diagnostics_test` at 46/0. Every sequence exits
zero. GCC and Clang consume one byte-matched fixed ext4 snapshot with separate
build directories.

## Acceptance Decision

Accepted for the six pattern/import/using no-fix producers. Ownership/legacy
producer classification and LSP parity remain outside this record.

## 状态与产出记录

- 完成时间：2026-08-26 17:14 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：typed producer reasons、display/policy separation、inventory
  reduction 和 direct regression evidence。
