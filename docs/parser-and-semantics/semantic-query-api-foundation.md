---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic_calls.h
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_reaching_definitions.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_array_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_display.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_reaching_definition_navigation.c
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/semantic_calls.h
  - zr_vm_parser/include/zr_vm_parser/semantic_display.h
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/type_environment_types.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_reaching_definitions.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_array_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - user: 2026-06-20 参照 docs/plans/lsp 优化语义推断能力
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_display.c
  - tests/parser/test_type_inference.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/acceptance/2026-06-20-semantic-stage1-semantic-query.md
doc_type: module-detail
---

# Semantic Query API Foundation

## Purpose

`semantic_query.h` is the Stage 1 public parser query surface for shared semantic facts. It gives compiler/LSP/debug/REPL callers a stable entrypoint before concrete dataflow analyses are wired in, and both LSP semantic diagnostics and the compiler frontend now consume this surface as a gap-filling diagnostic and definition-query source after normal semantic analysis or script compilation.

## Query Scope

`SZrParserSemanticQueryScope` currently supports:
- module scope, which accepts all facts in the `SZrSemanticContext`;
- node scope, which filters facts and query positions by the root node source range.

Node scope is a range filter only. It does not yet trigger local analysis, rebuild facts, or understand lexical parentage beyond the AST node's `location`.

## Query Functions

`ZrParser_SemanticQuery_TypeAt` finds the narrowest exact expression fact at a position and copies its `SZrInferredType` into the caller-owned output value. Missing facts, `UNKNOWN` or `APPROXIMATE` facts, invalid context, invalid output, or an out-of-scope position return `ZR_FALSE`; consumers must not reconstruct a type from source text after that failure.

`ZrParser_SemanticQuery_ReferencesOf` scans reference facts by `symbolId`, applies the optional query scope, and appends borrowed `const SZrSemanticReferenceFact *` entries to the caller-owned `SZrArray`. If the output array is already initialized, the query clears its length before appending so reused arrays do not keep stale references.

`ZrParser_SemanticQuery_DefinitionOf` finds the reference fact at a position. A declaration reference returns itself. A resolved read with `hasDefinitionRange` first looks for the declaration/write fact with the same `symbolId` and matching `definitionRange`, so straight-line `seed` after `seed = 3` resolves to the write token. The payload may be produced by the linear resolver or by the CFG-backed resolver. If no single reaching definition is present, for example after divergent branch writes, the query falls back to declaration lookup by `symbolId` and `declarationRange`; write/call/member references still use that declaration fallback until richer ranking is added.

`ZrParser_SemanticQuery_DefinitionsOf` is the multi-result companion. It accepts the same context, position, and scope, clears or initializes the caller-owned `SZrArray`, and appends borrowed `const SZrSemanticReferenceFact *` entries. For read facts with a `definitionRanges` payload, it maps each stored range back to the matching declaration/write fact, removes duplicate matches, and sorts same-source results by `sourceRange` so LSP multi-location output is deterministic. For a single `definitionRange`, it returns the same single match. If no reaching-definition payload exists, it falls back to the declaration result so existing navigation callers still get a usable location.

`ZrParser_SemanticQuery_FactsAt` aggregates facts available at a position into `SZrParserSemanticQueryFacts`: expression, reference, numeric, reachability, logical, and ownership. Numeric facts are found by position because the lower-level fact API only exposed by-node lookup.

`ZrParser_SemanticQuery_SymbolAt` is the resolved-reference projection for position-based symbol consumers. It reads the existing `FactsAt` result and succeeds only when the selected reference fact is resolved and has a valid `symbolId`. The query copies the fact's `symbolId`, `typeId`, role, declaration range, and direct or resolved definition range, then returns borrowed display and signature strings from that same fact. It clears the caller output and returns `ZR_FALSE` for a missing or unresolved reference. The current fact schema has no owner identity, so `ownerSymbolId` remains `ZR_SEMANTIC_ID_INVALID`; consumers must not infer an owner, target, or display name from source text.

`ZrParser_SemanticQuery_VisibleSymbols` is the corresponding scoped collection projection. It reads parser-owned `SZrSemanticScopeFact` parentage and `SZrSemanticVisibleSymbolFact` candidates, finds the narrowest containing scope, then walks only that parent chain. Candidates carry the already-resolved `SymbolId`, owner identity, access flags, declaration order and ranges; the global symbol registry is used only to materialize a selected fact's stable record, never to discover visibility by name. The query filters pre-declaration symbols unless they are explicitly hoisted, preserves one overload set, keeps type and value namespaces separate for shadowing, excludes instance receiver members in static context, and filters receiver members, imports/aliases, and inaccessible candidates through explicit options. Its output ordering is scope distance, declaration order, then `SymbolId`. `displayName` and `signatureDisplay` are borrowed snapshot views. An empty result, a missing scope fact, or an out-of-scope position fails closed and clears a correctly typed reused output array. LSP consumers must not recreate this walk from tokens, AST nodes, member spelling, or a language-server symbol table.

After compiler semantic references have resolved, `ZrParser_Semantic_BuildSourceScopeFacts` publishes source module, function, block, loop, and type scope facts. It projects function declarations as hoisted module candidates and exact resolved parameter/local declaration facts into their lexical scopes. Top-level `struct`, `class`, and `interface` declarations register their declaration AST identity with the type environment, which publishes the canonical type `SymbolId` and exact declaration range; the builder then projects that record as a hoisted module candidate and creates a type-owned scope. A duplicate type registration does not invent a second identity. Loop scopes end at their body rather than using an over-wide parser statement range, so a `for` initializer cannot leak into the following statement. The builder uses AST nodes only to identify scope boundaries or the already-published type record; candidate identity, ranges, and display contract come from the exact resolved declaration or type symbol, with no range, name, or member fallback. A missing resolved declaration or type symbol is omitted, so the query fails closed rather than inventing source visibility. Source type and free-function generic parameters are exact scope candidates: a parameter's canonical `TypeId` is interned from its owning declaration `SymbolId` and ordinal, while its visible symbol uses the parameter AST identity and exact name range. Struct methods now first receive a canonical function `SymbolId` from the compiler's exact member declaration, then receive a method-owned scope whose generic parameters and body are published through the same pipeline. Function generic parameters are published before value parameters, but only become visible at their own declaration offset. When overlapping scopes have a parent relationship, the query selects the descendant regardless of parser range width; only unrelated overlaps fall back to the narrower range, and equal-width siblings retain deterministic order. Const and class/interface method generic parameters, type members, imports/aliases, receiver members, and binary/native scope producers remain separate pending work.

