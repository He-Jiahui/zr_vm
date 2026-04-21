//
// Extracted object-call, stack-anchor, and pinning helpers.
//

#include "object/object_call_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_common/zr_contract_conf.h"
#include <string.h>

/*
 * Object-call scratch setup is internal runtime machinery; avoid helper-profile
 * bookkeeping for its repeated stack slot reads, value copies, and null
 * initialization.
 */
#define ZrCore_Stack_GetValue ZrCore_Stack_GetValueNoProfile
#define ZrCore_Value_Copy ZrCore_Value_CopyNoProfile
#define ZrCore_Value_ResetAsNull ZrCore_Value_ResetAsNullNoProfile

typedef struct ZrLibModuleDescriptor ZrLibModuleDescriptor;
typedef struct ZrLibTypeDescriptor ZrLibTypeDescriptor;
typedef struct ZrLibFunctionDescriptor ZrLibFunctionDescriptor;
typedef struct ZrLibMethodDescriptor ZrLibMethodDescriptor;
typedef struct ZrLibMetaMethodDescriptor ZrLibMetaMethodDescriptor;

typedef struct ZrLibParameterDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
} ZrLibParameterDescriptor;

typedef struct ZrLibGenericParameterDescriptor {
    const TZrChar *name;
    const TZrChar *documentation;
    const TZrChar *const *constraintTypeNames;
    TZrSize constraintTypeCount;
} ZrLibGenericParameterDescriptor;

typedef struct ZrLibCallContext {
    SZrState *state;
    const ZrLibModuleDescriptor *moduleDescriptor;
    const ZrLibTypeDescriptor *typeDescriptor;
    const ZrLibFunctionDescriptor *functionDescriptor;
    const ZrLibMethodDescriptor *methodDescriptor;
    const ZrLibMetaMethodDescriptor *metaMethodDescriptor;
    SZrObjectPrototype *ownerPrototype;
    SZrObjectPrototype *constructTargetPrototype;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argumentBase;
    SZrTypeValue *argumentValues;
    SZrTypeValue **argumentValuePointers;
    TZrSize argumentCount;
    SZrTypeValue *selfValue;
    SZrFunctionStackAnchor functionBaseAnchor;
    TZrStackValuePointer stackBasePointer;
    TZrSize rawArgumentCount;
    TZrBool stackLayoutUsesReceiver;
    TZrBool stackLayoutAnchored;
} ZrLibCallContext;

typedef TZrBool (*FZrLibBoundCallback)(ZrLibCallContext *context, SZrTypeValue *result);
typedef TZrBool (*FZrLibMetaMethodReadonlyInlineGetFastCallback)(SZrState *state,
                                                                 const SZrTypeValue *selfValue,
                                                                 const SZrTypeValue *argument0,
                                                                 SZrTypeValue *result);
typedef TZrBool (*FZrLibMetaMethodReadonlyInlineSetNoResultFastCallback)(SZrState *state,
                                                                         const SZrTypeValue *selfValue,
                                                                         const SZrTypeValue *argument0,
                                                                         const SZrTypeValue *argument1);

static ZR_FORCE_INLINE void object_update_direct_binding_hot_flags(SZrObjectKnownNativeDirectDispatch *dispatch);

enum {
    ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT = 1u << 0,
    ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND = 1u << 1,
    ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT = 1u << 2,
    ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN = 1u << 3,
    ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT = 1u << 4,
    ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL = 1u << 5
};

struct ZrLibFunctionDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 contractRole;
    TZrUInt32 dispatchFlags;
};

struct ZrLibMethodDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    TZrBool isStatic;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    TZrUInt32 contractRole;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 dispatchFlags;
};

struct ZrLibMetaMethodDescriptor {
    EZrMetaType metaType;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 dispatchFlags;
    FZrLibMetaMethodReadonlyInlineGetFastCallback readonlyInlineGetFastCallback;
    FZrLibMetaMethodReadonlyInlineSetNoResultFastCallback readonlyInlineSetNoResultFastCallback;
};

static ZR_FORCE_INLINE void object_stack_copy_value_no_profile(struct SZrState *state,
                                                               TZrStackValuePointer destination,
                                                               const SZrTypeValue *source) {
    SZrTypeValue *destinationValue;

    if (destination == ZR_NULL || source == ZR_NULL) {
        return;
    }

    destinationValue = ZrCore_Stack_GetValue(destination);
    if (destinationValue == ZR_NULL) {
        return;
    }

    if (ZR_LIKELY(ZrCore_Value_TryCopyFastNoProfile(state, destinationValue, source))) {
        return;
    }

    ZrCore_Value_CopySlow(state, destinationValue, source);
}

#define ZrCore_Stack_CopyValue object_stack_copy_value_no_profile

static TZrStackValuePointer object_resolve_call_scratch_base(TZrStackValuePointer stackTop,
                                                             const SZrCallInfo *callInfo) {
    TZrStackValuePointer base = stackTop;

    if (callInfo != ZR_NULL && callInfo->functionTop.valuePointer > base) {
        base = callInfo->functionTop.valuePointer;
    }

    return base;
}

static ZR_FORCE_INLINE TZrMemoryOffset object_save_stack_pointer_offset(const SZrState *state,
                                                                        TZrStackValuePointer pointer) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(pointer != ZR_NULL);
    return (TZrMemoryOffset)((TZrBytePtr)pointer - (TZrBytePtr)state->stackBase.valuePointer);
}

static ZR_FORCE_INLINE TZrStackValuePointer object_restore_stack_pointer_offset(SZrState *state,
                                                                                TZrMemoryOffset offset) {
    ZR_ASSERT(state != ZR_NULL);
    return ZR_CAST_STACK_VALUE((TZrBytePtr)state->stackBase.valuePointer + offset);
}

static TZrBool object_pin_value_object(SZrState *state, const SZrTypeValue *value, TZrBool *addedByCaller) {
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

    return ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(state->global, state, object, addedByCaller);
}

static void object_unpin_value_object(SZrGlobalState *global, const SZrTypeValue *value, TZrBool addedByCaller) {
    SZrRawObject *object;

    if (!addedByCaller || global == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return;
    }

    object = ZrCore_Value_GetRawObject(value);
    if (object != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(global, object);
    }
}

typedef struct SZrObjectFastOperandState {
    SZrFunctionStackAnchor anchor;
    TZrBool hasAnchor;
    TZrBool pinAdded;
} SZrObjectFastOperandState;

typedef enum EZrObjectNativeBindingResolvedKind {
    ZR_OBJECT_NATIVE_BINDING_FUNCTION = 0,
    ZR_OBJECT_NATIVE_BINDING_METHOD = 1,
    ZR_OBJECT_NATIVE_BINDING_META_METHOD = 2
} EZrObjectNativeBindingResolvedKind;

static TZrBool object_value_is_struct_instance(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT;
}

static TZrBool object_sync_struct_receiver_value(SZrState *state,
                                                 SZrTypeValue *receiverTarget,
                                                 const SZrTypeValue *stackReceiver) {
    SZrObject *targetObject;
    SZrObject *sourceObject;

    if (state == ZR_NULL || receiverTarget == ZR_NULL || stackReceiver == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!object_value_is_struct_instance(state, receiverTarget) ||
        !object_value_is_struct_instance(state, stackReceiver)) {
        ZrCore_Value_Copy(state, receiverTarget, stackReceiver);
        return state->threadStatus == ZR_THREAD_STATUS_FINE;
    }

    targetObject = ZR_CAST_OBJECT(state, receiverTarget->value.object);
    sourceObject = ZR_CAST_OBJECT(state, stackReceiver->value.object);
    if (targetObject == ZR_NULL || sourceObject == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetObject == sourceObject) {
        return ZR_TRUE;
    }

    targetObject->prototype = sourceObject->prototype;
    targetObject->internalType = sourceObject->internalType;
    if (object_node_map_is_ready(sourceObject)) {
        for (TZrSize bucketIndex = 0; bucketIndex < sourceObject->nodeMap.capacity; bucketIndex++) {
            for (SZrHashKeyValuePair *pair = sourceObject->nodeMap.buckets[bucketIndex]; pair != ZR_NULL; pair = pair->next) {
                ZrCore_Object_SetValue(state, targetObject, &pair->key, &pair->value);
                if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                    return ZR_FALSE;
                }
            }
        }
    }

    receiverTarget->type = stackReceiver->type;
    receiverTarget->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(targetObject);
    receiverTarget->isGarbageCollectable = stackReceiver->isGarbageCollectable;
    receiverTarget->isNative = stackReceiver->isNative;
    return ZR_TRUE;
}

