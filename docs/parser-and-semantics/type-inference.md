---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_property_requirements.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_property_contract.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_property.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
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
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_internal.h
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/reflection_property.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_property_requirements.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_property_contract.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_property.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
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
  - zr_vm_core/src/zr_vm_core/reflection_property.c
plan_sources:
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/05-property-unified-ast/m1-unified-ast-symbol-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m2-explicit-field-init-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m3-access-lowering-receiver-effect-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m4-ref-return-place-region-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m5-property-consumers-reflection-migration-implementation-plan.md
tests:
  - tests/parser/test_property_unified_ast.c
  - tests/parser/test_property_explicit_field_init.c
  - tests/parser/test_property_access_lowering.c
  - tests/parser/test_property_ref_return.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_property_consumer_contracts.c
  - tests/parser/test_property_consumer_runtime_bootstrap_cases.h
  - tests/acceptance/2026-08-01-syntax-05-m5-task4-property-import-bootstrap.md
  - tests/language_server/test_lsp_interface.c
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

## Reference-return Property Contracts

M4 accepts a property whose declared value type is `ref T` or `ref readonly T` only when it has one
concrete getter. A reference property cannot declare `set` or `init`, and its getter must explicitly
return a reference with the same access. The property and getter retain separate canonical SymbolIds,
while the getter callable TypeId and the property value TypeId share the same reference node. A
writable result sets `exportsWritableRef`; a readonly result does not.

The binder derives receiver capability from that canonical result. A writable reference getter has a
mutable receiver effect, a readonly reference getter has a readonly receiver effect, and a static
getter has no receiver. Override and interface implementation require invariant reference access.
Calling a writable reference getter through a readonly receiver is rejected before lowering.

Expression lowering invokes the linked getter once and keeps the result as a reference value. Value
context dereferences and loads it; `ref propertyAccess` preserves identity; assignment and compound
assignment dereference and store through `ref T` without selecting a setter. `ref readonly` rejects
the store before any partial mutation. All paths use the visible property and linked accessor
SymbolIds, never the property name, hidden accessor spelling, source text, or display signature.

The pre-execution semantic projection publishes `PROPERTY_REF_GET` with the exact property and
accessor SymbolIds, result TypeId, receiver Place, LoanId/region and source range. `DEREFERENCE`, load,
and store facts consume that reference result. Local or temporary references cannot escape; class,
addressable struct, ref-struct/view, static and ownership-receiver sources retain their structured
source region and capability.

## M4 Boundary

M4 closes parser/compiler/runtime/artifact/AOT behavior for source reference properties. LSP hover,
signature, navigation, code-action projection, final PropertyDef reflection, and legacy-property
migration are M5 work. Those consumers must join the canonical property/accessor SymbolIds and
reference TypeId; they may not reconstruct reference access from a member name or formatted text.

## Canonical Property Consumers

M5 publishes one owned property contract after binding has validated the visible declaration and
its linked accessors. `ZrParser_SemanticQuery_PropertyAt` chooses the narrowest declaration or
reference range, while `ZrParser_SemanticQuery_PropertyBySymbolId` joins the exact visible
PropertySymbol. Both return a zeroed unavailable result when the property identity, canonical value
TypeId, or accessor link is incomplete. Getter, setter, and initializer callable TypeIds remain the
source of receiver and reference-export effects; a display label is never parsed back into a fact.

Imported executable prototypes follow the same rule. The current `.zro` v34 bridge may merge an
exact compiled property row into an otherwise empty imported placeholder, but only when the visible
row and accessor rows already share structured `propertyIdentity`, `accessorRole`, value TypeId, and
reference fields. A missing or conflicting row stays unavailable. Ordinary methods that happen to
use a legacy-looking `__get_` or `__set_` spelling are not promoted to properties.

`ZrParser_TypeInference_RegisterRuntimePrototypes` is the public bootstrap for consumers that already
hold an exact compiled `SZrFunction`. A null compiler or function is an invalid call and fails; a
valid function with no prototype payload is a successful no-op. Registration may fill only an empty
imported type placeholder (or add a previously absent compiled prototype), and property publication
still requires the structured property/accessor identity, canonical type, and reference fields above.
If an accessor identity is removed or conflicts, the property query remains unavailable even when the
compiled accessor has a legacy-looking hidden name.

Interface refactors query transitive required PropertySymbols rather than scanning declaration
names. A missing setter/initializer action is offered only for one unambiguous canonical contract;
an explicit field proxy creates a distinct field SymbolId. Legacy accessor syntax is parsed into a
temporary migration node, emits exact related ranges and at most one machine-applicable complete
property replacement, and is then discarded as a semantic member. Mismatched, non-adjacent,
binary-only, reference, or ambiguous cases deliberately publish no action.

Property facts are declaration contracts, not body snapshots. A body-only incremental edit retains
the same PropertySymbolId and TypeId, whereas a value-type edit changes the affected TypeId without
invalidating an unrelated property's identity. The stress gate binds 128 visible properties and 256
accessors and checks every linked identity; the LSP gate repeats the same invariant across a
64-property document and a contract-changing reparse.
