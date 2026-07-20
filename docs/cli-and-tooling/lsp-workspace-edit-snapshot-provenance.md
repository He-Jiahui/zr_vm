---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/stdio/stdio_rename.c
  - zr_vm_language_server/stdio/stdio_editing.c
  - zr_vm_language_server/stdio/stdio_editing_json.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_workspace_edit_snapshot.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c
  - zr_vm_language_server/stdio/stdio_editing.c
  - zr_vm_language_server/stdio/stdio_editing_json.c
  - zr_vm_language_server/stdio/stdio_rename.c
  - zr_vm_language_server/stdio/stdio_workspace_files.c
plan_sources:
  - user: 2026-07-21 strict LSP semantic inference plan execution with per-submilestone records and commits
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/test_lsp_project_source_rename_edit_cases.h
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# LSP Workspace Edit Snapshot Provenance

## Scope

This module provides the shared capture and revalidation boundary for workspace edits whose ranges were computed from one or more documents. Ordinary `textDocument/rename`, source-file `workspace/willRenameFiles`, and `textDocument/codeAction` capture the affected document fingerprints before publishing edits, validate them after their producers return, and discard or disable the edit when any document no longer matches.

The module does not derive rename targets or replacement text. Those remain canonical semantic-query and project-graph outputs. Snapshot fingerprints only prove that the content and document origin used to compute those locations are still current.

## Explicit Document Provenance

`SZrFileVersion` and `SZrFileVersionContentSnapshot` carry `isOpenDocument` explicitly. Numeric version is not an origin tag: an LSP client may legally send `didOpen` with version `0`, while project loading also uses synthetic version `0` for disk-backed cache entries.

`ZrLanguageServer_IncrementalParser_UpdateOpenDocument` permits exactly one equal-version origin transition when a synthetic closed cache entry becomes an opened overlay. A disk-backed version `0` may therefore become client-owned version `0` without being rejected as stale. After the document is open, equal or lower versions are rejected by the existing monotonic gate. Internal project/provider refreshes continue to create closed disk provenance.

This distinction is retained in acquired content snapshots and consumed directly by workspace-edit capture. No producer infers open state from version, URI shape, member name, source text, or whether a project record happens to exist.

## Transient Snapshot Contract

`SZrLspWorkspaceEditDocumentSnapshot` is a request-scoped POD containing URI, stable content hash, content length, LSP version, content generation, and explicit open state. `SZrLspSourceRenameDocumentSnapshot` remains a compatibility alias to the same layout.

The shared API is:

- `ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot` and `ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot`, which bind a single producer to one exact document fingerprint;
- `ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshots`, which deduplicates edited locations by URI and captures one fingerprint per document;
- `ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshots`, which recaptures every URI and requires exact equality of all fingerprint fields;
- `ZrLanguageServer_LspWorkspaceEdit_FindDocumentSnapshot`, which supplies the captured version to `TextDocumentEdit` serialization.

Opened documents are captured from an acquired incremental-parser content snapshot. Unopened documents are read from disk. When a closed cache record already exists, acquisition must succeed and its hash and length must match the disk bytes; a missing cache text block is a validation failure rather than permission to fall back silently. Missing files, disk/cache mismatch, open-state changes, version or generation changes, and content hash or length changes invalidate the entire edit plan.

The stable hash is a change detector, not semantic identity. The transient POD does not alter semantic-fact, public-contract, cache-key, artifact, or binary schemas. The persistent document snapshot schema changes only by publishing explicit open provenance.

## Producer Integration

Ordinary `textDocument/rename` captures the semantic locations returned for the request, validates them before serialization, and passes the snapshot array to the common workspace-edit serializer. `documentChanges` therefore uses the version captured with the ranges. An opened version-zero document emits `version: 0`; an unopened disk document emits `version: null`.

Source-file rename keeps its existing project compatibility entry points. `CollectSourceRenameEditPlan`, `ValidateSourceRenameEditPlan`, and `FindSourceRenameDocumentSnapshot` delegate to the generic snapshot API, so `workspace/willRenameFiles` and ordinary rename share one fingerprint contract without changing canonical ModuleIdentity planning.

`textDocument/codeAction` captures the request document before converting the requested range or generating edits, validates it after the producer returns, and serializes only the captured version. Its opaque `data.snapshot` carries version, generation, open state, length, and a fixed-width hexadecimal hash. `codeAction/resolve` validates that token again: a stale or malformed token removes the edit and returns a disabled action instead of reconstructing an edit from title, kind, diagnostic text, or current source.

Any capture, validation, lookup, or serialization failure rejects the whole workspace edit. The server does not publish a partial response assembled from mixed generations, and it does not refresh locations or reconstruct edits from names or text after a mismatch.

## Validation

The incremental-parser regression opens a synthetic disk version-zero entry with client version zero, then proves later same/stale versions remain rejected. Interface coverage proves single and multi-document capture/find/validate APIs and ordinary rename version-zero serialization. Project coverage keeps source-rename opened/unopened fingerprints, rejects a missing closed cache snapshot, and rejects disk drift. The protocol smoke performs rename before the first `didChange`, verifies captured `version: 0`, then advances the document to version `2`; its code-action section separately proves version-zero opaque data, stale resolve disabling after a same-content version increment, and a fresh version-one resolve.

The isolated source snapshot `.codex/snapshots/l6-general-rename-red-gcc-r1` matches all fifteen LSP code/test paths. GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228 each pass the same sixteen-target matrix with real process exit `0` and zero failure markers; incremental parser is `8/8`, interface is `90/90`, project features are `54/54`, language feature matrix is `8/8`, UTF-16 ranges are `3/3`, and source contracts are `38/38`. Each toolchain also passes the updated stdio/CLI smoke with real exit `0`.

The code-action follow-up snapshot `.codex/snapshots/l6-code-action-red-gcc-r1` matches its eight LSP code/test paths. The same three toolchains each pass the preceding sixteen targets plus advanced editor features, for `17/17` real exits and zero markers; interface is `90` Pass and advanced editor is `39` Pass. All three updated stdio/CLI smoke runs exit `0`.

Ordinary rename, source-file rename, and current code actions now use this helper. Parser diagnostic safe-fix, workspace-command, and other workspace-edit producers remain open, along with cancellation, concurrent stale-response stress, latency percentiles, and peak-memory budgets.
