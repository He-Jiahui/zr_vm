---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
plan_sources:
  - user: 2026-07-30 execute AOT plans 07 through 12 and record each completed sub-milestone
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/12-code-stripping.md
tests:
  - tests/parser/test_aot_reachability.c
  - tests/parser/test_aot_c_code_stripping.c
  - tests/parser/test_aot_c_generic_reference_sharing.c
  - tests/acceptance/2026-07-30-aot-12-s1b-s2f-s6a-function-reachability-manifest.md
  - tests/acceptance/2026-07-30-aot-12-property-accessor-required-root.md
  - tests/acceptance/2026-07-30-aot-12-resource-drop-required-root.md
  - tests/acceptance/2026-07-30-aot-12-generic-methodspec-required-root.md
  - tests/acceptance/2026-07-30-aot-12-reflection-constructor-required-root.md
  - tests/acceptance/2026-07-30-aot-12-package-method-export-required-root.md
  - tests/acceptance/2026-07-30-aot-12-native-callback-materialization-edge.md
  - tests/acceptance/2026-07-30-aot-08-12-canonical-generic-dictionary-reachability.md
doc_type: module-detail
---

# AOT Function Reachability Manifest

## Purpose

The function reachability layer gives opt-in AOT code stripping an explicit graph and a deterministic explanation for
every retained function. It separates language-level retention from linker dead stripping: the compiler decides which
nodes are reachable, records why each node survived, publishes the result, and only then filters the generated function
table.

This slice owns function nodes and the first owner-linked generic dictionary nodes. The first type/layout
reason-manifest slice is documented separately in `aot-type-layout-reachability-manifest.md`; canonical shared-body
definition identity, constraint witnesses, explicit native callback descriptor roots, module initializer, reflection
metadata, debug sidecar, and constructor type-reachability narrowing still need to converge on the broader graph
required by AOT plan 12.
The function graph never scans source or generated strings to guess reflection or native reachability.

## Graph Contract

`SZrAotReachabilityEdge` uses stable flat function indices. A mark records its state, the reason that first retained it,
and the predecessor index that supplied that edge. Root reasons are:

- `ROOT_ENTRY`
- `ROOT_EXPORT`
- `MANIFEST`
- `REFLECTION_ANNOTATION`
- `PROPERTY_ACCESSOR`
- `RESOURCE_DROP`
- `GENERIC_METHODSPEC`
- `REFLECTION_CONSTRUCTOR`
- `PACKAGE_EXPORT`

Dependency-edge reasons are:

- `DIRECT_CALL`
- `FIELD_ACCESS`
- `VIRTUAL_CALL`
- `REFLECTION`
- `GENERIC_INSTANCE`
- `NATIVE_CALLBACK`

The two classes are intentionally disjoint. `backend_aot_reachability_compute()` rejects an edge reason in the root
array, a root reason on an edge, `NONE`, and unknown enum values before initializing or publishing marks. It also
rejects missing buffers, out-of-range roots or endpoints, and a queue smaller than the function index space.

`PROPERTY_ACCESSOR` roots come from non-abstract serialized compiled prototype members with a valid property identity
and accessor role `1` (getter), `2` (setter), or `3` (initializer). Their `functionConstantIndex` must resolve to a
stable function table entry. An executable accessor whose callable constant is missing or out of range fails graph
construction; it is not silently trimmed. Abstract accessors are contract-only and have no executable target, so they
remain ignored. Members without a property identity or a valid accessor role also remain outside this collector.

`RESOURCE_DROP` roots cover the runtime metadata dispatch in `ZrCore_OwnershipResource_Drop()`. A non-abstract member
is required when its serialized owner prototype has the `RESOURCE` modifier, `isMetaMethod` is true, and `metaType` is
`ZR_META_DESTRUCTOR`. Its `functionConstantIndex` must resolve to a stable function table entry; otherwise graph
construction fails closed. Non-resource destructors, non-meta members, other meta methods, and abstract destructor
contracts are ignored. This slice conservatively roots every executable resource destructor present in serialized
prototype metadata; narrowing those roots by reachable type/layout is a later graph-convergence stage.

`REFLECTION_CONSTRUCTOR` roots protect runtime `ZrCore_Reflection_CreateInstance()` binding. The conservative safe
policy retains every public, non-abstract `ZR_META_CONSTRUCTOR` serialized on a non-abstract, non-resource class or
struct prototype. A qualifying constructor must resolve its `functionConstantIndex` to a stable function-table entry;
otherwise graph construction and public writer output fail closed. Interface constructors, resource or abstract
prototype contracts, non-public or abstract members, non-meta members, and other meta methods remain ignored. This
first slice deliberately does not prove that the owner type itself is reflection-reachable; coupling constructor roots
to retained reflection type nodes is later convergence work.

`GENERIC_METHODSPEC` roots connect writer preserve metadata to executable code stripping. For every
`SZrAotManifestGenericRoot` with `hasMethodSpecBinding`, the collector resolves `methodSpecMethodToken` as a
current-module `MemberDef` through the root function's typed symbols. Exactly one symbol must map to a callable child
and stable function-table entry. A missing, non-`MemberDef`, ambiguous, or unmappable target fails graph construction
and removes partial writer output. Generic roots that carry only TypeSpec/type-instantiation identity do not retain a
function in this collector. Cross-module `MemberRef`, generic dictionaries, and constraint witnesses remain separate
graph work.

`PACKAGE_EXPORT` roots connect package manifest method declarations to executable code stripping. A declaration of
kind `METHOD` must carry a current-module `MemberDef` binding that resolves through exactly one typed callable symbol
to a stable function-table entry. Missing bindings, non-`MemberDef` tokens, missing or ambiguous symbols, unmappable
children, and unknown declaration kinds fail graph construction and remove partial writer output. Type and field
declarations remain metadata-only in the function collector. The declaration target string is never scanned to infer
the callable. Canonical cross-module `ModuleIdentity` plus `MemberRef` resolution remains a later graph slice.