static TZrBool object_make_callable_value(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrUInt32 object_native_binding_descriptor_dispatch_flags(TZrUInt32 bindingKind,
                                                                                 TZrPtr descriptorPointer) {
    switch ((EZrObjectNativeBindingResolvedKind)bindingKind) {
        case ZR_OBJECT_NATIVE_BINDING_FUNCTION: {
            const ZrLibFunctionDescriptor *functionDescriptor = (const ZrLibFunctionDescriptor *)descriptorPointer;
            return functionDescriptor != ZR_NULL ? functionDescriptor->dispatchFlags : 0U;
        }
        case ZR_OBJECT_NATIVE_BINDING_METHOD: {
            const ZrLibMethodDescriptor *methodDescriptor = (const ZrLibMethodDescriptor *)descriptorPointer;
            return methodDescriptor != ZR_NULL ? methodDescriptor->dispatchFlags : 0U;
        }
        case ZR_OBJECT_NATIVE_BINDING_META_METHOD: {
            const ZrLibMetaMethodDescriptor *metaMethodDescriptor =
                    (const ZrLibMetaMethodDescriptor *)descriptorPointer;
            return metaMethodDescriptor != ZR_NULL ? metaMethodDescriptor->dispatchFlags : 0U;
        }
        default:
            return 0U;
    }
}

static ZR_FORCE_INLINE TZrBool object_native_binding_descriptor_fixed_argument_count(
        TZrUInt32 bindingKind,
        TZrPtr descriptorPointer,
        TZrSize *outCount) {
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;

    if (descriptorPointer == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    switch ((EZrObjectNativeBindingResolvedKind)bindingKind) {
        case ZR_OBJECT_NATIVE_BINDING_FUNCTION: {
            const ZrLibFunctionDescriptor *functionDescriptor = (const ZrLibFunctionDescriptor *)descriptorPointer;
            if (functionDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = functionDescriptor->minArgumentCount;
            maxArgumentCount = functionDescriptor->maxArgumentCount;
            break;
        }
        case ZR_OBJECT_NATIVE_BINDING_METHOD: {
            const ZrLibMethodDescriptor *methodDescriptor = (const ZrLibMethodDescriptor *)descriptorPointer;
            if (methodDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = methodDescriptor->minArgumentCount;
            maxArgumentCount = methodDescriptor->maxArgumentCount;
            break;
        }
        case ZR_OBJECT_NATIVE_BINDING_META_METHOD: {
            const ZrLibMetaMethodDescriptor *metaMethodDescriptor =
                    (const ZrLibMetaMethodDescriptor *)descriptorPointer;
            if (metaMethodDescriptor == ZR_NULL) {
                return ZR_FALSE;
            }
            minArgumentCount = metaMethodDescriptor->minArgumentCount;
            maxArgumentCount = metaMethodDescriptor->maxArgumentCount;
            break;
        }
        default:
            return ZR_FALSE;
    }

    if (minArgumentCount != maxArgumentCount) {
        return ZR_FALSE;
    }

    *outCount = (TZrSize)minArgumentCount;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE FZrLibBoundCallback object_native_binding_descriptor_callback(TZrUInt32 bindingKind,
                                                                                     TZrPtr descriptorPointer) {
    switch ((EZrObjectNativeBindingResolvedKind)bindingKind) {
        case ZR_OBJECT_NATIVE_BINDING_FUNCTION: {
            const ZrLibFunctionDescriptor *functionDescriptor = (const ZrLibFunctionDescriptor *)descriptorPointer;
            return functionDescriptor != ZR_NULL ? functionDescriptor->callback : ZR_NULL;
        }
        case ZR_OBJECT_NATIVE_BINDING_METHOD: {
            const ZrLibMethodDescriptor *methodDescriptor = (const ZrLibMethodDescriptor *)descriptorPointer;
            return methodDescriptor != ZR_NULL ? methodDescriptor->callback : ZR_NULL;
        }
        case ZR_OBJECT_NATIVE_BINDING_META_METHOD: {
            const ZrLibMetaMethodDescriptor *metaMethodDescriptor =
                    (const ZrLibMetaMethodDescriptor *)descriptorPointer;
            return metaMethodDescriptor != ZR_NULL ? metaMethodDescriptor->callback : ZR_NULL;
        }
        default:
            return ZR_NULL;
    }
}

static ZR_FORCE_INLINE void object_refresh_direct_binding_callback_dispatch_cache(SZrClosureNative *closure) {
    TZrSize fixedArgumentCount;
    TZrUInt32 dispatchFlags;

    if (closure == ZR_NULL) {
        return;
    }

    memset(&closure->nativeBindingDirectDispatch, 0, sizeof(closure->nativeBindingDirectDispatch));
    if (closure->nativeBindingDescriptor == ZR_NULL || closure->nativeBindingUsesReceiver == 0u) {
        return;
    }

    dispatchFlags =
            object_native_binding_descriptor_dispatch_flags(closure->nativeBindingKind, closure->nativeBindingDescriptor);
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT) == 0u ||
        !object_native_binding_descriptor_fixed_argument_count(closure->nativeBindingKind,
                                                              closure->nativeBindingDescriptor,
                                                              &fixedArgumentCount)) {
        return;
    }

    closure->nativeBindingDirectDispatch.callback =
            object_native_binding_descriptor_callback(closure->nativeBindingKind, closure->nativeBindingDescriptor);
    if (closure->nativeBindingDirectDispatch.callback == ZR_NULL) {
        memset(&closure->nativeBindingDirectDispatch, 0, sizeof(closure->nativeBindingDirectDispatch));
        return;
    }

    closure->nativeBindingDirectDispatch.moduleDescriptor = closure->nativeBindingModuleDescriptor;
    closure->nativeBindingDirectDispatch.typeDescriptor = closure->nativeBindingTypeDescriptor;
    closure->nativeBindingDirectDispatch.ownerPrototype = (SZrObjectPrototype *)closure->nativeBindingOwnerPrototype;
    closure->nativeBindingDirectDispatch.rawArgumentCount = (TZrUInt32)(fixedArgumentCount + 1u);
    closure->nativeBindingDirectDispatch.usesReceiver = ZR_TRUE;
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND);
    }
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT);
    }
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN);
    }
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT);
    }
    if ((dispatchFlags & ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL) != 0u) {
        closure->nativeBindingDirectDispatch.reserved0 =
                (TZrUInt8)(closure->nativeBindingDirectDispatch.reserved0 |
                           ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL);
    }
    switch ((EZrObjectNativeBindingResolvedKind)closure->nativeBindingKind) {
        case ZR_OBJECT_NATIVE_BINDING_FUNCTION:
            closure->nativeBindingDirectDispatch.functionDescriptor = closure->nativeBindingDescriptor;
            break;
        case ZR_OBJECT_NATIVE_BINDING_METHOD:
            closure->nativeBindingDirectDispatch.methodDescriptor = closure->nativeBindingDescriptor;
            break;
        case ZR_OBJECT_NATIVE_BINDING_META_METHOD:
            closure->nativeBindingDirectDispatch.metaMethodDescriptor = closure->nativeBindingDescriptor;
            closure->nativeBindingDirectDispatch.readonlyInlineGetFastCallback =
                    ((const ZrLibMetaMethodDescriptor *)closure->nativeBindingDescriptor)
                            ->readonlyInlineGetFastCallback;
            closure->nativeBindingDirectDispatch.readonlyInlineSetNoResultFastCallback =
                    ((const ZrLibMetaMethodDescriptor *)closure->nativeBindingDescriptor)
                            ->readonlyInlineSetNoResultFastCallback;
            break;
        default:
            memset(&closure->nativeBindingDirectDispatch, 0, sizeof(closure->nativeBindingDirectDispatch));
            break;
    }

    object_update_direct_binding_hot_flags(&closure->nativeBindingDirectDispatch);
}

static ZR_FORCE_INLINE void object_update_direct_binding_hot_flags(SZrObjectKnownNativeDirectDispatch *dispatch) {
    if (dispatch == ZR_NULL) {
        return;
    }

    dispatch->reserved1 = 0u;
    if (dispatch->readonlyInlineGetFastCallback != ZR_NULL &&
        dispatch->usesReceiver &&
        dispatch->rawArgumentCount == 2u &&
        (dispatch->reserved0 &
         (ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT)) ==
                (ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                 ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                 ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
                 ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT)) {
        dispatch->reserved1 = (TZrUInt16)(dispatch->reserved1 |
                                          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_GET_FAST_READY);
    }
    if (dispatch->readonlyInlineSetNoResultFastCallback != ZR_NULL &&
        dispatch->usesReceiver &&
        dispatch->rawArgumentCount == 3u &&
        (dispatch->reserved0 &
         (ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL)) ==
                (ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
                 ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
                 ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
                 ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL)) {
        dispatch->reserved1 = (TZrUInt16)(dispatch->reserved1 |
                                          ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_SET_NO_RESULT_FAST_READY);
    }
}