`ZrParser_SemanticQuery_MaterializeDiagnostics` is the mutable analysis-lifecycle operation for one exact query scope. It clears and rebuilds the context-owned structured diagnostic cache after semantic facts have been resolved. `ZrParser_SemanticQuery_Diagnostics` is then a read-only query: it returns the already-materialized borrowed view for that same scope, returns an empty view before materialization, and fails closed for a different scope. It maps `ZR_SEMANTIC_REACHABILITY_UNREACHABLE` facts to warning diagnostics with code `unreachable_code`, message `Unreachable code`, and cause/suggestion text derived from the reachability cause. It also maps read reference facts with definite-assignment state to structured diagnostics: `UNINIT` becomes an error with code `uninitialized_read`, and `MAYBE_INIT` becomes a warning with code `possibly_uninitialized_read`. These states can be pre-populated by callers, produced by `ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments` for straight-line reference fact order, or produced by `ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments` for source reads reached through CFG branch joins, declaration initializers, and cloned `finally` paths. When a definite-assignment diagnostic is built from a read reference fact with a declaration range, it owns one `relatedInformation` entry at that declaration with message `Variable declaration is here`.

All pointer fields exposed by this API, including `FactsAt` facts and diagnostic items, are borrowed snapshot views. They remain valid only while their `SZrSemanticContext` is alive and unchanged. A caller that crosses a snapshot boundary retains stable ids and copied ranges only; it must never retain an AST, fact, or diagnostic pointer. Repeated read-only calls over an unchanged context return the same semantic content without mutating the context.

Computed array member inference records parser-owned expression diagnostic facts for bounds and index-type problems, and the query layer maps those facts into `array_index_out_of_bounds`, `array_index_may_be_out_of_bounds`, or `array_index_type_mismatch`. Fixed arrays, `min==max`, and finite `arrayMaxSize` constraints provide an upper bound; min-only / no finite-upper-bound arrays do not fabricate an upper-bound error. They still warn when a known integer index range may be negative, and the message intentionally omits an `array max size` label in that no-upper-bound case.

`ZrLanguageServer_SemanticAnalyzer_AppendSemanticQueryDiagnostics` runs after normal LSP semantic type checking. It runs the CFG-backed definite-assignment resolver when the analyzer AST is available and otherwise falls back to the straight-line resolver, explicitly materializes the module-scope diagnostic view, then reads it through `ZrParser_SemanticQuery_Diagnostics`. It converts each item with `ZrLanguageServer_Diagnostic_FromStructured` while preserving related information, and appends only diagnostics whose source range is not already reported by the existing analyzer diagnostics. The regular `ZrLanguageServer_Lsp_GetDiagnostics` path then publishes those diagnostics through the existing LSP conversion flow.

Named-call compatibility failures now use the same query channel. The semantic analyzer keeps the parser compiler current error alive until `ZrParser_Compiler_PublishCurrentDiagnostic` has deep-copied it into persistent diagnostic facts, then clears the transient compiler error. When publication succeeds, the LSP analyzer removes the same-call-range `cannot_infer_exact_type` placeholder before `AppendSemanticQueryDiagnostics` projects the canonical `compiler_error`; it does not classify the error by message text or rebuild a receiver/member diagnostic locally.

Resolved callable consumers also use `ZrParser_SemanticQuery_CallAt` and `ZrParser_SemanticQuery_FormatCall` directly. `hasResolvedTarget`, `targetSymbolId`, and `targetDeclarationRange` are consumed only when the target is resolved; signature help and receiver hover do not infer a target from the member spelling.

`ZrLanguageServer_LspSemanticDefinitionQuery_AppendReachingDefinition` bridges parser definition query results into LSP locations for local symbol definition requests. The local-symbol path runs the straight-line reaching-definition resolver and, when the analyzer AST is available, the CFG-backed reaching-definition resolver before querying `DefinitionsOf`. A read after a single reaching write navigates to the write token. A read after divergent branch writes now returns both branch write locations when the CFG-backed producer can enumerate them; otherwise it still falls back through the query layer to a declaration location.

`ZrParser_Compiler_PublishSemanticQueryDiagnostics` is the compiler-side bridge. `compile_script` calls it after statement compilation and script typed metadata generation succeed. The bridge runs the straight-line definite-assignment resolver, the straight-line reaching-definition resolver, and, when the script AST is available, the CFG-backed definite-assignment and reaching-definition resolvers before explicitly materializing and then reading module-scope diagnostics; it leaves the resulting structured diagnostics in `SZrSemanticContext.queryDiagnostics`. Because the reaching-definition payload is resolved on the same context, compiler-side callers can also ask `ZrParser_SemanticQuery_DefinitionOf` on the compiled semantic context and get a unique write definition when one reaches a read. The bridge does not route warnings through `ZrParser_Compiler_Error`, so semantic query warnings such as `unreachable_code` or `possibly_uninitialized_read` do not set `hasError` or `hasStructuredError`. This is currently context-cache publication only; binary metadata, CLI output, or another external compiler diagnostic channel still needs a later serialization path.

## Binary Generic Callable Artifact Contract

Typed `.zro` exports carry an open callable generic contract separately from a
call-site closed canonical `TypeId`. Patch 42 adds a version-gated generic row
after each typed export's parameter TypeRefs. A row preserves the declaration
generic name, kind, variance, class/struct/new/owner constraints, ownership
qualifier constraint, and constraint type names. Old artifacts do not expose a
partial substitute: their generic row count is zero and a generic invocation
remains unavailable rather than being reconstructed from signature text.

`compiler_typed_export_generics.c` is the narrow producer adapter. It gathers
the parser declaration's existing `SZrTypeGenericParameterInfo` records into
the typed export carrier, and can carry the same records for an imported
callable alias. The writer, IO reader, runtime copier, and function free path
own the serialized row lifecycle. The metadata token's method-signature arity
is emitted from the same typed export count, but token arity is not used as a
replacement for the structured rows.

The importer projects only those rows into `SZrTypeMemberInfo.genericParameters`.
For a typed function export, parameter metadata comes from the export's stable
`callableChildIndex`; it no longer recursively selects a runtime child by
member spelling and parameter count. Invalid child indexes or malformed generic
rows leave the metadata unavailable, so type inference and LSP consumers fail
closed instead of selecting a same-name overload.

For `fn identity<T>(value: T): T`, inferred `identity(1)` and explicit
`identity<string>("text")` share the imported external declaration SymbolId,
have distinct closed callable TypeIds, retain a zero external declaration
range, and format through the canonical fact as
`identity<T>(value: int): int` and
`identity<T>(value: string): string`. The free-call form deliberately has no
receiver-effect `fn ` prefix; receiver calls retain their separately published
`fn ` or `const fn ` contract.

