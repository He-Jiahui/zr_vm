---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/stdio_rename.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/stdio_rename.c
plan_sources:
  - user: 2026-07-20 execute the LSP semantic inference plan and record every completed submilestone
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_source_rename_edit_cases.h
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# LSP Source Rename Workspace Edits

## Scope

This module implements the planning half of a canonical source-file rename. Before a client moves a project `.zr` file, `workspace/willRenameFiles` returns edits for the provider's explicit `%module` declaration and every import target that resolves to the provider's old `ModuleIdentity`.

The implementation is split between:

- `lsp_project_source_rename.c`, which validates the old/new URI pair, derives the new canonical module key, and collects semantic source locations without changing project state;
- `lsp_project_navigation.c`, which exposes the existing project-wide import-target traversal so opened and unopened source files use the same parsed import bindings;
- `stdio_workspace_files.c`, which merges all supported file operations in one request into one workspace edit;
- `stdio_rename.c`, which serializes the shared location list into both `changes` and snapshot-aware `documentChanges`.

## Canonical Contract

`ZrLanguageServer_LspProject_CollectSourceRenameEdits` remains the compatibility entry point for canonical locations. `ZrLanguageServer_LspProject_CollectSourceRenameEditPlan` runs the same collector and then records one `SZrLspSourceRenameDocumentSnapshot` per unique edited URI. Both accept only distinct `.zr` URIs where the old URI owns an existing project source record, the new native path remains inside the same source root, and no other record owns the new URI. Replacement text comes only from `ZrLibrary_Project_DeriveCurrentModuleKey`; a filename, raw import spelling, display string, or member name is never semantic identity.

The provider declaration is included only when its parsed string value equals the record's old canonical module name. Import edits come from parsed import bindings and their exact `modulePathLocation` ranges. Project traversal includes unopened `.zr` files under the source root, so the result does not depend on editor-open state. Quoted module declarations replace only the literal contents, while import ranges retain the canonical AST target span.

Collection is read-only. It does not rekey the project record, remove analyzers, update incremental-parser state, read the new file, or publish diagnostics. Those mutations remain in the later `workspace/didRenameFiles` path through `ZrLanguageServer_LspProject_PrepareSourceRename`.

## Snapshot Fingerprints

Every edited URI is fingerprinted before serialization:

- an opened document records its URI, LSP version, content generation, content length, and stable 64-bit content hash from the current parser text block;
- an unopened source records its URI, disk content length, and stable 64-bit disk-content hash; project traversal's synthetic version-zero cache must match the disk bytes;
- the plan stores no borrowed content buffer and no AST pointer, so validation cannot outlive mutable parser or file buffers accidentally.

`ZrLanguageServer_LspProject_ValidateSourceRenameEditPlan` recaptures every fingerprint. A version, generation, open/closed state, length, hash, missing file, or cache-versus-disk mismatch invalidates the whole plan. Validation occurs immediately before JSON serialization for each batch item.

## Protocol Behavior

The stdio handler accepts batched `files` operations. Every supported operation contributes its own derived module name, exact locations, and validated fingerprints to one workspace edit. The response publishes equivalent edits through `changes` and `documentChanges`; opened documents use the version captured with the ranges, while unopened documents emit an explicit JSON `null` version. The serializer never re-reads a newer parser version for a source-rename plan. Ordinary symbol rename keeps its existing live-version behavior.

Unsupported operations are skipped. The request returns JSON `null` when no supported operation produces an edit, including same-URI operations, non-source files, unknown records, cross-root moves, URI collisions, unchanged canonical identities, and sources with no matching declaration or import location. Snapshot capture, validation, or serialization failure discards the entire partial response instead of publishing mixed-snapshot edits.

## Failure Boundaries

- The response is a plan only; the server does not move files or mutate project identity during `willRenameFiles`.
- The client must apply the returned edits before or together with the physical rename, then send `didRenameFiles` so the old/new canonical graph edge migration can run.
- Package-root moves, package/alias export changes, `.zrp/.zrm` generation changes, binary/native provider replacement, and public type/layout contract invalidation remain separate operations.
- The server can validate only before returning the workspace edit; the client remains responsible for rejecting a later apply when its local document version no longer matches `documentChanges`.
- Cancellation accounting, concurrent stress, latency percentiles, and memory budgets remain outside this narrow submilestone.

## Validation

The snapshot RED linked only after the three plan APIs existed. GREEN first proves three exact canonical ranges and three unique fingerprints, then proves that an identical-content version increment invalidates an opened-document plan and a disk rewrite invalidates an unopened-document plan. The protocol smoke proves two opened importers serialize captured `version: 1`, the unopened provider serializes `version: null`, and the existing `didRenameFiles` hover/definition workflow remains intact.

On the isolated `HEAD 5e3c68e + 7 LSP code/test paths` snapshot, GCC 11.4, Clang 14.0, and MSVC 19.44.35228 each pass the same sixteen-target matrix with real process exits and no failure markers. Project features are `54/54`, language feature matrix `8/8`, project UTF-16 ranges `3/3`, source contracts `38/38`, and interface `90/90`. Each toolchain also passes the updated stdio/CLI smoke.
