---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
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
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_reference_callable_consumer_cases.h
  - tests/language_server/test_lsp_reaching_definition_navigation.c
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
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

## Test Coverage

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
- The same target compiles `fn identity<T>(value: T): T`. It verifies that `T` is absent at the function declaration name, becomes visible at the generic declaration, carries the exact function owner `SymbolId` and canonical generic-parameter `TypeId`, and is materialized from the published function scope rather than its overlapping module scope.
- The same target compiles `struct Box<T> { fn echo<U>(value: U): U { return value; } }`. It verifies that `U` is absent at `echo`, becomes visible at its declaration with the compiler-registered exact method owner `SymbolId`, has a canonical generic-parameter `TypeId`, and sees enclosing `T` through published scope parentage.
- `tests/parser/test_semantic_query_contract.c` freezes Plan 03 query contract behavior: `TypeAt` fails closed for an approximate expression fact, `FactsAt` returns the same borrowed fact view on repeated reads, and `Diagnostics` cannot materialize context state without the explicit lifecycle operation.
- `tests/language_server/test_lsp_semantic_query_diagnostics.c` covers LSP publishing of semantic query diagnostics by opening a constant ternary branch fixture and requiring `GetDiagnostics` to include `unreachable_code`; it also covers `numeric_overflow`, array bounds errors/warnings, known non-integer array index errors, min-only array negative-interval warnings, and a branch-join `possibly_uninitialized_read` with one declaration relatedInformation entry through the same LSP diagnostic path.
- `tests/language_server/test_lsp_reference_callable_consumer_cases.h` covers resolved callable `CallAt/FormatCall` parity across signature help and hover, scoped reference parameter information, exact receiver target identity, and one persistent `%ref` call-marker `compiler_error` without an inference cascade.
- `tests/language_server/test_lsp_reaching_definition_navigation.c` covers LSP definition navigation through parser reaching-definition facts by requiring `return seed` after `seed = 3` to jump to the write token instead of the declaration token. It also covers divergent branch writes by requiring `return seed` after an `if/else` pair of writes to return both branch write locations and not the declaration.

The parser test targets are `zr_vm_compiler_semantic_query_diagnostics_test`, `zr_vm_semantic_query_test`, and `zr_vm_semantic_query_symbols_test`; the LSP publication targets are `zr_vm_language_server_semantic_query_diagnostics_test` and `zr_vm_language_server_reaching_definition_navigation_test`. They are registered in `tests/CMakeLists.txt`.

## Limits And Next Steps

The API does not yet expose local re-analysis, compiler frontend binary/external serialization of query diagnostics, or CFG loop reaching-definition fixed points. `VisibleSymbols` now consumes compiler-published source module/function/block facts for functions, parameters, locals, top-level types, source type/free-function generic parameters, and struct method type-generic parameters. Const and class/interface method generic scopes, type members, imports/aliases, receiver members, `.zro`, and native descriptor analyzers still need to publish the remaining candidate facts before completion can serve real workspaces. No LSP consumer has migrated in this slice. `DefinitionsOf` exposes the first multi-definition surface and deterministic same-source source-order ranking, but ranking remains local-symbol oriented rather than overload/member aware. LSP definition navigation and compiler-side semantic contexts now consume both linear and first-slice CFG-backed reaching definitions for local symbols. Definite-assignment diagnostics can consume explicit read states, the straight-line semantic-facts resolver, or the CFG-backed resolver for source reads across branch joins, declaration initializers, and cloned `finally` paths. Current diagnostic related information is limited to declaration locations for definite-assignment read diagnostics; fix-its, descriptor IDs, registry entries, ownership related locations, and type-mismatch related locations remain pending. Array index diagnostics still keep truly unknown and no-inferable-range indexes silent. Loop precision, remaining finally edge cases, local re-analysis, richer source mapping, and non-cache compiler diagnostic channels remain pending.
