---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/astra/lsp/review.md
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_exact_type_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
doc_type: module-detail
---

# LSP Type Query Capability Boundary

`ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition` is a presentation
projection over one parser semantic snapshot. It does not establish semantic
facts while serving a request.

## Canonical Projection

The resolver calls `ZrParser_SemanticQuery_CanonicalTypeAt` with the current
position. The returned `TypeId` must resolve in the same canonical type graph.
For an expression, the fact must be exact, precise, and carry the same `TypeId`;
the resolver copies its `SZrInferredType`. For a type reference, the reference
must be a resolved `ZR_SEMANTIC_REFERENCE_TYPE`, and the matching semantic type
record must contain a precise inferred type. Invalid, missing, approximate,
unknown, stale, or conflicting facts return no result.

The canonical query's expression, reference, and type-node pointers are borrowed
from the snapshot. The resolver does not retain them. Its output is a copied
`SZrInferredType` owned by the caller, so nested generic element types and the
`ownershipQualifier` remain available for the synchronous hover or token
projection and are released by the caller.

## Non-Fallback Boundary

The request path does not traverse the AST to locate a type node, invoke
`ZrParser_ExpressionType_Infer`, search the LSP symbol table, match a name, or
call the declared-type builder. Those operations belong to analysis producers
that publish facts before the snapshot is queried. If a producer has not
published a canonical type or the snapshot is no longer coherent, the LSP
surface stays unavailable instead of inventing a weak object type.

## Validation

The semantic analyzer regression first proves that an exact literal expression
is available through parser `TypeAt`, then marks its fact approximate. It also
detaches the analyzer semantic context while leaving the AST and compiler state
available; the old AST fallback would return `int`, while the canonical resolver
returns false. LSP interface coverage keeps canonical `Vector3` receiver
resolution and `Unique<Socket>` type annotation projection working. The source
contract checks the canonical query and `TypeId` bridge and rejects all former
request-time fallback symbols. GCC and Clang ASan/UBSan focused targets pass;
the broader analyzer and interface runners retain their previously recorded
baseline failures.