static ZR_FORCE_INLINE const SZrObjectKnownNativeDirectDispatch *object_try_get_direct_binding_callback_dispatch(
        SZrState *state,
        SZrRawObject *callableObject,
        TZrSize expectedArgumentCount) {
    SZrClosureNative *closure;
    const SZrObjectKnownNativeDirectDispatch *dispatch;

    if (state == ZR_NULL || callableObject == ZR_NULL || callableObject->type != ZR_RAW_OBJECT_TYPE_CLOSURE) {
        return ZR_NULL;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, callableObject);
    if (closure == ZR_NULL) {
        return ZR_NULL;
    }

    dispatch = &closure->nativeBindingDirectDispatch;
    if (dispatch->callback != ZR_NULL &&
        dispatch->usesReceiver &&
        dispatch->rawArgumentCount == (TZrUInt32)(expectedArgumentCount + 1u)) {
        return dispatch;
    }

    object_refresh_direct_binding_callback_dispatch_cache(closure);
    dispatch = &closure->nativeBindingDirectDispatch;
    if (dispatch->callback != ZR_NULL &&
        dispatch->usesReceiver &&
        dispatch->rawArgumentCount == (TZrUInt32)(expectedArgumentCount + 1u)) {
        return dispatch;
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE TZrBool object_try_resolve_direct_binding_callback_dispatch(
        SZrState *state,
        SZrRawObject *callableObject,
        TZrSize expectedArgumentCount,
        SZrObjectKnownNativeDirectDispatch *outDispatch) {
    const SZrObjectKnownNativeDirectDispatch *dispatch;

    if (outDispatch == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outDispatch, 0, sizeof(*outDispatch));
    dispatch = object_try_get_direct_binding_callback_dispatch(state, callableObject, expectedArgumentCount);
    if (dispatch == ZR_NULL) {
        return ZR_FALSE;
    }

    *outDispatch = *dispatch;
    object_update_direct_binding_hot_flags(outDispatch);
    return ZR_TRUE;
}

TZrBool ZrCore_Object_TryResolveKnownNativeDirectDispatch(SZrState *state,
                                                          SZrRawObject *callableObject,
                                                          TZrSize expectedArgumentCount,
                                                          SZrObjectKnownNativeDirectDispatch *outDispatch) {
    return object_try_resolve_direct_binding_callback_dispatch(state,
                                                               callableObject,
                                                               expectedArgumentCount,
                                                               outDispatch);
}

static ZR_FORCE_INLINE TZrBool object_try_resolve_permanent_native_callable_from_function(
        SZrState *state,
        SZrFunction *function,
        SZrRawObject **outCallableObject,
        FZrNativeFunction *outNativeFunction) {
    SZrRawObject *rawCallable;
    SZrClosureNative *nativeClosure;
    FZrNativeFunction nativeFunction;

    if (state == ZR_NULL || function == ZR_NULL || outCallableObject == ZR_NULL || outNativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    rawCallable = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
    if (rawCallable == ZR_NULL || !rawCallable->isNative || !ZrCore_RawObject_IsPermanent(state, rawCallable)) {
        return ZR_FALSE;
    }

    if (rawCallable->type != ZR_RAW_OBJECT_TYPE_CLOSURE) {
        return ZR_FALSE;
    }

    nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, rawCallable);
    nativeFunction = nativeClosure != ZR_NULL ? nativeClosure->nativeFunction : ZR_NULL;
    if (nativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    *outCallableObject = rawCallable;
    *outNativeFunction = nativeFunction;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool object_value_resides_on_vm_stack(const SZrState *state,
                                                                const SZrTypeValue *value) {
    TZrStackValuePointer stackValue;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    stackValue = ZR_CAST(TZrStackValuePointer, value);
    return (TZrBool)(stackValue >= state->stackBase.valuePointer && stackValue < state->stackTail.valuePointer);
}

static ZR_FORCE_INLINE void object_fast_operand_state_init(SZrObjectFastOperandState *operandState) {
    if (operandState == ZR_NULL) {
        return;
    }

    operandState->hasAnchor = ZR_FALSE;
    operandState->pinAdded = ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool object_prepare_fast_operand(SZrState *state,
                                                           const SZrTypeValue *value,
                                                           SZrObjectFastOperandState *operandState) {
    if (operandState != ZR_NULL) {
        object_fast_operand_state_init(operandState);
    }
    if (value == ZR_NULL) {
        return ZR_TRUE;
    }
    if (operandState == ZR_NULL) {
        return !ZrCore_Value_IsGarbageCollectable(value);
    }

    if (object_value_resides_on_vm_stack(state, value)) {
        ZrCore_Function_StackAnchorInit(state, ZR_CAST(TZrStackValuePointer, value), &operandState->anchor);
        operandState->hasAnchor = ZR_TRUE;
        return ZR_TRUE;
    }

    return object_pin_value_object(state, value, &operandState->pinAdded);
}

static ZR_FORCE_INLINE const SZrTypeValue *object_restore_fast_operand(SZrState *state,
                                                                       const SZrTypeValue *fallback,
                                                                       const SZrObjectFastOperandState *operandState) {
    if (operandState == ZR_NULL || !operandState->hasAnchor) {
        return fallback;
    }

    return ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &operandState->anchor));
}

static ZR_FORCE_INLINE void object_cleanup_fast_operand(SZrGlobalState *global,
                                                        const SZrTypeValue *value,
                                                        SZrObjectFastOperandState *operandState) {
    if (operandState == ZR_NULL) {
        return;
    }

    object_unpin_value_object(global, value, operandState->pinAdded);
    operandState->pinAdded = ZR_FALSE;
}

static ZR_FORCE_INLINE TZrBool object_value_requires_cloned_stable_copy(SZrState *state,
                                                                        const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || !value->isGarbageCollectable ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_STRUCT;
}

static ZR_FORCE_INLINE TZrBool object_value_can_shallow_stable_copy(SZrState *state, const SZrTypeValue *value) {
    if (value == ZR_NULL || value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE || value->ownershipControl != ZR_NULL ||
        value->ownershipWeakRef != ZR_NULL) {
        return ZR_FALSE;
    }

    return !object_value_requires_cloned_stable_copy(state, value);
}

static ZR_FORCE_INLINE TZrBool object_prepare_stable_value_raw(SZrState *state,
                                                               SZrTypeValue *destination,
                                                               TZrBool *needsRelease,
                                                               const SZrTypeValue *source) {
    if (state == ZR_NULL || destination == ZR_NULL || needsRelease == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    *needsRelease = ZR_FALSE;
    if (object_value_can_shallow_stable_copy(state, source)) {
        *destination = *source;
        return ZR_TRUE;
    }

    ZrCore_Value_ResetAsNull(destination);
    ZrCore_Value_Copy(state, destination, source);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    *needsRelease = ZR_TRUE;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool object_stable_value_requires_pin(const SZrState *state,
                                                                const SZrTypeValue *source,
                                                                TZrBool needsRelease) {
    if (source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (needsRelease) {
        return ZrCore_Value_IsGarbageCollectable(source);
    }

    return ZrCore_Value_IsGarbageCollectable(source) && !object_value_resides_on_vm_stack(state, source);
}

static ZR_FORCE_INLINE TZrBool object_prepare_inline_pinned_stable_value(SZrState *state,
                                                                         const SZrTypeValue *source,
                                                                         SZrTypeValue *stableValue,
                                                                         TZrBool *needsRelease,
                                                                         TZrBool *pinAdded) {
    if (pinAdded != ZR_NULL) {
        *pinAdded = ZR_FALSE;
    }

    if (!object_prepare_stable_value_raw(state, stableValue, needsRelease, source)) {
        return ZR_FALSE;
    }

    if (!object_stable_value_requires_pin(state, source, needsRelease != ZR_NULL && *needsRelease)) {
        return ZR_TRUE;
    }

    return object_pin_value_object(state, stableValue, pinAdded);
}

static ZR_FORCE_INLINE TZrBool object_can_reuse_readonly_inline_value_context_input(SZrState *state,
                                                                                    const SZrTypeValue *source) {
    if (state == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (object_value_resides_on_vm_stack(state, source)) {
        return ZR_TRUE;
    }

    return object_value_can_shallow_stable_copy(state, source) &&
           !object_stable_value_requires_pin(state, source, ZR_FALSE);
}

static ZR_FORCE_INLINE void object_release_inline_pinned_stable_value(SZrState *state,
                                                                      SZrTypeValue *stableValue,
                                                                      TZrBool *needsRelease,
                                                                      TZrBool pinAdded) {
    if (state == ZR_NULL || stableValue == ZR_NULL) {
        return;
    }

    object_unpin_value_object(state->global, stableValue, pinAdded);
    if (needsRelease != ZR_NULL && *needsRelease) {
        ZrCore_Ownership_ReleaseValue(state, stableValue);
        *needsRelease = ZR_FALSE;
    }
}

static ZR_FORCE_INLINE void object_prepare_fast_scratch_destination(TZrStackValuePointer slot) {
    SZrTypeValue *destination;

    if (slot == ZR_NULL) {
        return;
    }

    destination = ZrCore_Stack_GetValue(slot);
    if (destination == ZR_NULL) {
        return;
    }

    if (ZR_UNLIKELY(destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
                    destination->ownershipControl != ZR_NULL ||
                    destination->ownershipWeakRef != ZR_NULL)) {
        ZrCore_Value_ResetAsNull(destination);
    }
}

static ZR_FORCE_INLINE TZrBool object_can_stage_fast_operand_without_growth(const SZrState *state,
                                                                            const SZrTypeValue *value) {
    return value == ZR_NULL ||
           object_value_resides_on_vm_stack(state, value) ||
           !ZrCore_Value_IsGarbageCollectable(value);
}

static ZR_FORCE_INLINE TZrBool object_can_stage_fast_operands_without_growth(
        const SZrState *state,
        TZrStackValuePointer base,
        TZrSize scratchSlots,
        const SZrTypeValue *receiver,
        const SZrTypeValue *const *arguments,
        TZrSize argumentCount) {
    TZrSize index;

    if (state == ZR_NULL || base == ZR_NULL) {
        return ZR_FALSE;
    }

    if (state->stackTail.valuePointer - base < (TZrMemoryOffset)scratchSlots ||
        !object_can_stage_fast_operand_without_growth(state, receiver)) {
        return ZR_FALSE;
    }

    for (index = 0; index < argumentCount; index++) {
        if (!object_can_stage_fast_operand_without_growth(state, arguments[index])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool object_stage_known_native_fast_scratch(SZrState *state,
                                                                      TZrStackValuePointer base,
                                                                      const SZrTypeValue *callable,
                                                                      const SZrTypeValue *receiver,
                                                                      const SZrTypeValue *const *arguments,
                                                                      TZrSize argumentCount) {
    TZrSize receiverOffset = receiver != ZR_NULL ? 1u : 0u;
    TZrSize index;

    if (state == ZR_NULL || base == ZR_NULL || callable == ZR_NULL) {
        return ZR_FALSE;
    }

    if (receiver != ZR_NULL) {
        object_prepare_fast_scratch_destination(base + 1);
        ZrCore_Stack_CopyValue(state, base + 1, receiver);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }
    }

    for (index = 0; index < argumentCount; index++) {
        object_prepare_fast_scratch_destination(base + 1 + receiverOffset + index);
        ZrCore_Stack_CopyValue(state, base + 1 + receiverOffset + index, arguments[index]);
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }
    }

    object_prepare_fast_scratch_destination(base);
    ZrCore_Stack_CopyValue(state, base, callable);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE TZrBool object_stage_known_native_fast_scratch_one_argument(
        SZrState *state,
        TZrStackValuePointer base,
        const SZrTypeValue *callable,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0) {
    if (state == ZR_NULL || base == ZR_NULL || callable == ZR_NULL || receiver == ZR_NULL || argument0 == ZR_NULL) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 1);
    ZrCore_Stack_CopyValue(state, base + 1, receiver);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 2);
    ZrCore_Stack_CopyValue(state, base + 2, argument0);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base);
    ZrCore_Stack_CopyValue(state, base, callable);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE TZrBool object_stage_known_native_fast_scratch_raw_callable(
        SZrState *state,
        TZrStackValuePointer base,
        SZrRawObject *callableObject) {
    SZrTypeValue *callableValue;

    if (state == ZR_NULL || base == ZR_NULL || callableObject == ZR_NULL) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base);
    callableValue = ZrCore_Stack_GetValueNoProfile(base);
    if (callableValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, callableValue, callableObject);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE TZrBool object_stage_known_native_fast_scratch_one_argument_raw_callable(
        SZrState *state,
        TZrStackValuePointer base,
        SZrRawObject *callableObject,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0) {
    if (state == ZR_NULL || base == ZR_NULL || callableObject == ZR_NULL || receiver == ZR_NULL || argument0 == ZR_NULL) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 1);
    ZrCore_Stack_CopyValue(state, base + 1, receiver);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 2);
    ZrCore_Stack_CopyValue(state, base + 2, argument0);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    return object_stage_known_native_fast_scratch_raw_callable(state, base, callableObject);
}

static ZR_FORCE_INLINE TZrBool object_stage_direct_binding_fast_scratch_one_argument(
        SZrState *state,
        TZrStackValuePointer base,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0) {
    if (state == ZR_NULL || base == ZR_NULL || receiver == ZR_NULL || argument0 == ZR_NULL) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base);
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    object_prepare_fast_scratch_destination(base + 1);
    ZrCore_Stack_CopyValue(state, base + 1, receiver);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 2);
    ZrCore_Stack_CopyValue(state, base + 2, argument0);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE TZrBool object_stage_known_native_fast_scratch_two_arguments(
        SZrState *state,
        TZrStackValuePointer base,
        const SZrTypeValue *callable,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1) {
    if (state == ZR_NULL || base == ZR_NULL || callable == ZR_NULL || receiver == ZR_NULL ||
        argument0 == ZR_NULL || argument1 == ZR_NULL) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 1);
    ZrCore_Stack_CopyValue(state, base + 1, receiver);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 2);
    ZrCore_Stack_CopyValue(state, base + 2, argument0);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 3);
    ZrCore_Stack_CopyValue(state, base + 3, argument1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base);
    ZrCore_Stack_CopyValue(state, base, callable);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE TZrBool object_stage_known_native_fast_scratch_two_arguments_raw_callable(
        SZrState *state,
        TZrStackValuePointer base,
        SZrRawObject *callableObject,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1) {
    if (state == ZR_NULL || base == ZR_NULL || callableObject == ZR_NULL || receiver == ZR_NULL ||
        argument0 == ZR_NULL || argument1 == ZR_NULL) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 1);
    ZrCore_Stack_CopyValue(state, base + 1, receiver);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 2);
    ZrCore_Stack_CopyValue(state, base + 2, argument0);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 3);
    ZrCore_Stack_CopyValue(state, base + 3, argument1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    return object_stage_known_native_fast_scratch_raw_callable(state, base, callableObject);
}

static ZR_FORCE_INLINE TZrBool object_stage_direct_binding_fast_scratch_two_arguments(
        SZrState *state,
        TZrStackValuePointer base,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1) {
    if (state == ZR_NULL || base == ZR_NULL || receiver == ZR_NULL || argument0 == ZR_NULL || argument1 == ZR_NULL) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base);
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    object_prepare_fast_scratch_destination(base + 1);
    ZrCore_Stack_CopyValue(state, base + 1, receiver);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 2);
    ZrCore_Stack_CopyValue(state, base + 2, argument0);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    object_prepare_fast_scratch_destination(base + 3);
    ZrCore_Stack_CopyValue(state, base + 3, argument1);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE void object_init_direct_binding_stack_root_context(
        ZrLibCallContext *context,
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *dispatch,
        TZrStackValuePointer functionBase,
        TZrMemoryOffset functionBaseOffset) {
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(dispatch != ZR_NULL);
    ZR_ASSERT(functionBase != ZR_NULL);
    ZR_ASSERT(dispatch->usesReceiver);

    context->state = state;
    context->moduleDescriptor = (const ZrLibModuleDescriptor *)dispatch->moduleDescriptor;
    context->typeDescriptor = (const ZrLibTypeDescriptor *)dispatch->typeDescriptor;
    context->functionDescriptor = (const ZrLibFunctionDescriptor *)dispatch->functionDescriptor;
    context->methodDescriptor = (const ZrLibMethodDescriptor *)dispatch->methodDescriptor;
    context->metaMethodDescriptor = (const ZrLibMetaMethodDescriptor *)dispatch->metaMethodDescriptor;
    context->ownerPrototype = dispatch->ownerPrototype;
    context->constructTargetPrototype = context->ownerPrototype;
    context->functionBase = functionBase;
    context->argumentValues = ZR_NULL;
    context->argumentValuePointers = ZR_NULL;
    context->rawArgumentCount = dispatch->rawArgumentCount;
    context->stackLayoutUsesReceiver = ZR_TRUE;
    context->functionBaseAnchor.offset = functionBaseOffset;
    context->stackBasePointer = state->stackBase.valuePointer;
    context->stackLayoutAnchored = ZR_TRUE;

    context->selfValue = dispatch->rawArgumentCount > 0 ? ZrCore_Stack_GetValueNoProfile(functionBase + 1) : ZR_NULL;
    context->argumentBase = dispatch->rawArgumentCount > 0 ? functionBase + 2 : functionBase + 1;
    context->argumentCount = dispatch->rawArgumentCount > 0 ? dispatch->rawArgumentCount - 1u : 0u;
}

