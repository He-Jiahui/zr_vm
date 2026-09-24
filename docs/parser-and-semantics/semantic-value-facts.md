---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/semantic_value_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_value_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_value_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_value_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
tests:
  - tests/parser/test_semantic_value_facts.c
  - tests/parser/test_ssa_source_value_facts.c
  - tests/parser/test_ssa_builder_fact_identity.c
  - tests/parser/test_ssa_builder_control_edges.c
  - tests/acceptance/ssa-value-facts.md
doc_type: module-detail
---

# Canonical value ownership and nullability

## Purpose and publication boundary

The SemIR-to-ExecIR builder previously retained the canonical type token but
marked every semantic value's ownership and nullability unknown. That loses
the distinction between an owning handle and a scoped reference before owner
state analysis or state-map construction can consume it.

`SZrSemanticIrValue.facts` is a pointer-free snapshot produced from canonical
types. The normal compiler refreshes it at `ValidatePreSemanticIr`, while its
semantic context is still available. The builder consumes the snapshot; it
does not look up a compiler context, decode bytecode, infer ownership from an
opcode, or retain an AST/type-node pointer.

The snapshot carries the type ID from which it was derived. A changed value
type cannot silently reuse the old snapshot: SemIR validation and builder
preflight reject a nonmatching witness. Refresh resolves the new canonical
type. Zero-initialized hand-built SemIR remains a supported unknown-facts
input, but a zero witness cannot accompany asserted ownership or nullability.

## Representation contract

| Canonical top-level category | Semantic ownership | ExecIR ownership |
| --- | --- | --- |
| `Unique<T>` | unique | unique |
| `Shared<T>` | shared | shared |
| atomic shared | atomic shared | shared lifetime, original type token retained |
| `Weak<T>` | weak | unknown; no strong-root assertion |
| `ref T`, `ref readonly T` | borrowed | borrowed |
| Proven scalar GC handle | GC | GC |
| Ordinary primitive value | value | unknown (no separate ExecIR value category) |
| Unresolved or unproven representation | unknown | unknown |

Readonly and nullable wrappers retain the enclosed ownership. An outer
nullable wrapper makes the value nullable; a nullable pointee does not make
its enclosing non-null reference or owner handle nullable. A null primitive
is nullable. Generic or native-pointer representations must not receive
unproven non-null assertions.

Primitive string, buffer, array, function, closure, object and thread handles,
and canonical managed arrays have known GC representations. A nominal value,
tuple, union or inline aggregate may *contain* GC references without being a
scalar GC handle. GC scan metadata alone therefore does not produce `GC`
ownership. Native pointers likewise do not become roots merely because their
payload resembles an address.

Weak remains a distinct semantic fact even though the current ExecIR lifetime
enum lacks a weak-handle protocol. Projecting it to unknown is conservative;
projecting it to shared or GC would incorrectly retain its target. Atomic
shared uses the existing shared lifetime category without claiming that this
projection implements atomic retain/release operations.

## Validation and failure behavior

The resolver checks array shape and canonical lookup identity, traverses the
reachable type graph, and validates its enums and edges. It must handle deep
wrapper chains without recursive stack exhaustion and reject cycles,
unresolved nonzero IDs, malformed nested arrays and invalid enum values.
The invalid type sentinel remains unknown. Opaque nominal definitions are not
recursively expanded as if they were structural wrapper nodes.

Publication is all-or-nothing: scratch facts are prepared before any existing
value fact is replaced. A bad later value cannot leave an earlier value
updated. The function owns the resulting scalar snapshots; destroying the
semantic context does not invalidate them. Input type/CFG/value mutations
still require the normal compiler validation boundary before publication.

Builder preflight validates facts before replacing caller output. Both
external-entry and instruction-defined semantic values receive the facts.
Exception CFG normalization retains the same semantic value table, including
the witness and nullability. Synthetic Place-address/provenance values remain
separate from owned data values and are not relabeled as owners of the data
they address.

## Consumers and limits

Source `own`, `share`, borrow, `degrade` and `wake` results exercise the normal
compiler/validation/builder path. Owner-state analysis can now observe an
initialized unique owner before an explicit drop and a dropped owner after it.
Explicit semantic `DROP` still maps to strict `DROP`, never automatically to
guarded cleanup.

The end-to-end fixtures include a source `if (true) {}` continuation, which
activates the compiler's existing CFG producer. A separate straight-line
fixture verifies fact publication without claiming its legacy synthetic
return edge is lowerable. Pure straight-line fallback CFG lowering remains
an existing gap; the tests do not edit or synthesize the compiler's IR.

This closes a prerequisite in 01.02, not the entire SSA construction or state
map milestones. It does not generate memory/effect token chains, synthesize
scope cleanup, implement weak execution, publish physical interpreter frames,
or enable ownership-sensitive optimizations on an otherwise unverified
function. Full effect and state-map verification remain separate gates.

## Reference rationale and tests

Rust MIR's `BorrowKind` retains shared/mutable borrow identity before lowering;
`alloc/rc.rs` explicitly separates weak handles from strong target ownership.
Lua `src/lgc.c` checks collectable tags before marking, and `testes/gc.lua`
tests weak tables. QuickJS `quickjs.h` separates refcounted tags, ordinary
values and null. These local references support keeping ownership and
representation explicit; they do not define zr's language semantics.

Focused tests cover primitive and structural types, owner/ref/wrapper
combinations, stale snapshots, repeated refresh, array corruption, deep graphs,
cycles and transactional rejection. Builder fixtures cover all ownership
projections, enum boundaries, unchanged output on failure and split INVOKEs.
Actual source fixtures cover ownership/ref lowering, weak/nullable wake and
strict drop consumption. Exact executed matrices and remaining limitations
are recorded in `tests/acceptance/ssa-value-facts.md`.
