---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_descriptor_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_descriptor_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_virtual_documents.c
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

# LSP Descriptor Metadata Coordinate Projection

## Purpose

Native descriptor plugins publish structured type/member metadata but their declaration identity can point at the physical plugin file, which is not an opened source document. The descriptor provider therefore assigns deterministic compact declaration coordinates to each type field and method. These coordinates must survive the shared semantic query and reach definition, references and document highlights without degrading to the module entry at `0:0`.

This module owns that narrow projection. It applies only to compact physical-plugin declarations resolved as `ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN`; ordinary source documents and binary artifacts keep their existing conversion contracts.

## Behavior Model

### Receiver member resolution

`ZrLanguageServer_LspSemanticQuery_ResolveAtPosition` tries the canonical receiver-type member resolver before the generic import-chain resolver. The receiver resolver consumes the analyzer's inferred receiver type and the metadata provider's resolved member fact. It does not infer a target from the member spelling.

This ordering matters for expressions such as `point.y`: a generic import-chain lookup can recognize the terminal token as an imported-module member before the more specific receiver fact is considered. When the receiver resolver succeeds, the query retains the descriptor member kind, owner type, declaration URI and exact compact declaration range. If it fails, the existing import alias, import-chain and external metadata fallbacks remain available.

### Compact descriptor coordinates

`lsp_virtual_documents.c` assigns compact one-based coordinates for physical plugin declarations:

- type `i` uses line `i + 2`;
- field `j` starts at column `j * 8 + 1`;
- method `k` starts at column `k * 8 + 129`.

`ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates` validates that structural range and converts it to zero-based LSP coordinates. The helper is intentionally independent from the binary metadata adapter: descriptor coordinates are provider-owned synthetic slots, not artifact byte columns.

`lsp_semantic_query.c` and `lsp_project_navigation.c` select this helper only after the resolved query reports descriptor-plugin `sourceKind`. Invalid or reversed ranges return unavailable rather than fabricating an empty declaration.

## Data Flow

```text
descriptor plugin type/member table
  -> compact declaration record
  -> metadata provider resolved member fact
  -> receiver-type semantic query
  -> sourceKind == NATIVE_DESCRIPTOR_PLUGIN
  -> descriptor coordinate projection
  -> definition / references / documentHighlight
```

Completion uses the same inferred receiver type and descriptor member facts. An incomplete edit such as `point.;` is supported through the incremental parser's existing last-good AST contract: a valid version 1 establishes `point`, version 2 carries the incomplete member edit, and completion reuses the preserved semantic snapshot. A first document version that never contains a valid receiver declaration is not repaired by local text inference.

## Design Rationale

The ordinary document helper deliberately returns an unavailable range when no content snapshot exists. Restoring a generic no-content fallback would leak byte columns into UTF-16 positions and weaken every provider boundary. Reusing the binary helper would also conflate artifact coordinates with descriptor-owned synthetic slots. A separate 21-line adapter makes the exceptional contract explicit and keeps both provider gates reviewable.

The semantic-query ordering change is similarly narrow. It promotes a more specific resolved receiver fact over a generic import-chain interpretation while retaining all existing fallback paths when no receiver member is resolved.

## Edge Cases And Constraints

- Compact records require positive, ordered one-based line/column values.
- Current descriptor fixtures use ASCII member names. The compact physical-plugin coordinate contract does not claim source-text UTF-16 width for arbitrary non-ASCII names.
- Module entry navigation remains the plugin-file `0:0` target; only resolved type members use compact member ranges.
- Binary metadata continues through `lsp_binary_metadata_coordinates.c`; descriptor projection does not call or emulate that adapter.
- No completion, definition or reference target is reconstructed from member name, displayed signature or raw AST text at the LSP boundary.

## Test Coverage

- `test_lsp_project_features.c` covers descriptor receiver completion, exact field/method definition, references and document highlights from source usages and compact declarations. It also covers valid version 1 to incomplete version 2 completion recovery.
- `test_lsp_project_utf16_ranges.c` freezes compact one-based to zero-based projection and invalid-range rejection.
- `test_lsp_source_contracts.c` freezes the descriptor-specific helper, its two `sourceKind` gates and independence from the binary adapter.
- `stdio_smoke.js` replays the protocol and CLI process boundary on GCC, Clang and MSVC builds.

## Open Issues

Property, constructor, meta-member and imported/native callable target identity still need the same query-shape parity. Snapshot race, cancellation, performance percentile and peak-memory gates remain part of the wider L6 plan.
