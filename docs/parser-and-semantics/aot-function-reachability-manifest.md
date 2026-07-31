---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_native_imports.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_native_imports.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_source_location.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_source_location.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_layout_manifest.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_layout_manifest.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_debug_sidecar_manifest.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_debug_sidecar_manifest.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_native_imports.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_frame.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_source_location.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_layout_manifest.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_debug_sidecar_manifest.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
plan_sources:
  - user: 2026-07-30 execute AOT plans 07 through 12 and record each completed sub-milestone
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
tests:
  - tests/parser/test_aot_reachability.c
  - tests/parser/test_aot_c_code_stripping.c
  - tests/parser/test_aot_c_generic_reference_sharing.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/acceptance/2026-07-30-aot-12-s1b-s2f-s6a-function-reachability-manifest.md
  - tests/acceptance/2026-07-30-aot-12-property-accessor-required-root.md
  - tests/acceptance/2026-07-30-aot-12-resource-drop-required-root.md
  - tests/acceptance/2026-07-30-aot-12-generic-methodspec-required-root.md
  - tests/acceptance/2026-07-30-aot-12-reflection-constructor-required-root.md
  - tests/acceptance/2026-07-30-aot-12-package-method-export-required-root.md
  - tests/acceptance/2026-07-30-aot-12-native-callback-materialization-edge.md
  - tests/acceptance/2026-07-30-aot-08-12-canonical-generic-dictionary-reachability.md
  - tests/acceptance/2026-07-30-aot-11-12-native-import-contract-reachability.md
  - tests/acceptance/2026-07-30-aot-07-execir-frame-abi-verifier.md
  - tests/acceptance/2026-07-30-aot-07-frame-type-layout-closure-verifier.md
  - tests/acceptance/2026-07-30-aot-07-complete-frame-parameter-identity-verifier.md
  - tests/acceptance/2026-08-01-aot-07-constructor-bitmap-layout-verifier.md
  - tests/acceptance/2026-08-01-aot-07-receiver-role-frame-verifier.md
  - tests/acceptance/2026-08-01-aot-07-parameter-binding-identity-verifier.md
  - tests/acceptance/2026-07-30-aot-12-debug-sidecar-reachability.md
doc_type: module-detail
---

# AOT Function Reachability Manifest

## Purpose

The function reachability layer gives opt-in AOT code stripping an explicit graph and a deterministic explanation for
every retained function. It separates language-level retention from linker dead stripping: the compiler decides which
nodes are reachable, records why each node survived, publishes the result, and only then filters the generated function
table.

This slice owns function nodes and the first owner-linked frame-layout slot, debug sidecar, generic dictionary, and
native import contract nodes. The first type/layout
reason-manifest slice is documented separately in `aot-type-layout-reachability-manifest.md`; canonical shared-body
definition identity, constraint witnesses, explicit native callback descriptor roots, module initializer, reflection
metadata, safepoint variable maps, and constructor type-reachability narrowing still need to converge on the broader graph
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

The separate canonical-contract node schema also defines `NATIVE_IMPORT`. It is valid only for a retained function's
owner edge to one of its `SZrNativeImportContract` rows. The function reachability engine rejects it as a
function-to-function edge, so contract ownership cannot accidentally retain an unrelated function.

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

Native import contract nodes use canonical contract fields already produced by binding: `symbolId`,
`callableContractHash`, and the complete validated `FfiSignature`. The C writer validates every row in the original
function tree before ExecIR construction, including contracts on functions that code stripping would later remove.
A nonempty/null table, per-function count overflow, function-table index space larger than capacity,
schema/hash/ABI corruption, or invalid policy therefore fails closed instead of disappearing with an unreachable
owner. After function filtering, the retained contract manifest walks sparse flat function indices and contract
indices in ascending order. Entry-point and library strings remain payload, not reachability identity or
graph-discovery input.

