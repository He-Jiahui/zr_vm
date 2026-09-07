---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_type_lifetime_cases.h
  - tests/parser/test_canonical_type_graph.c
  - tests/language_server/test_semantic_analyzer.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: module-detail
---

# Semantic Call Fact Ownership

## Publication Contract

The parser publishes a primary call reference with the resolved callable's
`SymbolId`, instantiated canonical function `TypeId`, declaration range,
signature display, and argument mapping. LSP consumers query this fact from the
owning semantic snapshot. They do not instantiate the signature again.

For a canonical declared function, signature instantiation preserves declaration
contracts through `ZrParser_CanonicalType_RebindFunctionSignature`: parameter
passing and escape contracts, outer reference access, receiver effect, and
callable effects remain attached to the resolved types. A missing canonical
declared function retains the existing syntax-callable refinement path. Failure
to construct a valid call type prevents publication.

## Borrowed Type Lifetime

`ZrParser_CanonicalType_Find` returns a borrowed pointer into
`context->canonicalTypes`. Interning a signature or one of its component types
can grow that array and invalidate the pointer. The call producer therefore
copies the declared-function validity, receiver effect, and effect flags before
`ZrParser_CanonicalType_FromFunctionSignature`. Its later branch uses the copied
validity, and the rebinder resolves records again through their stable ids.

The ids belong to the current semantic context. Copying an id does not preserve
a record across context reset, provider reload, or snapshot replacement.
Published facts and mapping arrays remain owned by the semantic context under
the existing append/reset/free contract.

## Regression and Scope

The deterministic regression fills the canonical type array and forces the next
growth to move its storage. A test allocator copies the records and quarantines
the cleared old allocation until publication finishes. A stale function-kind
check therefore chooses the wrong branch independently of allocator behavior.
The fixture checks the call identity, substituted return pointee, callable
effect, and preservation of the declaration's readonly return access.

The allocator is restored and both old and replacement allocations are released
before final assertions. The original implementation loses readonly access;
the corrected implementation preserves it. The broad Clang analyzer replay
also checks the generic-call path that originally reported a use-after-free.

The production change is a narrow lifetime correction in the existing call-fact
producer. This is a written exception for editing that large file without
splitting it: no responsibility or public API is added. The allocator machinery
lives in a separate test header.
