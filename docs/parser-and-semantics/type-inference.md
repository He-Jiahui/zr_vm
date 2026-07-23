---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_internal.h
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/reflection.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/reflection.c
plan_sources:
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/05-property-unified-ast/m1-unified-ast-symbol-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m2-explicit-field-init-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m3-access-lowering-receiver-effect-implementation-plan.md
tests:
  - tests/parser/test_property_unified_ast.c
  - tests/parser/test_property_explicit_field_init.c
  - tests/parser/test_property_access_lowering.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_semantic_query.c
doc_type: module-detail
---

# Type Inference Contracts

## Canonical Property Identity

M1 binds one visible property member and zero or more callable accessor members. The visible member
uses `ZR_SEMANTIC_SYMBOL_KIND_PROPERTY` and owns:

- `propertySymbolId` and `propertyIdentity`
- canonical `propertyValueTypeId`
- optional getter, setter, and initializer accessor SymbolIds
- source name, access, static/modifier flags, declaration node, and value type

Accessor members retain their callable `TypeId`, role, property SymbolId, and property identity.
Hidden runtime names are dispatch payload only. Semantic lookup first resolves the visible property,
then selects the linked accessor by exact SymbolId/role. Name-derived lookup is limited to importing
legacy/native artifacts that do not yet publish the M1 links.

## Validation And Receiver Effects

Binding rejects duplicate accessor kinds, `set` plus `init`, missing accessors, wider accessor
visibility, bodyless concrete accessors, and interface accessor bodies. Getter callable contracts use
a readonly receiver, setter and initializer contracts use a mutable/initializing boundary, and static
properties have no receiver effect. Setter/initializer expose one readonly compiler-bound `value`
parameter whose type is the property's canonical value type.

Read/write type inference consumes the visible property's structured value type. A read requires the
linked getter; a write requires the linked setter or initializer contract. It does not infer a pair by
matching `__get_` and `__set_` names.

## Serialization And Reflection

The serialized visible member carries `memberType`, source name, property identity, decorator
metadata, and property type name. Core reflection recognizes the shared appended AST constant and
creates the canonical `property` entry before accessor payload entries. Runtime decorators target
that visible property identity. Hidden accessor parsing remains a compatibility reader for older
artifacts, not a source-language semantic path.

## Explicit Field Initialization

An explicit immutable field (`let`, or a compatibility const field) may be initialized by its
declaration or exactly once through a direct `this.field = value` write during object initialization.
Constructor and `init` accessor compilation enter distinct values of one structured initialization
phase; ordinary methods and nested callable bodies run with phase `NONE`. Replacement, compound
assignment, foreign-receiver assignment, and mutation through an inline-struct immutable field are
rejected. Immutability is shallow for object handles, so mutation through an immutable handle remains
valid while replacing the handle does not.

Property writes select an accessor by the visible property's linked identity and exact structured
role. Initialization prefers `INIT`; ordinary execution requires `SET`. Static or foreign receivers
cannot consume the initialization capability. A property `init` body may proxy an explicit mutable
field, but property bodies are not analyzed to infer required-field assignment or field/property
association.

Class and struct member binding first publishes fields, then processes ordinary methods and properties
in source order, and finally compiles meta functions such as constructors. A stable declaration-order
restore makes the emitted member array match source order, including visible-property/accessor groups.
This makes explicit fields available to accessor bodies, preserves preceding-method resolution inside
accessors, and makes all PropertySymbols available to constructors regardless of source order; it does
not infer association between either category or invent forward ordinary-method semantics.

Accessor call inference excludes every member with a non-`NONE` accessor role. Only the property
lowering path can consume getter/setter/init callable identity through its linked PropertySymbol.
Source code cannot call hidden `__get_`/`__set_` payload names directly, so an init body cannot be
re-entered as an ordinary post-construction method.

Runtime initialization provenance is serialized in function member-entry flags. A field store receives
`INITIALIZATION_WRITE` only after compiler exactly-once checks; generic and member-slot dispatch then
permit an otherwise readonly non-static field store only through `ZrCore_Object_InitializeMember`.
An init-property call receives the distinct `PROPERTY_INITIALIZER` flag and resolves the descriptor's
`initializerFunction`; setter-only meta dispatch never treats that flag as a setter. PIC state remains
a performance cache and cannot decide either semantic capability.

