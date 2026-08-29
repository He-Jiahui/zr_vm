---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_external_callable_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-29-plan03-task07-canonical-receiver-signature.md
  - docs/plans/lsp/optimize/2026-08-29-plan03-task07-canonical-source-constructor-signature.md
tests:
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/acceptance/2026-08-29-plan03-task07-canonical-receiver-signature.md
  - tests/acceptance/2026-08-29-plan03-task07-canonical-source-constructor-signature.md
doc_type: module-detail
---

# Canonical Signature-Help Consumption

## Purpose

Source call signature help is a projection of parser semantic facts, not a second call resolver.
The language server may select the call at the cursor and format query output, but it cannot infer
the receiver again, search a member by name, close generic arguments locally, or rebuild a method
contract from source AST.

## Dispatch Order

For an ordinary function-call context, signature help evaluates these adapters in order:

1. the exact local canonical call at the callee range;
2. the external callable adapter for binary/native provider contracts;
3. the general canonical call at the request range;
4. source-call fail-closed classification.

Only `SemanticQuery_CallAt` and `SemanticQuery_FormatCall` authorize a source signature. The
external adapter still consumes canonical external callable metadata and provider generation; it
does not grant permission to inspect source prototypes by member name.

Source `new Type(...)` and `init Type(...)` constructor calls now run through the same canonical
query before the compiler-state guard. The dispatcher uses the syntax tree only to select the
construct node and argument positions. A resolved CALL SymbolId must lead to a source class/struct
meta-function declaration before this source-only branch is authorized. Clearing its canonical
call payload therefore makes signature help unavailable even if compiler and symbol state remain.

Native and imported constructors retain their structured metadata adapter. Their declaration does
not masquerade as a source meta-function, so they cannot enter the source branch. This preserves
native closed-type specialization and existing provider identity while later producer work closes
binary/native canonical constructor parity. `super(...)` also remains a separate structured
adapter and still requires compiler state.

## Removed Receiver Fallback

The former receiver fallback performed request-time semantic work inside the LSP:

- built a temporary primary-expression prefix;
- called `ZrParser_ExpressionType_Infer` at request time;
- searched the compiler prototype graph recursively by member name;
- searched source type declarations and methods by AST name;
- substituted receiver and method generic arguments locally;
- formatted a new method signature from reconstructed metadata.

That path and its private dead helper chain are removed. A source receiver call with no canonical
call fact returns unavailable. A binary/native call must be handled by the external adapter or a
canonical query fact; it cannot fall through to source reconstruction.

## Exactness And Lifetime

The canonical query owns call target range, callable `TypeId`, optional target `SymbolId`, target
declaration range, receiver effect, parameter passing modes, and display text. The LSP borrows this
data only for the analyzer snapshot lifetime and copies the formatted protocol result into its
response allocation.

The request URI or cursor position may select a snapshot and call context. It cannot replace a
missing source identity, retarget a same-name member, or specialize an open signature. Stale,
unresolved, or missing call facts fail closed.

## Regression Coverage

Interface tests cover direct calls, callable values, lambdas, readonly/mutable receiver calls,
closed generic receiver methods, source class constructors, and source struct constructors.
Clearing a source call payload keeps signature help unavailable; detaching compiler and symbol
state leaves valid canonical source signatures available. Native constructor tests preserve the
structured external boundary. Source contracts reject reintroduction of the method fallback and
require canonical source-constructor dispatch before the legacy constructor resolver.

Project tests cover imported constructors and native/provider receiver callables. Their current
four external producer markers are tracked by exact parent/overlay A/B; deleting the source method
fallback does not alter that marker set. Those producer gaps remain separate Plan 03 work and are
not hidden by an LSP compatibility path.
