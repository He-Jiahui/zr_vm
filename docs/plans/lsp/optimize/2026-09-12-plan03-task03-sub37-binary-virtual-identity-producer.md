---
plan_id: optimize
task: plan03-task03-sub37
status: completed
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.h
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_virtual_document_identity_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
doc_type: milestone-record
---

# Plan 03 Task 3.37: Binary Virtual Identity Producer

## Failure And Change

Binary module entries previously exposed only the physical `.zro` file URI.
That URI is still the correct compatibility origin for typed-export
coordinates, but it cannot identify a source-less declaration in a project or
survive a provider reload. The metadata projection now publishes a second,
optional virtual identity for binary module entries.

`ResolveBinaryUri` uses the existing binary path resolver as its origin and
serializes the module name, owning project URI, physical origin URI and the
current nonzero provider generation through the established scoped virtual
document identity format. A physical URI is never relabeled as a virtual
document. `SZrLspResolvedImportedModuleEntry` keeps both values explicit:
`declarationUri` remains the physical coordinate origin and
`virtualDeclarationUri` is the metadata-owned identity for a future
source-less projection.

The project parser callback now returns this optional binary identity through
the same admission boundary used by native imports. Relation consumption
validates a supplied virtual URI against the provider result. Source-ranged
binary relations continue to return the physical `.zro` location; a future
sourceless relation can use the validated virtual URI and its module-entry
range without making the LSP layer infer a target.

## Verification

The focused GCC build is `/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc`.
The parity target was rebuilt after the producer and consumer changes and
finished with exit 0. The new case
`LSP Binary Virtual Identity Is Project Scoped And Generation Bound` passed;
the full parity target had no failures.

The focused Clang build is
`/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`.
The parity target and source-contract target both finished with exit 0. The
Clang parity run also passed the new case. The Clang interface runner and the
GCC interface runner each retain only the two frozen functional failures:

- `LSP Class Member Navigation And Completion`;
- `LSP Hover And Completion Surface Explicit Exact Type Failures`.

The adjacent project module summary and source bootstrap cases pass. The
existing Clang LSan report (`20144 bytes/422 allocations`) remains a baseline
outside this slice. The GCC glob recheck completed after a short delay; no
build process was left running.

## 状态与产出记录

- Started: 2026-09-12 03:05 +08:00.
- Completed: 2026-09-12 03:43 +08:00.
- Status: completed for the binary identity/metadata producer submilestone;
  parent Task 3 and Tasks 7/8 remain in progress.
- Contract: the producer is project-scoped and generation-bound; physical
  `.zro` coordinates and virtual identity are represented separately.
- GREEN: GCC and Clang parity targets pass, source contracts pass, and both
  interface runners preserve the exact frozen two-failure set.
- Outputs: binary identity API, metadata entry field, parser callback wiring,
  relation validation, project/generation regression, and this record.
- Remaining: binary virtual-document rendering, binary member-level identity,
  source/binary sourceless origin producers, multi-definition relations,
  MSVC replay for this code slice, and the full Task 3/7/8 acceptance matrix.