## Relation Graph Foundation

`SZrSemanticContext.relationFacts` is the snapshot-owned carrier for
declaration/definition, override, implementation, base-type, constructor,
property-accessor, alias-target, and import/export-origin edges. Every edge
uses independently stable source/target SymbolId and TypeId values, exact
endpoint ranges when available, and explicit external-origin and virtual-
declaration URIs when the relation has no source range.

`ZrParser_SemanticRelations_Append` retains both URIs in the semantic snapshot.
An external fact without a source range is rejected unless its metadata
projection supplies both values. A source import relation still has the local
alias source range and may omit a virtual declaration until an exact metadata
producer supplies one; consumers must not synthesize that URI.
`RelationsOfSymbol`, `ImplementationsOf`, `BaseTypesOf`, and `DerivedTypesOf`
are read-only projections: they clear reused output arrays, return copied values
with borrowed URI fields, and order edges by relation kind, stable ids, and
ranges. A node scope admits only an edge whose source or target range is within
the root. The query does not scan AST names or manufacture external origins.

Task 3.2 publishes source property-accessor edges from the existing
`SZrSemanticPropertyContract` rows after compiler property binding completes.
Each edge has the visible property SymbolId as its source and the canonical
getter, setter, or initializer function SymbolId as its target. The producer
requires the contract's callable TypeId to match the registered accessor symbol
and takes the source declaration range and target declaration range directly
from those facts. It validates every contract before appending any edge, so a
missing or inconsistent canonical accessor fails closed without leaving partial
relations. Repeated publication is idempotent. It never scans property AST
nodes or reconstructs hidden accessor names.

Task 3.3 publishes `ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION` edges from
resolved `WRITE` reference facts. The producer looks up the existing stable
symbol record by `SymbolId`, takes the declaration range from that record, and
takes each definition range from the canonical reference fact. It does not
turn reads into definitions, look up a spelling, or use an AST fallback. A
repeated publication skips an existing symbol-and-definition-range edge, so
the snapshot remains idempotent while preserving distinct definition sites.
Facts without a resolved symbol, canonical type, or exact ranges are omitted
until a source, binary, or native producer can provide a complete relation.

Task 3.4 publishes `ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN` for source
`import(...)` aliases. During source-scope construction, the compiler copies
the normalized import URI into the snapshot-owned visible-symbol fact together
with the resolved local alias SymbolId. The relation producer runs after that
scope pass and uses only the visible fact and its registered symbol record: the
local alias is the source endpoint, the canonical TypeId is retained for the
external endpoint, and the URI is the explicit external origin. It emits no
target SymbolId, source range, or relation when a canonical URI, SymbolId,
TypeId, or declaration range is absent. Re-publication is idempotent by local
SymbolId, canonical TypeId, and URI. It never finds an imported entity by a
spelling, so a native symbol with the same name cannot replace the source
alias.

Task 3.6 publishes source hierarchy edges after the compiler resolver has
selected a concrete base or interface prototype. Each source
`SZrTypePrototypeInfo` retains its declaration AST identity; the compact
compiler bridge passes that identity and the resolved target prototype's
declaration identity to
`ZrParser_SemanticRelations_PublishTypeDeclarationRelation`. The relation
producer maps both endpoints only by exact `symbol.astNode` identity in the
already-published semantic registry. It then emits `BASE_TYPE` for class and
struct bases, `IMPLEMENTATION` only when the resolver has already classified an
implemented interface, and `BASE_TYPE` for interface inheritance. Repeated
publication is idempotent by relation kind and endpoint SymbolIds. Metadata-only,
imported, builtin, or unresolved targets have no source declaration identity and
therefore publish no local edge. LSP and query consumers must not recover such
an absent edge from type names, member spellings, or AST traversal. Override,
alias, binary, and native relation producers remain later slices.

Task 3.7 publishes `ZR_SEMANTIC_RELATION_OVERRIDE` after class override
validation has selected an inherited member and both member declarations
already have canonical SymbolIds. The compiler bridge passes exactly those two
ids to `ZrParser_SemanticRelations_PublishSymbolRelation`; the relation module
looks up the existing symbol records, takes their TypeIds and declaration
ranges, and rejects missing ids, ranges, or type identities. It does not pair
members by name, virtual slot, AST traversal, property accessor spelling, or
language-server state. Re-publication is idempotent by relation kind and the
source/target SymbolId pair. Imported, metadata-only, and otherwise unresolved
base members remain unavailable until their producer supplies a stable target
identity and explicit provenance.

Class meta functions participate in the same identity contract as ordinary
class methods. The compiler registers each named `ZR_AST_CLASS_META_FUNCTION`
as a canonical function symbol before override validation, so `@call` and other
non-constructor meta methods cannot reach relation publication with an invalid
endpoint. Explicit constructors reuse that already-registered SymbolId when
their constructor contract is published later. The focused regression compiles
a virtual/override `@call` pair and verifies both the directed `OVERRIDE` edge
and the reverse `ImplementationsOf` query; no meta spelling or virtual-slot
fallback is used.

Task 3.8 publishes `ZR_SEMANTIC_RELATION_CONSTRUCTOR` only after
`ZrParser_Semantic_BuildSourceScopeFacts` has registered source type symbols.
`compiler_publish_source_constructor_relations` iterates only existing compiler
`typePrototypes`, takes the retained exact type declaration node and an already
registered explicit `@constructor` member SymbolId, then delegates to
`ZrParser_SemanticRelations_PublishConstructorRelation`. The relation module
maps the source solely through `symbol.astNode`, then projects TypeIds and
ranges from registered symbol records. Synthesized default constructors have no
member SymbolId or source range and emit no edge; invalid or missing source
identities fail closed. No name, constructor token, TypeId-only, AST
traversal/query fallback, or LSP reconstruction is allowed.

Task 3.9 projects endpoint module identities directly from each relation's
canonical source and target TypeId. Nominal nodes expose their snapshot-owned
`moduleIdentity`; generic instances follow only their canonical
`definitionTypeId` until reaching that nominal definition. Invalid, unknown,
unqualified, primitive, structural, or malformed cyclic endpoints remain
unavailable. The query never substitutes a URI, display name, symbol spelling,
or source path for module identity. Returned module strings are borrowed from
the semantic snapshot just like relation URI fields. A consumer that retains a
relation across snapshot replacement must copy stable ids, module identity
text, URIs, and ranges into its own snapshot-owned data rather than retaining
these pointers.

