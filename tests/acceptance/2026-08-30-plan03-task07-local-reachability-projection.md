---
plan: docs/plans/lsp/optimize/03-canonical-semantic-query.md
implementation:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
tests:
  - tests/language_server/test_lsp_local_semantic_query.c
doc_type: acceptance-record
---

# Plan 03 Task 7.37 Acceptance: Local Reachability Projection

## Acceptance contract

At a logical operator query point, the consumer may use only the direct reachability fact or
the exact `relatedNode` carried by an already-resolved logical fact. It must not infer a fact from
source text, identifier names, diagnostic messages, types, or member names.

## Evidence

- GCC, Clang, and MSVC rebuilt the local semantic query target with real exit `0`.
- The short-circuit local query case passed on all three toolchains after projecting from
  `logicalFact.relatedNode` to the right operand's canonical range.
- The member-write case remains a producer failure: the parser fact is unresolved and has the
  member-access kind, so the LSP consumer does not rewrite it.
- Interface and stdio smoke were rerun after the production change. Their real exits remain
  `1` for the known class-member fixture and missing `short_circuit_unreachable` producer warning.
- The valid 16-target replay completed at `2026-08-30 07:44 +08:00` with the same
  `10 PASS / 6 FAIL` process pattern on GCC, Clang, and MSVC. Per-process real exits matched
  the markers; the Task 7.37 short-circuit case remained passing, and all three CLI
  `--version` checks exited `0`.

## 状态与产出记录

- 完成时间：2026-08-30 07:29 +08:00。
- 最终矩阵/CLI 回放时间：2026-08-30 07:44 +08:00。
- 状态：focused local reachability projection GREEN；global Plan 03 remains in progress。
- 完成项目：direct/range reachability lookup、logical `relatedNode` projection、三工具链
  focused verification and post-change interface/stdio gate audit。
- 未完成项目：parser member-write fact correction、remaining producer/metadata ownership,
  semantic-token canonical migration, and final 16-target/stdio all-green gate。
