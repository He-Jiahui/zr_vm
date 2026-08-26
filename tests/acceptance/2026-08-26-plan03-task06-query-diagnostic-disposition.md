---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.6 Query Diagnostic Disposition

## Required Results

- Unreachable diagnostics publish `UNSAFE_EDIT`.
- Overflow and array bounds/index diagnostics publish
  `REQUIRES_USER_DECISION`.
- Generic compiler errors publish `INSUFFICIENT_CONTEXT`.
- Existing typed definite-assignment fixes remain valid.
- No reason is inferred from diagnostic text.

## Evidence

GCC 11.4, Clang 14, and MSVC 19.44 directly execute
`zr_vm_compiler_semantic_query_diagnostics_test` at 46 Tests/0 Failures. Every
sequence exits zero. GCC and Clang consume one byte-matched fixed ext4
snapshot with separate build directories.

## Acceptance Decision

Accepted for canonical query-materialized diagnostics and the generic compiler
error bridge. Parser/compiler rule producers and LSP parity remain outside
this record.

## 状态与产出记录

- 完成时间：2026-08-26 17:34 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：typed query disposition、generic bridge reason、failure cleanup
  和 direct regression evidence。