Task 3.10 publishes `ZR_SEMANTIC_RELATION_ALIAS_TARGET` only from an existing
`SZrSemanticVisibleSymbolFact` whose `isAlias` bit is set and whose exact
SymbolId resolves to a registered semantic symbol. The edge uses that symbol's
canonical TypeId as the alias target identity and its declaration range as the
source range. When no exact target declaration SymbolId exists, the target
SymbolId and target range remain unavailable; the producer never searches a
same-name symbol, traverses an alias AST, parses display text, or treats an
import URI as target identity. Import aliases retain a separate
`IMPORT_EXPORT_ORIGIN` edge, so type identity and external origin provenance
remain orthogonal. Re-publication is idempotent by source SymbolId and target
TypeId.

Task 3.11 defines `ImplementationsOf(targetSymbolId)` as the reverse lookup for
both `IMPLEMENTATION` and `OVERRIDE` relation facts. Interface implementations
and class overrides therefore share one consumer query while preserving their
distinct relation kinds. Results retain the canonical direction from the
implementing/overriding source SymbolId to the requested base target SymbolId,
apply the existing relation scope filter, and use the same stable sort. The
query never searches member names, inheritance syntax, virtual slots, or ASTs;
if a lower-layer producer has not published an exact edge, no implementation is
returned.

Task 3.12 publishes source class-to-interface member `IMPLEMENTATION` facts at
the point where class validation has already selected the exact
`SZrTypeMemberInfo` pair and verified its signature, receiver effect, abstract,
and shadow contracts. The relation source is the implementing class member's
registered SymbolId and the target is the required interface member's
registered SymbolId. TypeIds and declaration ranges remain projections of
those symbol records. Repeated validation is idempotent in the shared relation
store; the producer never reconstructs an edge from a member name, signature
text, interface slot, inheritance spelling, or an LSP-side AST walk.

The source syntax currently permits interface requirements on classes but does
not expose a struct inheritance/interface list. This relation milestone keeps
that syntax boundary: it publishes class implementation edges only and does
not add a struct parser extension. Binary metadata, native descriptors, and
external interface member identities need their own canonical relation
producers before `ImplementationsOf` can return those edges.

Task 3.13 adds `virtualDeclarationUri` to the relation fact and query value.
The append boundary clones it beside `externalOriginUri`, and the query returns
both as borrowed snapshot views. An external relation with no source range must
provide both URIs or fail before mutating the relation store. This is a metadata
projection contract only: it does not derive a URI from module text, symbol
spelling, a file path, or the language server's virtual-document scheme.

## Call Edge Snapshot Foundation

`SZrSemanticContext.callEdgeFacts` is a separate snapshot-owned call graph
carrier. `ZrParser_SemanticCalls_Publish` runs only after source lexical scope
facts exist. For every existing `CALL` reference, it selects the narrowest
containing scope whose owner is an already-registered function SymbolId. It
uses the matching compiler expression fact for the full call-site range and
uses the resolved call reference's SymbolId, closed callable TypeId, and
declaration range for the target. It does not traverse a name, choose an
overload by spelling, or retain an AST pointer in the edge.

`CallEdgesAt`, `OutgoingCalls`, and `IncomingCalls` project copied values from
that carrier and sort by call-site range then stable ids. An edge whose source
scope has no function owner is published with `CALLER_UNAVAILABLE`; an edge
whose call reference is unresolved or whose target is absent is published with
`TARGET_UNRESOLVED`. Such an edge retains the call-site and callable TypeId but
never selects a same-name function. A resolved target without a declaration
range uses `TARGET_DECLARATION_UNAVAILABLE`. Output arrays are reusable and
contain no pointers into mutable AST structures.

Call-edge value rows have no exactness field. When a matching call expression
fact is present, the producer therefore publishes an edge only when that fact
is `EXACT`; `UNKNOWN` and `APPROXIMATE` facts publish no hierarchy edge. It
does not substitute a wider expression, a reference spelling, or a target name
for an inexact selected fact. A missing expression fact retains the existing
reference-only unresolved-edge behavior, because it has no conflicting
inexactness assertion to project.

`CallAt` remains a borrowed query and exposes its selected expression fact, so
its caller can inspect that fact's exactness. `CallCandidatesAt` instead
returns only copied candidate values. It therefore requires the selected
`CallAt` expression to be `EXACT` before projecting the overload set; an
inexact selected call returns an empty candidate result rather than exposing
uncertainty without an exactness field.

`ZrParser_SemanticQuery_FormatCall` is also a value-only projection: its
caller receives text rather than the borrowed expression fact. It consequently
requires an `EXACT` selected expression before producing a signature. It clears
a usable output buffer before validating its inputs, so a failed inexact query
cannot leave a previous signature visible. It does not recover a display from a
name, AST, broader fact, or language-server state.

Before formatting, `FormatCall` also verifies that its call reference is a
`CALL` fact with the same callable TypeId and a range inside the selected
expression's call target. A caller cannot combine two otherwise-valid facts to
display a signature for the wrong call.

`ZrParser_SemanticQuery_CanonicalTypeAt` similarly returns a value `TypeId` when
it falls back to an expression fact. That fallback requires an `EXACT`
expression. A reference-backed result remains governed by its published
reference identity, while an approximate expression fallback leaves `typeId`
invalid and returns false; callers may still inspect the borrowed fact pointers
without treating an inexact type as a value result.

`ReferencesOf` clears or initializes its reusable borrowed-reference array
before rejecting an invalid SymbolId. A failed navigation query therefore
cannot leave a previous symbol's references visible.

`OutgoingCalls` and `IncomingCalls` apply the same reusable value-array rule:
they clear output before rejecting an invalid caller or target SymbolId.

Relation queries clear reusable relation arrays before rejecting invalid symbol
or type identities, so implementation and type-hierarchy consumers cannot
reuse a previous graph result after a failed lookup.

This first Task 4 slice covers source function callers and resolved overload
declaration membership only. Lambda scopes, argument-to-parameter mappings,
compatibility scores, conversions, receiver TypeIds, and binary/native
call-edge producers remain unavailable until their canonical producers can
supply complete identity. LSP hierarchy consumers must continue to fail closed
for those cases.

`CallCandidatesAt` adds the declaration-membership portion of overload facts.
It first requires `CallAt` to expose a resolved target, then projects only the
registered function members of that target's `overloadSetId`, sorted by
SymbolId. Each candidate carries its declaration callable TypeId and range;
exactly one carries `isSelected`. The selected invocation's potentially closed
callable TypeId remains on `CallAt`, so a generic call does not overwrite every
candidate with a call-site specialization. The candidate list deliberately
does not claim overload viability, conversion score, argument-to-parameter
mapping, or a text-derived alternative when the target is unresolved.

