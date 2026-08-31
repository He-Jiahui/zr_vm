---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_regions.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_statements.c
tests:
  - tests/parser/test_semantic_facts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 1.4: Ownership Dataflow Source Identity

## Scope

This submilestone makes ownership statement membership, region release, and
weak receiver wake range fallbacks require exact optional source identity.
Exact AST-node identity remains valid, while equal offsets on different nodes
cannot cross a missing-source boundary.

The change is below LSP protocol projection. It does not alter ownership
qualifiers, move rules, region construction, receiver guards, or diagnostic
formatting.

## TDD And Root Cause

The RED constructs a sourced statement target and a distinct sourceless read
fact at the same offsets. It checks the three ownership helpers directly:
statement membership, explicit drop/release recognition, and weak receiver
wake detection must all reject the fact.

The original sixteen tests pass and the new case is the only failure: `17
Tests / 1 Failure`, `Expected FALSE Was TRUE`. Each module's local source
comparator treated either null operand as a wildcard, so the first statement
membership check admitted the foreign fact.

## Implementation

The three comparators now accept pointer identity, including two null sources,
or string-value equality when both sources are non-null. One-sided missing
source fails closed. Existing exact-node checks and all ownership transfer
logic are unchanged.

## Verification

The fixed isolated source is HEAD `42c4bf6` plus the four exact code/test paths
from this record. WSL GCC 11.4 and Clang 14.0.0 directly passed:

- semantic facts/query/query diagnostics: `17/30/13`;
- compiler and LSP semantic-query diagnostics: `64/19`;
- LSP semantic-query parity/source contracts: `15/70`;
- parser type inference: `124/124`.

All true-green executables returned real process exit zero. The GCC and Clang
LSP interface executables retained exactly the same eight pre-existing
producer markers; failure delta is zero. MSVC, the complete 16-target matrix,
and the three stdio smoke suites were not run for this narrow lower-layer
correction.

## 状态与产出记录

- 完成时间：2026-08-31 10:28 +08:00。
- 状态：Task 1.4 ownership dataflow source identity 子里程碑已完成；
  Plan 03整体计划继续进行。
- 完成项目：statement/release/weak-wake one-sided source RED、三个ownership
  range comparator的exact optional source identity、模块契约更新、GCC/Clang
  `17/30/13/64/19/15/70/124`真实退出门禁、interface fixed8 delta 0。
- 后续项目：继续审计剩余public/property query source identity，并按Syntax05
  ownership串行处理重叠路径；MSVC、完整矩阵和stdio由后续阶段统一验收。
