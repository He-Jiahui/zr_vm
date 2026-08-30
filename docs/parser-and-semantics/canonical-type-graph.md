---
related_code:
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_definition.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_projection.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union_canonical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union_canonical.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_tuple.c
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_format.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_definition.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lifecycle.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_projection.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union_canonical.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union_canonical.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_tuple.c
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_format.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
plan_sources:
  - user: 2026-07-19 按 docs/plans/syntax 严格实施语法优化并逐里程碑记录和提交
  - docs/plans/syntax/README.md
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
tests:
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_canonical_type_graph_semantic_cases.h
  - tests/parser/test_parser.c
  - tests/parser/test_union.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_cfg_union_exhaustiveness.c
  - tests/language_server/test_lsp_expression_fact_hover.c
  - tests/acceptance/2026-07-19-syntax-01-m1-canonical-type-graph.md
doc_type: module-detail
---

# Canonical Type Graph

## Purpose

The canonical type graph gives parser semantics one structural type identity. A `TZrTypeId` now names an immutable node instead of naming a registration record whose identity also depends on display text, an AST address, or a source range.

This is the M1 foundation for later Place, CFG, borrow, artifact, VM, and AOT work. M1 keeps `SZrInferredType` as a compatibility input so current inference remains executable, but semantic registration, function symbols, and LSP type display can consume the same canonical `TypeId`.

## Node Model

The public graph supports these node kinds:

- `Primitive`, identified by `EZrValueType` rather than a display name.
- `Nominal`: resolved definitions use module identity plus definition token; unresolved token-zero names use module identity plus name until binding supplies a token.
- `GenericParameter`, identified by owner symbol and ordinal.
- `GenericInstance`, identified by definition `TypeId` and ordered typed arguments. Arguments are type `TypeId` values, const integer literals, or open const parameters identified by owner symbol plus ordinal.
- `Array`, identified by element `TypeId`, rank, and storage kind.
- `Tuple`, identified by its ordered element `TypeId` list.
- `Union`, identified by its definition `TypeId` and ordered variant `TypeId` list.
- `Error` and `Never` singleton nodes.
- `Ref`, identified by pointee `TypeId` and writable or readonly access.
- `Owner`, identified by target `TypeId` and unique, shared, weak, or atomic-shared ownership.
- `ReadonlyView` and `Nullable`, each identified by a target `TypeId`.
- `Function`, identified by all parameter contracts, return `TypeId`, receiver effect, and callable effect flags.

Source ranges, AST pointers, variable names, local lifetime regions, numeric ranges, and flow-sensitive initialization state do not enter structural identity. Those values remain semantic facts associated with a use site.

## Structural Interning

Every constructor computes an in-session structural hash from the complete identity payload. Composite hashes may contain child `TypeId` values and therefore are not persisted ABI hashes. M4 owns stable artifact hashes. Interning first visits the collision chain for the session hash and then performs exact structural equality. A hash match alone never establishes type equality.

`canonical_type_index.c` owns a power-of-two bucket table and a per-node collision link array. The table grows before its load exceeds 75 percent and rebuilds all chains from the stored node hashes. This keeps large graphs from degenerating into a full scan for every new type. The index deliberately stores node indexes rather than pointers because `SZrArray` growth may relocate the canonical node storage.

Canonical nodes are appended in monotonically increasing `TypeId` order. `ZrParser_CanonicalType_Find` uses binary search over that invariant, so ordinary `TypeId` lookup does not scan the hash table or the complete graph. Structural lookup and identity lookup therefore have separate, explicit paths.

The graph, definition registry, and index share `SZrSemanticContext` lifetime. Lifecycle implementation is isolated in `canonical_type_lifecycle.c`:

- context initialization creates the node array and index;
- reset frees per-node arrays, clears nodes, and clears buckets and collision links;
- free performs reset and releases all graph and index storage.

## Legacy Projection

`ZrParser_CanonicalType_FromInferred` recursively projects the existing inferred representation:

- primitive base types become primitive nodes;
- named object types become nominal nodes;
- parsed generic names plus inferred element types become generic instances;
- explicit generic bindings map a name to the `GenericParameter(ownerSymbolId, ordinal)` already reserved for its function or type declaration;
- const generic bindings preserve literal values or map an open const name to `ConstParameter(ownerSymbolId, ordinal)`; display names do not enter identity;
- arrays recurse into their element type;
- tuple AST conversion stores ordered element inferred types, which become tuple nodes;
- legacy ownership qualifiers become owner or ref wrappers;
- nullable becomes an outer nullable wrapper.

Generic name decomposition reuses the existing type-inference generic parser. The adapter does not add a second generic string grammar. The remaining `typeName` fields belong to the compatibility representation and diagnostics; new semantic consumers should query `TypeId` and the canonical graph.

`ZrParser_Semantic_RegisterInferredType` and `ZrParser_Semantic_RegisterNamedType` now obtain a canonical ID before publishing compatibility records. Registrations of structurally identical types reuse the same ID even when their use sites differ. Compatibility records retain structural fields only; source AST/name, numeric ranges, known values, and array-size refinements remain expression/reference facts.

Top-level union compilation registers a nominal definition, reserves its type symbol, binds type and const generic parameters to that symbol, converts unit/tuple/struct payload fields through inference, interns the ordered canonical union payload list, and rebinds the semantic type symbol to the resulting Union `TypeId`. Projection recursively substitutes nested type and const parameters and gives a closed union the closed generic instance as its definition identity. The union-specific builder lives in `compiler_union_canonical.c` so layout/lowering orchestration remains focused.

