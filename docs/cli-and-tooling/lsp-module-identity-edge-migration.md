---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
tests:
  - tests/language_server/test_lsp_project_module_identity_edge_cases.h
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/2026-09-07-plan01-task06-sub07-rename-canonical-type-assertions.md
doc_type: module-detail
---

# LSP ModuleIdentity Edge Migration

## Scope

This module covers source-file rename notifications where a project source moves from one canonical module path to another. It preserves enough old project-record identity for the ordinary public-contract refresh to invalidate importers attached to both the removed and added `ModuleIdentity` edges.

The implementation is split between:

- `lsp_project_source_rename.c`, which rekeys an existing source record from the old URI to the new URI while retaining its previous canonical module name and public-contract hash until the new document is analyzed;
- `lsp_project.c`, which seeds reverse-dependency traversal from both the previous and current canonical module names and deduplicates overlapping importers;
- `stdio_workspace_files.c`, which routes same-project `.zr` entries from `workspace/didRenameFiles` through this migration path before reading the renamed file from disk.

## Canonical Contract

`ZrLanguageServer_LspProject_PrepareSourceRename` accepts only `.zr` URI pairs whose old URI already owns a project source record and whose new native path remains inside that record's project source root. A collision with a different record rejects migration. Cross-project moves, non-source files, unknown records, invalid URIs, and allocation failures retain the existing delete/create fallback.

Successful preparation removes the old analyzer and incremental-parser entry, then changes only the record URI and path. It deliberately preserves the old `moduleName`, public-contract hash, export count, and availability. The subsequent standard document update parses the new path, publishes its canonical module identity and contract, and compares them with that retained snapshot. No raw filename, import literal, member name, display text, or AST spelling becomes a semantic target identity.

When the public contract changes, reverse traversal first queues importers of the removed identity and then importers of the added identity. Both passes share one `discovered` URI set, so a document importing both names is analyzed exactly once. Transitive traversal continues to use each successfully refreshed record's canonical module name.

## Protocol Behavior

For a supported source rename, `workspace/didRenameFiles` clears diagnostics for the old URI and updates the new URI from disk. The regular update path then publishes diagnostics for the new URI and refreshes affected open importers. Other create/delete/rename operations continue through the prior file-operation handlers.

The protocol test renames `legacy.zr` to `modern.zr`, updates `module legacy;` to `module modern;`, and keeps two open importers. One importer uses the removed identity; the other imports both old and new identities to fix the deduplication boundary. After the notification, hover on the added edge reports `Resolved Type: double` and definition resolves to the renamed source URI. The provider's source annotation `float` maps to the canonical DOUBLE primitive; inferred display is checked against that canonical contract.

## Failure Boundaries

- Preparation is not a filesystem move and runs only after the client has completed the physical rename.
- Invalid renamed source content may still fail the ordinary document update; the migration path does not fabricate a module identity from the new filename.
- Package-root moves, cross-project moves, `.zrp/.zrm` generation changes, public import/export hash coverage, and binary/native provider replacement remain separate graph operations.
- The stage does not add `willRenameFiles` workspace edits for rewriting import specifiers.

## Validation

The focused project test first failed to link because the source-rename preparation API did not exist. The completed test records exactly two reverse-dependency analyses: the old-only importer once and the overlapping old/new importer once. Its current assertion queries the cached local's canonical primitive before and after migration: OBJECT while the new provider is absent, then DOUBLE after the rename. It also checks the exact hover type section. The original pre-canonical hover assertion used the source spelling `float`.

On the isolated `HEAD 229022f + 7 LSP code/test paths` snapshot, GCC 11.4, Clang 14.0, and MSVC 19.44.35228 each pass the same sixteen-target matrix with real process exits, no failure markers, project features `50/50`, descriptor/UTF range `3/3`, and source contracts `38/38`. Each toolchain also passes the stdio/CLI smoke with the real rename notification.

The 2026-09-07 replay passes the updated migration case on GCC, Clang ASan/UBSan
and MSVC. The full project runner retains ten unrelated functional failures and
Clang reports 19,160 bytes in 481 leaked allocations. A three-file protocol probe
passes canonical hover and exact renamed definition checks on all three builds;
MSVC full stdio smoke passes. Linux full smoke stops earlier at the separately
recorded CLI binary-member identity failure. These results do not supersede the
remaining parent acceptance gates.

Canonical nodes and query results are borrowed from the current analyzer snapshot.
The test checks primitive kind and value type in each snapshot; it does not compare
numeric TypeIds across reanalysis or retain node pointers across the rename.
