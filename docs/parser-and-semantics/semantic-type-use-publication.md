---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_type_use.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_use.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_canonical_completion.c
tests:
  - tests/parser/test_semantic_query_type_use_cases.h
  - tests/language_server/test_lsp_type_use.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
  - docs/plans/lsp/optimize/2026-09-07-plan01-task06-sub03-generic-type-use-identity.md
doc_type: module-detail
---

# Semantic Type-Use Publication

`ZrParser_SemanticTypeUse_Publish` builds the shared TYPE reference used by native
compiler analysis and the current LSP declaration-analysis adapter. Conversion
has already produced a canonical TypeId when publication runs. Queries only read
the resulting snapshot.

The compiler's public conversion entry publishes only after the whole generic
conversion succeeds. Recursive conversion uses a private helper that does not
publish references. Once the root succeeds, the publisher pairs AST type
arguments with the canonical instance's argument TypeIds and publishes their
independent uses. Invalid generic arity or argument kinds therefore cannot leave
partially converted type-reference facts behind. Argument publication does not
perform additional type inference or create new canonical types.

## Identity and Exactness

The reference retains the use-site TypeId. A generic instance therefore keeps
`Derived<Item, 4>` while its SymbolId refers to the open `Derived` declaration.
The publisher follows the canonical generic definition edge and selects one
source type symbol with that exact definition TypeId. Display names are not
identity keys. Missing or conflicting source symbols leave SymbolId unavailable.

An exact declaration reference supplies its range. A source class can also use
its parser-owned `nameLocation`. Other declarations without an exact published
name range remain unavailable. Qualified member chains and wrapper types do not
acquire a target through the enclosed type's name.

Identifier ranges come from their AST node. Generic name ranges start at the
parser-owned `wholeRange` and cover only the name, independently of whitespace
before `<`. The range must share the AST source identity and contain the name;
column arithmetic is checked before addition. Nested generic uses retain
independent ranges, including split closing angles.

Repeated publication of the same node, role, and TypeId updates resolution and
declaration fields in place. It does not append duplicate references. A different
TypeId for an already-published node is rejected. The caller's resolved flag is
preserved: a known TypeId without a resolved symbol can still serve the existing
type query contract, while `SymbolAt` requires a resolved, nonzero SymbolId.

## Lifetime and Ownership

TypeId, SymbolId, source strings, and AST pointers belong to the semantic
snapshot. The publisher adds context-owned reference rows; it does not retain
an inferred-type temporary. Queries return borrowed fields valid only while the
same context remains alive and unchanged. Document updates build new facts.

The canonical completion projector compares the selected type-use SymbolId with
each visible candidate inside that same snapshot. Only a matching candidate with
a different instantiated TypeId receives the `Resolved Type` detail. Formatting
uses a checked bounded buffer, and completion-item construction copies the text
before local buffers or borrowed views expire.

## Validation and Limits

Parser tests exercise compiled const-generic normalization, exact source ranges,
same-name types in separate modules, missing and conflicting declarations,
unresolved references, repeated publication, and inconsistent range sources.
LSP tests exercise canonical identity, hover, direct completion projection,
document replacement, whitespace/CRLF, and nested generics. The direct test
compiles the internal completion projector itself on Windows; it does not add a
production DLL export.

This support fix removes the first generic-completion failure in full stdio
smoke. The smoke still contains an independent missing-declaration-type detail
failure, recorded in the linked milestone. Sourceless external type declarations,
qualified type segments, and remaining analyzer semantic production continue
under Plan 03; this module does not close that phase's full consumer matrix.