Debug sidecar nodes are the canonical `SZrFunctionExecutionLocationInfo` rows already owned by each function. ExecIR
validates the complete source function tree before any reachability filtering: a nonzero row count requires both the
row table and instruction table; instruction offsets must be nonnegative, in range, and nondecreasing, and explicit
line/column end positions cannot precede their starts. Row count may exceed instruction count because quickening can
coalesce distinct source ranges onto one instruction offset. This preserves valid duplicate offsets while rejecting
malformed unreachable rows before they can disappear. The sidecar is linked only to its owning function; generated
text and source strings are never scanned to infer debug reachability.

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

The native import contract manifest follows the same owner/predecessor rule:

```text
/* reachability.nativeImportManifest.version = 1 */
/* reachability.nativeImportManifest.count = 1 */
/* reachability.nativeImportManifest.node[0] = ownerFunction=0 contractIndex=0 symbolId=0x0000000000000101 callableContractHash=0x4478327a8def8f8b reason=edge.native_import predecessor=0 */
```

`code_stripping.nativeImportsBefore`, `nativeImportsAfter`, and `nativeImportsRemoved` report contract-node trimming
independently from function and generic dictionary counts.

The retained frame-layout manifest is generated from verified ExecIR rather than the mutable source function table:

```text
/* reachability.frameLayoutManifest.version = 1 */
/* reachability.frameLayoutManifest.count = 1 */
/* reachability.frameLayoutManifest.node[0] = reason=edge.frame_layout predecessorFunction=1 ownerFunction=1 slotLayout=0 stackSlot=0 byteOffset=0 byteSize=8 byteAlign=8 typeLayoutId=1 slotKind=inline_struct isParameter=0 flags=0x0000 */
```

Rows are ordered by ascending flat owner and then source slot-layout index. `frameLayoutSlotsBefore`,
`frameLayoutSlotsAfter`, and `frameLayoutSlotsRemoved` measure sidecar rows independently from function and referenced
TypeLayout counts. Every row names its owner as predecessor because frame storage cannot survive without that retained
function. The earlier type-layout manifest remains a separate graph: it deduplicates referenced TypeLayout identities,
whereas this manifest preserves every retained owner/slot ABI row.

Before either manifest is derived, every inline-struct frame row in the complete source function tree resolves through
the canonical prototype-frame TypeLayout cache. ExecIR rejects unresolved identities, invalid schema/hash state,
`cTypeId` identity drift, non-aggregate kinds, and payload size/alignment drift even when the owner would later be
trimmed. Alias payload identity is checked separately from the existing physical binding size/alignment rules, retaining
legal direct overlap and indirect/borrowed storage shapes.

For a complete frame table, retained `isParameter` values are also verified graph inputs rather than unchecked
diagnostics. ExecIR derives the canonical parameter slots from the same typed-binding rule as the producer, or from the
stack-slot prefix when typed bindings are absent, and rejects missing or swapped markers before filtering. Zero-row and
sparse hybrid tables deliberately remain legal so register-only scalar parameters need not be materialized merely to
satisfy the frame manifest.

The constructor initialization-bitmap flag is also a verified graph input. Before filtering, its owner must be a
constructor with a direct inline-struct parameter at stack slot 0, and the core-owned layout query must resolve a
canonical, schema/hash-valid receiver TypeLayout whose identity and payload shape match the frame row. The query derives
the uint64 bitmap tail from receiver field count and frame size/alignment, then proves every direct, indirect-alias, and
borrowed-alias physical storage envelope ends before the tail. Malformed unreachable owners therefore fail closed
instead of disappearing during trimming. The retained frame manifest remains unchanged because the bitmap is verified
frame ABI state, not a new reachability-node kind.

Canonical receiver roles are also verified graph inputs rather than trim-discardable typed-local annotations. ExecIR
rejects a missing typed-local table, unknown role bits, duplicate receivers, incomplete receiver identity, a non-slot-0
receiver, a receiver on a zero-parameter function, or a materialized receiver row without the parameter marker before
filtering any owner. A canonical receiver role participates in complete-frame parameter classification without a local
name; sparse and zero-frame layouts remain legal, and role-free older artifacts are not upgraded by inference. The
retained frame manifest stays version 1 because this gate validates owner metadata and does not add a new graph node.

