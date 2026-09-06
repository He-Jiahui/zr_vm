---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_hover.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
tests:
  - tests/language_server/test_lsp_source_contract_canonical_completion_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_project_features.c
doc_type: module-detail
---

# LSP Metadata Hover Capability Boundary

The imported-member metadata provider now formats external declaration hover
from the resolved declaration symbol and an owned content snapshot. It keeps
the existing FFI metadata, leading-comment extraction, source label, and
descriptor fallback behavior.

## Snapshot Projection

For binary, native, and other external metadata members, the provider resolves
the declaration URI and acquires the current file-version snapshot. The
snapshot markdown builder receives the resolved declaration symbol and
snapshot content; FFI metadata and leading comments are appended as separate
sections. If no markdown content can be built, the existing descriptor-based
member formatting remains the final fallback.

## No Analyzer Hover Fallback

`CreateImportedMemberHover` no longer calls
`SemanticAnalyzer_GetHoverInfo`. A missing or stale external declaration
snapshot therefore cannot trigger request-time AST hover reconstruction. The
provider still uses its existing analyzer handle for declaration and snapshot
projection helpers, but it does not ask that analyzer to infer a new hover
result.

Project-source imported members continue through the canonical source-symbol
query path. This boundary covers only the metadata provider's external
declaration integration; the remaining metadata refresh and consumer matrix
is still tracked by Plan 03 Task 7/8.

## Validation

The bounded source contract rejects `SemanticAnalyzer_GetHoverInfo` inside
`CreateImportedMemberHover` and requires the snapshot markdown builder. GCC
source-contract execution passes. GCC interface and project-feature targets
retain the existing unrelated baseline failures while native receiver hover,
descriptor-plugin navigation, and binary/native declaration navigation remain
covered by their focused cases.
