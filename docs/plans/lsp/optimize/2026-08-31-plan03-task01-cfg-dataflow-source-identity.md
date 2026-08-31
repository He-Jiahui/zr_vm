---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_reaching_definitions.c
tests:
  - tests/parser/test_semantic_facts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 1.3: CFG Dataflow Source Identity

## Scope

This submilestone makes CFG-backed definite-assignment and reaching-definition
analysis map semantic reference facts to AST statements only when their
optional source identities match exactly. A missing source is an identity, not
a wildcard for a sourced snapshot.

The change is below LSP protocol projection. It does not alter CFG topology,
symbol identity, assignment joins, reaching-definition joins, or diagnostic
formatting.

## TDD And Root Cause

The RED parses a sourced function but publishes declaration, write, and read
facts at the same offsets with no source. Neither dataflow pass may attach
results to those facts through the sourced AST.

The original fifteen tests pass and the new case is the only failure: `16
Tests / 1 Failure`, `Expected FALSE Was TRUE`. Both local source comparators
treated either null operand as a wildcard, so statement containment admitted
the foreign facts and definite-assignment published a state.

## Implementation

Both comparators now accept pointer identity, including two null sources, or
string-value equality when both sources are non-null. One-sided missing source
fails closed. Range containment, symbol maps, transfer functions, and join
ordering are unchanged.

## Verification

The fixed isolated source is HEAD `fc0bdc1` plus the three exact code/test
paths from this record. WSL GCC 11.4 and Clang 14.0.0 directly passed:

- semantic facts/query/query diagnostics: `16/30/13`;
- compiler and LSP semantic-query diagnostics: `64/19`;
- LSP semantic-query parity/source contracts: `15/70`;
- parser type inference: `124/124`.

All true-green executables returned real process exit zero. The GCC and Clang
LSP interface executables retained exactly the same eight pre-existing
producer markers; failure delta is zero. MSVC, the complete 16-target matrix,
and the three stdio smoke suites were not run for this narrow lower-layer
correction.

## 状态与产出记录

- 完成时间：2026-08-31 10:16 +08:00。
- 状态：Task 1.3 CFG dataflow source identity 子里程碑已完成；Plan 03
  整体计划继续进行。
- 完成项目：sourceless fact/sourced AST RED、definite-assignment与
  reaching-definition exact optional source identity、模块契约更新、GCC/Clang
  `16/30/13/64/19/15/70/124`真实退出门禁、interface fixed8 delta 0。
- 后续项目：继续审计property与ownership lower-layer source identity，并按
  Syntax05 ownership串行处理重叠路径；MSVC、完整矩阵和stdio由后续阶段统一验收。