Canonical parameter binding identity is likewise validated before filtering. For a present typed-local table, ExecIR
selects the first `parameterCount` named-or-receiver rows under the producer ordering rule, requires unique in-range
stack slots, and rejects an incomplete or mixed-availability identity set. Fully canonical rows require unique SymbolId
and PlaceId while allowing equal TypeId. Nameless non-receiver rows do not consume the prefix, ordinary rows after it
are not reclassified as parameters, and a receiver after the prefix is malformed. Older artifacts with no typed-local
table, or a uniformly all-zero legacy identity set, retain their compatibility behavior. Because this is an owner
metadata gate rather than a retained node, malformed unreachable owners fail closed without changing the version 1
frame manifest.

The retained debug sidecar manifest is generated from the validated source-location rows and matched ExecIR owners:

```text
/* reachability.debugSidecarManifest.version = 1 */
/* reachability.debugSidecarManifest.count = 1 */
/* reachability.debugSidecarManifest.node[0] = reason=edge.debug_sidecar predecessorFunction=1 ownerFunction=1 locationIndex=0 instructionOffset=0 lineStart=10 columnStart=3 lineEnd=10 columnEnd=8 */
```

Rows are ordered by ascending flat owner and then source location index. `debugLocationsBefore`,
`debugLocationsAfter`, and `debugLocationsRemoved` report sidecar-row trimming independently from function, frame,
and metadata counts. Each row names its retained owner as predecessor. This generated-C diagnostic manifest does not
define the versioned AOT 11 DebugMap artifact section, source checksums, local-variable scopes, spill rewrites, or
safepoint variable-location maps.

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

Native import coverage proves four original contracts become three retained contracts. Two contracts on flat owner 0
and one contract on sparse flat owner 2 are published in owner/contract-index order, while owner 1 has an empty range
and its unreachable entry point is absent from generated C. The suite checks canonical hashes, descriptor-table order,
the exact owner range table, an index-space/capacity bound, and malformed unreachable contract metadata failing before
ExecIR with no artifact. Reason-schema coverage also proves `NATIVE_IMPORT` has a stable name but is rejected by the
function-to-function graph.

Frame-layout coverage proves three original owner slots become two retained rows in stable owner order and a second
four-to-three fixture preserves two same-offset owner slots in source layout order. It accepts direct overlap plus
high-alignment payloads stored through lower-alignment indirect and borrowed bindings, publishes a zero-row manifest
for an all-empty fixture, and rejects non-power-of-two alignment plus an out-of-frame storage span on an unreachable
function before filtering. The malformed writer paths leave no generated artifact. The adjacent generic-sharing
suite also keeps its nonempty/null frame-table rejection gate.

The TypeLayout closure matrix uses canonical union layouts for legal direct, indirect, and borrowed rows, then isolates
six unreachable-owner failures: unresolved identity, nonzero hash mismatch, `cTypeId` mismatch, valid non-aggregate
VALUE kind, payload-size drift, and payload-alignment drift. Each public-writer failure leaves no generated artifact.

Complete-frame parameter coverage first accepts a zero-frame function with one scalar parameter and a sparse hybrid
function that materializes only local slot 1 while parameter slot 0 remains register-only. It then rejects an
unreachable complete table with one missing parameter marker and a two-slot complete table whose marker count is
correct but swapped from canonical parameter slot 0 to local slot 1. Failed output is removed in both cases; the
existing borrowed receiver fixture remains the complete-table alias positive.

Debug sidecar coverage proves four original location rows become three retained rows after owner-function trimming.
The retained manifest orders flat owner 0 before owner 1 and preserves two source rows for owner 1 by location index;
those two rows legally share one quickening-style instruction offset, so row count may exceed instruction count. An
all-empty property-accessor fixture publishes zero counts and no node. A malformed unreachable owner is rejected for
a nonempty/null table, negative and out-of-range offsets, decreasing offsets, inverted line ranges, and inverted
same-line column ranges. Every failed public-writer call leaves its output path absent.

The acceptance records run the focused reachability, stripping, and generic-sharing targets on WSL GCC, WSL Clang,
and Windows MSVC. Broader graph-node convergence and behavior/size comparisons remain separate AOT 12 stages.