static ZR_FORCE_INLINE void object_init_direct_binding_inline_value_context(
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
    context->moduleDescriptor = (const ZrLibModuleDescriptor *)dispatch->moduleDescriptor;
    context->typeDescriptor = (const ZrLibTypeDescriptor *)dispatch->typeDescriptor;
    context->functionDescriptor = (const ZrLibFunctionDescriptor *)dispatch->functionDescriptor;
    context->methodDescriptor = (const ZrLibMethodDescriptor *)dispatch->methodDescriptor;
    context->metaMethodDescriptor = (const ZrLibMetaMethodDescriptor *)dispatch->metaMethodDescriptor;
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

static ZR_FORCE_INLINE void object_init_direct_binding_inline_pointer_value_context(
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
    context->moduleDescriptor = (const ZrLibModuleDescriptor *)dispatch->moduleDescriptor;
    context->typeDescriptor = (const ZrLibTypeDescriptor *)dispatch->typeDescriptor;
    context->functionDescriptor = (const ZrLibFunctionDescriptor *)dispatch->functionDescriptor;
    context->methodDescriptor = (const ZrLibMethodDescriptor *)dispatch->methodDescriptor;
    context->metaMethodDescriptor = (const ZrLibMetaMethodDescriptor *)dispatch->metaMethodDescriptor;
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

static ZR_FORCE_INLINE void object_refresh_direct_binding_stack_root_context(ZrLibCallContext *context) {
    TZrStackValuePointer functionBase;

    if (context == ZR_NULL || !context->stackLayoutAnchored || context->state == ZR_NULL ||
        context->stackBasePointer == context->state->stackBase.valuePointer) {
        return;
    }

    functionBase = object_restore_stack_pointer_offset(context->state, context->functionBaseAnchor.offset);
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

static ZR_FORCE_INLINE void object_sync_direct_binding_self_to_stack_slot(
        SZrState *state,
        ZrLibCallContext *context,
        TZrStackValuePointer stackBaseBefore,
        TZrStackValuePointer stackTailBefore) {
    SZrTypeValue *syncedSelf;
    SZrTypeValue *stackSelf;
    TZrMemoryOffset syncedSelfOffset = 0;
    TZrBool syncedSelfUsesOldStackAnchor = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || context->selfValue == ZR_NULL) {
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

    object_refresh_direct_binding_stack_root_context(context);
    if (syncedSelfUsesOldStackAnchor && stackBaseBefore != state->stackBase.valuePointer) {
        syncedSelf = ZrCore_Stack_GetValueNoProfile(object_restore_stack_pointer_offset(state, syncedSelfOffset));
    }

    stackSelf = context->functionBase != ZR_NULL ? ZrCore_Stack_GetValueNoProfile(context->functionBase + 1) : ZR_NULL;
    if (stackSelf != ZR_NULL) {
        if (stackSelf != syncedSelf) {
            ZrCore_Value_Copy(state, stackSelf, syncedSelf);
        }
        context->selfValue = stackSelf;
    }
}

static TZrBool object_call_known_native_fast(SZrState *state,
                                             const SZrTypeValue *callable,
                                             const SZrTypeValue *receiver,
                                             const SZrTypeValue *const *arguments,
                                             TZrSize argumentCount,
                                             const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
                                             SZrTypeValue *result);

static ZR_FORCE_INLINE TZrBool object_complete_known_native_fast_direct_binding_call_no_self_rebind(
        SZrState *state,
        TZrStackValuePointer base,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        TZrMemoryOffset savedStackTopOffset,
        SZrCallInfo *savedCallInfo,
        TZrMemoryOffset originalCallInfoTopOffset,
        TZrBool hasOriginalCallInfoTopAnchor,
        SZrTypeValue *result,
        TZrMemoryOffset resultOffset,
        TZrBool hasResultAnchor) {
    ZrLibCallContext context;
    FZrObjectKnownNativeDirectCallback callback;
    SZrTypeValue callbackResultStorage;
    SZrTypeValue *callbackResult;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || base == ZR_NULL || directBindingDispatch == ZR_NULL ||
        directBindingDispatch->callback == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    object_init_direct_binding_stack_root_context(&context,
                                                  state,
                                                  directBindingDispatch,
                                                  base,
                                                  object_save_stack_pointer_offset(state, base));
    callback = directBindingDispatch->callback;
    callbackResult = hasResultAnchor ? &callbackResultStorage : result;
    ZrCore_Value_ResetAsNull(callbackResult);

    success = callback(&context, callbackResult);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNull(callbackResult);
        success = ZR_TRUE;
    }

    state->stackTop.valuePointer = object_restore_stack_pointer_offset(state, savedStackTopOffset);
    if (savedCallInfo != ZR_NULL && hasOriginalCallInfoTopAnchor) {
        savedCallInfo->functionTop.valuePointer = object_restore_stack_pointer_offset(state, originalCallInfoTopOffset);
    }
    if (hasResultAnchor) {
        result = ZrCore_Stack_GetValueNoProfile(object_restore_stack_pointer_offset(state, resultOffset));
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE && success && callbackResult != result) {
        ZrCore_Value_Copy(state, result, callbackResult);
    }

    state->callInfoList = savedCallInfo;
    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

enum {
    ZR_OBJECT_DIRECT_BINDING_INLINE_NO_SELF_REBIND_FLAGS =
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT,
    ZR_OBJECT_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS =
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT
};

static ZR_FORCE_INLINE TZrBool object_call_direct_binding_fast_one_argument_readonly_inline_value_context(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        SZrTypeValue *result) {
    ZrLibCallContext context;
    TZrBool success = ZR_FALSE;

    object_init_direct_binding_inline_value_context(&context,
                                                    state,
                                                    directBindingDispatch,
                                                    (SZrTypeValue *)receiver,
                                                    (SZrTypeValue *)argument0,
                                                    1u);
    success = directBindingDispatch->callback(&context, result);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNull(result);
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_call_direct_binding_fast_one_argument_inline_value_context(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        SZrTypeValue *result) {
    ZrLibCallContext context;
    SZrTypeValue stableReceiverValue;
    SZrTypeValue stableArguments[1];
    FZrObjectKnownNativeDirectCallback callback;
    TZrBool readonlyInlineValueContext;
    TZrBool resultAlwaysWritten;
    TZrBool success;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || receiver == ZR_NULL || argument0 == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    callback = directBindingDispatch->callback;
    if (callback == ZR_NULL) {
        return ZR_FALSE;
    }

    resultAlwaysWritten =
            (TZrBool)((directBindingDispatch->reserved0 &
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN) != 0u);
    readonlyInlineValueContext =
            (TZrBool)((directBindingDispatch->reserved0 &
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT) != 0u);
    if (readonlyInlineValueContext &&
        object_can_reuse_readonly_inline_value_context_input(state, receiver) &&
        object_can_reuse_readonly_inline_value_context_input(state, argument0)) {
        object_init_direct_binding_inline_value_context(&context,
                                                        state,
                                                        directBindingDispatch,
                                                        (SZrTypeValue *)receiver,
                                                        (SZrTypeValue *)argument0,
                                                        1u);
    } else {
        stableReceiverValue = *receiver;
        stableArguments[0] = *argument0;
        object_init_direct_binding_inline_value_context(&context,
                                                        state,
                                                        directBindingDispatch,
                                                        &stableReceiverValue,
                                                        stableArguments,
                                                        1u);
    }
    if (!resultAlwaysWritten) {
        ZrCore_Value_ResetAsNull(result);
    }
    success = callback(&context, result);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNull(result);
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_call_direct_binding_fast_two_arguments_readonly_inline_value_context(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        SZrTypeValue *result) {
    ZrLibCallContext context;
    TZrBool success;

    object_init_direct_binding_inline_value_context(&context,
                                                    state,
                                                    directBindingDispatch,
                                                    (SZrTypeValue *)receiver,
                                                    (SZrTypeValue *)argument0,
                                                    2u);
    success = directBindingDispatch->callback(&context, result);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNull(result);
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_call_direct_binding_fast_two_arguments_inline_pinned_value_context_no_result(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1) {
    ZrLibCallContext context;
    SZrTypeValue stableReceiverValue;
    SZrTypeValue stableArguments[2];
    SZrTypeValue *argumentValuePointers[2];
    TZrBool receiverNeedsRelease = ZR_FALSE;
    TZrBool argumentNeedsRelease[2] = {ZR_FALSE, ZR_FALSE};
    TZrBool receiverPinAdded = ZR_FALSE;
    TZrBool argumentPinAdded[2] = {ZR_FALSE, ZR_FALSE};
    FZrObjectKnownNativeDirectCallback callback;
    TZrBool readonlyInlineValueContext;
    TZrBool reusedReadonlyInputs = ZR_FALSE;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || receiver == ZR_NULL || argument0 == ZR_NULL ||
        argument1 == ZR_NULL) {
        return ZR_FALSE;
    }

    callback = directBindingDispatch->callback;
    if (callback == ZR_NULL) {
        return ZR_FALSE;
    }

    readonlyInlineValueContext =
            (TZrBool)((directBindingDispatch->reserved0 &
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT) != 0u);
    if (readonlyInlineValueContext &&
        object_can_reuse_readonly_inline_value_context_input(state, receiver) &&
        object_can_reuse_readonly_inline_value_context_input(state, argument0) &&
        object_can_reuse_readonly_inline_value_context_input(state, argument1)) {
        reusedReadonlyInputs = ZR_TRUE;
        if (argument1 == argument0 + 1) {
            object_init_direct_binding_inline_value_context(&context,
                                                            state,
                                                            directBindingDispatch,
                                                            (SZrTypeValue *)receiver,
                                                            (SZrTypeValue *)argument0,
                                                            2u);
        } else {
            argumentValuePointers[0] = (SZrTypeValue *)argument0;
            argumentValuePointers[1] = (SZrTypeValue *)argument1;
            object_init_direct_binding_inline_pointer_value_context(&context,
                                                                    state,
                                                                    directBindingDispatch,
                                                                    (SZrTypeValue *)receiver,
                                                                    argumentValuePointers,
                                                                    2u);
        }
    } else {
        if (!object_prepare_inline_pinned_stable_value(state,
                                                       receiver,
                                                       &stableReceiverValue,
                                                       &receiverNeedsRelease,
                                                       &receiverPinAdded) ||
            !object_prepare_inline_pinned_stable_value(state,
                                                       argument0,
                                                       &stableArguments[0],
                                                       &argumentNeedsRelease[0],
                                                       &argumentPinAdded[0]) ||
            !object_prepare_inline_pinned_stable_value(state,
                                                       argument1,
                                                       &stableArguments[1],
                                                       &argumentNeedsRelease[1],
                                                       &argumentPinAdded[1])) {
            goto cleanup;
        }

        object_init_direct_binding_inline_value_context(&context,
                                                        state,
                                                        directBindingDispatch,
                                                        &stableReceiverValue,
                                                        stableArguments,
                                                        2u);
    }
    success = callback(&context, ZR_NULL);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        success = ZR_TRUE;
    }

cleanup:
    if (reusedReadonlyInputs) {
        return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
    }
    object_release_inline_pinned_stable_value(state,
                                              &stableArguments[1],
                                              &argumentNeedsRelease[1],
                                              argumentPinAdded[1]);
    object_release_inline_pinned_stable_value(state,
                                              &stableArguments[0],
                                              &argumentNeedsRelease[0],
                                              argumentPinAdded[0]);
    object_release_inline_pinned_stable_value(state,
                                              &stableReceiverValue,
                                              &receiverNeedsRelease,
                                              receiverPinAdded);
    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_call_direct_binding_fast_two_arguments_inline_pinned_value_context(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1,
        SZrTypeValue *result) {
    ZrLibCallContext context;
    SZrTypeValue stableReceiverValue;
    SZrTypeValue stableArguments[2];
    SZrTypeValue *argumentValuePointers[2];
    TZrBool receiverNeedsRelease = ZR_FALSE;
    TZrBool argumentNeedsRelease[2] = {ZR_FALSE, ZR_FALSE};
    TZrBool receiverPinAdded = ZR_FALSE;
    TZrBool argumentPinAdded[2] = {ZR_FALSE, ZR_FALSE};
    FZrObjectKnownNativeDirectCallback callback;
    TZrBool readonlyInlineValueContext;
    TZrBool resultAlwaysWritten;
    TZrBool reusedReadonlyInputs = ZR_FALSE;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || receiver == ZR_NULL || argument0 == ZR_NULL ||
        argument1 == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    callback = directBindingDispatch->callback;
    if (callback == ZR_NULL) {
        return ZR_FALSE;
    }

    resultAlwaysWritten =
            (TZrBool)((directBindingDispatch->reserved0 &
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN) != 0u);
    readonlyInlineValueContext =
            (TZrBool)((directBindingDispatch->reserved0 &
                       ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT) != 0u);
    if (readonlyInlineValueContext &&
        object_can_reuse_readonly_inline_value_context_input(state, receiver) &&
        object_can_reuse_readonly_inline_value_context_input(state, argument0) &&
        object_can_reuse_readonly_inline_value_context_input(state, argument1)) {
        reusedReadonlyInputs = ZR_TRUE;
        if (argument1 == argument0 + 1) {
            object_init_direct_binding_inline_value_context(&context,
                                                            state,
                                                            directBindingDispatch,
                                                            (SZrTypeValue *)receiver,
                                                            (SZrTypeValue *)argument0,
                                                            2u);
        } else {
            argumentValuePointers[0] = (SZrTypeValue *)argument0;
            argumentValuePointers[1] = (SZrTypeValue *)argument1;
            object_init_direct_binding_inline_pointer_value_context(&context,
                                                                    state,
                                                                    directBindingDispatch,
                                                                    (SZrTypeValue *)receiver,
                                                                    argumentValuePointers,
                                                                    2u);
        }
    } else {
        if (!object_prepare_inline_pinned_stable_value(state,
                                                       receiver,
                                                       &stableReceiverValue,
                                                       &receiverNeedsRelease,
                                                       &receiverPinAdded) ||
            !object_prepare_inline_pinned_stable_value(state,
                                                       argument0,
                                                       &stableArguments[0],
                                                       &argumentNeedsRelease[0],
                                                       &argumentPinAdded[0]) ||
            !object_prepare_inline_pinned_stable_value(state,
                                                       argument1,
                                                       &stableArguments[1],
                                                       &argumentNeedsRelease[1],
                                                       &argumentPinAdded[1])) {
            goto cleanup;
        }

        object_init_direct_binding_inline_value_context(&context,
                                                        state,
                                                        directBindingDispatch,
                                                        &stableReceiverValue,
                                                        stableArguments,
                                                        2u);
    }
    if (!resultAlwaysWritten) {
        ZrCore_Value_ResetAsNull(result);
    }
    success = callback(&context, result);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNull(result);
        success = ZR_TRUE;
    }

cleanup:
    if (reusedReadonlyInputs) {
        return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
    }
    object_release_inline_pinned_stable_value(state,
                                              &stableArguments[1],
                                              &argumentNeedsRelease[1],
                                              argumentPinAdded[1]);
    object_release_inline_pinned_stable_value(state,
                                              &stableArguments[0],
                                              &argumentNeedsRelease[0],
                                              argumentPinAdded[0]);
    object_release_inline_pinned_stable_value(state,
                                              &stableReceiverValue,
                                              &receiverNeedsRelease,
                                              receiverPinAdded);
    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_complete_known_native_fast_direct_binding_call(
        SZrState *state,
        TZrStackValuePointer base,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        TZrMemoryOffset savedStackTopOffset,
        SZrCallInfo *savedCallInfo,
        TZrMemoryOffset originalCallInfoTopOffset,
        TZrBool hasOriginalCallInfoTopAnchor,
        SZrTypeValue *result,
        TZrMemoryOffset resultOffset,
        TZrBool hasResultAnchor) {
    ZrLibCallContext context;
    FZrObjectKnownNativeDirectCallback callback;
    SZrTypeValue callbackResultStorage;
    SZrTypeValue *callbackResult;
    SZrTypeValue *selfValueBefore;
    TZrStackValuePointer stackBaseBefore;
    TZrStackValuePointer stackTailBefore;
    TZrBool success;

    if (state == ZR_NULL || base == ZR_NULL || directBindingDispatch == ZR_NULL ||
        directBindingDispatch->callback == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((directBindingDispatch->reserved0 & ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND) != 0u) {
        return object_complete_known_native_fast_direct_binding_call_no_self_rebind(state,
                                                                                    base,
                                                                                    directBindingDispatch,
                                                                                    savedStackTopOffset,
                                                                                    savedCallInfo,
                                                                                    originalCallInfoTopOffset,
                                                                                    hasOriginalCallInfoTopAnchor,
                                                                                    result,
                                                                                    resultOffset,
                                                                                    hasResultAnchor);
    }

    object_init_direct_binding_stack_root_context(&context,
                                                  state,
                                                  directBindingDispatch,
                                                  base,
                                                  object_save_stack_pointer_offset(state, base));
    callback = directBindingDispatch->callback;
    callbackResult = hasResultAnchor ? &callbackResultStorage : result;
    ZrCore_Value_ResetAsNull(callbackResult);

    selfValueBefore = context.selfValue;
    stackBaseBefore = state->stackBase.valuePointer;
    stackTailBefore = state->stackTail.valuePointer;
    success = callback(&context, callbackResult);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNull(callbackResult);
        success = ZR_TRUE;
    }

    if (success &&
        (context.selfValue != selfValueBefore || state->stackBase.valuePointer != stackBaseBefore)) {
        object_sync_direct_binding_self_to_stack_slot(state, &context, stackBaseBefore, stackTailBefore);
    }

    state->stackTop.valuePointer = object_restore_stack_pointer_offset(state, savedStackTopOffset);
    if (savedCallInfo != ZR_NULL && hasOriginalCallInfoTopAnchor) {
        savedCallInfo->functionTop.valuePointer = object_restore_stack_pointer_offset(state, originalCallInfoTopOffset);
    }
    if (hasResultAnchor) {
        result = ZrCore_Stack_GetValueNoProfile(object_restore_stack_pointer_offset(state, resultOffset));
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE && success && callbackResult != result) {
        ZrCore_Value_Copy(state, result, callbackResult);
    }

    state->callInfoList = savedCallInfo;
    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

static ZR_FORCE_INLINE TZrBool object_complete_known_native_fast_call(
        SZrState *state,
        TZrStackValuePointer base,
        TZrSize scratchSlots,
        FZrNativeFunction nativeFunction,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        TZrMemoryOffset savedStackTopOffset,
        SZrCallInfo *savedCallInfo,
        TZrMemoryOffset originalCallInfoTopOffset,
        TZrBool hasOriginalCallInfoTopAnchor,
        SZrTypeValue *result,
        TZrMemoryOffset resultOffset,
        TZrBool hasResultAnchor) {
    if (state == ZR_NULL || base == ZR_NULL || nativeFunction == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    state->stackTop.valuePointer = base + scratchSlots;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    if (ZR_LIKELY(state->debugHookSignal == 0u)) {
        if (directBindingDispatch != ZR_NULL && directBindingDispatch->callback != ZR_NULL) {
            return object_complete_known_native_fast_direct_binding_call(
                    state,
                    base,
                    directBindingDispatch,
                    savedStackTopOffset,
                    savedCallInfo,
                    originalCallInfoTopOffset,
                    hasOriginalCallInfoTopAnchor,
                    result,
                    resultOffset,
                    hasResultAnchor);
        }
        ZrCore_Value_ResetAsNull(result);
        return ZrCore_Function_CallPreparedResolvedNativeFunctionSingleResultFastRestore(state,
                                                                                         base,
                                                                                         state->stackTop.valuePointer,
                                                                                         nativeFunction,
                                                                                         savedStackTopOffset,
                                                                                         savedCallInfo,
                                                                                         originalCallInfoTopOffset,
                                                                                         hasOriginalCallInfoTopAnchor,
                                                                                         result,
                                                                                         resultOffset,
                                                                                         hasResultAnchor);
    }

    ZrCore_Value_ResetAsNull(result);
    (void)ZrCore_Function_CallPreparedResolvedNativeFunction(state,
                                                             base,
                                                             state->stackTop.valuePointer,
                                                             nativeFunction,
                                                             1,
                                                             ZR_NULL);
    base = state->stackTop.valuePointer != ZR_NULL ? state->stackTop.valuePointer - 1 : ZR_NULL;
    state->stackTop.valuePointer = object_restore_stack_pointer_offset(state, savedStackTopOffset);
    if (savedCallInfo != ZR_NULL && hasOriginalCallInfoTopAnchor) {
        savedCallInfo->functionTop.valuePointer = object_restore_stack_pointer_offset(state, originalCallInfoTopOffset);
    }
    if (hasResultAnchor) {
        result = ZrCore_Stack_GetValue(object_restore_stack_pointer_offset(state, resultOffset));
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE && base != ZR_NULL) {
        ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(base));
    }

    state->callInfoList = savedCallInfo;
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static ZR_FORCE_INLINE TZrBool object_call_known_native_fast_one_argument(
        SZrState *state,
        SZrRawObject *callableObject,
        FZrNativeFunction nativeFunction,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        SZrTypeValue *result) {
    const SZrTypeValue *arguments[1];
    SZrTypeValue callableValue;
    const SZrObjectKnownNativeDirectDispatch *effectiveDirectBindingDispatch = directBindingDispatch;
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrStackValuePointer base;
    TZrMemoryOffset savedStackTopOffset;
    TZrMemoryOffset resultOffset = 0;
    TZrMemoryOffset originalCallInfoTopOffset = 0;
    TZrBool hasOriginalCallInfoTopAnchor = ZR_FALSE;

    if (state == ZR_NULL || callableObject == ZR_NULL || nativeFunction == ZR_NULL || receiver == ZR_NULL ||
        argument0 == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (effectiveDirectBindingDispatch == ZR_NULL || effectiveDirectBindingDispatch->callback == ZR_NULL) {
        effectiveDirectBindingDispatch = object_try_get_direct_binding_callback_dispatch(state, callableObject, 1u);
    }
    arguments[0] = argument0;
    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    if (state->stackTail.valuePointer - base < 3 ||
        !object_can_stage_fast_operand_without_growth(state, receiver) ||
        !object_can_stage_fast_operand_without_growth(state, argument0)) {
        ZrCore_Value_InitAsRawObject(state, &callableValue, callableObject);
        return object_call_known_native_fast(
                state,
                &callableValue,
                receiver,
                arguments,
                1u,
                effectiveDirectBindingDispatch,
                result);
    }
    if (object_value_resides_on_vm_stack(state, result)) {
        resultOffset = object_save_stack_pointer_offset(state, ZR_CAST(TZrStackValuePointer, result));
        hasResultAnchor = ZR_TRUE;
    }

    savedStackTopOffset = object_save_stack_pointer_offset(state, savedStackTop);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < base + 3) {
        originalCallInfoTopOffset = object_save_stack_pointer_offset(state, savedCallInfo->functionTop.valuePointer);
        hasOriginalCallInfoTopAnchor = ZR_TRUE;
    }

    if (!object_stage_known_native_fast_scratch_one_argument_raw_callable(state,
                                                                          base,
                                                                          callableObject,
                                                                          receiver,
                                                                          argument0)) {
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        return ZR_FALSE;
    }

    return object_complete_known_native_fast_call(state,
                                                  base,
                                                  3u,
                                                  nativeFunction,
                                                  effectiveDirectBindingDispatch,
                                                  savedStackTopOffset,
                                                  savedCallInfo,
                                                  originalCallInfoTopOffset,
                                                  hasOriginalCallInfoTopAnchor,
                                                  result,
                                                  resultOffset,
                                                  hasResultAnchor);
}

TZrBool ZrCore_Object_CallDirectBindingFastOneArgument(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        SZrTypeValue *result) {
    TZrUInt8 dispatchFlags;
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrStackValuePointer base;
    TZrMemoryOffset savedStackTopOffset;
    TZrMemoryOffset resultOffset = 0;
    TZrMemoryOffset originalCallInfoTopOffset = 0;
    TZrBool hasOriginalCallInfoTopAnchor = ZR_FALSE;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || directBindingDispatch->callback == ZR_NULL ||
        directBindingDispatch->rawArgumentCount != 2u || !directBindingDispatch->usesReceiver ||
        receiver == ZR_NULL || argument0 == ZR_NULL || result == ZR_NULL || state->debugHookSignal != 0u) {
        return ZR_FALSE;
    }

    dispatchFlags = directBindingDispatch->reserved0;
    if ((dispatchFlags & ZR_OBJECT_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS) ==
                ZR_OBJECT_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS &&
        object_can_reuse_readonly_inline_value_context_input(state, receiver) &&
        object_can_reuse_readonly_inline_value_context_input(state, argument0)) {
        return object_call_direct_binding_fast_one_argument_readonly_inline_value_context(state,
                                                                                          directBindingDispatch,
                                                                                          receiver,
                                                                                          argument0,
                                                                                          result);
    }

    if ((dispatchFlags & ZR_OBJECT_DIRECT_BINDING_INLINE_NO_SELF_REBIND_FLAGS) ==
        ZR_OBJECT_DIRECT_BINDING_INLINE_NO_SELF_REBIND_FLAGS) {
        return object_call_direct_binding_fast_one_argument_inline_value_context(state,
                                                                                 directBindingDispatch,
                                                                                 receiver,
                                                                                 argument0,
                                                                                 result);
    }

    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    if (state->stackTail.valuePointer - base < 3 ||
        !object_can_stage_fast_operand_without_growth(state, receiver) ||
        !object_can_stage_fast_operand_without_growth(state, argument0)) {
        return ZR_FALSE;
    }

    if (object_value_resides_on_vm_stack(state, result)) {
        resultOffset = object_save_stack_pointer_offset(state, ZR_CAST(TZrStackValuePointer, result));
        hasResultAnchor = ZR_TRUE;
    }

    savedStackTopOffset = object_save_stack_pointer_offset(state, savedStackTop);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < base + 3) {
        originalCallInfoTopOffset = object_save_stack_pointer_offset(state, savedCallInfo->functionTop.valuePointer);
        hasOriginalCallInfoTopAnchor = ZR_TRUE;
    }

    if (!object_stage_direct_binding_fast_scratch_one_argument(state, base, receiver, argument0)) {
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = base + 3;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    return object_complete_known_native_fast_direct_binding_call(state,
                                                                 base,
                                                                 directBindingDispatch,
                                                                 savedStackTopOffset,
                                                                 savedCallInfo,
                                                                 originalCallInfoTopOffset,
                                                                 hasOriginalCallInfoTopAnchor,
                                                                 result,
                                                                 resultOffset,
                                                                 hasResultAnchor);
}

TZrBool ZrCore_Object_CallKnownNativeFastOneArgument(SZrState *state,
                                                     SZrRawObject *callableObject,
                                                     FZrNativeFunction nativeFunction,
                                                     const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
                                                     const SZrTypeValue *receiver,
                                                     const SZrTypeValue *argument0,
                                                     SZrTypeValue *result) {
    return object_call_known_native_fast_one_argument(state,
                                                      callableObject,
                                                      nativeFunction,
                                                      directBindingDispatch,
                                                      receiver,
                                                      argument0,
                                                      result);
}

static ZR_FORCE_INLINE TZrBool object_call_known_native_fast_two_arguments(
        SZrState *state,
        SZrRawObject *callableObject,
        FZrNativeFunction nativeFunction,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1,
        SZrTypeValue *result) {
    const SZrTypeValue *arguments[2];
    SZrTypeValue callableValue;
    const SZrObjectKnownNativeDirectDispatch *effectiveDirectBindingDispatch = directBindingDispatch;
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrStackValuePointer base;
    TZrMemoryOffset savedStackTopOffset;
    TZrMemoryOffset resultOffset = 0;
    TZrMemoryOffset originalCallInfoTopOffset = 0;
    TZrBool hasOriginalCallInfoTopAnchor = ZR_FALSE;

    if (state == ZR_NULL || callableObject == ZR_NULL || nativeFunction == ZR_NULL || receiver == ZR_NULL ||
        argument0 == ZR_NULL || argument1 == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (effectiveDirectBindingDispatch == ZR_NULL || effectiveDirectBindingDispatch->callback == ZR_NULL) {
        effectiveDirectBindingDispatch = object_try_get_direct_binding_callback_dispatch(state, callableObject, 2u);
    }
    arguments[0] = argument0;
    arguments[1] = argument1;
    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    if (state->stackTail.valuePointer - base < 4 ||
        !object_can_stage_fast_operand_without_growth(state, receiver) ||
        !object_can_stage_fast_operand_without_growth(state, argument0) ||
        !object_can_stage_fast_operand_without_growth(state, argument1)) {
        ZrCore_Value_InitAsRawObject(state, &callableValue, callableObject);
        return object_call_known_native_fast(
                state,
                &callableValue,
                receiver,
                arguments,
                2u,
                effectiveDirectBindingDispatch,
                result);
    }
    if (object_value_resides_on_vm_stack(state, result)) {
        resultOffset = object_save_stack_pointer_offset(state, ZR_CAST(TZrStackValuePointer, result));
        hasResultAnchor = ZR_TRUE;
    }

    savedStackTopOffset = object_save_stack_pointer_offset(state, savedStackTop);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < base + 4) {
        originalCallInfoTopOffset = object_save_stack_pointer_offset(state, savedCallInfo->functionTop.valuePointer);
        hasOriginalCallInfoTopAnchor = ZR_TRUE;
    }

    if (!object_stage_known_native_fast_scratch_two_arguments_raw_callable(state,
                                                                           base,
                                                                           callableObject,
                                                                           receiver,
                                                                           argument0,
                                                                           argument1)) {
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        return ZR_FALSE;
    }

    return object_complete_known_native_fast_call(state,
                                                  base,
                                                  4u,
                                                  nativeFunction,
                                                  effectiveDirectBindingDispatch,
                                                  savedStackTopOffset,
                                                  savedCallInfo,
                                                  originalCallInfoTopOffset,
                                                  hasOriginalCallInfoTopAnchor,
                                                  result,
                                                  resultOffset,
                                                  hasResultAnchor);
}

TZrBool ZrCore_Object_CallDirectBindingFastTwoArguments(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1,
        SZrTypeValue *result) {
    TZrUInt8 dispatchFlags;
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrStackValuePointer base;
    TZrMemoryOffset savedStackTopOffset;
    TZrMemoryOffset resultOffset = 0;
    TZrMemoryOffset originalCallInfoTopOffset = 0;
    TZrBool hasOriginalCallInfoTopAnchor = ZR_FALSE;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || directBindingDispatch->callback == ZR_NULL ||
        directBindingDispatch->rawArgumentCount != 3u || !directBindingDispatch->usesReceiver ||
        receiver == ZR_NULL || argument0 == ZR_NULL || argument1 == ZR_NULL || result == ZR_NULL ||
        state->debugHookSignal != 0u) {
        return ZR_FALSE;
    }

    dispatchFlags = directBindingDispatch->reserved0;
    if ((dispatchFlags & ZR_OBJECT_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS) ==
                ZR_OBJECT_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS &&
        argument1 == argument0 + 1 &&
        object_can_reuse_readonly_inline_value_context_input(state, receiver) &&
        object_can_reuse_readonly_inline_value_context_input(state, argument0) &&
        object_can_reuse_readonly_inline_value_context_input(state, argument1)) {
        return object_call_direct_binding_fast_two_arguments_readonly_inline_value_context(state,
                                                                                           directBindingDispatch,
                                                                                           receiver,
                                                                                           argument0,
                                                                                           result);
    }

    if ((dispatchFlags & ZR_OBJECT_DIRECT_BINDING_INLINE_NO_SELF_REBIND_FLAGS) ==
        ZR_OBJECT_DIRECT_BINDING_INLINE_NO_SELF_REBIND_FLAGS) {
        return object_call_direct_binding_fast_two_arguments_inline_pinned_value_context(state,
                                                                                         directBindingDispatch,
                                                                                         receiver,
                                                                                         argument0,
                                                                                         argument1,
                                                                                         result);
    }

    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    if (state->stackTail.valuePointer - base < 4 ||
        !object_can_stage_fast_operand_without_growth(state, receiver) ||
        !object_can_stage_fast_operand_without_growth(state, argument0) ||
        !object_can_stage_fast_operand_without_growth(state, argument1)) {
        return ZR_FALSE;
    }

    if (object_value_resides_on_vm_stack(state, result)) {
        resultOffset = object_save_stack_pointer_offset(state, ZR_CAST(TZrStackValuePointer, result));
        hasResultAnchor = ZR_TRUE;
    }

    savedStackTopOffset = object_save_stack_pointer_offset(state, savedStackTop);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < base + 4) {
        originalCallInfoTopOffset = object_save_stack_pointer_offset(state, savedCallInfo->functionTop.valuePointer);
        hasOriginalCallInfoTopAnchor = ZR_TRUE;
    }

    if (!object_stage_direct_binding_fast_scratch_two_arguments(state, base, receiver, argument0, argument1)) {
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = base + 4;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    return object_complete_known_native_fast_direct_binding_call(state,
                                                                 base,
                                                                 directBindingDispatch,
                                                                 savedStackTopOffset,
                                                                 savedCallInfo,
                                                                 originalCallInfoTopOffset,
                                                                 hasOriginalCallInfoTopAnchor,
                                                                 result,
                                                                 resultOffset,
                                                                 hasResultAnchor);
}

TZrBool ZrCore_Object_CallDirectBindingFastTwoArgumentsNoResult(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1) {
    TZrUInt8 dispatchFlags;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || directBindingDispatch->callback == ZR_NULL ||
        directBindingDispatch->rawArgumentCount != 3u || !directBindingDispatch->usesReceiver ||
        receiver == ZR_NULL || argument0 == ZR_NULL || argument1 == ZR_NULL || state->debugHookSignal != 0u) {
        return ZR_FALSE;
    }

    dispatchFlags = directBindingDispatch->reserved0;
    if ((dispatchFlags & ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL) == 0u) {
        return ZR_FALSE;
    }

    if ((dispatchFlags & ZR_OBJECT_DIRECT_BINDING_INLINE_NO_SELF_REBIND_FLAGS) ==
        ZR_OBJECT_DIRECT_BINDING_INLINE_NO_SELF_REBIND_FLAGS) {
        return object_call_direct_binding_fast_two_arguments_inline_pinned_value_context_no_result(state,
                                                                                                   directBindingDispatch,
                                                                                                   receiver,
                                                                                                   argument0,
                                                                                                   argument1);
    }

    return ZR_FALSE;
}

TZrBool ZrCore_Object_CallKnownNativeFastTwoArguments(SZrState *state,
                                                      SZrRawObject *callableObject,
                                                      FZrNativeFunction nativeFunction,
                                                      const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
                                                      const SZrTypeValue *receiver,
                                                      const SZrTypeValue *argument0,
                                                      const SZrTypeValue *argument1,
                                                      SZrTypeValue *result) {
    return object_call_known_native_fast_two_arguments(state,
                                                       callableObject,
                                                       nativeFunction,
                                                       directBindingDispatch,
                                                       receiver,
                                                       argument0,
                                                       argument1,
                                                       result);
}

static TZrBool object_call_known_native_fast(SZrState *state,
                                             const SZrTypeValue *callable,
                                             const SZrTypeValue *receiver,
                                             const SZrTypeValue *const *arguments,
                                             TZrSize argumentCount,
                                             const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
                                             SZrTypeValue *result) {
    SZrClosureNative *nativeClosure;
    FZrNativeFunction nativeFunction;
    const SZrObjectKnownNativeDirectDispatch *effectiveDirectBindingDispatch = directBindingDispatch;
    SZrFunctionStackAnchor resultAnchor;
    SZrObjectFastOperandState receiverOperand;
    SZrObjectFastOperandState argumentOperands[ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY];
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrSize scratchSlots;
    TZrStackValuePointer base;
    TZrMemoryOffset savedStackTopOffset;
    TZrMemoryOffset resultOffset = 0;
    TZrMemoryOffset originalCallInfoTopOffset = 0;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor originalCallInfoTopAnchor;
    TZrBool hasOriginalCallInfoTopAnchor = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || callable == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (argumentCount > 0 && arguments == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, callable->value.object);
    nativeFunction = nativeClosure != ZR_NULL ? nativeClosure->nativeFunction : ZR_NULL;
    if (nativeFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (effectiveDirectBindingDispatch == ZR_NULL || effectiveDirectBindingDispatch->callback == ZR_NULL) {
        effectiveDirectBindingDispatch =
                object_try_get_direct_binding_callback_dispatch(state, callable->value.object, argumentCount);
    }
    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    scratchSlots = 1 + argumentCount + (receiver != ZR_NULL ? 1 : 0);
    base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    if (object_value_resides_on_vm_stack(state, result)) {
        ZrCore_Function_StackAnchorInit(state, ZR_CAST(TZrStackValuePointer, result), &resultAnchor);
        hasResultAnchor = ZR_TRUE;
        resultOffset = object_save_stack_pointer_offset(state, ZR_CAST(TZrStackValuePointer, result));
    }

    savedStackTopOffset = object_save_stack_pointer_offset(state, savedStackTop);
    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < base + scratchSlots) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &originalCallInfoTopAnchor);
        hasOriginalCallInfoTopAnchor = ZR_TRUE;
        originalCallInfoTopOffset = object_save_stack_pointer_offset(state, savedCallInfo->functionTop.valuePointer);
    }
    if (object_can_stage_fast_operands_without_growth(state, base, scratchSlots, receiver, arguments, argumentCount)) {
        if (!object_stage_known_native_fast_scratch(state, base, callable, receiver, arguments, argumentCount)) {
            state->stackTop.valuePointer = savedStackTop;
            state->callInfoList = savedCallInfo;
            return ZR_FALSE;
        }
    } else {
        object_fast_operand_state_init(&receiverOperand);
        if (!object_prepare_fast_operand(state, receiver, &receiverOperand)) {
            return ZR_FALSE;
        }

        for (index = 0; index < argumentCount; index++) {
            object_fast_operand_state_init(&argumentOperands[index]);
            if (!object_prepare_fast_operand(state, arguments[index], &argumentOperands[index])) {
                while (index > 0) {
                    index--;
                    object_cleanup_fast_operand(state->global, arguments[index], &argumentOperands[index]);
                }
                object_cleanup_fast_operand(state->global, receiver, &receiverOperand);
                return ZR_FALSE;
            }
        }

        ZrCore_Function_StackAnchorInit(state, base, &baseAnchor);

        base = ZrCore_Function_CheckStackAndGc(state, scratchSlots, base);
        if (base == ZR_NULL) {
            for (index = argumentCount; index > 0; index--) {
                object_cleanup_fast_operand(state->global, arguments[index - 1], &argumentOperands[index - 1]);
            }
            object_cleanup_fast_operand(state->global, receiver, &receiverOperand);
            return ZR_FALSE;
        }

        savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
        base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
        if (savedCallInfo != ZR_NULL) {
            base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
        }

        if (receiver != ZR_NULL) {
            object_prepare_fast_scratch_destination(base + 1);
            ZrCore_Stack_CopyValue(state,
                                   base + 1,
                                   object_restore_fast_operand(state, receiver, &receiverOperand));
        }

        for (index = 0; index < argumentCount; index++) {
            object_prepare_fast_scratch_destination(base + 1 + (receiver != ZR_NULL ? 1 : 0) + index);
            ZrCore_Stack_CopyValue(state,
                                   base + 1 + (receiver != ZR_NULL ? 1 : 0) + index,
                                   object_restore_fast_operand(state, arguments[index], &argumentOperands[index]));
        }

        object_prepare_fast_scratch_destination(base);
        ZrCore_Stack_CopyValue(state, base, callable);

        for (index = argumentCount; index > 0; index--) {
            object_cleanup_fast_operand(state->global, arguments[index - 1], &argumentOperands[index - 1]);
        }
        object_cleanup_fast_operand(state->global, receiver, &receiverOperand);

        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            state->stackTop.valuePointer = savedStackTop;
            state->callInfoList = savedCallInfo;
            return ZR_FALSE;
        }
    }

    state->stackTop.valuePointer = base + scratchSlots;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    if (hasResultAnchor) {
        result = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &resultAnchor));
    }
    return object_complete_known_native_fast_call(state,
                                                  base,
                                                  scratchSlots,
                                                  nativeFunction,
                                                  effectiveDirectBindingDispatch,
                                                  savedStackTopOffset,
                                                  savedCallInfo,
                                                  originalCallInfoTopOffset,
                                                  hasOriginalCallInfoTopAnchor,
                                                  result,
                                                  resultOffset,
                                                  hasResultAnchor);
}

