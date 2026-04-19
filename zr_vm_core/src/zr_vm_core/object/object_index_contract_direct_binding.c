//
// Narrow direct-binding lanes for cached index-contract dispatch.
//

#include "object/object_call_internal.h"

#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"

enum {
    ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS =
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
    ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_NO_RESULT_FLAGS =
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL
};

static ZR_FORCE_INLINE TZrBool object_index_contract_value_resides_on_vm_stack(const SZrState *state,
                                                                                const SZrTypeValue *value) {
    TZrStackValuePointer stackValue;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    stackValue = ZR_CAST(TZrStackValuePointer, value);
    return (TZrBool)(stackValue >= state->stackBase.valuePointer && stackValue < state->stackTail.valuePointer);
}

static ZR_FORCE_INLINE void object_index_contract_init_direct_binding_inline_value_context(
        ZrLibCallContext *context,
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *dispatch,
        SZrTypeValue *selfValue,
        SZrTypeValue *argumentValues,
        TZrSize argumentCount) {
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(dispatch != ZR_NULL);

    context->state = state;
    context->moduleDescriptor = (const struct ZrLibModuleDescriptor *)dispatch->moduleDescriptor;
    context->typeDescriptor = (const struct ZrLibTypeDescriptor *)dispatch->typeDescriptor;
    context->functionDescriptor = (const struct ZrLibFunctionDescriptor *)dispatch->functionDescriptor;
    context->methodDescriptor = (const struct ZrLibMethodDescriptor *)dispatch->methodDescriptor;
    context->metaMethodDescriptor = (const struct ZrLibMetaMethodDescriptor *)dispatch->metaMethodDescriptor;
    context->ownerPrototype = dispatch->ownerPrototype;
    context->constructTargetPrototype = context->ownerPrototype;
    context->functionBase = ZR_NULL;
    context->argumentBase = ZR_NULL;
    context->argumentValues = argumentValues;
    context->argumentValuePointers = ZR_NULL;
    context->argumentCount = argumentCount;
    context->selfValue = selfValue;
    context->functionBaseAnchor.offset = 0;
    context->stackBasePointer = state->stackBase.valuePointer;
    context->rawArgumentCount = (TZrSize)dispatch->rawArgumentCount;
    context->stackLayoutUsesReceiver = dispatch->usesReceiver;
    context->stackLayoutAnchored = ZR_FALSE;
}

static ZR_FORCE_INLINE void object_index_contract_init_direct_binding_inline_pointer_value_context(
        ZrLibCallContext *context,
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *dispatch,
        SZrTypeValue *selfValue,
        SZrTypeValue **argumentValuePointers,
        TZrSize argumentCount) {
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(dispatch != ZR_NULL);

    context->state = state;
    context->moduleDescriptor = (const struct ZrLibModuleDescriptor *)dispatch->moduleDescriptor;
    context->typeDescriptor = (const struct ZrLibTypeDescriptor *)dispatch->typeDescriptor;
    context->functionDescriptor = (const struct ZrLibFunctionDescriptor *)dispatch->functionDescriptor;
    context->methodDescriptor = (const struct ZrLibMethodDescriptor *)dispatch->methodDescriptor;
    context->metaMethodDescriptor = (const struct ZrLibMetaMethodDescriptor *)dispatch->metaMethodDescriptor;
    context->ownerPrototype = dispatch->ownerPrototype;
    context->constructTargetPrototype = context->ownerPrototype;
    context->functionBase = ZR_NULL;
    context->argumentBase = ZR_NULL;
    context->argumentValues = ZR_NULL;
    context->argumentValuePointers = argumentValuePointers;
    context->argumentCount = argumentCount;
    context->selfValue = selfValue;
    context->functionBaseAnchor.offset = 0;
    context->stackBasePointer = state->stackBase.valuePointer;
    context->rawArgumentCount = (TZrSize)dispatch->rawArgumentCount;
    context->stackLayoutUsesReceiver = dispatch->usesReceiver;
    context->stackLayoutAnchored = ZR_FALSE;
}

TZrBool ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        SZrTypeValue *result) {
    ZrLibCallContext context;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || directBindingDispatch->callback == ZR_NULL ||
        receiver == ZR_NULL || argument0 == ZR_NULL || result == ZR_NULL || state->debugHookSignal != 0u ||
        directBindingDispatch->rawArgumentCount != 2u || !directBindingDispatch->usesReceiver ||
        (directBindingDispatch->reserved0 & ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS) !=
                ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS ||
        !object_index_contract_value_resides_on_vm_stack(state, receiver) ||
        !object_index_contract_value_resides_on_vm_stack(state, argument0)) {
        return ZR_FALSE;
    }

    if (directBindingDispatch->readonlyInlineGetFastCallback != ZR_NULL) {
        success = directBindingDispatch->readonlyInlineGetFastCallback(state, receiver, argument0, result);
        if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
            ZrCore_Value_ResetAsNullNoProfile(result);
            success = ZR_TRUE;
        }
        return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
    }

    object_index_contract_init_direct_binding_inline_value_context(&context,
                                                                   state,
                                                                   directBindingDispatch,
                                                                   (SZrTypeValue *)receiver,
                                                                   (SZrTypeValue *)argument0,
                                                                   1u);
    success = directBindingDispatch->callback(&context, result);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNullNoProfile(result);
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

TZrBool ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1) {
    ZrLibCallContext context;
    SZrTypeValue *argumentValuePointers[2];
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || directBindingDispatch->callback == ZR_NULL ||
        receiver == ZR_NULL || argument0 == ZR_NULL || argument1 == ZR_NULL || state->debugHookSignal != 0u ||
        directBindingDispatch->rawArgumentCount != 3u || !directBindingDispatch->usesReceiver ||
        (directBindingDispatch->reserved0 & ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_NO_RESULT_FLAGS) !=
                ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_NO_RESULT_FLAGS ||
        !object_index_contract_value_resides_on_vm_stack(state, receiver) ||
        !object_index_contract_value_resides_on_vm_stack(state, argument0) ||
        !object_index_contract_value_resides_on_vm_stack(state, argument1)) {
        return ZR_FALSE;
    }

    if (directBindingDispatch->readonlyInlineSetNoResultFastCallback != ZR_NULL) {
        success = directBindingDispatch->readonlyInlineSetNoResultFastCallback(state, receiver, argument0, argument1);
        if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
            success = ZR_TRUE;
        }
        return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
    }

    if (argument1 == argument0 + 1) {
        object_index_contract_init_direct_binding_inline_value_context(&context,
                                                                       state,
                                                                       directBindingDispatch,
                                                                       (SZrTypeValue *)receiver,
                                                                       (SZrTypeValue *)argument0,
                                                                       2u);
    } else {
        argumentValuePointers[0] = (SZrTypeValue *)argument0;
        argumentValuePointers[1] = (SZrTypeValue *)argument1;
        object_index_contract_init_direct_binding_inline_pointer_value_context(&context,
                                                                               state,
                                                                               directBindingDispatch,
                                                                               (SZrTypeValue *)receiver,
                                                                               argumentValuePointers,
                                                                               2u);
    }
    success = directBindingDispatch->callback(&context, ZR_NULL);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}
