---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_target_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_target_identity.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_target_identity.c
tests:
  - tests/language_server/test_lsp_cross_snapshot_external_reference_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: module-design
---

# Cross-Snapshot External References

## Ownership and Inputs

Parser `ExternalReferences` owns resolved reference identity, role and source
range. `lsp_cross_snapshot_references.c` searches current source analyzers and
projects those facts to LSP locations. It does not infer receivers, walk AST
members, or aggregate references by module/member display names.

Two inputs have different identity contracts:

| Target | Selection identity | Cross-snapshot query |
| --- | --- | --- |
| Source declaration | Canonical declaration URI and exact range | Resolve each external identity to a declaration, then compare exact source identity |
| Imported metadata member | Exact owner, provider generation, metadata token, signature token/hash and target kind | Compare every field of each resolved external reference |

`AppendExternal` first searches the requesting analyzer, including standalone
documents. For projects it scans the source import graph and queries each
available analyzer. Snapshot-local SymbolId is not compared across analyzers.
Location insertion uses `LspSemanticReferenceQuery_AppendRange`, preserving
source binding, negotiated position conversion and duplicate suppression.

## Validity and Lifetime

The semantic-query entry point checks document version and canonical identity
before emitting declarations or references. External metadata identity must
match the hydrated member. A nonzero provider generation must match the current
context generation; generation zero retains the parser's existing convention
for an unavailable provider generation. Cross-file candidates must have exactly the same generation
as the target. Missing owner, zero tokens/hash, unknown target kind, unresolved
facts and mismatched identity cannot produce references.

All identity strings and metadata views are borrowed from current semantic
snapshots. Temporary `ExternalReferences` arrays are freed after each analyzer.
Returned locations belong to the caller. Existing request cancellation checks
apply during file and fact traversal. This module does not add a retained cache.

Same-document external highlights also consume parser `ExternalReferences`.
Both consumers use `LspExternalTargetIdentity_MatchesReference` to compare the
complete identity. A candidate's local SymbolId need not equal the selected
target's id, but both must be valid. Candidate facts with incomplete identity or
unresolved status cannot be accepted through a same-id local reference lookup.

The highlight range projector accepts the fact's range and role, filters to the
requested document, and merges duplicate ranges with write precedence. It does
not reconstruct a reference fact from metadata or add a local declaration for
an external member. Ordinary source highlights retain the canonical SymbolId
path. Source definition navigation still uses exact declaration identity.
Module-entry and receiver type-member legacy reference adapters remain separate
pending migrations.

## Evidence and Regression

Roslyn's `FindReferences/Finders/AbstractReferenceFinder.cs` matches original
symbols through `SymbolEquivalenceComparer`; `MetadataUnifyingEquivalenceComparer.cs`
keeps stricter equality for source symbols. Rust analyzer's `ide/src/references.rs`
collects usages of a resolved definition and deduplicates exact ranges; its
`goto_ref_on_short_associated_function_complicated_type_magic_can_confuse_our_logic`
test retains no references for a same-spelling expression with another identity.
ZR uses already-published parser facts instead of resolving candidate text again.

The generated binary and native fixtures place references behind different import
aliases in two project modules and include a same-named local decoy. The sibling
analyzer's AST, symbol table and reference tracker are detached before querying.
The test then invalidates metadata/signature tokens, signature hash, owner,
generation, resolution, external marking, SymbolId and target kind independently.
Both references and same-document highlights reject those candidates. Published
read and member-write roles retain their exact ranges and highlight kinds.
Callable inference may
publish several facts at the same range, so each mutation covers every external
fact at the sibling usage before restoring the original snapshot. Target invalidation must
also reject include-declaration and highlight requests.

## File Boundaries

Traversal stays in the focused cross-snapshot module. Exact external identity
comparison is shared in `lsp_external_target_identity.c`; the reference query
module owns same-document highlight filtering and range projection.
The oversized `lsp_semantic_query.c` changes only dispatch and the existing
identity gate. Its remaining receiver-member AST traversal is a concrete future
extraction/removal boundary, not expanded by this change. Test setup is isolated
in a dedicated header; the large parity runner only registers the two cases.
