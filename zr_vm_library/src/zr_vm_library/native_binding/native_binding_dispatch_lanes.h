//
// Internal native binding dispatch-lane helpers.
//

#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_DISPATCH_LANES_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_DISPATCH_LANES_H

#include "native_binding/native_binding_internal.h"
#include "zr_vm_core/ownership.h"

static ZR_FORCE_INLINE TZrBool native_binding_value_has_detached_gc_ownership_inline(const SZrTypeValue *value) {
    SZrOwnershipControl *control;
    SZrRawObject *object;

    if (value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_FALSE;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    control = value->ownershipControl;
    return control != ZR_NULL && control->object == object && control->isDetachedFromGc;
}

static ZR_FORCE_INLINE TZrBool native_binding_pin_value_object_inline(SZrState *state,
                                                                      const SZrTypeValue *value,
                                                                      TZrBool *addedByCaller) {
    SZrRawObject *object;

    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_TRUE;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object == ZR_NULL) {
        return ZR_TRUE;
    }

    if (native_binding_value_has_detached_gc_ownership_inline(value)) {
        return ZR_TRUE;
    }

    return ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(state->global, state, object, addedByCaller);
}

static ZR_FORCE_INLINE void native_binding_unpin_value_object_inline(SZrGlobalState *global,
                                                                     const SZrTypeValue *value,
                                                                     TZrBool addedByCaller) {
    SZrRawObject *object;

    if (!addedByCaller || global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(global, object);
    }
}

