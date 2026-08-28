---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/reference_tracker.h
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_source.c
tests:
  - tests/language_server/test_reference_tracker.c
  - tests/language_server/test_lsp_reaching_definition_navigation.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.2: Reference SymbolId Index

## Scope

Task 7 requires reference consumers to stop grouping symbols by spelling. This
slice migrates `SZrReferenceTracker` to canonical `SymbolId`, preserves a
strict syntax-recovery boundary for invalid ids, and binds source-less analyzer
queries to the analyzer's single AST snapshot before lookup.

Syntax05 still owns property and symbol-analyzer paths, so this slice does not
enter those files or migrate their remaining consumer loops.

## TDD And Root Cause

On fixed HEAD `f499cab7fb69d39706d8080a3acaf69c51c357be`, the new
tracker case proved that same-name symbols with different ids were not mixed,
but a second wrapper carrying the same `SymbolId` could not find the first
wrapper's references. The map used `symbol->name` as its key and then filtered
by wrapper address, so canonical identity could not survive wrapper changes.

After making tracker source identity strict in Task 7.1, full analyzer tests
also exposed seven source-less query regressions. The analyzer owns exactly one
AST snapshot, but passed positions without that known source into the now
correctly fail-closed tracker.

## Implementation

Each reference copies `symbol->semanticId` when published. Valid ids use an
unsigned-value hash key and are compared by copied id. Invalid ids are excluded
from the canonical map and retain pointer-only linear recovery. No name,
coordinate, type-text, or path fallback exists.

The analyzer query-source helper supplies `analyzer->ast->location.source` only
when a query position has no source. This binding happens at the single-snapshot
owner boundary; the tracker remains strict for callers that cannot prove source
identity. The helper is extracted from the 2800-line analyzer module.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for:

- reference tracker, five scenarios;
- reaching-definition navigation `2/2`;
- source/binary/native semantic-query parity `3/3`;
- LSP source contracts, 55 pass markers.

The full analyzer runner returns real exit one on all three toolchains with
exactly the same two prior baseline markers: closed-generic receiver metadata
and borrowed-return escape range. No new Task 7.2 marker remains. Workspace,
WSL, and MSVC SHA-256 match for all seven code/test paths (`7/7`).

## 状态与产出记录

- 完成时间：2026-08-28 18:55 +08:00。
- 状态：Task 7.2 子里程碑已完成；Plan 03 Task 7 继续进行。
- 完成项目：SymbolId-keyed reference index、same-id wrapper parity、same-name
  isolation、invalid-id pointer-only recovery、single-snapshot query source
  binding、三工具链 `5/2/3/55` 门禁、三处 `7/7` byte audit。
- 未完成项目：definition/references/highlights/rename relation consumer 继续
  迁移；Syntax05 释放 property/symbol exact paths 后删除对应 local consumer。
