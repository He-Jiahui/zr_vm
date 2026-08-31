---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
tests:
  - tests/parser/test_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 6.39: Query Scope Source Identity

## Scope

This submilestone makes the shared semantic-query position and node-scope
range checks use exact optional source identity. A missing source now matches
only another missing source. Non-null sources still match by pointer identity
or string value.

The change is exercised through diagnostic materialization because a
source-unknown node scope previously admitted a source-known persistent
diagnostic when their offsets overlapped. The same comparison helper also
protects `TypeAt`, `CanonicalTypeAt`, fact lookup, and reference definition
queries. No LSP fallback or protocol projection changed.

## TDD And Root Cause

The RED creates a persistent structured diagnostic with source identity and a
node scope whose root has the same containing offsets but no source identity.
The existing eleven parser diagnostic tests pass, while the new test is the
only failure: `12 Tests / 1 Failure`, expected zero diagnostics and received
one.

`semantic_query_same_source` treated either null operand as a wildcard. This
was older than the exact optional identity contracts already used by relation,
call-edge, call-at, reference, and LSP diagnostic duplicate matching.

## Implementation

The shared source comparator now returns equality for a null operand only when
both operands are null. Pointer equality and non-null string-value equality
remain unchanged. Range containment and position matching therefore fail
closed before considering identical offsets from a different or unknown
source.

## Verification

The fixed isolated source is HEAD `0238f97` plus the two exact code/test paths
from this record. WSL GCC 11.4 and Clang 14.0.0 directly
passed the same four executables with real process exit zero:

- semantic query: `30/30`;
- semantic query contract: `4/4`;
- parser semantic query diagnostics: `12/12`;
- compiler semantic query diagnostics: `64/64`.

MSVC, the complete 16-target matrix, and the three stdio smoke suites were not
run for this narrow parser query correction.

## 状态与产出记录

- 完成时间：2026-08-31 08:46 +08:00。
- 状态：Task 6.39 query-scope source identity 子里程碑已完成；Plan 03
  Task 6 与整体计划继续进行。
- 完成项目：one-sided source RED、公共 semantic-query exact optional
  source identity、node-scope diagnostic fail-closed、模块契约更新、GCC/Clang
  `30/4/12/64` 真实退出门禁。
- 后续项目：继续迁移 analyzer-side structured producers，完成 compiler/LSP
  golden parity 与 Task 6 总门禁；MSVC、完整矩阵和 stdio 由后续阶段统一验收。