static ZR_FORCE_INLINE TZrBool native_binding_copy_stable_value_inline(SZrState *state,
                                                                       SZrTypeValue *destination,
                                                                       const SZrTypeValue *source) {
    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNullNoProfile(destination);
    ZrCore_Value_CopyNoProfile(state, destination, source);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE TZrBool native_binding_value_requires_cloned_stable_copy_inline(SZrState *state,
                                                                                        const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || !value->isGarbageCollectable ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT;
}

static ZR_FORCE_INLINE TZrBool native_binding_value_can_shallow_stable_copy_inline(SZrState *state,
                                                                                    const SZrTypeValue *value) {
    if (value == ZR_NULL || value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE || value->ownershipControl != ZR_NULL ||
        value->ownershipWeakRef != ZR_NULL) {
        return ZR_FALSE;
    }

    return !native_binding_value_requires_cloned_stable_copy_inline(state, value);
}

static ZR_FORCE_INLINE TZrBool native_binding_prepare_stable_value_raw_inline(SZrState *state,
                                                                              SZrTypeValue *destination,
                                                                              TZrBool *needsRelease,
                                                                              const SZrTypeValue *source) {
    if (state == ZR_NULL || destination == ZR_NULL || needsRelease == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    *needsRelease = ZR_FALSE;
    if (native_binding_value_can_shallow_stable_copy_inline(state, source)) {
        *destination = *source;
        return ZR_TRUE;
    }

    if (!native_binding_copy_stable_value_inline(state, destination, source)) {
        return ZR_FALSE;
    }

    *needsRelease = ZR_TRUE;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE void native_binding_release_stable_value_raw_inline(SZrState *state,
                                                                           SZrTypeValue *value,
                                                                           TZrBool *needsRelease) {
    if (state == ZR_NULL || value == ZR_NULL || needsRelease == ZR_NULL || !*needsRelease) {
        return;
    }

    ZrCore_Ownership_ReleaseValue(state, value);
    *needsRelease = ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool native_binding_prepare_stable_value_inline(SZrState *state,
                                                                          ZrLibStableValueCopy *copy,
                                                                          const SZrTypeValue *source) {
    if (copy == ZR_NULL) {
        return ZR_FALSE;
    }

    return native_binding_prepare_stable_value_raw_inline(state, &copy->value, &copy->needsRelease, source);
}

static ZR_FORCE_INLINE void native_binding_release_stable_value_inline(SZrState *state, ZrLibStableValueCopy *copy) {
    if (copy == ZR_NULL) {
        return;
    }

    native_binding_release_stable_value_raw_inline(state, &copy->value, &copy->needsRelease);
}

static ZR_FORCE_INLINE TZrBool native_binding_pin_stable_value_if_needed_inline(SZrState *state,
                                                                                const SZrTypeValue *value,
                                                                                TZrBool needsPin,
                                                                                TZrBool *addedByCaller) {
    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    /*
     * Shallow stable copies still alias the live VM-stack source slot, so that
     * slot already acts as the GC root. Only cloned/released stable copies need
     * an extra ignore pin during the native callback.
     */
    if (!needsPin) {
        return ZR_TRUE;
    }

    return native_binding_pin_value_object_inline(state, value, addedByCaller);
}

static ZR_FORCE_INLINE FZrLibBoundCallback native_binding_entry_callback_inline(const ZrLibBindingEntry *entry) {
    if (entry == ZR_NULL) {
        return ZR_NULL;
    }

    switch (entry->bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION:
            return entry->descriptor.functionDescriptor != ZR_NULL ? entry->descriptor.functionDescriptor->callback
                                                                   : ZR_NULL;
        case ZR_LIB_RESOLVED_BINDING_METHOD:
            return entry->descriptor.methodDescriptor != ZR_NULL ? entry->descriptor.methodDescriptor->callback
                                                                 : ZR_NULL;
        case ZR_LIB_RESOLVED_BINDING_META_METHOD:
            return entry->descriptor.metaMethodDescriptor != ZR_NULL ? entry->descriptor.metaMethodDescriptor->callback
                                                                     : ZR_NULL;
        default:
            return ZR_NULL;
    }
}

static ZR_FORCE_INLINE TZrUInt32 native_binding_entry_dispatch_flags_inline(const ZrLibBindingEntry *entry) {
    if (entry == ZR_NULL) {
        return 0U;
    }

    switch (entry->bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION:
            return entry->descriptor.functionDescriptor != ZR_NULL ? entry->descriptor.functionDescriptor->dispatchFlags
                                                                   : 0U;
        case ZR_LIB_RESOLVED_BINDING_METHOD:
            return entry->descriptor.methodDescriptor != ZR_NULL ? entry->descriptor.methodDescriptor->dispatchFlags
                                                                 : 0U;
        case ZR_LIB_RESOLVED_BINDING_META_METHOD:
            return entry->descriptor.metaMethodDescriptor != ZR_NULL
                           ? entry->descriptor.metaMethodDescriptor->dispatchFlags
                           : 0U;
        default:
            return 0U;
    }
}

static ZR_FORCE_INLINE TZrBool native_binding_entry_fixed_argument_count_inline(const ZrLibBindingEntry *entry,
                                                                                TZrSize *outCount) {
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;

    if (entry == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (entry->bindingKind) {
        case ZR_LIB_RESOLVED_BINDING_FUNCTION:
            if (entry->descriptor.functionDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = entry->descriptor.functionDescriptor->minArgumentCount;
            maxArgumentCount = entry->descriptor.functionDescriptor->maxArgumentCount;
            break;
        case ZR_LIB_RESOLVED_BINDING_METHOD:
            if (entry->descriptor.methodDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = entry->descriptor.methodDescriptor->minArgumentCount;
            maxArgumentCount = entry->descriptor.methodDescriptor->maxArgumentCount;
            break;
        case ZR_LIB_RESOLVED_BINDING_META_METHOD:
            if (entry->descriptor.metaMethodDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = entry->descriptor.metaMethodDescriptor->minArgumentCount;
            maxArgumentCount = entry->descriptor.metaMethodDescriptor->maxArgumentCount;
            break;
        default:
            return ZR_FALSE;
    }

    if (minArgumentCount != maxArgumentCount) {
        return ZR_FALSE;
    }

    *outCount = (TZrSize)minArgumentCount;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool native_binding_value_can_use_fast_lane_self_inline(const SZrTypeValue *value) {
    return value == ZR_NULL ||
           (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
            value->ownershipControl == ZR_NULL &&
            value->ownershipWeakRef == ZR_NULL);
}

static ZR_FORCE_INLINE TZrBool native_binding_value_can_use_fast_lane_argument_inline(const SZrTypeValue *value) {
    return value != ZR_NULL &&
           !value->isGarbageCollectable &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
           value->ownershipControl == ZR_NULL &&
           value->ownershipWeakRef == ZR_NULL;
}

static ZR_FORCE_INLINE void native_binding_context_enable_stack_layout_anchor_inline(ZrLibCallContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL || context->functionBase == ZR_NULL) {
        return;
    }

    ZrCore_Function_StackAnchorInit(context->state, context->functionBase, &context->functionBaseAnchor);
    context->stackBasePointer = context->state->stackBase.valuePointer;
    context->stackLayoutAnchored = ZR_TRUE;
}

static ZR_FORCE_INLINE void native_binding_context_adopt_stack_layout_anchor_inline(
        ZrLibCallContext *context,
        const SZrFunctionStackAnchor *functionBaseAnchor) {
    if (context == ZR_NULL || context->state == ZR_NULL || context->functionBase == ZR_NULL || functionBaseAnchor == ZR_NULL) {
        return;
    }

    context->functionBaseAnchor = *functionBaseAnchor;
    context->stackBasePointer = context->state->stackBase.valuePointer;
    context->stackLayoutAnchored = ZR_TRUE;
}

static ZR_FORCE_INLINE void native_binding_context_refresh_stack_layout_inline(ZrLibCallContext *context) {
    TZrStackValuePointer functionBase;

    if (context == ZR_NULL || !context->stackLayoutAnchored || context->state == ZR_NULL ||
        context->stackBasePointer == context->state->stackBase.valuePointer) {
        return;
    }

    functionBase = ZrCore_Function_StackAnchorRestore(context->state, &context->functionBaseAnchor);
    if (functionBase == ZR_NULL) {
        return;
    }

    context->functionBase = functionBase;
    context->stackBasePointer = context->state->stackBase.valuePointer;
    context->argumentValues = ZR_NULL;
    context->argumentValuePointers = ZR_NULL;

    if (!context->stackLayoutUsesReceiver) {
        context->argumentBase = functionBase + 1;
        context->argumentCount = context->rawArgumentCount;
        context->selfValue = ZR_NULL;
        return;
    }

    context->selfValue = context->rawArgumentCount > 0 ? ZrCore_Stack_GetValueNoProfile(functionBase + 1) : ZR_NULL;
    context->argumentBase = context->rawArgumentCount > 0 ? functionBase + 2 : functionBase + 1;
    context->argumentCount = context->rawArgumentCount > 0 ? context->rawArgumentCount - 1u : 0u;
}

static ZR_FORCE_INLINE void native_binding_sync_self_to_stack_slot_inline(
        SZrState *state,
        const SZrFunctionStackAnchor *functionBaseAnchor,
        ZrLibCallContext *context,
        TZrStackValuePointer stackBaseBefore,
        TZrStackValuePointer stackTailBefore) {
    SZrTypeValue *syncedSelf;
    TZrStackValuePointer currentFunctionBase;
    SZrTypeValue *stackSelf;
    TZrMemoryOffset syncedSelfOffset = 0;
    TZrBool syncedSelfUsesOldStackAnchor = ZR_FALSE;

    if (state == ZR_NULL || functionBaseAnchor == ZR_NULL || context == ZR_NULL || context->selfValue == ZR_NULL) {
        return;
    }

    syncedSelf = context->selfValue;
    if (stackBaseBefore != ZR_NULL && stackTailBefore != ZR_NULL) {
        TZrStackValuePointer syncedSelfSlot = ZR_CAST(TZrStackValuePointer, syncedSelf);
        if (syncedSelfSlot >= stackBaseBefore && syncedSelfSlot < stackTailBefore) {
            syncedSelfOffset = (TZrMemoryOffset)((TZrBytePtr)syncedSelfSlot - (TZrBytePtr)stackBaseBefore);
            syncedSelfUsesOldStackAnchor = ZR_TRUE;
        }
    }

    native_binding_context_refresh_stack_layout_inline(context);
    if (syncedSelfUsesOldStackAnchor && stackBaseBefore != state->stackBase.valuePointer) {
        syncedSelf = ZrCore_Stack_GetValueNoProfile(ZrCore_Stack_LoadOffsetToPointer(state, syncedSelfOffset));
    }

    currentFunctionBase = context->functionBase;
    if (stackBaseBefore != state->stackBase.valuePointer &&
        (!context->stackLayoutAnchored || context->stackBasePointer != state->stackBase.valuePointer)) {
        currentFunctionBase = ZrCore_Function_StackAnchorRestore(state, functionBaseAnchor);
    }
    stackSelf = currentFunctionBase != ZR_NULL ? ZrCore_Stack_GetValueNoProfile(currentFunctionBase + 1) : ZR_NULL;
    if (stackSelf != ZR_NULL) {
        if (stackSelf != syncedSelf) {
            ZrCore_Value_CopyNoProfile(state, stackSelf, syncedSelf);
        }
        context->selfValue = stackSelf;
    }
}

static ZR_FORCE_INLINE TZrBool native_binding_can_use_fast_lane_inline(const ZrLibBindingEntry *entry,
                                                                       const ZrLibCallContext *context) {
    TZrSize expectedArgumentCount;
    TZrSize index;

    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(context->state != ZR_NULL);

    if (native_binding_entry_callback_inline(entry) == ZR_NULL ||
        !native_binding_entry_fixed_argument_count_inline(entry, &expectedArgumentCount) ||
        context->argumentCount != expectedArgumentCount ||
        context->argumentCount > ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY) {
        return ZR_FALSE;
    }

    if (!native_binding_value_can_use_fast_lane_self_inline(context->selfValue)) {
        return ZR_FALSE;
    }

    for (index = 0; index < context->argumentCount; index++) {
        const SZrTypeValue *argument = ZrCore_Stack_GetValueNoProfile(context->argumentBase + index);
        if (!native_binding_value_can_use_fast_lane_argument_inline(argument)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool native_binding_can_use_stack_root_lane_inline(const ZrLibBindingEntry *entry,
                                                                              const ZrLibCallContext *context) {
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);

    return native_binding_entry_callback_inline(entry) != ZR_NULL &&
           (native_binding_entry_dispatch_flags_inline(entry) & ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT) != 0U &&
           context->functionBase != ZR_NULL;
}

static ZR_FORCE_INLINE TZrBool native_binding_can_use_inline_pinned_lane_inline(const ZrLibBindingEntry *entry,
                                                                                 const ZrLibCallContext *context) {
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);

    return native_binding_entry_callback_inline(entry) != ZR_NULL &&
           context->argumentCount <= ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY;
}

TZrBool native_binding_dispatch_stack_root_lane(SZrState *state,
                                                const ZrLibBindingEntry *entry,
                                                ZrLibCallContext *context,
                                                const SZrFunctionStackAnchor *functionBaseAnchor,
                                                SZrTypeValue *result);
TZrBool native_binding_dispatch_stack_root_callback_lane(SZrState *state,
                                                         FZrLibBoundCallback callback,
                                                         ZrLibCallContext *context,
                                                         const SZrFunctionStackAnchor *functionBaseAnchor,
                                                         SZrTypeValue *result);
TZrBool native_binding_dispatch_fast_lane(SZrState *state,
                                          const ZrLibBindingEntry *entry,
                                          ZrLibCallContext *context,
                                          const SZrFunctionStackAnchor *functionBaseAnchor,
                                          SZrTypeValue *result);
TZrBool native_binding_dispatch_inline_pinned_lane(SZrState *state,
                                                   const ZrLibBindingEntry *entry,
                                                   ZrLibCallContext *context,
                                                   const SZrFunctionStackAnchor *functionBaseAnchor,
                                                   SZrTypeValue *result);

#endif // ZR_VM_LIBRARY_NATIVE_BINDING_DISPATCH_LANES_H
