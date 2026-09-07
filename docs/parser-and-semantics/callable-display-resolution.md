---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
tests:
  - tests/parser/test_semantic_display_unresolved_cases.h
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_query_calls.c
  - tests/language_server/test_lsp_unresolved_callable_display_cases.h
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/2026-09-07-plan01-task06-sub04-unresolved-callable-display.md
doc_type: module-detail
---

# Callable Display Resolution

`CreateCallableSignature` formats a source callable's registered canonical
contract and parser-owned parameter names. An explicit type use can have a
canonical nominal TypeId while binding has failed; its retained name is not
evidence of successful resolution.

## Resolution and Exactness

During signature construction, the producer checks published TYPE references
with the exact annotation node identity. Any unresolved fact or TypeId that
disagrees with the corresponding callable type renders that position as
`cannot infer exact type`. Parameter and return resolution are independent.
Reference-passing parameters compare the displayed pointee TypeId, while their
passing prefix remains part of the canonical parameter contract.

Canonical-only contracts without a TYPE reference retain their existing
formatting behavior. This change makes explicit negative evidence authoritative;
it does not establish universal annotation-fact availability or add an error
type to the canonical graph. Those producer-coverage boundaries remain part of
the full Plan 03 matrix. Missing canonical nodes, malformed callable contracts,
and insufficient signature capacity still fail signature construction.

## Publication and Ownership

`PublishCallableSignature` runs during source-scope analysis, after declaration
and type references have been produced. It creates one state-owned string and
updates resolved references whose SymbolId and TypeId both match that callable.
The scope's visible-symbol fact receives the same signature. Different overload
identities, specialized TypeIds and unresolved symbol references retain their
own state; there is no name-based propagation.

The string, IDs and AST nodes belong to the semantic snapshot. Publication may
replace borrowed signature views and must finish before consumers query that
snapshot. Queries cannot invoke the publisher. Completion and hover read the
stored signature and copy text into request-owned results. A document update
builds and publishes facts for the replacement snapshot.

## Validation

Parser tests cover unresolved parameters, unresolved returns, both together,
resolved controls and conflicting TypeIds. Every case also verifies publication
to the matching reference and preservation of references with a different
SymbolId, different TypeId or unresolved symbol identity. Existing malformed
contract, effect/passing, symbol and call-query tests protect canonical display
behavior. LSP regressions check the public completion and hover paths for the
same unresolved source declaration and verify that a replacement snapshot
recovers its resolved signature once the missing type is declared.
