---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
related_module_docs:
  - docs/cli-and-tooling/lsp-hover-capability-boundary.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
  - docs/plans/astra/lsp/review.md
doc_type: milestone-record
---

# Plan 03 Task 7.65: Public Hover Has No Analyzer Fallback

## Goal

Make the public hover request path read-only over canonical semantic and local
query results. The request must not re-enter the legacy analyzer hover builder
to infer a type or signature from the AST after canonical projection fails.

## Contract

- `Lsp_GetHover` tries structured metadata, canonical signature, semantic
  query, signature projection, and local semantic query providers in their
  existing order.
- Its remaining symbol markdown fallback uses the current content snapshot and
  projected symbol only.
- `SemanticAnalyzer_GetHoverInfo` is not called from the public hover request.
- The metadata provider's separate external declaration path remains outside
  this slice and is tracked independently.

## RED/GREEN

The source contract first failed on the old `SemanticAnalyzer_GetHoverInfo`
call inside the bounded `Lsp_GetHover` function. Removing that branch and its
temporary `SZrHoverInfo` ownership converted the same contract to GREEN. The
remaining markdown fallback keeps the symbol range and content snapshot path
intact.

## Verification

- GCC rebuilt the source-contract and LSP interface targets successfully.
- GCC source-contract executable exits `0`, including the public-hover
  no-analyzer-fallback assertion.
- GCC interface output keeps canonical native receiver hover, native construct
  completion/signature, and canonical call hover cases passing.
- The existing container-matrix hover failure remains recorded; this leaf does
  not claim the complete hover matrix or Plan 03 Task 8.

## 状态与产出记录

- 完成时间：2026-09-07。
- 状态：Plan 03 Task 7.65 focused 子里程碑完成；Task 7、Task 3、Task 8
  及完整跨工具链矩阵仍进行中。
- 完成项目：删除公开 `Lsp_GetHover` 的 analyzer hover fallback；增加
  source-contract；保留 canonical/local query 与 snapshot markdown 路径。
- 未完成项目：metadata provider external hover fallback、其余 consumer 的
  source/binary/native/stale/unresolved 矩阵、Task 3 sourceless/provider
  generation producer、Task 8 的 16-target 与 stdio/CLI smoke。
