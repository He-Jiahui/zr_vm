---
plan_id: optimize
task: plan03-task03-sub37
status: partial-acceptance
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.c
tests:
  - tests/language_server/test_lsp_virtual_document_identity_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Task 3.37 Acceptance Evidence

## Scope

This record covers the metadata producer for a project-scoped, provider-
generation-bound virtual identity for binary module entries. The physical
`.zro` URI remains the declaration origin used by existing coordinate
projections. Rendering a binary virtual document, member-level virtual
coordinates, and a parser producer for genuinely sourceless facts are not part
of this slice.

## Test Inventory

- `LSP Binary Virtual Identity Is Project Scoped And Generation Bound` creates
  a real `.zro` fixture and checks that metadata exposes both the physical URI
  and a distinct scoped URI carrying module, project, origin and current
  generation.
- The same case detaches the request-time AST, verifies the parser import
  relation carries the metadata URI while semantic navigation still returns
  the physical `.zro` range, and rejects the old URI after a provider-generation
  change.
- Existing binary snapshot, external-reference, project-module and source
  bootstrap cases continue to pass in the focused runners.

## Tooling Evidence

GCC focused parity was rebuilt and ran with exit 0. Clang focused parity and
source-contract targets were rebuilt and ran with exit 0. GCC and Clang
interface runners both exit 1 only because of the frozen pair of functional
failures (`Class Member Navigation And Completion` and `Hover And Completion
Surface Explicit Exact Type Failures`); the new producer case and adjacent
project cases pass. The known Clang LSan baseline remains unchanged. GCC's
glob recheck completed before linking, and no command was left running.

## Acceptance Decision

The binary identity/producer slice is accepted for a path-scoped commit. This
does not advance the parent Task 3/7/8 status: MSVC replay, binary virtual
rendering, sourceless source/binary producers, multi-definition identity and
the complete cross-toolchain/16-target matrix remain open.
