---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
related_module_docs:
  - docs/cli-and-tooling/lsp-completion-capability-boundary.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
  - docs/plans/astra/lsp/review.md
doc_type: milestone-record
---

# Plan 03 Task 7.64: Completion Consumer Does Not Reanalyze

## Goal

Keep completion request handling read-only after the canonical visible-symbol,
receiver, and metadata providers have been queried. A missing completion must
not trigger a second semantic analyzer or request-time AST analysis.

## Contract

- `CollectCompletionItems` uses the current analyzer snapshot and existing
  canonical completion/metadata paths.
- It does not call `GetOrCreateScopedQueryAnalyzer`, `Analyze`, `AnalyzeScope`,
  or `FindAnalysisRootAtPosition` while serving a request.
- Missing, stale, or unavailable facts remain unavailable; the request does
  not materialize replacement facts from the AST.
- The canonical completion projector remains responsible for copying
  snapshot-borrowed display and documentation strings into completion items.

## RED/GREEN

The new source contract first ran against the old consumer and reported the
scoped analyzer, analysis, fallback variable, and analysis-root symbols. The
fallback branch was then removed. The same contract passes and the existing
native-construct completion regression still proves that an exact receiver
fact produces descriptor members while an unavailable fact returns an empty
result.

## Verification

- GCC rebuilt `zr_vm_language_server_lsp_source_contracts_test` and
  `zr_vm_language_server_lsp_interface_test` successfully.
- GCC source-contract executable exits `0`, including the new no-reanalysis
  assertion.
- Native construct completion and signature fail-closed cases remain PASS in
  the interface runner.
- The broad interface runner still has the previously recorded
  container-matrix hover failure; this leaf does not claim the full Plan 03
  consumer matrix or Task 8 gate.

## 状态与产出记录

- 完成时间：2026-09-07。
- 状态：Plan 03 Task 7.64 focused 子里程碑完成；Task 7、Task 3、Task 8
  及完整跨工具链矩阵仍进行中。
- 完成项目：删除 completion consumer 的 request-time scoped analyzer
  fallback；增加 source-contract 约束；保留 canonical completion 与
  fail-closed receiver regression。
- 未完成项目：其余 consumer 的 source/binary/native/stale/unresolved
  完整矩阵、Task 3 sourceless/provider-generation producer、Task 8 的
  16-target 与 stdio/CLI smoke。