static ZR_FORCE_INLINE TZrBool object_can_use_known_native_fast(SZrState *state,
                                                                const SZrTypeValue *callable,
                                                                const SZrTypeValue *receiver,
                                                                const SZrTypeValue *const *arguments,
                                                                TZrSize argumentCount,
                                                                SZrTypeValue *result) {
    SZrRawObject *rawCallable;

    if (state == ZR_NULL ||
        callable == ZR_NULL ||
        result == ZR_NULL ||
        argumentCount > ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY ||
        receiver == ZR_NULL ||
        object_value_is_struct_instance(state, receiver) ||
        !callable->isNative ||
        (callable->type != ZR_VALUE_TYPE_FUNCTION && callable->type != ZR_VALUE_TYPE_CLOSURE) ||
        callable->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (argumentCount > 0 && arguments == ZR_NULL) {
        return ZR_FALSE;
    }

    rawCallable = ZrCore_Value_GetRawObject(callable);
    return rawCallable != ZR_NULL && ZrCore_RawObject_IsPermanent(state, rawCallable);
}

TZrBool ZrCore_Object_CallValue(SZrState *state,
                                const SZrTypeValue *callable,
                                const SZrTypeValue *receiver,
                                const SZrTypeValue *arguments,
                                TZrSize argumentCount,
                                SZrTypeValue *result) {
    SZrTypeValue stableCallable;
    SZrTypeValue stableReceiver;
    SZrTypeValue inlineArguments[ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY];
    SZrTypeValue *stableArguments = ZR_NULL;
    TZrBool inlineArgumentPinAdded[ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY];
    TZrBool *argumentPinAdded = ZR_NULL;
    TZrBool freeStableArguments = ZR_FALSE;
    TZrBool freeArgumentPinAdded = ZR_FALSE;
    TZrBool callablePinAdded = ZR_FALSE;
    TZrBool receiverPinAdded = ZR_FALSE;
    TZrSize stableArgumentsBytes = 0;
    TZrSize argumentPinAddedBytes = 0;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrSize totalArguments;
    TZrSize scratchSlots;
    TZrStackValuePointer base;
    TZrStackValuePointer resultStackSlot;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor originalCallInfoTopAnchor;
    SZrFunctionStackAnchor activeCallInfoTopAnchor;
    SZrFunctionStackAnchor callInfoReturnAnchor;
    SZrFunctionStackAnchor receiverAnchor;
    SZrFunctionStackAnchor resultAnchor;
    TZrBool syncStructReceiver = ZR_FALSE;
    TZrBool hasAnchoredReturnDestination = ZR_FALSE;
    TZrBool hasReceiverAnchor = ZR_FALSE;
    TZrBool hasResultAnchor = ZR_FALSE;
    TZrBool hasCallInfoAnchors = ZR_FALSE;
    TZrBool hasActiveCallInfoTopAnchor = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || callable == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    stableCallable = *callable;
    memset(inlineArgumentPinAdded, 0, sizeof(inlineArgumentPinAdded));
    if (receiver != ZR_NULL) {
        stableReceiver = *receiver;
    }
    if (argumentCount > 0) {
        if (arguments == ZR_NULL) {
            return ZR_FALSE;
        }

        if (argumentCount <= ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY) {
            stableArguments = inlineArguments;
            argumentPinAdded = inlineArgumentPinAdded;
        } else {
            stableArgumentsBytes = argumentCount * sizeof(SZrTypeValue);
            stableArguments = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                              stableArgumentsBytes,
                                                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (stableArguments == ZR_NULL) {
                return ZR_FALSE;
            }
            freeStableArguments = ZR_TRUE;

            argumentPinAddedBytes = argumentCount * sizeof(TZrBool);
            argumentPinAdded = (TZrBool *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                          argumentPinAddedBytes,
                                                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (argumentPinAdded == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArguments,
                                              stableArgumentsBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
                return ZR_FALSE;
            }
            freeArgumentPinAdded = ZR_TRUE;
        }

        memset(argumentPinAdded, 0, argumentCount * sizeof(TZrBool));
        for (index = 0; index < argumentCount; index++) {
            stableArguments[index] = arguments[index];
        }
    }

    if (!object_pin_value_object(state, &stableCallable, &callablePinAdded)) {
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_FALSE;
    }

    if (receiver != ZR_NULL && !object_pin_value_object(state, &stableReceiver, &receiverPinAdded)) {
        object_unpin_value_object(state->global, &stableCallable, callablePinAdded);
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_FALSE;
    }

    for (index = 0; index < argumentCount; index++) {
        if (!object_pin_value_object(state, &stableArguments[index], &argumentPinAdded[index])) {
            while (index > 0) {
                index--;
                object_unpin_value_object(state->global, &stableArguments[index], argumentPinAdded[index]);
            }
            object_unpin_value_object(state->global, receiver != ZR_NULL ? &stableReceiver : ZR_NULL, receiverPinAdded);
            object_unpin_value_object(state->global, &stableCallable, callablePinAdded);
            if (freeArgumentPinAdded) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              argumentPinAdded,
                                              argumentPinAddedBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            if (freeStableArguments) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArguments,
                                              stableArgumentsBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            return ZR_FALSE;
        }
    }

    ZrCore_Value_ResetAsNull(result);
    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    totalArguments = argumentCount + (receiver != ZR_NULL ? 1 : 0);
    scratchSlots = 1 + totalArguments;
    base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    resultStackSlot = ZR_CAST(TZrStackValuePointer, result);
    syncStructReceiver = object_value_is_struct_instance(state, receiver);

    if (resultStackSlot >= state->stackBase.valuePointer && resultStackSlot < state->stackTail.valuePointer) {
        ZrCore_Function_StackAnchorInit(state, resultStackSlot, &resultAnchor);
        hasResultAnchor = ZR_TRUE;
    }

    if (syncStructReceiver) {
        TZrStackValuePointer receiverStackSlot = ZR_CAST(TZrStackValuePointer, receiver);
        if (receiverStackSlot >= state->stackBase.valuePointer && receiverStackSlot < state->stackTail.valuePointer) {
            ZrCore_Function_StackAnchorInit(state, receiverStackSlot, &receiverAnchor);
            hasReceiverAnchor = ZR_TRUE;
        }
    }

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    ZrCore_Function_StackAnchorInit(state, base, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &originalCallInfoTopAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &activeCallInfoTopAnchor);
        hasCallInfoAnchors = ZR_TRUE;
        hasActiveCallInfoTopAnchor = ZR_TRUE;
        hasAnchoredReturnDestination =
            (TZrBool)(savedCallInfo->hasReturnDestination && savedCallInfo->returnDestination != ZR_NULL);
        if (hasAnchoredReturnDestination) {
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->returnDestination, &callInfoReturnAnchor);
        }
    }

    ZrCore_Function_ReserveScratchSlots(state, scratchSlots, base);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &originalCallInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
        base = object_resolve_call_scratch_base(savedStackTop, savedCallInfo);
    }

    state->stackTop.valuePointer = base + scratchSlots;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &activeCallInfoTopAnchor);
        hasActiveCallInfoTopAnchor = ZR_TRUE;
    }
    if (receiver != ZR_NULL) {
        ZrCore_Stack_CopyValue(state, base + 1, &stableReceiver);
        base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
        if (savedCallInfo != ZR_NULL && hasCallInfoAnchors) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state,
                                                                                        hasActiveCallInfoTopAnchor
                                                                                                ? &activeCallInfoTopAnchor
                                                                                                : &originalCallInfoTopAnchor);
            if (hasAnchoredReturnDestination) {
                savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
            }
        }
    }
    for (index = 0; index < argumentCount; index++) {
        ZrCore_Stack_CopyValue(state,
                               base + 1 + (receiver != ZR_NULL ? 1 : 0) + index,
                               &stableArguments[index]);
        base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
        if (savedCallInfo != ZR_NULL && hasCallInfoAnchors) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state,
                                                                                        hasActiveCallInfoTopAnchor
                                                                                                ? &activeCallInfoTopAnchor
                                                                                                : &originalCallInfoTopAnchor);
            if (hasAnchoredReturnDestination) {
                savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
            }
        }
    }
    ZrCore_Stack_CopyValue(state, base, &stableCallable);
    base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    if (savedCallInfo != ZR_NULL && hasCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state,
                                                                                    hasActiveCallInfoTopAnchor
                                                                                            ? &activeCallInfoTopAnchor
                                                                                            : &originalCallInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
    }

    for (index = argumentCount; index > 0; index--) {
        object_unpin_value_object(state->global,
                                  &stableArguments[index - 1],
                                  argumentPinAdded[index - 1]);
    }
    object_unpin_value_object(state->global,
                              receiver != ZR_NULL ? &stableReceiver : ZR_NULL,
                              receiverPinAdded);
    object_unpin_value_object(state->global, &stableCallable, callablePinAdded);

    base = ZrCore_Function_CallWithoutYieldKnownValueAndRestore(state, base, &stableCallable, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL && hasCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &originalCallInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
    }
    if (hasResultAnchor) {
        result = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &resultAnchor));
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        if (syncStructReceiver) {
            SZrTypeValue *stackReceiver = ZrCore_Stack_GetValue(base + 1);
            SZrTypeValue *receiverTarget = ZR_NULL;

            if (hasReceiverAnchor) {
                receiverTarget = ZrCore_Stack_GetValue(ZrCore_Function_StackAnchorRestore(state, &receiverAnchor));
            } else if (receiver != ZR_NULL) {
                receiverTarget = ZR_CAST(SZrTypeValue *, receiver);
            }

            if (receiverTarget != ZR_NULL &&
                stackReceiver != ZR_NULL &&
                !object_sync_struct_receiver_value(state, receiverTarget, stackReceiver) &&
                state->threadStatus == ZR_THREAD_STATUS_FINE) {
                state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
            }
        }

        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            state->stackTop.valuePointer = savedStackTop;
            state->callInfoList = savedCallInfo;
            if (freeStableArguments) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArguments,
                                              stableArgumentsBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            if (freeArgumentPinAdded) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              argumentPinAdded,
                                              argumentPinAddedBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            return ZR_FALSE;
        }

        {
            SZrTypeValue *stackResult = ZrCore_Stack_GetValue(base);
            ZrCore_Value_Copy(state, result, stackResult);
        }
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return ZR_TRUE;
    }

    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;
    if (freeStableArguments) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      stableArguments,
                                      stableArgumentsBytes,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    if (freeArgumentPinAdded) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      argumentPinAdded,
                                      argumentPinAddedBytes,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    return ZR_FALSE;
}