### CallAt Metadata Projection

`CallAt` also copies the selected canonical expression fact's `range`,
`callTargetRange`, `argumentCount`, `hasNamedArguments`, and `isMemberCall` into
the value fields `callSiteRange`, `callTargetRange`, `argumentCount`,
`hasNamedArguments`, and `isMemberCall`. The query never recomputes the
arguments, resolves a receiver, scans source text, or substitutes a name when
the selected call fact is incomplete. The copied ranges remain valid after a
caller discards its borrowed expression/reference views, while their identity
continues to be scoped to the same semantic snapshot. Argument-to-parameter
mapping, conversion/exactness, receiver TypeId, and external-call metadata
remain unavailable until their canonical producers publish complete facts.

### Lambda Caller Scope Facts

For a lambda used as a variable initializer, the source scope-fact builder
visits that initializer and publishes a `FUNCTION` scope for the lambda's
existing compiler-registered function SymbolId. Its parameters and block are
then visited beneath that scope. No lambda name is synthesized and no call
edge retains an AST pointer.

When publishing call edges, the call carrier first selects the narrowest
containing `FUNCTION` scope and only then validates its owner as a registered
function SymbolId. An invalid nearest owner publishes `CALLER_UNAVAILABLE`; it
does not skip that scope and attribute the call to an outer function. This
keeps incoming and outgoing hierarchy facts fail-closed for incomplete nested
scope identity. Lambda expressions reached through other expression positions
remain unavailable until the source scope walker has a complete structured
traversal for those positions.

The walker also follows a return expression. A returned lambda therefore gets
the same nearest `FUNCTION` scope even when the compiler has not registered a
lambda SymbolId for that expression position. That scope deliberately has no
owner, so calls in its body publish `CALLER_UNAVAILABLE` rather than being
incorrectly attributed to the enclosing function. This is a containment
guarantee, not a synthetic lambda identity or a claim that the lambda is
eligible for an outgoing-calls query.

## Test Coverage

`tests/parser/test_semantic_query_symbols.c` now compiles source `class Meter`,
`struct Point`, and `interface Readable` fields and methods. It verifies exact
field symbols, opt-in receiver visibility, canonical owner identity, and
static-method exclusion of instance members through published scope state
rather than a member-name check.

`tests/parser/test_compiler_semantic_query_diagnostics.c` covers compiler-side publication by compiling `return true ? 1 : 2`, requiring `SZrSemanticContext.queryDiagnostics` to contain `unreachable_code`, and asserting that warning-level query diagnostics do not put the compiler in an error state. It also covers CFG-backed definite-assignment diagnostics by compiling `var seed: int; if (flag) { seed = 1; } return seed;` and requiring `possibly_uninitialized_read` at the branch-join read. For reaching definitions, it compiles `if/else` divergent branch writes and verifies the linear resolver would choose the last source-order write, while the CFG-backed resolver clears the final read's single-definition payload. It also compiles a straight-line `seed = 3; return seed;` fixture and verifies `ZrParser_SemanticQuery_DefinitionOf` can resolve the compiled context's read directly to the write token without a test-local resolver call.

The same compiler diagnostic target covers semantic query diagnostics for existing numeric overflow facts and array index diagnostics. Array coverage includes fixed-size constant indexes, definite out-of-range intervals, partial-overlap warnings, primitive full-range indexes, finite `arrayMaxSize` / `min==max` upper bounds, known non-integer indexes, and min-only `int[1 ..]` arrays whose index range may be negative while positive `u8` indexes remain silent.

`tests/parser/test_semantic_facts.c` covers the fact producers feeding this query layer: straight-line definite-assignment resolution marks a read before a write as `UNINIT` and a read after a write as `INIT`, while the CFG-backed resolver keeps `var seed: int = seed`'s initializer read as `UNINIT` until the declaration statement completes and joins cloned `finally` reads from normal/function-exit paths to `MAYBE_INIT`.

