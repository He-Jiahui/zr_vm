---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
tests:
  - tests/parser/test_semantic_query_symbols.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.4: Visible Symbol Source Identity

## Scope

This submilestone makes `ZrParser_SemanticQuery_VisibleSymbols` require exact
optional source identity while selecting a lexical scope and checking
declaration availability. A sourceless position cannot enter a sourced scope
merely because the offsets overlap.

The change does not modify compiler scope production, shadowing, overload
selection, completion projection, or LSP fallback behavior. It closes a public
query contract gap at the parser-owned snapshot boundary.

## TDD And Root Cause

The RED publishes one sourced module scope and one visible variable. A sourced
position first proves that the fixture returns exactly one symbol. The same
output array is then queried at the same offset with no source identity and is
expected to fail closed and clear its length.

The original twenty-one symbol-query tests pass, while the new assertion is the
only failure: `22 Tests / 1 Failure`, `Expected FALSE Was TRUE`.
`semantic_query_symbols_same_source` treated either null source as a wildcard,
so the sourceless position selected the sourced scope and projected its symbol.

## Implementation

The symbol-query source comparator now accepts null only when both operands are
null. Existing pointer equality and non-null string-value equality remain
unchanged. The one helper is shared by scope containment and declaration
availability, keeping both checks on the same exact identity rule.

## Verification

The fixed isolated source is HEAD `385e9e8` plus the two exact code/test paths
from this record. WSL GCC 11.4 and Clang 14.0.0 directly passed:

- semantic query symbols: `22/22`;
- LSP semantic-query parity: `15/15`;
- LSP source contracts: `70/70`.

All executables returned real process exit zero. The GCC and Clang LSP interface
executables retained exactly the same eight pre-existing producer markers;
failure delta is zero. MSVC, the complete 16-target matrix, and the three stdio
smoke suites were not run for this narrow parser query correction.

## 状态与产出记录

- 完成时间：2026-08-31 09:48 +08:00。
- 状态：Task 2.4 visible-symbol source identity 子里程碑已完成；Plan 03
  整体计划继续进行。
- 完成项目：one-sided source RED、`VisibleSymbols` scope/declaration exact
  optional identity、reused output fail-closed、模块契约更新、GCC/Clang
  `22/15/70`真实退出门禁、interface fixed8 delta 0。
- 后续项目：继续完成binary/native scope producer、Task 7 consumer迁移和
  Task 8总门禁；MSVC、完整矩阵和stdio由后续阶段统一验收。