`NATIVE_CALLBACK` edges identify callable materialization whose destination slot has a structured
`NATIVE_BINDING` escape row carrying `NATIVE_HANDLE`. The collector applies this reason to resolved
`GET_SUB_FUNCTION`, callable `GET_CONSTANT`, and `CREATE_CLOSURE` targets; the same instructions remain
`DIRECT_CALL` edges when that exact slot contract is absent. A nonempty escape-binding table with a null row pointer
fails graph construction and public writer output. This edge retains callbacks created from a reachable function and
does not replace the future explicit descriptor root needed for externally registered or cross-module callbacks.

Reference-generic dictionary nodes use the compiler-owned typed-local `typeId` as their instance identity. Different
display names with the same nonzero `typeId` share one dictionary, while equal display names with different TypeIds
remain distinct. Every retained owner resolves back to that canonical dictionary ID. The version 1 dictionary manifest
publishes the canonical TypeId, stable owner function index, `edge.generic_instance`, and the owner as predecessor;
before/after/removed counts prove that dictionaries owned only by trimmed functions disappear. A pre-ExecIR tree check
rejects nonempty/null typed-local, frame-layout, and child-function tables; a reference-generic candidate without
TypeId or conflicting layout IDs for one TypeId also fails writer output. Type text remains discovery/debug input in
this baseline, not dictionary instance identity.

## Marking And Reason Chains

The engine performs breadth-first marking with caller-owned mark and queue buffers. Roots are enqueued in caller order;
edges are visited in input order. The first discovery owns the node's reason and predecessor, so an explicit root keeps
its root reason even when another retained function also reaches it.

Every successful retained mark is `PROCESSED`. A root has `predecessor=none`; an edge-retained node points to another
processed node. Unmarked entries must remain `NONE` with no predecessor. These invariants preserve stable sparse flat
indices while allowing the function table to replace trimmed entries without renumbering retained functions.

## Manifest Publication

Before code stripping filters the function table, the C emitter writes a versioned generated-C comment manifest:

```text
/* reachability.functionManifest.version = 1 */
/* reachability.functionManifest.count = 2 */
/* reachability.functionManifest.node[0] = reason=root.entry predecessor=none */
/* reachability.functionManifest.node[1] = reason=edge.direct_call predecessor=0 */
```

Rows are emitted in ascending flat-index order and use stable reason names. The writer revalidates the complete mark
array before writing the first byte. Pending marks, dirty unmarked entries, an edge with no predecessor, a root with a
predecessor, an out-of-range predecessor, and predecessor cycles all fail closed. Therefore a successful manifest gives
every retained function a finite chain to an explicit root.

The manifest is diagnostic evidence embedded in generated C; it is not parsed back as graph input and does not replace
the compiled function table or metadata token remap.

The owner-linked dictionary manifest follows the same diagnostic rule:

```text
/* reachability.genericDictionaryManifest.version = 1 */
/* reachability.genericDictionaryManifest.count = 1 */
/* reachability.genericDictionaryManifest.node[0] = typeId=41 ownerFunction=0 reason=edge.generic_instance predecessor=0 */
```

## Test Coverage

`test_aot_reachability.c` covers transitive marking, disconnected nodes, first-root reason preservation, invalid graph
bounds, invalid root/edge/unknown reasons, stable manifest order, malformed states, out-of-range predecessors, and
cycles. It also covers getter, setter, and initializer property roots, unresolved callable constants for all three
roles, abstract contract-only accessors, and non-accessor filtering. `test_aot_c_code_stripping.c` proves the emitter
publishes direct-call, export-root, manifest-root, and property-accessor-root chains while omitting trimmed function
nodes. The same suites cover resource destructor retention, unresolved required destructors, non-resource/meta/abstract
filtering, `root.resource_drop` publication, and partial-file cleanup.

The suites also cover a non-exported MethodSpec-bound callable retained as `root.generic_methodspec`, null/missing,
non-`MemberDef`, and ambiguous bindings rejected fail closed, TypeSpec-only roots ignored by the function collector,
the generated-C reason row, and public-writer partial-file cleanup.

Reflection constructor coverage includes class and struct roots, unresolved required callables, and a negative filter
matrix for abstract/resource/interface/non-public/abstract-member/non-meta/non-constructor cases. The writer suite
proves `root.reflection_constructor` publication, unrelated-function trimming, and failed-output cleanup.

Package export coverage proves a non-source-exported method survives as `root.package_export`; null declaration
arrays, missing/non-`MemberDef`/unresolved/ambiguous method bindings, and unknown kinds fail closed. Type and field
declarations do not retain functions, while the public writer publishes the bound token and removes failed output.

Native callback coverage proves all three callable materialization opcodes use `edge.native_callback` for an exact
native escape binding, ordinary materialization remains `edge.direct_call`, malformed escape metadata fails closed,
and the public writer publishes the callback predecessor chain while removing failed output.

Generic dictionary coverage proves canonical TypeId dedup despite differing display names, distinct identity despite
equal display names, canonical dictionary reuse by two retained owners, 2-to-1 unreachable-node trimming, stable
manifest/stat publication, and fail-closed missing TypeId, null binding table, and conflicting layout schema cases.
The null-table matrix includes frame-layout metadata and proves rejection occurs before ExecIR construction.

The acceptance records run the focused reachability, stripping, and generic-sharing targets on WSL GCC, WSL Clang,
and Windows MSVC. Broader graph-node convergence and behavior/size comparisons remain separate AOT 12 stages.