`tests/parser/test_semantic_query.c` covers:
- `TypeAt` picks the narrowest expression fact and copies the inferred type;
- `FactsAt` aggregates expression, numeric, reachability, logical, and ownership facts;
- `DefinitionOf` resolves a read reference to a declaration fact with the same `symbolId`;
- `DefinitionOf` prefers the linear reaching-definition write fact when a read carries `definitionRange`;
- `DefinitionsOf` returns multiple reaching write facts when a read carries a `definitionRanges` payload;
- `ReferencesOf` collects declaration/read/write facts for a symbol inside scope and clears reused output arrays before a miss;
- node scope accepts in-range facts and rejects facts outside the root range;
- `Diagnostics` returns an empty list when no diagnostic facts exist;
- `Diagnostics` maps scope-filtered unreachable reachability facts to one structured `unreachable_code` warning while ignoring reachable facts and out-of-scope facts;
- `Diagnostics` maps scope-filtered definite-assignment read facts to `uninitialized_read` / `possibly_uninitialized_read` while ignoring initialized or out-of-scope read facts, and it attaches declaration relatedInformation when a declaration range is present.
- `Diagnostics` consumes linear definite-assignment resolver output, reporting the read before a write while ignoring the read after the write.
- `tests/parser/test_semantic_query_symbols.c` covers `SymbolAt` projection of resolved `symbolId`/`typeId`, reference role, declaration and definition ranges, and borrowed display/signature text. It also verifies that an unresolved reference clears a reused output structure and fails closed.
- The same target covers `VisibleSymbols` with parser-owned scope parentage: nested lexical shadowing, declaration-before-use filtering, generic parameters, preserved overload-set members, deterministic ordering, optional receiver/import/inaccessible candidates, and static-context exclusion of instance members. The test constructs canonical facts directly so it cannot pass through a language-server name or AST fallback.
- The same target compiles a source function with an outer and inner `value` declaration, then queries the inner return position. It verifies that the compiler-published source facts expose exactly one visible `value` and the enclosing `seed` parameter without a test, LSP, or query-side scope reconstruction. A second source fixture proves a `for` initializer is not visible after its loop.
- The same target compiles source `struct Point`, `class Meter`, and `interface Readable` declarations before `probe`. It verifies that the module query exposes exactly one canonical type candidate for each declaration, proving the type environment preserves declaration identity through scope-fact publication instead of rediscovering types by name.
- The same target compiles `struct Box<T>`, `class Crate<U>`, and `interface Readable<V>`. It verifies that `T`, `U`, and `V` are each visible only from their owning type scope after declaration, retain an owner `SymbolId` and canonical generic-parameter `TypeId`, do not leak into sibling type scopes, and are not available before their declaration token.
- The same target compiles `struct Matrix<const N: int> { }`. It verifies that `N` is absent at `Matrix`, becomes visible at its own declaration, retains the exact type owner `SymbolId`, and receives a canonical generic-parameter `TypeId`. The scope producer classifies it from the parser-owned `ZR_GENERIC_PARAMETER_CONST_INT` enum and does not derive a candidate from the `const` text or its `int` annotation.
- The same target compiles `fn identity<T>(value: T): T`. It verifies that `T` is absent at the function declaration name, becomes visible at the generic declaration, carries the exact function owner `SymbolId` and canonical generic-parameter `TypeId`, and is materialized from the published function scope rather than its overlapping module scope.
- The same target compiles `struct Box<T> { fn echo<U>(value: U): U { return value; } }`. It verifies that `U` is absent at `echo`, becomes visible at its declaration with the compiler-registered exact method owner `SymbolId`, has a canonical generic-parameter `TypeId`, and sees enclosing `T` through published scope parentage.
- The same target compiles `class Crate<T> { fn echo<U>(value: U): U { return value; } }`. It verifies that class method `U` is absent at `echo`, becomes visible only at its declaration, retains the exact compiler-registered method owner `SymbolId` and canonical generic-parameter `TypeId`, and can see enclosing `T` through the published type-to-method scope chain. The class compiler and struct canonical-definition pass share the same type-member registration helper, which resolves the canonical owner `TypeId` and registers the exact member declaration node before scope-fact publication; the query layer does not recover the symbol from method text or an AST scan.
- The same target compiles direct and object-destructured `zr.math` imports plus an `int[][]` type-value alias. It verifies that the imported binding key and type-value declaration retain their compiler-registered SymbolId, TypeId, and exact declaration range through `SymbolAt` and `VisibleSymbols`; imports and aliases appear only when `includeImports` is set, without an AST, member-name, or module-text fallback in the query layer.
- The same target compiles `var math = import("zr.math"); return math.abs(-3.0);`. It verifies that a resolved non-generic native module function call publishes a stable snapshot `SymbolId` and closed callable `TypeId` through the same call reference fact consumed by `SymbolAt`. Native descriptors have no source declaration AST, so both the reference and query declaration range remain the explicit zero range rather than reusing the call-site range.
- The same target registers a native `NativeEchoDevice.echo<T>(value: T): T` descriptor and compiles both inferred `device.echo(1)` and explicit `device.echo<string>("text")` calls. The first resolved call registers one snapshot `SymbolId` for the external open declaration without binding the symbol record to either closed callable `TypeId`; cached later calls reuse that identity. Each call fact and `CallAt` result retains its own closed `TypeId`, and `FormatCall` keeps the declaration generic clause while displaying the corresponding closed parameter and return types. External declaration ranges remain zero. This producer consumes only the resolved `SZrTypeMemberInfo`; it does not use a member-name, AST, or LSP fallback.
- The same target compiles `interface Readable<T> { fn echo<U>(value: U): U; }`. It verifies that interface signature `U` is absent at `echo`, becomes visible only at its declaration, retains the exact compiler-registered signature owner `SymbolId` and canonical generic-parameter `TypeId`, and can see enclosing `T` through the published type-to-signature scope chain. The interface compiler uses the same canonical type-member registration helper; the scope producer emits no interface candidate unless that exact symbol exists.
- `tests/parser/test_semantic_query_contract.c` freezes Plan 03 query contract behavior: `TypeAt` fails closed for an approximate expression fact, `FactsAt` returns the same borrowed fact view on repeated reads, and `Diagnostics` cannot materialize context state without the explicit lifecycle operation.
- `tests/language_server/test_lsp_semantic_query_diagnostics.c` covers LSP publishing of semantic query diagnostics by opening a constant ternary branch fixture and requiring `GetDiagnostics` to include `unreachable_code`; it also covers `numeric_overflow`, array bounds errors/warnings, known non-integer array index errors, min-only array negative-interval warnings, and a branch-join `possibly_uninitialized_read` with one declaration relatedInformation entry through the same LSP diagnostic path.
- `tests/language_server/test_lsp_reference_callable_consumer_cases.h` covers resolved callable `CallAt/FormatCall` parity across signature help and hover, scoped reference parameter information, exact receiver target identity, and one persistent `%ref` call-marker `compiler_error` without an inference cascade.
- `tests/language_server/test_lsp_reaching_definition_navigation.c` covers LSP definition navigation through parser reaching-definition facts by requiring `return seed` after `seed = 3` to jump to the write token instead of the declaration token. It also covers divergent branch writes by requiring `return seed` after an `if/else` pair of writes to return both branch write locations and not the declaration.

The parser test targets are `zr_vm_compiler_semantic_query_diagnostics_test`, `zr_vm_semantic_query_test`, `zr_vm_semantic_query_symbols_test`, and `zr_vm_semantic_query_calls_test`; the LSP publication targets are `zr_vm_language_server_semantic_query_diagnostics_test` and `zr_vm_language_server_reaching_definition_navigation_test`. They are registered in `tests/CMakeLists.txt`.

`tests/parser/test_semantic_query_calls.c` covers source call-edge identity
and fail-closed caller selection. It compiles a variable-initialized lambda
whose body calls `callee`, then requires the incoming and outgoing edge to use
the compiler-registered lambda SymbolId rather than the enclosing `outer`
function. A fact-only nested function scope with an invalid owner proves that
the publisher returns `CALLER_UNAVAILABLE` instead of falling back to the
outer function scope.

The same target compiles a returned lambda that has no compiler-registered
lambda SymbolId. It verifies that the return-expression traversal creates the
nearest unowned function scope, so the resolved `callee` edge is
`CALLER_UNAVAILABLE` and does not appear in the enclosing function's outgoing
edges.

It also constructs a resolved call reference with a matching
`APPROXIMATE` call expression fact. The edge producer must leave the outgoing
array empty rather than projecting hierarchy identity whose exactness cannot
be represented by the value query.

After compiling an overloaded source call, the same target changes the exact
selected expression fact to `APPROXIMATE`. `CallAt` still exposes that borrowed
fact, while `CallCandidatesAt` must clear its reusable candidate array and
fail closed instead of returning value-only overload rows.

## Canonical Display Facade

`semantic_display.h` is a snapshot-scoped formatting facade for callers that
already hold canonical identities. `ZrParser_SemanticDisplay_FormatType` simply
delegates to `ZrParser_CanonicalType_Format`; it therefore preserves every
canonical wrapper and function contract without maintaining a second type-name
table. A missing TypeId, an invalid context, or insufficient output capacity
clears the caller buffer and returns `ZR_FALSE`.

