# LSP Source Rename Workspace Edits

## Scope

This module implements the planning half of a canonical source-file rename. Before a client moves a project `.zr` file, `workspace/willRenameFiles` returns edits for the provider's explicit `%module` declaration and every import target that resolves to the provider's old `ModuleIdentity`.

The implementation is split between:

- `lsp_project_source_rename.c`, which validates the old/new URI pair, derives the new canonical module key, and collects semantic source locations without changing project state;
- `lsp_project_navigation.c`, which exposes the existing project-wide import-target traversal so opened and unopened source files use the same parsed import bindings;
- `stdio_workspace_files.c`, which merges all supported file operations in one request into one workspace edit;
- `stdio_rename.c`, which serializes the shared location list into both `changes` and version-aware `documentChanges`.

## Canonical Contract

`ZrLanguageServer_LspProject_CollectSourceRenameEdits` accepts only distinct `.zr` URIs where the old URI owns an existing project source record, the new native path remains inside the same source root, and no other record owns the new URI. It derives the replacement text with `ZrLibrary_Project_DeriveCurrentModuleKey`; a filename, raw import spelling, display string, or member name is never treated as semantic identity.

The provider declaration is included only when its parsed string value equals the record's old canonical module name. Import edits come from parsed import bindings and their exact `modulePathLocation` ranges. Project traversal includes unopened `.zr` files under the source root, so the result does not depend on editor-open state. Quoted module declarations replace only the literal contents, while import ranges retain the canonical AST target span.

Collection is read-only. It does not rekey the project record, remove analyzers, update incremental-parser state, read the new file, or publish diagnostics. Those mutations remain in the later `workspace/didRenameFiles` path through `ZrLanguageServer_LspProject_PrepareSourceRename`.

## Protocol Behavior

The stdio handler accepts batched `files` operations. Every supported operation contributes its own derived module name and exact locations to one workspace edit. The response publishes equivalent edits through `changes` and `documentChanges`; open documents carry their known version and unopened documents omit the version.

Unsupported operations are skipped. The request returns JSON `null` when no supported operation produces an edit, including same-URI operations, non-source files, unknown records, cross-root moves, URI collisions, unchanged canonical identities, and sources with no matching declaration or import location. A serialization failure discards the partial response instead of publishing an incomplete rename plan.

## Failure Boundaries

- The response is a plan only; the server does not move files or mutate project identity during `willRenameFiles`.
- The client must apply the returned edits before or together with the physical rename, then send `didRenameFiles` so the old/new canonical graph edge migration can run.
- Package-root moves, package/alias export changes, `.zrp/.zrm` generation changes, binary/native provider replacement, and public type/layout contract invalidation remain separate operations.
- The current stage does not add snapshot-version revalidation at workspace-edit application time, cancellation accounting, or latency and memory budgets.

## Validation

The focused project RED first failed to link because the collection API did not exist. The stdio RED then failed because `workspace/willRenameFiles` returned no import edit. GREEN proves exact ranges for a provider declaration, an opened importer, and an unopened importer, plus a same-URI no-edit boundary. The real protocol smoke proves two open importers and the provider declaration produce exactly three versioned document changes before the existing `didRenameFiles` hover and definition checks.

On the isolated `HEAD d52bd4f + 9 LSP code/test paths` snapshot, GCC 11.4, Clang 14.0, and MSVC 19.44.35228 each pass the same sixteen-target matrix with real process exits, no failure markers, project features `51/51`, descriptor/UTF range `3/3`, and source contracts `38/38`. Each toolchain also passes the stdio/CLI smoke with the canonical source rename workspace edit.
