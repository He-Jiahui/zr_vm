---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-29-plan03-task07-canonical-callable-value-shadow.md
tests:
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/acceptance/2026-08-29-plan03-task07-canonical-callable-value-shadow.md
doc_type: module-detail
---

# Callable-Value Shadow Resolution

## Purpose

Source callable aliases and lambdas are lexical values, but their calls still need a canonical
function `TypeId`, `SymbolId`, declaration range, and formatted signature. The parser retains that
callable metadata beside normal function overload metadata so call inference can publish one
`ZR_SEMANTIC_REFERENCE_CALL` and one expression fact for `SemanticQuery_CallAt`.

The LSP does not recover a missing source-call fact from the compiler type environment, symbol
table, callee name, initializer AST, or argument count. Signature help consumes
`CallAt/FormatCall`; an unavailable fact is an unavailable signature.

## Binding Contract

`SZrFunctionTypeInfo.isCallableValueBinding` distinguishes metadata attached to a lexical value
from an ordinary function declaration. The compiler sets it for:

- lambda values registered under their local variable name;
- identifier aliases of source callables;
- aliases of externally supplied canonical callable contracts.

Ordinary functions, canonical function declarations, and unbound external callable contracts do
not set the flag. Registration treats the binding kind as part of duplicate identity, so a local
value and an ordinary same-name function can coexist without erasing either contract.

## Scope And Shadowing

Function lookup walks one type-environment scope at a time. When a scope contains a same-name
variable:

1. only callable metadata marked as a callable-value binding in that same scope is eligible;
2. ordinary same-name functions in that scope are hidden;
3. parent function scopes are not searched.

This preserves normal lexical shadowing. A nullable callable variable shadows a named function,
while a lambda or callable alias remains callable through its own binding. Optional-call checks
therefore cannot accidentally classify a same-name ordinary function as the value being called.

## Semantic And LSP Projection

Primary call inference always asks the type environment for runtime function metadata. The type
environment itself applies the shadowing rule; prototype and compile-time function lookup remain
disabled when a lexical variable is visible. A resolved callable value then publishes the same
canonical call facts as a direct source call.

`lsp_signature_help.c` resolves source direct calls through the canonical signature adapter. The
legacy source-function path that searched overloads, symbol-table entries, initializer identifiers,
or argument-count candidates has been removed. Constructor, super, and receiver-specific adapters
remain separate structured consumers.

## Fail-Closed Rules

- No callable-value metadata means no function candidate is fabricated.
- A non-callable variable blocks same-name functions in its scope and every parent scope.
- A missing call payload cannot be repaired from local compiler or symbol state.
- External callable aliases preserve their canonical external contract and do not gain fabricated
  source identity.
- Receiver calls continue to require their own canonical receiver-call projection.

## Regression Coverage

Parser canonical-consumer tests cover identifier aliases and lambda callable values. Ownership
tests preserve both sides of the shadowing boundary: redundant optional access on a named function
is rejected, while a nullable callable variable with the same name retains value-call behavior.

LSP interface tests cover direct, alias, lambda, receiver, and generic-receiver signatures. Each
source-call case remains available from `CallAt/FormatCall` and fails closed after the canonical
call payload is removed. Source-contract tests reject reintroduction of compiler overload lookup,
unresolved candidate matching, or symbol-table position lookup in source signature dispatch.
