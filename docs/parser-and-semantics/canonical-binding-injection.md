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

`ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace` registers a name in a
temporary type environment while retaining identity supplied by an already
validated canonical producer. The caller supplies the inferred type,
`SymbolId`, `TypeId`, `PlaceId`, and declaration range. The API accepts only
non-invalid symbol/type identity values and a non-empty declaration range. A
missing executable slot is represented by `PlaceId=0`; it is not reconstructed
from a binding name.

`ZrParser_TypeEnvironment_RegisterCanonicalVariable` remains the compatibility
entry for canonical producers that do not publish a Place and delegates with
`PlaceId=0`. Ordinary `RegisterVariableEx` registration also writes zero on
both creation and replacement, so a prior external Place cannot leak into a
source-local binding.

The environment owns the inferred-type copy but treats the supplied identity as
opaque. It does not register a replacement semantic symbol, allocate a new
inferred type, or rebind a symbol in the current semantic context. Registering
the same name replaces its inferred-type copy and the canonical identity as one
binding update.

## Semantic-Fact Projection

Identifier inference projects a type-environment binding into canonical
`ZR_SEMANTIC_REFERENCE_READ` and `ZR_SEMANTIC_REFERENCE_WRITE` facts. With an
externally injected binding, each fact therefore contains the supplied
`SymbolId`, `TypeId`, `PlaceId`, and declaration range. The formal expression
parser and ordinary inference path remain unchanged; no debug-specific grammar,
AST reconstruction, name lookup fallback, or synthetic `any` type is
introduced.

## Debug/REPL Boundary

LSP 04 E2b1 consumes this support API. The Debug semantic binder first obtains
an active, generation-validated frame binding through the runtime evaluation
context, then calls this API with that binding's canonical identity. It rejects
unavailable, stale, trimmed, or mismatched bindings before registration.

The API neither grants a write capability nor evaluates expressions. E2b must
still apply the read-only binding policy, canonical receiver/TypeRef/Place
queries, and E3 effect policy. `type_inference_import_metadata.c` and the
Syntax05 Task4 property import contract remain outside this support slice.

## Validation

`test_expression_fragment_parser.c` parses `paused`, injects external
`SymbolId=7001`, `TypeId=7002`, and `PlaceId=7003`, infers the identifier
through the ordinary parser type-inference path, and asserts that the resulting
canonical reference fact has the exact supplied identity and declaration
offsets `400..406`.

On 2026-07-28, GCC, Clang, and MSVC each built and directly ran
`zr_vm_expression_fragment_parser_test` with `4 Tests`, `0 Failures`, `0
Ignored`, and a real zero exit code.

`test_debug_semantic_binding_preserves_paused_frame_canonical_identity` adds a
live paused-frame consumer case: it formally parses `paused`, injects the exact
frame row through the Debug binder, and compares the ordinary inferred read
reference with the runtime query's `SymbolId`, `TypeId`, and declaration start.
On 2026-07-29, GCC, Clang, and MSVC each built and ran
`zr_vm_debug_expression_diagnostics_test` with `34 Tests`, `0 Failures`, `0
Ignored`, and a real zero exit code.
