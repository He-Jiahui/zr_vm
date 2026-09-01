---
related_code:
  - tests/language_server/test_semantic_analyzer.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
tests:
  - tests/language_server/test_semantic_analyzer.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/index.md
doc_type: milestone-record
---

# Plan 03 Task 7.60: Source-aware Fact Query Fixtures

## Goal

Reconcile the long-running semantic-analyzer regression tests with the canonical semantic fact
source-identity contract after Task 1.2 made position queries fail closed for a missing source.

## Contract

- A fact query at a named source range may match only facts from that same source identity.
- A range helper used by an analyzer test must preserve the fixture's `sourceName` when querying
  reference, reachability, logical, or ownership facts.
- This slice changes test evidence only. It does not relax `Find*AtPosition`, compare offsets across
  sources, or add a type/name/message fallback in the LSP.

## TDD

The full semantic-analyzer target had 14 failures. Twelve were direct `Find*AtPosition` queries
constructed by `file_range_for_nth_substring` with `source == NULL`, even though the AST was parsed
with a named source. The failures covered reference, reachability, short-circuit, ownership, and
unresolved-member fact assertions. The test now provides source-aware variants of the existing range
helpers and uses them only at those fact-query sites.

## Verification

- MSVC isolated semantic-analyzer snapshot: the 12 source-identity fixture failures are PASS after
  rebuilding and running the complete test executable.
- The two remaining generic producer failures are unchanged and remain outside this test-only slice:
  `Closed Generic Receiver Calls Stay Local To Type Metadata` and
  `Preserves Owner Generic Context In Member Signatures`.
- The full current 16-target matrix and the three stdio/CLI smokes remain pending for Task 8.

## 状态与产出记录

- 完成时间：2026-09-02 01:46 +08:00。
- 状态：Task 7.60 source-aware analyzer fact-query fixtures GREEN；Task 7 与 Task 8 继续进行。
- 完成项目：reference/reachability/logical/ownership/unresolved fact ranges 绑定具名 source；
  12 项历史夹具失败恢复为 PASS；主计划与 semantic-query foundation 文档更新。
- 未完成项目：两个 generic producer 缺口；Task 7 consumer 总迁移；source/binary/native、
  stale/unresolved 完整 consumer 矩阵；完整16-target与三套stdio/CLI smoke。
