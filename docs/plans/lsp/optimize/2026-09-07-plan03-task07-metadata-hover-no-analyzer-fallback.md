---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
  - docs/plans/astra/lsp/review.md
related_module_docs:
  - docs/cli-and-tooling/lsp-metadata-hover-capability-boundary.md
doc_type: milestone-record
---

# Plan 03 Task 7.66: Metadata Hover Has No Analyzer Fallback

## Goal

Make the external imported-member hover path consume the resolved declaration
symbol and current content snapshot without re-entering the legacy semantic
analyzer hover builder.

## Contract

- `CreateImportedMemberHover` keeps the external declaration snapshot path.
- The snapshot markdown, FFI metadata, leading-comment, source-label, and
  descriptor projections remain available.
- `SemanticAnalyzer_GetHoverInfo` is not called from the bounded metadata
  provider function.
- Missing or stale canonical external content remains unavailable instead of
  causing request-time AST hover reconstruction.

## RED/GREEN

The source contract first failed on the old analyzer hover call inside
`CreateImportedMemberHover`. Removing that branch and its temporary hover
ownership converted the same contract to GREEN. The provider now appends the
source label unconditionally after the snapshot markdown projection, while the
existing descriptor formatting handles a null content result.

## Verification

- GCC rebuilt the source-contract, LSP interface, and project-features targets.
- The GCC source-contract executable exits `0`, including the metadata-hover
  no-analyzer-fallback assertion.
- Existing native receiver hover, descriptor-plugin navigation, and binary or
  native declaration navigation cases remain covered; the broader interface
  and project-feature binaries still report their pre-existing baseline
  failures.

## 状态与产出记录

- 完成时间：2026-09-07。
- 状态：Plan 03 Task 7.66 focused 子里程碑完成；Task 7、Task 3、Task 8
  及完整跨工具链矩阵仍进行中。
- 完成项目：删除 metadata provider imported-member hover 的
  `SemanticAnalyzer_GetHoverInfo` fallback；保留 snapshot markdown、FFI、leading
  comment、source label 与 descriptor fallback；增加 bounded source-contract。
- 未完成项目：metadata provider 的完整 refresh/generation contract、其余
  consumer 的 source/binary/native/stale/unresolved 矩阵、Task 3
  sourceless/provider generation producer、Task 8 的 16-target 与
  stdio/CLI smoke。
