---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/parser/test_expression_fragment_parser.c
  - docs/plans/lsp/04-debug-and-repl/2026-07-28-e2b0-canonical-binding-injection.md
doc_type: module-detail
---

# Canonical External Binding Injection

## Contract

`ZrParser_TypeEnvironment_RegisterCanonicalVariable` registers a name in a
temporary type environment while retaining identity supplied by an already
validated canonical producer. The caller supplies the inferred type,
`SymbolId`, `TypeId`, and declaration range. The API accepts only non-invalid
identity values and a non-empty declaration range.

The environment owns the inferred-type copy but treats the supplied identity as
opaque. It does not register a replacement semantic symbol, allocate a new
inferred type, or rebind a symbol in the current semantic context. Registering
the same name replaces its inferred-type copy and the canonical identity as one
binding update.

## Semantic-Fact Projection

Identifier inference already projects a type-environment binding into the
canonical `ZR_SEMANTIC_REFERENCE_READ` fact. With an externally injected
binding, that fact therefore contains the supplied `SymbolId`, `TypeId`, and
declaration range. The formal expression parser and ordinary inference path
remain unchanged; no debug-specific grammar, AST reconstruction, name lookup
fallback, or synthetic `any` type is introduced.

The API deliberately does not claim a `PlaceId`. A `SZrSemanticReferenceFact`
has no place field, while the paused-frame Debug evaluation context retains the
separate verified `PlaceId` needed by E2b execution planning. Consumers must
keep those two fact carriers separate rather than inventing a place from a
binding name.

## Debug/REPL Boundary

This is the support slice for LSP 04 E2b. The future debug binder must first
obtain an active, generation-validated frame binding through the runtime
evaluation context, then call this API with that binding's canonical identity.
It must reject unavailable, stale, or trimmed bindings before registration.

The API neither grants a write capability nor evaluates expressions. E2b must
still apply the read-only binding policy, canonical receiver/TypeRef/Place
queries, and E3 effect policy. `type_inference_import_metadata.c` and the
Syntax05 Task4 property import contract remain outside this support slice.

## Validation

`test_expression_fragment_parser.c` parses `paused`, injects external
`SymbolId=7001` and `TypeId=7002`, infers the identifier through the ordinary
parser type-inference path, and asserts that the resulting canonical reference
fact has the exact supplied identity and declaration offsets `400..406`.

On 2026-07-28, GCC, Clang, and MSVC each built and directly ran
`zr_vm_expression_fragment_parser_test` with `4 Tests`, `0 Failures`, `0
Ignored`, and a real zero exit code.
