---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_declaration_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_return_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_identifier_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display_source_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_declaration_binding.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_inlay_hints.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_declaration_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_return_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_identifier_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display_source_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_declaration_binding.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_inlay_hints.c
plan_sources:
  - .codex/plans/20260926-lsp-experience-repair.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_canonical_type_graph_union_cases.h
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_ssa_source_straight_line_cfg.c
  - tests/parser/test_semantic_query_type_use_cases.h
  - tests/parser/test_semantic_declaration_binding_cases.h
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_display.c
  - tests/parser/test_semantic_display_source_generic_cases.h
  - tests/parser/test_reference_fact_emission.c
  - tests/language_server/test_semantic_analyzer_local_binding_identity_cases.h
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/language_server/test_lsp_inlay_canonical_declaration_cases.h
  - tests/language_server/test_lsp_semantic_query_parity.c
doc_type: module-detail
---

# Canonical declaration bindings

## Ownership of identity

A source declaration owns one semantic `SymbolId` per snapshot. A binding may
appear in a temporary environment used to infer a return type, a symbol
collection environment, and the formal checking environment. These environments
reuse the declaration's identity. The private declaration lookup requires the
same AST node, symbol kind and binding name; source ranges and textual name
searches cannot substitute for the declaration node. Distinct declarations that
shadow each other therefore have distinct identities.

`type_environment_bindings.c` owns variable registration and canonical variable
insertion. `semantic_analyzer_declaration_binding.c` attaches presentation symbols
to those records and connects the checking environment to declarations. It also
adopts the existing callable identity created by function registration. A
callable's canonical type describes its whole signature; a presentation symbol
may separately store its inferred return type for display. Registering the
presentation symbol must not replace the callable type with its return type.
If part of the signature is unavailable, the source callable retains an invalid
canonical type even when its return annotation alone is known. Its lexical
binding still carries the function identity, allowing call-site hover to display
the known declaration and explicit failures for unavailable signature parts.
Inferred-return inlay hints explicitly project the callable's `returnTypeId`.
The source-type display projection resolves generic parameter names from that
parameter's exact canonical owner `SymbolId` and ordinal in the owner's generic
declaration. It uses the same recursive canonical formatter for nested arrays,
generic instances and ownership qualifiers. Missing owners, invalid ordinals and
parameter-kind mismatches fail closed; no name, range or display-text search
recovers them. The raw canonical formatter and existing `FormatType` API retain
their internal identity text contract. Inlay hints use `FormatSourceType` after
validating the canonical declaration fact, so an inferred `T` never exposes an
internal `!owner:ordinal` label.
Reusing an identity also reuses its canonical declaration fact: a second
declaration copy must not hide an unresolved or invalidated original fact.

Compile-time and runtime environments can hold the same function declaration.
Function registration reuses its symbol and generic-owner identity across those
environments and rejects a conflicting canonical signature.

## Return and lambda inference

Parameter and local bindings used during return inference include their AST
declaration and identifier range. Blocks create and release lexical environments
so an inner binding does not replace the outer binding after the block ends.
Lambda inference uses the same semantic context while creating nested lexical
environments. The formal analyzer walks compile-time declaration wrappers so
their bodies publish ordinary parameter/local reference facts.

Identifier read and write publication share one helper that refuses anonymous
source bindings without a declaration range or a nonzero `SymbolId`. Such
temporary bindings can support inference but cannot claim an identifier-use
range as a declaration. Explicitly
registered implicit receivers retain their scope range and identity. Runtime
roots and closure captures retain their separate origin contracts.
If receiver type inference is unavailable, primary member access still publishes
an unresolved member fact. Computed access also retains the index expression's
own read fact; neither fact invents a resolved member identity.

## Unavailable exact types

Declaration identity and type exactness are independent. A source declaration
whose type is unavailable keeps a nonzero `SymbolId` and an invalid `TypeId`.
The internal object sentinel used by inference is not registered as the source
declaration's type. An explicit `: object` annotation is not that sentinel:
it registers the declared canonical type even though both cases share the
inference representation's `baseType`. When an unannotated local has a
canonical initializer value, its compiler-private SemanticIR place can use
that value's TypeId without publishing an inferred exact type on the source
declaration symbol. If neither declaration nor initializer has a canonical
type, the legacy compiler retains the binding but no executable SemanticIR
local is emitted; the source CFG completeness check must reject promotion.
An open generic parameter in an inactive declared callable likewise keeps
its source identity and legacy GET_STACK read when its type cannot be closed;
no `object` TypeId or executable SSA claim is synthesized. Typed or active
source-local LOAD errors are not subject to that compatibility path.
Identifier reads still publish the resolved declaration
identity, while exact expression inference fails. If later analysis obtains an
exact type, the type can be attached to the same declaration identity.
Canonical variable binding accepts an invalid type only when the semantic
context already owns a matching source declaration with that invalid type.
An arbitrary symbol number or anonymous temporary cannot use this path.

The declared-type builder separately validates resolution after rendering a
structural type. An interned nominal name alone does not make an unavailable
annotation exact. Unresolved type-use facts remain unresolved, and parameters
and locals still receive their source bindings after an inference diagnostic.
Return pre-inference also installs unavailable parameter bindings so a same-name
outer variable cannot supply the parameter's return type.

`DeclaredSymbols` includes these declarations only when the declaration fact,
canonical record and AST node agree. Hover and completion can consequently
report `cannot infer exact type` for the actual declaration. The analyzer hover
builder never borrows a same-name outer symbol's type. Public LSP hover continues
to use canonical projections, as described in the
[hover capability boundary](../cli-and-tooling/lsp-hover-capability-boundary.md).

## Regression boundary

The public completion/hover projection treats an unavailable canonical callable
type as unknown for the whole signature while retaining its function identity.
The analyzer's internal partial-signature formatter is tested separately; public
queries do not reconstruct its parameter placeholders from the AST.

An implicit constructor has no callable declaration of its own. Its actual type
identifier therefore publishes a TYPE reference through the shared type-use
producer, using the constructed canonical type to select the unique declaration.
Explicit constructors retain their CALL identity. This gives ordinary and derived
`new` expressions the same class navigation as annotations without introducing
a query-time name or range fallback. The small branch remains in the existing
constructor fact producer; it adds no helper or separate responsibility to that
module. A future broader constructor change should extract that producer and
its source-constructor helpers together.

Parser tests cover sharing parameter identity between environments, querying
an unknown declaration before refining its type, and rejecting anonymous temporary
reads and writes from source queries. Analyzer tests exercise inferred
returns, nested shadowing, declaration/read identity, canonical record uniqueness,
compile-time locals, lambda captures, generic signatures and ownership qualifiers.
Unknown inner declarations, including unavailable annotations and failed
initializer inference, are checked against a typed outer declaration to prevent
false exact type display. An unknown parameter also verifies the callable has no
false exact return type. Both reference facts and canonical symbol records must
retain invalid type identities in these cases. Existing LSP hover/completion and
semantic query parity cases check the public projection of those facts.
Source-type display tests cover nested type/const generic parameters and missing
declaration identity. Inlay tests verify direct `T` and nested `Box<Box<T>>`
inferred returns with the presentation symbol table detached.
