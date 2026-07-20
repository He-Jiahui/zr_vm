---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/module/lsp_module_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - user: 2026-07-20 严格执行 LSP semantic inference 计划并逐子里程碑记录产出
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_utf16_ranges.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# LSP Binary Metadata Coordinate Projection

## Purpose

Binary module artifacts preserve typed-export declaration identity as a target URI plus a one-based source line/byte-column range. They do not provide an opened text document by default. Ordinary LSP range conversion therefore cannot acquire a document snapshot and must not silently turn a valid artifact declaration into `0:0`.

This module owns the narrow conversion contract between canonical binary metadata coordinates and LSP coordinates. It is used only when the resolved provider kind is `ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA`. Project source, native virtual documents, diagnostics and ordinary editor buffers continue to use the shared content-snapshot conversion path.

## Behavior Model

### Artifact range to LSP range

`ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates` validates the artifact range before producing a result.

- If the binary URI has an incremental text snapshot, the stored one-based byte line/column is converted back to exact byte offsets. The normal position codec then maps those offsets to zero-based UTF-16 LSP positions.
- If no text snapshot exists, the artifact does not contain enough text to recalculate UTF-16 width. The helper preserves the available structural identity by mapping one-based line/column to zero-based line/character directly.
- Invalid, reversed or zero-column metadata is rejected. Consumers return unavailable instead of fabricating an empty `0:0` declaration.

The no-snapshot projection is exact for ASCII prefixes, which covers current binary declaration fixtures. Exact non-ASCII UTF-16 width requires a source snapshot or a future artifact encoding map.

### LSP position to artifact position

`ZrLanguageServer_Lsp_TryFilePositionFromBinaryMetadataCoordinates` provides the inverse lookup used when a request starts on the `.zro` declaration itself.

- With a snapshot, the regular UTF-16-to-byte codec produces the parser-style `SZrFilePosition`.
- Without a snapshot, zero-based LSP line/character is mapped to the artifact's one-based line/column coordinate space.
- Negative and overflowing protocol positions are rejected before arithmetic.

This inverse conversion allows definition, references and document highlights requested from a binary declaration to resolve the same typed-export fact that was used when navigating from source code.

## Data Flow

```text
typed export in .zro
  -> ResolveBinaryExportDeclaration
  -> SZrLspSemanticQuery.resolvedMember
  -> sourceKind == BINARY_METADATA
  -> binary metadata coordinate projection
  -> definition / references / documentHighlight
```

The outbound and inbound paths both consume the declaration range published by binary metadata. They do not search by a displayed signature, rebuild a declaration from AST text or infer a member from its spelling at the LSP boundary.

## Design Rationale

The shared document helpers deliberately return an empty range when no snapshot is available. Adding a generic no-content fallback there would reintroduce byte-column leakage for ordinary UTF-16 documents and violate the existing source-contract tests. A provider-specific module keeps the exceptional artifact coordinate contract explicit and reviewable.

The `sourceKind` gate is carried through both `lsp_semantic_query.c` and `lsp_project_navigation.c`. The semantic query path is the primary consumer; project navigation keeps the same projection for supported fallback and direct external-metadata entry points.

## Edge Cases And Constraints

- Binary module entry navigation remains the explicit `0:0` module-entry target and does not pass through export-range matching.
- A binary export declaration must have positive start/end columns and an ordered range.
- CRLF and non-ASCII prefixes are exact when a source snapshot is available because conversion is offset based.
- Without source text, the artifact currently cannot distinguish UTF-8 byte width from UTF-16 code-unit width before a declaration.
- Native descriptor virtual documents are outside this module and continue to use their rendered virtual-document ranges.
- The remaining descriptor-plugin receiver completion marker is unrelated to binary coordinate projection.

## Test Coverage

- `test_lsp_project_features.c` covers definition, references and document highlights from both source usages and the binary declaration with no binary text snapshot.
- `test_lsp_project_utf16_ranges.c` opens a binary source snapshot containing a multibyte prefix and verifies raw artifact range `[1:18-1:28]` projects to UTF-16 range `[0:16-0:26]`.
- `test_lsp_source_contracts.c` freezes the provider-specific helper names, snapshot acquisition and binary `sourceKind` gates while preserving the ordinary document-helper contract.
- `stdio_smoke.js` supplies the final protocol-level source/binary provider replay for each supported toolchain.

## Open Issues

Artifact schema work can remove the no-snapshot Unicode limitation by publishing source text, an encoding-aware line map or UTF-16 declaration columns together with a generation/digest. Until that contract exists, LSP must not claim exact no-snapshot non-ASCII width beyond the coordinates available in the artifact.
