---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_selection_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 4.27: Ambiguous Call Expression Fail-Closed

## Scope

This submilestone removes publication-order selection when `CallAt` sees two
equally narrow call expression facts at the same exactness priority but their
canonical call shapes disagree. It does not change call fact producers,
receiver inference, overload resolution, LSP projection, or formatting.

## TDD And Root Cause

The RED appends two exact, node-less call expression facts with the same
call-site range and width but different call-target ranges. The first fact has
a complete resolved callable reference. The old query returned that first
fact, so reversing publication order could change success and selected target.
All existing 29 call tests pass and the new case is the only failure:
`30 Tests / 1 Failure`.

`CallAt` ranked expression facts only by range width and whether exactness
allowed projection. Equal-ranked facts retained the first publication without
checking whether they represented the same call.

## Implementation

The selection pass now tracks conflicts at the current best rank. A narrower
fact or an exact fact replacing a weaker fact resets the conflict state.
Equal-ranked facts are duplicates only when their full ranges, expression
kind/exactness, optional target name, result TypeId, argument count,
named-argument state, and member-call state agree. Any disagreement makes
`CallAt` clear its output and return false before reference selection.

## Verification

The isolated baseline is HEAD `7b8e652` plus the three exact code/test paths.
WSL GCC 11.4 and Clang 14.0.0 each built without new diagnostics and directly
passed the same executables with real process exit zero:

- semantic query calls: `30/30`;
- semantic query: `30/30`;
- canonical consumers: `21/21`;
- LSP semantic query parity: `15/15`;
- LSP source contracts: `70/70`.

MSVC, the complete 16-target matrix, the interface runtime, and the three stdio
smoke suites were not run for this parser-only correction.

## 状态与产出记录

- 完成时间：2026-08-31 09:12 +08:00。
- 状态：Task 4.27 ambiguous call expression 子里程碑已完成；Plan 03
  Task 4 与整体计划继续进行。
- 完成项目：equal-rank expression RED、完整 call-shape consistency gate、
  output fail-closed、模块契约更新、GCC/Clang `30/30/21/15/70` 真实退出门禁。
- 后续项目：继续等待 Syntax05 发布 receiver/member 与 binary/native callable
  producer，再完成 Task 4 首项和跨来源 parity；MSVC、完整矩阵与 stdio 由后续总门禁验收。
