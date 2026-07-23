---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
plan_sources:
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/05-property-unified-ast/m3-access-lowering-receiver-effect-implementation-plan.md
tests:
  - tests/parser/test_property_access_lowering.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/parser/test_artifact_schema.c
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

Reference-return properties are deliberately outside this contract. M4 must project a `ref` getter
as a Place/region-aware result instead of treating it as the value-copy path documented here.