The heap-object initializer is an internal VM capability and succeeds only for an exact readonly,
non-static field descriptor that has no existing instance value. Writable fields, properties, static
fields, managed/dynamic members, and repeated initialization are rejected. Inline frame-field layout
projects the compiled field's immutable bit; normal stores reject immutable inline fields, while an
initialization-flagged store additionally requires a constructor bitmap position that is not already
set. Both generic and quickened dispatch inspect provenance before attempting an inline store.

Constructor dataflow requires complete immutable-field initialization before a normal return, but a
throw is a non-publishing exit and enters the existing partial-construction unwind. Init-accessor bodies
run the same path-sensitive repeat-write analysis without requiring them to initialize unrelated fields.
When a constructor writes a visible init-only property, definite-assignment follows the property's
canonical symbol, linked init-accessor symbol, and declaration order to project the accessor's direct
immutable-field write effect. All normal accessor returns are intersected with fallthrough, while throw
paths remain non-publishing. This makes an accessor-only field initialization complete, while a prior
direct write plus the same accessor effect is rejected before runtime. Hidden accessor functions are
excluded from ordinary call and member-reference lookup; only the visible property linkage can select one.

Field metadata and `TypeLayout` continue to enumerate only explicit fields. Property and accessor
rows preserve their own tokens and callable identity, consume no field offset, and consume no
constructor initialization-bitmap position. Source reflection and `.zro` loading preserve that same
separation.

## M2 Boundary

M2 establishes explicit field/init-phase rules but does not claim the full M3 lowering matrix,
compound accessor evaluation ordering, ref-return Place projection, or source/binary LSP parity.
Those stages must reuse these SymbolIds, TypeIds, accessor roles, and field definitions and may not
reconstruct contracts from member text.

## Typed Property Access Lowering

M3 resolves the visible `PropertySymbol` first and selects its linked getter, setter, or initializer
by structured accessor role. Ordinary reads require `GET`, ordinary writes require `SET`, and the
M2 construction phase is the only path allowed to select `INIT`. The emitted member entry preserves
the visible property identity, static mode, and canonical accessor contract; hidden accessor names
remain payload and are never exposed as source-callable members.

Compound property assignment is one lowering plan rather than a source rewrite. It captures the
receiver once, calls the getter once, evaluates the right-hand side once, applies the same typed
operator/conversion used by ordinary compound assignment, and calls the setter once. Getter-only and
setter-only properties are rejected before partial writes. Getter failure prevents RHS evaluation,
RHS failure prevents setter invocation, and setter failure preserves the existing exception unwind.
The assignment result is the computed value, not the receiver returned by the meta-set instruction.

Class and ownership-handle receivers retain one captured value. Inline-struct receivers retain their
frame slot/Place provenance: the VM may materialize an object-shaped view for descriptor resolution,
but accessor invocation receives the original frame base and slot so mutable setters write back to
the same inline storage. Static properties bind no receiver. Getter bodies are compiled with readonly
receiver capability; setter and init bodies carry mutable/initializing capability, and typed metadata
marks both readonly and mutable property accessor receivers as borrowed aliases rather than copied
values.

Virtual, interface, inherited, and static cases consume the descriptor/accessor identity already
attached to the property contract. Runtime cache entries may accelerate that lookup, but cache heat,
member spelling, and hidden accessor text do not decide behavior. The focused hierarchy also freezes
the language's explicit base-constructor rule: a derived constructor calls `super()` when it needs
base-field initialization; inherited accessor dispatch does not synthesize constructor inheritance.

## M3 Boundary

M3 completes ordinary value-property get/set/init and compound lowering for source and executable
artifacts. A getter returning `ref T` or `ref readonly T`, direct Place projection, managed interior
references, and reference-region propagation remain deferred to M4. LSP hover/signature/diagnostic
parity remains M5 and must consume the same canonical property/accessor facts without adding a text
fallback.