TZrBool ZrCore_Object_CallFunctionWithReceiver(SZrState *state,
                                               SZrFunction *function,
                                               SZrTypeValue *receiver,
                                               const SZrTypeValue *arguments,
                                               TZrSize argumentCount,
                                               SZrTypeValue *result) {
    SZrTypeValue callableValue;
    const SZrTypeValue *argumentPointers[ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY];
    const SZrTypeValue *const *fastArguments = ZR_NULL;
    TZrSize index;

    if (!object_make_callable_value(state, function, &callableValue)) {
        return ZR_FALSE;
    }

    if (argumentCount > 0) {
        if (arguments == ZR_NULL) {
            return ZR_FALSE;
        }

        if (argumentCount == 1u) {
            return ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(state,
                                                                         function,
                                                                         receiver,
                                                                         arguments,
                                                                         result);
        }

        if (argumentCount <= ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY) {
            for (index = 0; index < argumentCount; index++) {
                argumentPointers[index] = &arguments[index];
            }
            fastArguments = argumentPointers;
        }
    }

    /*
     * Prototype/index contracts already hand us a resolved callable. When that
     * callable is a permanent native closure, prefer the narrower known-native
     * bridge, which can consume either stack-rooted operands or pinned stable
     * copies without routing through the generic object-call path.
     */
    if (object_can_use_known_native_fast(state, &callableValue, receiver, fastArguments, argumentCount, result)) {
        return object_call_known_native_fast(state, &callableValue, receiver, fastArguments, argumentCount, ZR_NULL, result);
    }

    return ZrCore_Object_CallValue(state, &callableValue, receiver, arguments, argumentCount, result);
}