`ZrParser_SemanticDisplay_FormatSymbol` accepts an exact registered SymbolId.
It first considers resolved declaration reference facts with the same SymbolId
and TypeId. A matching `signatureDisplay` is returned unchanged, which retains
source or imported generic parameter names that a closed function TypeId does
not encode. If such a fact is absent, the fallback is limited to that exact
symbol record's display name and canonical TypeId. It never scans source,
searches a same-name record, or obtains a function signature from another
overload.

`ZrParser_SemanticDisplay_FormatProperty` accepts an already-published
`SZrSemanticPropertyContract`, validates its property SymbolId and TypeId
against the registry, and verifies every advertised getter, setter, or
initializer SymbolId is a registered function with a canonical function TypeId.
It formats only the structured
static, receiver-effect, reference-access, type, and accessor fields. An
incomplete contract leaves the output empty rather than guessing hidden
accessor names. `ZrParser_SemanticQuery_FormatCall` remains the existing
canonical call display API; the facade does not create a parallel call resolver.

## Documentation Facts

`ZrParser_SemanticDocumentation_Publish` attaches a documentation snapshot to
an existing `SymbolId`. The publisher validates the exact symbol registry row,
copies the incoming string into the semantic context state, and rejects a
conflicting second value for the same identity. Re-publishing identical text
is idempotent. It does not discover symbols by name or inspect comments,
tokens, AST spelling, or language-server state.

`ZrParser_SemanticQuery_DocumentationOfSymbol` returns a borrowed string only
for the requested exact `SymbolId`. Missing symbols, invalid identities, and
symbols without a documentation fact return `NULL`; a same-name symbol never
inherits another symbol's documentation. The returned string is valid only
until the semantic snapshot is reset or freed, so callers crossing a snapshot
boundary retain the `SymbolId` and copy the displayed text if required.

Source, binary, and native producers must publish the same fact shape. Origin
format and declaration URI are separate metadata, and no consumer may use a
documentation string to recover a missing identity. Hover, completion, and
signature help therefore query documentation independently instead of
extracting text from one another's display output.

## Diagnostic Fix Disposition

`SZrStructuredDiagnostic` owns the complete fix disposition for one semantic
diagnostic. A producer either appends one or more typed
`SZrStructuredDiagnosticFix` rows or sets a nonzero
`EZrDiagnosticNoFixReason`; the two states are mutually exclusive. An empty
fix array with an unspecified reason remains an incomplete producer result and
must not be interpreted by an LSP consumer as an intentional no-fix decision.

`ZrParser_StructuredDiagnostic_SetNoFixReason` accepts only a defined nonzero
reason, is idempotent for the same reason, rejects a conflicting reason, and
rejects diagnostics that already own a fix. `AddFix` rejects a diagnostic that
already owns a no-fix reason. `ZrParser_StructuredDiagnostic_Copy` preserves
the disposition through persistent semantic facts and the explicitly
materialized query snapshot while reusing those invariants to reject an
inconsistent source object.

The reason is structured policy metadata, not display text. Parser/compiler
producers select it from semantic circumstances; an LSP projection may expose
or retain it as protocol data but must not infer it from a message, diagnostic
code, source spelling, or the absence of edits.

The first classified syntax producers use this contract directly. An
assignment expression inside an array literal and a missing conditional
consequent or alternate require a user-selected replacement expression, so
they publish `REQUIRES_USER_DECISION`. A missing conditional colon publishes a
machine-applicable insertion only when an alternate expression already exists;
without that expression it publishes `INSUFFICIENT_CONTEXT`. These builders
live in the focused diagnostic fix-disposition module rather than expanding
the general diagnostic builder.

Syntax recovery producers follow the same rule. Missing assignment values,
right operands, condition expressions, and member names require user-authored
semantics and publish `REQUIRES_USER_DECISION`. The test declaration name-close
builder does not receive a precise insertion range, so it publishes
`INSUFFICIENT_CONTEXT` instead of manufacturing an edit from its primary
range. These producers also live in the focused fix-disposition module.

Invalid using binders, non-constant import paths, and union pattern
shape/field/arity/variant mismatches also publish
`REQUIRES_USER_DECISION`. Their dynamic names and available-field lists remain
display metadata only; they cannot select a fix policy or synthesize a source
edit. Correcting these diagnostics requires choosing a binding, module path,
or pattern that matches the canonical declaration.

Ownership diagnostics complete the direct-builder classification. Weak wake,
borrow escape, loan escape, owner-to-plain escape, ownership mismatch, and
use-after-move all require a semantic choice from the user and publish
`REQUIRES_USER_DECISION`. The legacy ownership type warning publishes
`INSUFFICIENT_CONTEXT`: it can describe the generic wrapper form, but its
builder does not own a precise typed replacement range and therefore cannot
publish a safe edit. Ownership type names remain display data only. The
focused disposition module owns the six general producers, while the
use-after-move producer remains in its ownership-specific module and applies
the same structured contract.

Query-materialized diagnostics also own an explicit disposition. Removing
unreachable code is not a semantics-preserving edit, so `unreachable_code`
publishes `UNSAFE_EDIT`. Numeric overflow and array bounds/type diagnostics
require choosing a wider type, guard, or different index and publish
`REQUIRES_USER_DECISION`. The generic compiler-error persistence bridge has
only an exact message and range after an unstructured compiler failure; it
publishes `INSUFFICIENT_CONTEXT` rather than inferring a repair from that
message. Definite-assignment diagnostics retain their typed placeholder fix.

Exact type inference failure follows the same producer boundary.
`ZrParser_Compiler_ReportCannotInferExactType` publishes descriptor `2020`,
code `cannot_infer_exact_type`, error severity, type category, the exact
declaration or expression range, canonical cause and suggestion text, zero
fixes, and `REQUIRES_USER_DECISION`. An LSP analyzer first preserves any more
specific structured compiler error already present; otherwise it invokes this
reporter and consumes the resulting persistent query fact. It must not create
the code locally, compare diagnostic text, or delete a same-range inference
diagnostic after another producer runs.

The semantic analyzer no longer exposes
`ZrLanguageServer_SemanticAnalyzer_AddDiagnostic`. That API accepted raw
severity, message, and code values and could bypass descriptor lookup, fix
disposition, and persistent query identity even after its production callers
were migrated. Semantic analysis tests now inject only a parser structured
diagnostic projected through `ZrLanguageServer_Diagnostic_FromStructured`.
The lower-level diagnostic allocator remains available to protocol and syntax
recovery layers; it is not a semantic fact producer. Remaining analyzer-side
structured builders are tracked as separate producer-migration work.

