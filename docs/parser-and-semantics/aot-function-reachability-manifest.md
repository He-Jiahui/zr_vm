---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
plan_sources:
  - user: 2026-07-30 execute AOT plans 07 through 12 and record each completed sub-milestone
  - docs/plans/aot/12-code-stripping.md
tests:
  - tests/parser/test_aot_reachability.c
  - tests/parser/test_aot_c_code_stripping.c
  - tests/acceptance/2026-07-30-aot-12-s1b-s2f-s6a-function-reachability-manifest.md
  - tests/acceptance/2026-07-30-aot-12-property-accessor-required-root.md
doc_type: module-detail
---

# AOT Function Reachability Manifest

## Purpose

The function reachability layer gives opt-in AOT code stripping an explicit graph and a deterministic explanation for
every retained function. It separates language-level retention from linker dead stripping: the compiler decides which
nodes are reachable, records why each node survived, publishes the result, and only then filters the generated function
table.

This slice owns function nodes. The first type/layout reason-manifest slice is documented separately in
`aot-type-layout-reachability-manifest.md`; generic dictionary, native callback, module initializer, reflection
metadata, debug sidecar, and resource Drop nodes still need to converge on the broader graph required by AOT plan 12.
The function graph never scans source or generated strings to guess reflection or native reachability.

## Graph Contract

`SZrAotReachabilityEdge` uses stable flat function indices. A mark records its state, the reason that first retained it,
and the predecessor index that supplied that edge. Root reasons are:

- `ROOT_ENTRY`
- `ROOT_EXPORT`
- `MANIFEST`
- `REFLECTION_ANNOTATION`
- `PROPERTY_ACCESSOR`

Dependency-edge reasons are:

- `DIRECT_CALL`
- `FIELD_ACCESS`
- `VIRTUAL_CALL`
- `REFLECTION`
- `GENERIC_INSTANCE`

The two classes are intentionally disjoint. `backend_aot_reachability_compute()` rejects an edge reason in the root
array, a root reason on an edge, `NONE`, and unknown enum values before initializing or publishing marks. It also
rejects missing buffers, out-of-range roots or endpoints, and a queue smaller than the function index space.

`PROPERTY_ACCESSOR` roots come from non-abstract serialized compiled prototype members with a valid property identity
and accessor role `1` (getter), `2` (setter), or `3` (initializer). Their `functionConstantIndex` must resolve to a
stable function table entry. An executable accessor whose callable constant is missing or out of range fails graph
construction; it is not silently trimmed. Abstract accessors are contract-only and have no executable target, so they
remain ignored. Members without a property identity or a valid accessor role also remain outside this collector.

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

## Test Coverage

`test_aot_reachability.c` covers transitive marking, disconnected nodes, first-root reason preservation, invalid graph
bounds, invalid root/edge/unknown reasons, stable manifest order, malformed states, out-of-range predecessors, and
cycles. It also covers getter, setter, and initializer property roots, unresolved callable constants for all three
roles, abstract contract-only accessors, and non-accessor filtering. `test_aot_c_code_stripping.c` proves the emitter
publishes direct-call, export-root, manifest-root, and property-accessor-root chains while omitting trimmed function
nodes; its property negative also verifies partial-file cleanup.

The acceptance record runs the focused reachability and stripping targets on WSL GCC, WSL Clang, and Windows MSVC.
Broader graph-node convergence and behavior/size comparisons remain separate AOT 12 stages.