TZrBool ZrCore_Object_CallFunctionWithReceiverOneArgumentFast(SZrState *state,
                                                              SZrFunction *function,
                                                              const SZrTypeValue *receiver,
                                                              const SZrTypeValue *argument0,
                                                              SZrTypeValue *result) {
    SZrTypeValue callableValue;
    SZrTypeValue fallbackArguments[1];
    SZrRawObject *nativeCallableObject = ZR_NULL;
    FZrNativeFunction nativeFunction = ZR_NULL;

    if (argument0 == ZR_NULL) {
        return ZR_FALSE;
    }

    if (result != ZR_NULL && receiver != ZR_NULL && !object_value_is_struct_instance(state, receiver) &&
        object_try_resolve_permanent_native_callable_from_function(state,
                                                                   function,
                                                                   &nativeCallableObject,
                                                                   &nativeFunction)) {
        return object_call_known_native_fast_one_argument(
                state,
                nativeCallableObject,
                nativeFunction,
                ZR_NULL,
                receiver,
                argument0,
                result);
    }

    if (!object_make_callable_value(state, function, &callableValue)) {
        return ZR_FALSE;
    }

    fallbackArguments[0] = *argument0;
    return ZrCore_Object_CallValue(state,
                                   &callableValue,
                                   ZR_CAST(SZrTypeValue *, receiver),
                                   fallbackArguments,
                                   1u,
                                   result);
}