Compiler and parser rule producers follow the same boundary. Ref-struct
storage violations, reference escapes, resource strong-cycle warnings, and
type mismatches without a typed conversion contract publish
`REQUIRES_USER_DECISION`. A type mismatch with a canonical conversion hint
retains its placeholder cast fix instead. Removed legacy syntax publishes
`INSUFFICIENT_CONTEXT` because the cutover parser owns the rejected token
range and migration guidance, but not one exact replacement expression or
declaration. Related reference-origin ranges and all existing stable codes,
severity, and messages remain unchanged.

Ownership return escapes use the same compiler reference-escape pass in normal
compilation and semantic analysis. The pass classifies an owner-backed return
only from the binding's structured `ownershipQualifier`, the callable's
`referenceAccess`, and the canonical reference provenance escape bound. A
`Unique` or `Shared` source whose reference cannot reach the caller publishes
`loan_escape` for writable returns or `borrow_escape` for readonly returns.
An input reference already bounded to the caller remains legal. The rule never
matches a parameter name, return text, diagnostic message, or LSP symbol.

The persistent diagnostic contains descriptor 4003 or 4002, the exact returned
expression range, two related ranges for the source use and callable-body
lifetime end, and `REQUIRES_USER_DECISION`. The same pass also publishes the
corresponding ownership violation fact. Semantic analysis invokes the public
compiler validation entry on its module or function analysis root, consumes
the current structured error, and then materializes the query snapshot. The
LSP diagnostic store merges that immediate projection with the same canonical
query row by code and exact range, so one rule produces one diagnostic. The
language server does not rebuild owner qualifiers, line-wide ranges, or
related information from the AST.

Interface variance diagnostics also have a parser-owned publication boundary.
`ZrParser_Variance_PublishInterfaceDiagnostics` enumerates every violation for
one canonical interface declaration, builds descriptor `2013` through the
shared variance diagnostic builder, and appends each result to the semantic
context as a persistent diagnostic fact. The API deliberately does not set the
compiler's current error: normal compilation retains its first-error gate,
while semantic consumers can materialize all independent violations from one
snapshot.

The LSP typecheck pass calls only this publication API. It does not enumerate
variance positions, invoke the diagnostic builder, append semantic facts, or
reconstruct code `invalid_variance`. The removed analyzer-local producer is
not replaced by a symbol-table, type-name, message, or source-text fallback.
Primary use ranges, related generic-parameter declaration ranges, descriptor
identity, help metadata, and no-fix disposition therefore originate in one
parser query contract.

Persistent semantic facts enforce disposition completeness. Append rejects a
diagnostic that has neither at least one typed fix nor a nonzero no-fix reason;
the producer must resolve that state before the fact crosses the snapshot
boundary. Legacy property migration without an exact replacement publishes
`REQUIRES_USER_DECISION`, while a removed ownership member without a canonical
intrinsic replacement publishes `INSUFFICIENT_CONTEXT`. Supplying an exact
replacement retains the existing machine-applicable fix and leaves the
no-fix reason unspecified.

## Limits And Next Steps

Source struct/class/interface fields and methods, direct imports, destructured
imports, type-value aliases, and native callable identities for non-generic and
generic descriptor members are complete in the source producer subset.
Their facts originate only from compiler-registered symbols and carry canonical
identity, declaration range, access, and static flags. Binary typed exports
now preserve structured open generic callable declaration rows for imported
`CallAt` and `FormatCall`, but they and native descriptor analyzers remain
missing visibility producers. Static method state propagates through nested
scopes for the existing receiver/static filter; no LSP consumer has migrated
in this slice.

Direct source module bindings of the shape `identifier = import("...")` now
reuse their compiler-registered variable declaration symbol and range as one
module-scope candidate. The scope producer classifies the binding from the
declaration and import-expression AST kinds alone, marks it as both import and
alias, and lets `includeImports` control exposure. It does not inspect a module
path, look up a module by text, or manufacture an alias target. Binary metadata
and native descriptor imports remain separate producers.

Source destructuring and type-value aliases now share the same canonical
declaration path. For `var {Vec3: Vector3} = import("zr.math")`, the compiler
registers the local binding node `Vec3`, rather than the imported member token
`Vector3`, with `RegisterVariableEx`; its semantic declaration fact, SymbolId,
TypeId, and declaration range therefore all identify the binding exactly. The
source-scope producer recognizes that only a destructuring declaration whose
initializer is an import expression is an import/alias candidate. For
`var MatrixType = int[][]`, it recognizes the type-literal initializer as an
alias but not an import. Both forms use the existing `includeImports` option,
which deliberately filters imports and aliases together; no query, LSP, or
name-based module/member fallback is involved. Binary metadata and native
descriptor aliases remain separate producers.

The API does not yet expose local re-analysis, compiler frontend binary/external serialization of query diagnostics, or CFG loop reaching-definition fixed points. `VisibleSymbols` now consumes compiler-published source module/function/block facts for functions, parameters, locals, top-level types, source type/free-function generic parameters including const parameters, struct/class/interface method type-generic parameters, source type members, receiver members, direct imports, object-destructured imports, and type-value aliases. Binary `.zro` metadata still needs to carry canonical open generic callable declaration rows and publish equivalent symbol/visible facts; native descriptor call identity is now covered separately from native visibility. Documentation facts now have an exact SymbolId-keyed parser query surface, while source/binary/native documentation producers and LSP consumer migration remain pending. No LSP consumer has migrated in this slice. `DefinitionsOf` exposes the first multi-definition surface and deterministic same-source source-order ranking, but ranking remains local-symbol oriented rather than overload/member aware. LSP definition navigation and compiler-side semantic contexts now consume both linear and first-slice CFG-backed reaching definitions for local symbols. Definite-assignment diagnostics can consume explicit read states, the straight-line semantic-facts resolver, or the CFG-backed resolver for source reads across branch joins, declaration initializers, and cloned `finally` paths. Structured diagnostics now preserve an explicit typed no-fix reason as an alternative to machine edits, but existing producers have not all classified their no-fix cases and LSP golden parity remains pending. Current diagnostic related information is limited to declaration locations for definite-assignment read diagnostics; complete descriptor/registry coverage, ownership related locations, and type-mismatch related locations remain pending. Array index diagnostics still keep truly unknown and no-inferable-range indexes silent. Loop precision, remaining finally edge cases, local re-analysis, richer source mapping, and non-cache compiler diagnostic channels remain pending.
