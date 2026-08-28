---
related_code:
  - zr_vm_parser/include/zr_vm_parser/variance.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_generic_semantics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_variance_query_diagnostics.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_variance_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task06-variance-fact-producer-migration.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.33 Variance Fact Producer Migration

## Required Results

- Make parser/compiler the only producer of persistent interface variance
  diagnostic facts.
- Publish every violation for one interface without converting semantic
  analysis into the normal compiler first-error path.
- Preserve descriptor `2013`, exact primary and related ranges, severity, help
  metadata, and explicit user-decision no-fix disposition.
- Delete the LSP enumeration, builder, fact append, and local producer module.
- Reject source-level reintroduction and preserve exact stdio projection.

## TDD Evidence

The fixed-parent parser target was a valid link RED for the missing public
publisher. After implementation, the three-invalid-position fixture returns
three query diagnostics, each with descriptor `2013`, one related declaration
range, no fixes, and `REQUIRES_USER_DECISION`; `compiler.hasError` stays false.

The source contract reads the remaining typecheck consumer and proves that it
contains only `ZrParser_Variance_PublishInterfaceDiagnostics`, not the parser
violation iterator, builder, semantic fact append, old local entry, or literal
diagnostic code. The old producer source is deleted.

## Final Evidence

GCC, Clang, and MSVC each pass the same direct-exit gates: producer `1/1`,
query disposition `11/11`, compiler diagnostics `64/64`, `54` source-contract
markers, and the dedicated stdio transport. The snapshots match all seven
present files by SHA-256 and omit the deleted producer.

GCC parent and overlay semantic-analyzer runs both retain exactly two unrelated
baseline failures. The overlay variance case passes; no completion claim is
made for the closed-generic receiver or borrow-range markers.

## Acceptance Decision

Accepted as the final variance persistent-fact producer migration for Plan 03
Task 6. Remaining analyzer-owned semantic producers keep Task 6 active.

## 状态与产出记录

- 完成时间：2026-08-28 17:39 +08:00。
- 状态：本子项已验收；Plan 03 Task 6 继续进行。
- 完成项目：valid parser RED、三项 persistent variance facts、compiler error
  state 隔离、LSP producer 删除、三工具链 `1/11/64/54/stdio`、GCC marker
  A/B、`7/7` byte match 与 deleted `0`。
