---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_unresolved_diagnostics.c
tests:
  - tests/parser/test_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 6.40: Unresolved Diagnostic Source Identity

## Scope

This submilestone makes parser diagnostic materialization suppress an
unresolved reference fact only when a resolved reference has the same name,
range, and exact optional source identity. Equal offsets and spelling do not
authorize suppression across a missing or different snapshot source.

The change is below LSP protocol projection. It does not alter diagnostic
messages, severity, stable codes, descriptors, fixes, or no-fix disposition.

## TDD And Root Cause

The existing test proves that a resolved reference shadows an unresolved fact
at the same sourced range. The RED adds the adjacent negative case: the
unresolved fact keeps its source, while the otherwise matching resolved fact
has no source. Materialization must retain one `unresolved_reference` row.

The original twelve tests pass and the new case is the only failure: `13 Tests
/ 1 Failure`, `Expected 1 Was 0`. The local
`semantic_query_unresolved_same_source` helper treated either null operand as a
wildcard, so the resolved row incorrectly hid the unresolved fact.

## Implementation

The comparator now accepts a null source only when both operands are null.
Pointer equality and non-null string-value equality remain unchanged. The
existing name and complete-range checks still participate in the suppression
key, and the same-source positive test remains GREEN.

## Verification

The fixed isolated source is HEAD `79787fd` plus the two exact code/test paths
from this record. WSL GCC 11.4 and Clang 14.0.0 directly passed:

- parser semantic-query diagnostics: `13/13`;
- compiler semantic-query diagnostics: `64/64`;
- LSP semantic-query diagnostics: `19/19`;
- LSP semantic-query parity/source contracts: `15/70`.

All executables returned real process exit zero. The GCC and Clang LSP interface
executables retained exactly the same eight pre-existing producer markers;
failure delta is zero. MSVC, the complete 16-target matrix, and the three stdio
smoke suites were not run for this narrow diagnostic correction.

## 状态与产出记录

- 完成时间：2026-08-31 09:55 +08:00。
- 状态：Task 6.40 unresolved diagnostic source identity 子里程碑已完成；
  Plan 03 Task 6 与整体计划继续进行。
- 完成项目：one-sided source suppression RED、resolved/unresolved exact
  source/range/name identity、模块契约更新、GCC/Clang `13/64/19/15/70`
  真实退出门禁、interface fixed8 delta 0。
- 后续项目：继续迁移analyzer-side structured producers，完成compiler/LSP
  golden parity与Task 6总门禁；MSVC、完整矩阵和stdio由后续阶段统一验收。
