---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/include/zr_vm_core/property_reference.h
  - zr_vm_core/src/zr_vm_core/property_reference.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/reflection_property.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/property_reference.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/reflection_property.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c
plan_sources:
  - user: 2026-08-03 严格按设计完成一次性破坏性切换并重新验收
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/05-property-unified-ast/m3-access-lowering-receiver-effect-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m4-ref-return-place-region-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m5-property-consumers-reflection-migration-implementation-plan.md
tests:
  - tests/parser/test_property_access_lowering.c
  - tests/parser/test_property_ref_return.c
  - tests/container/test_pooling_closed_type_runtime.c
  - tests/acceptance/2026-08-03-syntax-09-m3-canonical-pool-layout.md
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/parser/test_artifact_schema.c
  - tests/parser/test_property_consumer_contracts.c
  - tests/parser/test_property_consumer_stripping_cases.h
  - tests/module/test_module_system.c
doc_type: module-detail
---

# Property Accessor Dispatch

## Runtime contract

Typed property lowering emits the existing `META_GET` and `META_SET` families with a member entry
whose visible identity is the source property. Runtime resolution reads the structured property
descriptor and invokes its getter or setter function. Static accessors accept only a prototype/type
receiver and enter the function without an instance receiver. Instance, virtual, interface, and
inherited accessors keep the receiver chosen by normal descriptor dispatch; runtime does not search
hidden accessor names or infer a contract from property text.

`META_SET` remains an effecting call whose VM result slot may contain the receiver. The compiler
therefore copies the computed assigned value into a distinct expression-result slot. This keeps
compound assignment expression semantics independent of the meta-dispatch representation.

## Inline receiver provenance

An inline struct cannot be represented by copying an object handle without losing writeback. The
dispatcher carries an optional `receiverSourceFrameBase` and `receiverSourceSlot` alongside the
materialized receiver value. Descriptor lookup may use an object-shaped copy, while
`ZrCore_Function_PreCallKnownVmValueWithReceiverSource` binds the callee's receiver directly to the
original frame slot. Mutable setters therefore update the source bytes; readonly getters borrow the
same bytes without creating an owner copy.

Stack anchors refresh the source-frame pointer after scratch reservation or stack growth. The source
contract is used only when the frame layout says `INLINE_STRUCT`; ordinary class handles, native
values, static accessors, and non-inline calls retain the existing path. Cached meta calls preserve
the same frame/slot provenance, so warmup cannot change semantics.

## Failure and cleanup boundary

Getter, RHS, operator, and setter execute in source order. Each runtime helper records the active
call-info before invoking an accessor. A pending exception or an incomplete nested call prevents a
fallback member lookup and propagates failure to the interpreter's normal unwind. This ensures a
throwing getter does not evaluate the RHS, a throwing RHS does not enter the setter, and a throwing
setter runs only after the earlier stages completed.

Receiver, argument, result, and source-frame anchors are restored before returning to the caller.
Owned receiver temporaries follow the existing cleanup registration; the property layer does not
invent a second lifetime or release path.

## Artifact and AOT boundary

Executable artifacts preserve visible member entries, descriptor accessor functions,
`vmEntryClearStackSizePlusOne`, frame layouts, instruction bytes, and call-site kinds. The source and
loaded functions therefore select the same getter/setter identity and stack-clear boundary. C/LLVM
AOT continues to consume the existing typed/meta call contract; M3 does not add a property-name AOT
opcode or side channel.

## Managed property-reference runtime

M4 adds a managed property-reference value instead of representing an interior reference as a naked
native pointer. The value records its base object or frame anchor, exact member descriptor or index,
reference access and referent TypeId. Member, index, inline-frame, ref-struct/view and static sources
therefore retain enough structured provenance to refresh the base after stack growth or GC movement.

The interpreter uses four appended ExecBC operations: create a member reference, create an index
reference, load through a property reference, and store through a writable property reference. Their
numeric ids are appended so existing bytecode ids remain unchanged. Load/store resolve the current
base and descriptor on every use. A readonly reference rejects store; non-addressable receivers and
unsupported native raw pointers never enter this representation.

Cold and quickened property paths share the same managed reference helpers. Cache entries may speed
descriptor lookup, but they do not own reference access, receiver lifetime, or source identity.
Source and loaded executable artifacts preserve the instruction bytes, frame layout, property/accessor
identity, and execution SemIR operation, so cache heat and serialization cannot change the Place.

An ordinary value context consumes a terminal property-reference shell. Variable initialization,
assignment, and non-reference return type checking remove the temporary reference access and
borrow/loan qualifier because lowering emits `PROPERTY_REF_LOAD`; explicit `ref`/`out` arguments
retain the reference and are scored with the same exact referent-type rule used by final Place
validation. This keeps writable-reference overload selection strict without reporting a false
`Expected T but found T` mismatch for an ordinary value read.

When a writable property reference yields an inline struct and a later member is assigned, lowering
loads the struct into a value slot and records the property reference in the existing nested-struct
writeback stack. Writeback runs in reverse order: inner struct fields are stored first, then the
completed struct is stored through the property reference. A chain such as `guard.value.x = 41`
therefore mutates the referenced Place rather than a detached materialization.

Frame-slot property stores also update the registered to-be-closed stack mirror when the resolved
destination is an `out` argument value rather than the mirror itself. Replacing an existing guard
therefore closes the previous mirrored value before publishing the new guard. When `CLOSE_SCOPE`
can invoke native close metadata, the dispatcher saves the next instruction PC first and applies the
normal native-call exception, frame-base, and trap refresh afterward. Nested close execution cannot
resume at the property load/store that consumed the closed reference.

The C and LLVM AOT backends lower the stable property-reference SemIR operations to shared runtime
helpers for create/load/store. Generated code carries the managed value rather than computing a raw
interior address. Direct native/FFI pointer projection remains unavailable until a descriptor provides
an explicit managed or pinned contract.

## Reflection property projection

Core reflection builds one visible property descriptor before exposing its linked accessor
functions. The join key is `propertyIdentity` plus the structured accessor role and owner; source
name and hidden runtime spelling are presentation data only. The descriptor publishes visible
access/static/modifier flags, value TypeId, getter/setter/initializer identities, receiver effects,
reference access/export, decorators, and declaration token provenance. Explicit fields and
properties remain separate reflection entries and never share a SymbolId or layout slot.

`reflection_property.c` owns this projection so the general reflection dispatcher does not grow a
second property parser. Source modules and reloaded `.zro` prototypes use the same join. If the
visible carrier, owner, value TypeId, or accessor link is missing or inconsistent, the property is
unavailable; the runtime does not pair `__get_`/`__set_` names. A legacy-looking ordinary method is
therefore reflected as a method, while compile-time decorators and AOT reflection roots attach to
the exact visible property/accessor identities.

Opt-in AOT stripping also preserves concrete property accessors from those same compiled rows. A
row becomes a `root.property_accessor` only when its property identity and accessor role are valid
and its callable constant resolves to an exact function index. This protects descriptor/reflection
dispatch without turning ordinary unused methods into roots.