TZrBool ZrCore_Object_CallFunctionWithReceiverTwoArguments(SZrState *state,
                                                           SZrFunction *function,
                                                           SZrTypeValue *receiver,
                                                           const SZrTypeValue *argument0,
                                                           const SZrTypeValue *argument1,
                                                           SZrTypeValue *result) {
    return ZrCore_Object_CallFunctionWithReceiverTwoArgumentsFast(state,
                                                                  function,
                                                                  receiver,
                                                                  argument0,
                                                                  argument1,
                                                                  result);
}

TZrBool ZrCore_Object_CallFunctionWithReceiverTwoArgumentsFast(SZrState *state,
                                                               SZrFunction *function,
                                                               const SZrTypeValue *receiver,
                                                               const SZrTypeValue *argument0,
                                                               const SZrTypeValue *argument1,
                                                               SZrTypeValue *result) {
    SZrTypeValue callableValue;
    SZrTypeValue fallbackArguments[2];
    SZrRawObject *nativeCallableObject = ZR_NULL;
    FZrNativeFunction nativeFunction = ZR_NULL;

    if (argument0 == ZR_NULL || argument1 == ZR_NULL) {
        return ZR_FALSE;
    }

    if (result != ZR_NULL && receiver != ZR_NULL && !object_value_is_struct_instance(state, receiver) &&
        object_try_resolve_permanent_native_callable_from_function(state,
                                                                   function,
                                                                   &nativeCallableObject,
                                                                   &nativeFunction)) {
        return object_call_known_native_fast_two_arguments(
                state,
                nativeCallableObject,
                nativeFunction,
                ZR_NULL,
                receiver,
                argument0,
                argument1,
                result);
    }

    if (!object_make_callable_value(state, function, &callableValue)) {
        return ZR_FALSE;
    }

    fallbackArguments[0] = *argument0;
    fallbackArguments[1] = *argument1;
    return ZrCore_Object_CallValue(state,
                                   &callableValue,
                                   ZR_CAST(SZrTypeValue *, receiver),
                                   fallbackArguments,
                                   2,
                                   result);
}
