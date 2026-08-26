---
related_code:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fix_disposition.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_ownership.c
tests:
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.5 Ownership And Legacy No-Fix Producers

## Required Results

- Six ownership errors publish `REQUIRES_USER_DECISION`.
- The legacy ownership syntax warning publishes `INSUFFICIENT_CONTEXT`.
- Dynamic ownership type names remain display data and do not become policy
  or edit fallbacks.
- Stable diagnostics and parser call sites remain unchanged.
- The direct unclassified base-builder return inventory decreases from seven
  to zero.

## Evidence

GCC 11.4, Clang 14, and MSVC 19.44 directly execute
`zr_vm_semantic_query_diagnostics_test` at 6 Tests/0 Failures and
`zr_vm_compiler_semantic_query_diagnostics_test` at 46/0. Every sequence exits
zero. GCC and Clang consume one byte-matched fixed ext4 snapshot with separate
build directories.

## Acceptance Decision

Accepted for the seven ownership/legacy direct no-fix producers. Indirect
producer inventory and LSP parity remain outside this record.

## 状态与产出记录

- 完成时间：2026-08-26 17:26 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：typed ownership/legacy reasons、direct inventory 清零、模块化
  收口和 direct regression evidence。