Known class, struct, and union generic contracts validate arity, argument kind, and constraints before publishing a prototype, type-environment entry, variable, symbol, or semantic type. Failed canonicalization therefore cannot leave a visible half-registered instance.

## Callable Contracts

A function `TypeId` includes each parameter's type and call contract:

- passing form: value, `in`, `ref`, `ref readonly`, or `out`;
- escape upper bound;
- entry and exit initialization requirements;
- temporary acceptance;
- call-site marker.

The identity also includes the return `TypeId`, receiver effect, and async/throws/generator flags. Type-environment function registration now publishes this function `TypeId` on the function symbol instead of incorrectly reusing the return type's ID.

The legacy adapter normalizes `in`, `ref`, and `out` parameters into canonical ref nodes plus the appropriate callable contract. The interner rejects raw `out T`, readonly `out`, writable `in`, wrong escape/initialization/temporary/marker combinations, negative enum values, and unknown effect bits. This keeps `out` initialization state on the parameter contract rather than creating a general-purpose `out T` type.

Function registration reserves the function symbol before projecting generic parameter names, then canonicalizes the complete signature. It publishes the function environment entry, symbol, overload member, and declaration fact only after canonicalization succeeds; failure can consume an internal ID but cannot expose a `typeId == 0` symbol.

Source type members retain an owned `SZrInferredType` for explicit returns in addition to the compatibility display name. Closing a generic type recursively substitutes that structured return, including open and fixed const leaves. LSP signature help deep-copies and specializes the same structure; string parsing remains a boundary fallback for imported/native metadata that does not yet carry structured returns.

## Type Definition Capabilities

Nominal nodes can have a separate definition record with stable capability flags and GC scan kind. Generic definitions additionally record owner symbol, parameter kinds, and arity. A closed generic instance inherits the definition capabilities and matches constructor signatures after recursively substituting matching type parameters and typed generic arguments, including parameters nested under arrays, tuples, projected unions, refs, owners, wrappers, generic instances, and functions. Open const arguments bind through owner symbol plus ordinal before comparing a literal or still-open argument.

Value-construction resolution requires all of the following:

- the target is a registered nominal definition;
- the definition has `VALUE_CONSTRUCTIBLE` capability;
- a public constructor has an exact canonical parameter list match;
- the match is unambiguous.

A runtime `zr.reflection.Type` expression receives no implicit construction authority. Carrying or displaying a `TypeId` is not enough to enter static value construction. Reflection construction remains a separate capability boundary planned for the reflection library work.

## Formatting And LSP

The formatter recursively renders every current canonical shape, including qualified nominal names, generic instances, arrays, tuples, unions, refs, owners, readonly views, nullable types, and function contracts. Rank-one and nested arrays use `T[]`/`T[][]`; a single rank-two node uses `T[,]`, so distinct TypeIds cannot format identically. It returns failure and clears the destination when the buffer is too small or the graph is invalid.

`ZrLanguageServer_SemanticAnalyzer_FormatTypeId` delegates to that formatter. Local semantic hover prefers the `TypeId` on a resolved reference fact and falls back to the legacy inferred display only when no canonical ID is available. This makes a real tuple parameter hover display `(int, bool)` from the graph rather than rebuilding the type from LSP-local strings.

## Boundaries

M1 establishes an in-session canonical graph. It does not claim that `.zrs`, `.zri`, or `.zro` already serialize the new graph. Stable artifact rows, signature hashes, loader validation, and source-versus-binary identity are M4 work.

M1 also does not remove `SZrInferredType`; doing so before Place/CFG and pre-execution SemIR consumers exist would mix the compatibility migration with later semantic changes. New code should avoid adding concrete type-name dispatch and should extend canonical nodes or definition capabilities instead.

## Test Coverage

`test_canonical_type_graph.c` contains 19 tests covering exact structural identity and distinction for every node family, normalized callable contracts, all current formatter shapes, inferred-type projection, type/const generic function symbol registration, tuple AST projection, parser-to-compiler union/generic-parameter binding, capability-based generic constructor substitution (including open const and projected-union patterns), invalid kind/arity/constraint publication boundaries, structured closed member returns, and lifecycle cleanup. The tuple AST fixture uses the current `fn pair(): [int, bool]` declaration contract; keywordless function syntax is not accepted or emulated by the canonical layer.

Its stress case interns 100,000 unique generic-parameter types, re-interns sampled values without growing the graph, verifies an actual hash-bucket collision-chain lookup, and then builds and formats a 256-level array type. The pre-index implementation exceeds a five-second process gate; the indexed implementation completes the same test within that gate.

Parent regression coverage includes parser, union, type inference, semantic facts, CFG union exhaustiveness, and real LSP hover tests. Compiler-specific results and exact commands are recorded in the acceptance document and the syntax-plan M1 status record.

## Follow-up

- M2 consumes `TypeId` while introducing Place identity and general CFG edges.
- M3 makes pre-execution Semantic IR the source of load/store/move/borrow behavior.
- M4 serializes stable type, signature, layout, and callable-contract projections.
- M5 removes remaining consumer-side type-name inference in VM, AOT, LSP, reflection, and debug paths.
