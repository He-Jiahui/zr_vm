#include "native_binding/native_binding_internal.h"

#include <setjmp.h>

#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/ownership.h"
#include "native_binding/native_binding_dispatch_lanes.h"

/*
 * These helpers are pure runtime plumbing on hot paths; keep benchmark
 * execution off the helper-profile branches for stack slot reads, value
 * copy/reset.
 */
#define ZrCore_Stack_GetValue ZrCore_Stack_GetValueNoProfile
#define ZrCore_Value_Copy ZrCore_Value_CopyNoProfile
#define ZrCore_Value_ResetAsNull ZrCore_Value_ResetAsNullNoProfile
#define native_binding_pin_value_object native_binding_pin_value_object_inline
#define native_binding_unpin_value_object native_binding_unpin_value_object_inline
#define native_binding_copy_stable_value native_binding_copy_stable_value_inline
#define native_binding_prepare_stable_value_raw native_binding_prepare_stable_value_raw_inline
#define native_binding_release_stable_value_raw native_binding_release_stable_value_raw_inline
#define native_binding_prepare_stable_value native_binding_prepare_stable_value_inline
#define native_binding_release_stable_value native_binding_release_stable_value_inline
#define native_binding_can_use_stack_root_lane native_binding_can_use_stack_root_lane_inline
#define native_binding_can_use_fast_lane native_binding_can_use_fast_lane_inline
#define native_binding_can_use_inline_pinned_lane native_binding_can_use_inline_pinned_lane_inline

#if defined(_MSC_VER)
    #define ZR_LIB_THREAD_LOCAL __declspec(thread)
#else
    #define ZR_LIB_THREAD_LOCAL _Thread_local
#endif

TZrBool native_binding_auto_check_arity(const ZrLibCallContext *context) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    if (context->functionDescriptor != ZR_NULL) {
        return ZrLib_CallContext_CheckArity(context,
                                            context->functionDescriptor->minArgumentCount,
                                            context->functionDescriptor->maxArgumentCount);
    }

    if (context->methodDescriptor != ZR_NULL) {
        return ZrLib_CallContext_CheckArity(context,
                                            context->methodDescriptor->minArgumentCount,
                                            context->methodDescriptor->maxArgumentCount);
    }

    if (context->metaMethodDescriptor != ZR_NULL) {
        return ZrLib_CallContext_CheckArity(context,
                                            context->metaMethodDescriptor->minArgumentCount,
                                            context->metaMethodDescriptor->maxArgumentCount);
    }

    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool native_binding_stack_pointer_in_current_range(const SZrState *state,
                                                                             TZrStackValuePointer pointer) {
    return state != ZR_NULL && pointer != ZR_NULL && state->stackBase.valuePointer != ZR_NULL &&
           state->stackTail.valuePointer != ZR_NULL && pointer >= state->stackBase.valuePointer &&
           pointer <= state->stackTail.valuePointer;
}

static ZR_FORCE_INLINE TZrStackValuePointer native_binding_resolve_temp_root_stack_top(
        const SZrState *state,
        const SZrCallInfo *callInfo) {
    TZrStackValuePointer candidate;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    candidate = state->stackTop.valuePointer;
    if (native_binding_stack_pointer_in_current_range(state, candidate)) {
        return candidate;
    }

    candidate = callInfo != ZR_NULL ? callInfo->functionTop.valuePointer : ZR_NULL;
    if (native_binding_stack_pointer_in_current_range(state, candidate)) {
        return candidate;
    }

    return ZR_NULL;
}

static ZR_FORCE_INLINE SZrTypeValue *native_binding_temp_root_try_begin_direct_slot(SZrState *state,
                                                                                     ZrLibTempValueRoot *root) {
    TZrStackValuePointer savedStackTop;
    SZrTypeValue *slotValue;

    if (state == ZR_NULL || root == ZR_NULL) {
        return ZR_NULL;
    }

    savedStackTop = native_binding_resolve_temp_root_stack_top(state, state->callInfoList);
    if (savedStackTop == ZR_NULL || savedStackTop >= state->stackTail.valuePointer) {
        return ZR_NULL;
    }
    state->stackTop.valuePointer = savedStackTop;

    memset(root, 0, sizeof(*root));
    root->state = state;
    root->callInfo = state->callInfoList;
    ZrCore_Function_StackAnchorInit(state, savedStackTop, &root->savedStackTopAnchor);
    root->slotAnchor = root->savedStackTopAnchor;
    root->savedStackTopPointer = savedStackTop;
    root->slotPointer = savedStackTop;
    root->stackBasePointer = state->stackBase.valuePointer;
    root->usesDirectPointers = ZR_TRUE;

    if (root->callInfo != ZR_NULL && root->callInfo->functionTop.valuePointer != ZR_NULL) {
        if (root->callInfo->functionTop.valuePointer == savedStackTop) {
            root->restoreCallInfoTopFromSavedStackTop = ZR_TRUE;
        } else {
            ZrCore_Function_StackAnchorInit(state,
                                            root->callInfo->functionTop.valuePointer,
                                            &root->savedCallInfoTopAnchor);
            root->hasSavedCallInfoTop = ZR_TRUE;
            root->savedCallInfoTopPointer = root->callInfo->functionTop.valuePointer;
        }
    }

    state->stackTop.valuePointer = savedStackTop + 1;
    if (root->callInfo != ZR_NULL &&
        (root->callInfo->functionTop.valuePointer == ZR_NULL ||
         root->callInfo->functionTop.valuePointer < state->stackTop.valuePointer)) {
        root->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    slotValue = ZrCore_Stack_GetValue(savedStackTop);
    ZR_ASSERT(slotValue != ZR_NULL);
    ZrCore_Value_ResetAsNull(slotValue);
    root->active = ZR_TRUE;
    return slotValue;
}

static ZR_FORCE_INLINE TZrBool native_binding_temp_root_try_begin_direct(SZrState *state, ZrLibTempValueRoot *root) {
    return native_binding_temp_root_try_begin_direct_slot(state, root) != ZR_NULL;
}

static ZR_FORCE_INLINE TZrBool native_binding_temp_root_direct_pointers_valid(const ZrLibTempValueRoot *root) {
    return root != ZR_NULL && root->active && root->state != ZR_NULL && root->usesDirectPointers &&
           root->stackBasePointer != ZR_NULL && root->state->stackBase.valuePointer == root->stackBasePointer;
}

static ZR_FORCE_INLINE void native_binding_temp_root_capture_direct_pointers(ZrLibTempValueRoot *root,
                                                                             TZrStackValuePointer savedStackTop,
                                                                             TZrStackValuePointer slot) {
    ZR_ASSERT(root != ZR_NULL);
    ZR_ASSERT(root->state != ZR_NULL);
    ZR_ASSERT(savedStackTop != ZR_NULL);
    ZR_ASSERT(slot != ZR_NULL);

    root->savedStackTopPointer = savedStackTop;
    root->slotPointer = slot;
    root->stackBasePointer = root->state->stackBase.valuePointer;
    if (root->hasSavedCallInfoBase && root->callInfo != ZR_NULL) {
        root->savedCallInfoBasePointer = root->callInfo->functionBase.valuePointer;
    }
    if (root->hasSavedCallInfoTop && root->callInfo != ZR_NULL) {
        root->savedCallInfoTopPointer = root->callInfo->functionTop.valuePointer;
    }
    if (root->hasSavedCallInfoReturn && root->callInfo != ZR_NULL) {
        root->savedCallInfoReturnPointer = root->callInfo->returnDestination;
    }
    root->usesDirectPointers = ZR_TRUE;
}

static ZR_FORCE_INLINE SZrTypeValue *native_binding_temp_root_value_slot(ZrLibTempValueRoot *root) {
    TZrStackValuePointer slot;

    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return ZR_NULL;
    }

    if (native_binding_temp_root_direct_pointers_valid(root)) {
        ZR_ASSERT(root->slotPointer != ZR_NULL);
        return ZrCore_Stack_GetValue(root->slotPointer);
    }

    slot = ZrCore_Function_StackAnchorRestore(root->state, &root->slotAnchor);
    return slot != ZR_NULL ? ZrCore_Stack_GetValue(slot) : ZR_NULL;
}

static ZR_FORCE_INLINE TZrBool native_binding_pin_raw_object(SZrState *state,
                                                             SZrRawObject *object,
                                                             TZrBool *addedByCaller) {
    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }
    if (object == ZR_NULL) {
        return ZR_TRUE;
    }

    return ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(state->global, state, object, addedByCaller);
}

static ZR_FORCE_INLINE void native_binding_unpin_raw_object(SZrGlobalState *global,
                                                            SZrRawObject *object,
                                                            TZrBool addedByCaller) {
    if (!addedByCaller || global == ZR_NULL || object == ZR_NULL) {
        return;
    }

    ZrCore_GarbageCollector_UnignoreObject(global, object);
}

static ZR_FORCE_INLINE SZrObjectPrototype *native_binding_context_resolve_construct_target_prototype(
        ZrLibCallContext *context) {
    SZrObject *selfObject;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    native_binding_context_refresh_stack_layout_inline(context);

    if (context->selfValue != ZR_NULL &&
        (context->selfValue->type == ZR_VALUE_TYPE_OBJECT || context->selfValue->type == ZR_VALUE_TYPE_ARRAY) &&
        context->selfValue->value.object != ZR_NULL) {
        selfObject = ZR_CAST_OBJECT(context->state, context->selfValue->value.object);
        if (selfObject != ZR_NULL && context->ownerPrototype != ZR_NULL &&
            ZrCore_Object_IsInstanceOfPrototype(selfObject, context->ownerPrototype)) {
            return selfObject->prototype;
        }
    }

    return context->constructTargetPrototype;
}

static ZR_FORCE_INLINE void native_binding_dispatch_finish_result(SZrState *state,
                                                                  const SZrFunctionStackAnchor *functionBaseAnchor,
                                                                  const SZrTypeValue *result) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || functionBaseAnchor == ZR_NULL || result == ZR_NULL) {
        return;
    }

    functionBase = ZrCore_Function_StackAnchorRestore(state, functionBaseAnchor);
    closureValue = functionBase != ZR_NULL ? ZrCore_Stack_GetValue(functionBase) : ZR_NULL;
    if (closureValue != ZR_NULL) {
        ZrCore_Value_Copy(state, closureValue, result);
    }
    if (functionBase != ZR_NULL) {
        state->stackTop.valuePointer = functionBase + 1;
    }
}

TZrInt64 native_binding_dispatcher(SZrState *state) {
    ZrLibrary_NativeRegistryState *registry;
    TZrStackValuePointer functionBase;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrFunctionStackAnchor stableBaseAnchor;
    SZrFunctionStackAnchor callInfoTopAnchor;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor callInfoReturnAnchor;
    SZrTypeValue *closureValue;
    SZrClosureNative *closure;
    ZrLibBindingEntry *entry;
    ZrLibBindingEntry cachedEntry;
    const ZrLibBindingEntry *entryView;
    ZrLibCallContext context;
    SZrTypeValue result;
    SZrTypeValue stableSelfCopy;
    SZrTypeValue inlineArgumentCopies[ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY];
    SZrTypeValue *stableArgumentCopies;
    TZrBool inlineArgumentPinAdded[ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY];
    TZrBool *argumentPinAdded;
    TZrSize rawArgumentCount;
    TZrSize stableArgumentCopyBytes;
    TZrSize argumentPinAddedBytes;
    TZrSize stableSlotCount;
    TZrStackValuePointer stableBase;
    TZrSize copiedArgumentCount;
    TZrBool hasSavedCallInfoTop;
    TZrBool hasSavedCallInfoBase;
    TZrBool hasSavedCallInfoReturn;
    TZrBool hasCopiedSelf;
    TZrBool selfPinAdded;
    TZrBool freeStableArgumentCopies;
    TZrBool freeArgumentPinAdded;
    TZrBool success;
    TZrSize index;
    TZrBool enteredStableScratchLayout;
    TZrStackValuePointer stackTopBeforeStableScratchLayout;

    if (state == ZR_NULL || state->global == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    closureValue = ZrCore_Stack_GetValue(functionBase);
    if (closureValue == ZR_NULL) {
        return 0;
    }

    closure = ZR_CAST_NATIVE_CLOSURE(state, closureValue->value.object);
    entryView = ZR_NULL;
    if (native_binding_closure_try_build_cached_entry(closure, &cachedEntry)) {
        entryView = &cachedEntry;
    } else {
        registry = native_registry_get(state->global);
        if (registry == ZR_NULL) {
            return 0;
        }
        entry = native_registry_find_binding(registry, closure);
        if (entry == ZR_NULL) {
            return 0;
        }
        entryView = entry;
    }
    rawArgumentCount = (TZrSize)(state->stackTop.valuePointer - (functionBase + 1));

    memset(&context, 0, sizeof(context));
    context.state = state;
    context.moduleDescriptor = entryView->moduleDescriptor;
    context.typeDescriptor = entryView->typeDescriptor;
    context.ownerPrototype = entryView->ownerPrototype;
    context.constructTargetPrototype = entryView->ownerPrototype;
    context.functionDescriptor = entryView->bindingKind == ZR_LIB_RESOLVED_BINDING_FUNCTION
                                         ? entryView->descriptor.functionDescriptor
                                         : ZR_NULL;
    context.methodDescriptor = entryView->bindingKind == ZR_LIB_RESOLVED_BINDING_METHOD
                                       ? entryView->descriptor.methodDescriptor
                                       : ZR_NULL;
    context.metaMethodDescriptor = entryView->bindingKind == ZR_LIB_RESOLVED_BINDING_META_METHOD
                                           ? entryView->descriptor.metaMethodDescriptor
                                           : ZR_NULL;
    context.functionBase = functionBase;
    native_binding_init_call_context_layout_cached(&context,
                                                   functionBase,
                                                   rawArgumentCount,
                                                   closure != ZR_NULL
                                                           ? (TZrBool)(closure->nativeBindingUsesReceiver != 0u)
                                                           : (context.methodDescriptor != ZR_NULL
                                                                      ? !context.methodDescriptor->isStatic
                                                                      : (context.metaMethodDescriptor != ZR_NULL)));

    ZrLib_Value_SetNull(&result);

    if (!native_binding_auto_check_arity(&context)) {
        return 0;
    }

    if (native_binding_can_use_stack_root_lane(entryView, &context)) {
        success =
                native_binding_dispatch_stack_root_lane(state, entryView, &context, &functionBaseAnchor, &result);
        if (!success) {
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return 0;
            }
            ZrLib_Value_SetNull(&result);
        }

        native_binding_dispatch_finish_result(state, &functionBaseAnchor, &result);
        return 1;
    }

    if (native_binding_can_use_fast_lane(entryView, &context)) {
        success = native_binding_dispatch_fast_lane(state, entryView, &context, &functionBaseAnchor, &result);
        if (!success) {
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return 0;
            }
            ZrLib_Value_SetNull(&result);
        }

        native_binding_dispatch_finish_result(state, &functionBaseAnchor, &result);
        return 1;
    }

    if (native_binding_can_use_inline_pinned_lane(entryView, &context)) {
        success = native_binding_dispatch_inline_pinned_lane(state, entryView, &context, &functionBaseAnchor, &result);
        if (!success) {
            if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
                return 0;
            }
            ZrLib_Value_SetNull(&result);
        }

        native_binding_dispatch_finish_result(state, &functionBaseAnchor, &result);
        return 1;
    }

    stableArgumentCopies = inlineArgumentCopies;
    argumentPinAdded = inlineArgumentPinAdded;
    stableArgumentCopyBytes = 0;
    argumentPinAddedBytes = 0;
    freeStableArgumentCopies = ZR_FALSE;
    freeArgumentPinAdded = ZR_FALSE;
    stableSlotCount = context.argumentCount + (context.selfValue != ZR_NULL ? 1u : 0u);
    hasSavedCallInfoTop = ZR_FALSE;
    hasSavedCallInfoBase = ZR_FALSE;
    hasSavedCallInfoReturn = ZR_FALSE;
    stableBase = native_binding_resolve_call_scratch_base(state->stackTop.valuePointer, state->callInfoList);
    copiedArgumentCount = 0;
    hasCopiedSelf = ZR_FALSE;
    selfPinAdded = ZR_FALSE;
    enteredStableScratchLayout = ZR_FALSE;
    stackTopBeforeStableScratchLayout = ZR_NULL;

    if (context.selfValue != ZR_NULL) {
        if (!native_binding_copy_stable_value(state, &stableSelfCopy, context.selfValue)) {
            if (freeStableArgumentCopies) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArgumentCopies,
                                              stableArgumentCopyBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            if (freeArgumentPinAdded) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              argumentPinAdded,
                                              argumentPinAddedBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            return 0;
        }
        hasCopiedSelf = ZR_TRUE;
    }
    if (context.argumentCount > 0) {
        if (context.argumentCount > ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY) {
            stableArgumentCopyBytes = context.argumentCount * sizeof(SZrTypeValue);
            stableArgumentCopies = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                                   stableArgumentCopyBytes,
                                                                                   ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (stableArgumentCopies == ZR_NULL) {
                return 0;
            }
            freeStableArgumentCopies = ZR_TRUE;
        }
        if (context.argumentCount > ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY) {
            argumentPinAddedBytes = context.argumentCount * sizeof(TZrBool);
            argumentPinAdded = (TZrBool *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                          argumentPinAddedBytes,
                                                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (argumentPinAdded == ZR_NULL) {
                if (freeStableArgumentCopies) {
                    ZrCore_Memory_RawFreeWithType(state->global,
                                                  stableArgumentCopies,
                                                  stableArgumentCopyBytes,
                                                  ZR_MEMORY_NATIVE_TYPE_OBJECT);
                }
                return 0;
            }
            freeArgumentPinAdded = ZR_TRUE;
        }
        memset(argumentPinAdded, 0, context.argumentCount * sizeof(TZrBool));

        for (index = 0; index < context.argumentCount; index++) {
            if (!native_binding_copy_stable_value(state,
                                                  &stableArgumentCopies[index],
                                                  ZrCore_Stack_GetValue(context.argumentBase + index))) {
                for (index = copiedArgumentCount; index > 0; index--) {
                    ZrCore_Ownership_ReleaseValue(state, &stableArgumentCopies[index - 1]);
                }
                if (hasCopiedSelf) {
                    ZrCore_Ownership_ReleaseValue(state, &stableSelfCopy);
                }
                if (freeStableArgumentCopies) {
                    ZrCore_Memory_RawFreeWithType(state->global,
                                                  stableArgumentCopies,
                                                  stableArgumentCopyBytes,
                                                  ZR_MEMORY_NATIVE_TYPE_OBJECT);
                }
                if (freeArgumentPinAdded) {
                    ZrCore_Memory_RawFreeWithType(state->global,
                                                  argumentPinAdded,
                                                  argumentPinAddedBytes,
                                                  ZR_MEMORY_NATIVE_TYPE_OBJECT);
                }
                return 0;
            }
            copiedArgumentCount++;
        }
    }

    if (!native_binding_pin_value_object(state, context.selfValue != ZR_NULL ? &stableSelfCopy : ZR_NULL, &selfPinAdded)) {
        for (index = copiedArgumentCount; index > 0; index--) {
            ZrCore_Ownership_ReleaseValue(state, &stableArgumentCopies[index - 1]);
        }
        if (hasCopiedSelf) {
            ZrCore_Ownership_ReleaseValue(state, &stableSelfCopy);
        }
        if (freeStableArgumentCopies) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArgumentCopies,
                                          stableArgumentCopyBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        if (freeArgumentPinAdded) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          argumentPinAdded,
                                          argumentPinAddedBytes,
                                          ZR_MEMORY_NATIVE_TYPE_OBJECT);
        }
        return 0;
    }
    for (index = 0; index < context.argumentCount; index++) {
        if (!native_binding_pin_value_object(state, &stableArgumentCopies[index], &argumentPinAdded[index])) {
            while (index > 0) {
                index--;
                native_binding_unpin_value_object(state->global, &stableArgumentCopies[index], argumentPinAdded[index]);
            }
            native_binding_unpin_value_object(state->global, &stableSelfCopy, selfPinAdded);
            for (index = copiedArgumentCount; index > 0; index--) {
                ZrCore_Ownership_ReleaseValue(state, &stableArgumentCopies[index - 1]);
            }
            if (hasCopiedSelf) {
                ZrCore_Ownership_ReleaseValue(state, &stableSelfCopy);
            }
            if (freeStableArgumentCopies) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              stableArgumentCopies,
                                              stableArgumentCopyBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            if (freeArgumentPinAdded) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              argumentPinAdded,
                                              argumentPinAddedBytes,
                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            }
            return 0;
        }
    }

    if (stableSlotCount > 0) {
        enteredStableScratchLayout = ZR_TRUE;
        stackTopBeforeStableScratchLayout = state->stackTop.valuePointer;
        ZrCore_Function_StackAnchorInit(state, stableBase, &stableBaseAnchor);
        if (state->callInfoList->functionBase.valuePointer != ZR_NULL) {
            ZrCore_Function_StackAnchorInit(state, state->callInfoList->functionBase.valuePointer, &callInfoBaseAnchor);
            hasSavedCallInfoBase = ZR_TRUE;
        }
        if (state->callInfoList->functionTop.valuePointer != ZR_NULL) {
            ZrCore_Function_StackAnchorInit(state, state->callInfoList->functionTop.valuePointer, &callInfoTopAnchor);
            hasSavedCallInfoTop = ZR_TRUE;
        }
        if (state->callInfoList->hasReturnDestination && state->callInfoList->returnDestination != ZR_NULL) {
            ZrCore_Function_StackAnchorInit(state, state->callInfoList->returnDestination, &callInfoReturnAnchor);
            hasSavedCallInfoReturn = ZR_TRUE;
        }

        stableBase = ZrCore_Function_CheckStackAndAnchor(state, stableSlotCount, stableBase, stableBase, &stableBaseAnchor);
        functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        stableBase = ZrCore_Function_StackAnchorRestore(state, &stableBaseAnchor);
        if (hasSavedCallInfoBase) {
            state->callInfoList->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        }
        if (hasSavedCallInfoTop) {
            state->callInfoList->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        }
        if (hasSavedCallInfoReturn) {
            state->callInfoList->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }

        context.functionBase = functionBase;
        if (context.selfValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(stableBase));
            ZrCore_Stack_CopyValue(state, stableBase, &stableSelfCopy);
            context.selfValue = &stableSelfCopy;
        }
        if (context.argumentCount > 0) {
            TZrStackValuePointer stableArgumentBase = stableBase + (context.selfValue != ZR_NULL ? 1 : 0);
            for (index = 0; index < context.argumentCount; index++) {
                ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(stableArgumentBase + index));
                ZrCore_Stack_CopyValue(state, stableArgumentBase + index, &stableArgumentCopies[index]);
            }
            context.argumentBase = stableArgumentBase;
            context.argumentValues = stableArgumentCopies;
            context.argumentValuePointers = ZR_NULL;
        } else {
            context.argumentBase = stableBase + (context.selfValue != ZR_NULL ? 1 : 0);
            context.argumentValues = ZR_NULL;
            context.argumentValuePointers = ZR_NULL;
        }

        state->stackTop.valuePointer = stableBase + stableSlotCount;
        if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||
            state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {
            state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;
        }
    }
    success = ZR_FALSE;
    if (context.functionDescriptor != ZR_NULL && context.functionDescriptor->callback != ZR_NULL) {
        success = context.functionDescriptor->callback(&context, &result);
    } else if (context.methodDescriptor != ZR_NULL && context.methodDescriptor->callback != ZR_NULL) {
        success = context.methodDescriptor->callback(&context, &result);
    } else if (context.metaMethodDescriptor != ZR_NULL && context.metaMethodDescriptor->callback != ZR_NULL) {
        success = context.metaMethodDescriptor->callback(&context, &result);
    }

    if (!success) {
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            goto cleanup_after_native_callback;
        }
        ZrLib_Value_SetNull(&result);
    }

    if (success && context.selfValue != ZR_NULL) {
        TZrStackValuePointer currentFunctionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        SZrTypeValue *stackSelf = currentFunctionBase != ZR_NULL ? ZrCore_Stack_GetValue(currentFunctionBase + 1)
                                                                 : ZR_NULL;
        if (stackSelf != ZR_NULL) {
            /* Native callbacks observe a stable self copy; sync mutations back to the call receiver slot. */
            ZrCore_Value_Copy(state, stackSelf, context.selfValue);
        }
    }

cleanup_after_native_callback:
    for (index = context.argumentCount; index > 0; index--) {
        native_binding_unpin_value_object(state->global,
                                          &stableArgumentCopies[index - 1],
                                          argumentPinAdded[index - 1]);
    }
    native_binding_unpin_value_object(state->global,
                                      context.selfValue != ZR_NULL ? &stableSelfCopy : ZR_NULL,
                                      selfPinAdded);
    for (index = copiedArgumentCount; index > 0; index--) {
        ZrCore_Ownership_ReleaseValue(state, &stableArgumentCopies[index - 1]);
    }
    if (hasCopiedSelf) {
        ZrCore_Ownership_ReleaseValue(state, &stableSelfCopy);
    }
    if (freeStableArgumentCopies) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      stableArgumentCopies,
                                      stableArgumentCopyBytes,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }
    if (freeArgumentPinAdded) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      argumentPinAdded,
                                      argumentPinAddedBytes,
                                      ZR_MEMORY_NATIVE_TYPE_OBJECT);
    }

    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        if (enteredStableScratchLayout) {
            state->stackTop.valuePointer = stackTopBeforeStableScratchLayout;
        }
        return 0;
    }

    functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
    closureValue = ZrCore_Stack_GetValue(functionBase);
    ZrCore_Value_Copy(state, closureValue, &result);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

TZrStackValuePointer native_binding_temp_root_slot(ZrLibTempValueRoot *root) {
    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return ZR_NULL;
    }

    if (native_binding_temp_root_direct_pointers_valid(root)) {
        ZR_ASSERT(root->slotPointer != ZR_NULL);
        return root->slotPointer;
    }

    return ZrCore_Function_StackAnchorRestore(root->state, &root->slotAnchor);
}

TZrSize ZrLib_CallContext_ArgumentCount(const ZrLibCallContext *context) {
    return context != ZR_NULL ? context->argumentCount : 0;
}

SZrTypeValue *ZrLib_CallContext_Self(const ZrLibCallContext *context) {
    ZrLibCallContext *mutableContext = (ZrLibCallContext *)context;
    native_binding_context_refresh_stack_layout_inline(mutableContext);
    return mutableContext != ZR_NULL ? mutableContext->selfValue : ZR_NULL;
}

SZrTypeValue *ZrLib_CallContext_Argument(const ZrLibCallContext *context, TZrSize index) {
    ZrLibCallContext *mutableContext = (ZrLibCallContext *)context;

    native_binding_context_refresh_stack_layout_inline(mutableContext);
    if (mutableContext == ZR_NULL || index >= mutableContext->argumentCount) {
        return ZR_NULL;
    }
    if (mutableContext->argumentValues != ZR_NULL) {
        return &mutableContext->argumentValues[index];
    }
    if (mutableContext->argumentValuePointers != ZR_NULL) {
        return mutableContext->argumentValuePointers[index];
    }
    return ZrCore_Stack_GetValueNoProfile(mutableContext->argumentBase + index);
}

SZrObjectPrototype *ZrLib_CallContext_OwnerPrototype(const ZrLibCallContext *context) {
    return context != ZR_NULL ? context->ownerPrototype : ZR_NULL;
}

SZrObjectPrototype *ZrLib_CallContext_GetConstructTargetPrototype(const ZrLibCallContext *context) {
    return native_binding_context_resolve_construct_target_prototype((ZrLibCallContext *)context);
}

TZrBool ZrLib_CallContext_CheckArity(const ZrLibCallContext *context,
                                     TZrSize minArgumentCount,
                                     TZrSize maxArgumentCount) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    if (context->argumentCount < minArgumentCount ||
        (maxArgumentCount != UINT16_MAX && context->argumentCount > maxArgumentCount)) {
        ZrLib_CallContext_RaiseArityError(context, minArgumentCount, maxArgumentCount);
    }

    return ZR_TRUE;
}

ZR_NO_RETURN void ZrLib_CallContext_RaiseTypeError(const ZrLibCallContext *context,
                                                   TZrSize index,
                                                   const TZrChar *expectedType) {
    const TZrChar *callName = native_binding_call_name(context);
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    const TZrChar *actualType = native_binding_value_type_name(context != ZR_NULL ? context->state : ZR_NULL, value);
    ZrCore_Debug_RunError(context->state,
                          "%s argument %u expected %s but got %s",
                          callName,
                          (unsigned)(index + 1),
                          expectedType != ZR_NULL ? expectedType : "value",
                          actualType != ZR_NULL ? actualType : "value");
}

ZR_NO_RETURN void ZrLib_CallContext_RaiseArityError(const ZrLibCallContext *context,
                                                    TZrSize minArgumentCount,
                                                    TZrSize maxArgumentCount) {
    const TZrChar *callName = native_binding_call_name(context);
    if (maxArgumentCount == UINT16_MAX) {
        ZrCore_Debug_RunError(context->state,
                              "%s expected at least %u arguments but got %u",
                              callName,
                              (unsigned)minArgumentCount,
                              (unsigned)context->argumentCount);
    } else if (minArgumentCount == maxArgumentCount) {
        ZrCore_Debug_RunError(context->state,
                              "%s expected %u arguments but got %u",
                              callName,
                              (unsigned)minArgumentCount,
                              (unsigned)context->argumentCount);
    } else {
        ZrCore_Debug_RunError(context->state,
                              "%s expected %u..%u arguments but got %u",
                              callName,
                              (unsigned)minArgumentCount,
                              (unsigned)maxArgumentCount,
                              (unsigned)context->argumentCount);
    }
}

TZrBool ZrLib_TempValueRoot_Begin(SZrState *state, ZrLibTempValueRoot *root) {
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer slot;

    if (state == ZR_NULL || root == ZR_NULL) {
        return ZR_FALSE;
    }

    if (native_binding_temp_root_try_begin_direct(state, root)) {
        return ZR_TRUE;
    }

    memset(root, 0, sizeof(*root));
    root->state = state;
    root->callInfo = state->callInfoList;
    savedStackTop = native_binding_resolve_temp_root_stack_top(state, root->callInfo);
    if (savedStackTop == ZR_NULL) {
        return ZR_FALSE;
    }
    state->stackTop.valuePointer = savedStackTop;

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &root->savedStackTopAnchor);
    if (root->callInfo != ZR_NULL && root->callInfo->functionBase.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, root->callInfo->functionBase.valuePointer, &root->savedCallInfoBaseAnchor);
        root->hasSavedCallInfoBase = ZR_TRUE;
    }
    if (root->callInfo != ZR_NULL && root->callInfo->functionTop.valuePointer != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, root->callInfo->functionTop.valuePointer, &root->savedCallInfoTopAnchor);
        root->hasSavedCallInfoTop = ZR_TRUE;
    }
    if (root->callInfo != ZR_NULL && root->callInfo->hasReturnDestination && root->callInfo->returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state,
                                        root->callInfo->returnDestination,
                                        &root->savedCallInfoReturnAnchor);
        root->hasSavedCallInfoReturn = ZR_TRUE;
    }

    slot = ZrCore_Function_CheckStackAndAnchor(state, 1, savedStackTop, savedStackTop, &root->slotAnchor);
    if (slot == ZR_NULL) {
        memset(root, 0, sizeof(*root));
        return ZR_FALSE;
    }

    if (root->hasSavedCallInfoBase && root->callInfo != ZR_NULL) {
        root->callInfo->functionBase.valuePointer =
                ZrCore_Function_StackAnchorRestore(state, &root->savedCallInfoBaseAnchor);
    }
    if (root->hasSavedCallInfoTop && root->callInfo != ZR_NULL) {
        root->callInfo->functionTop.valuePointer =
                ZrCore_Function_StackAnchorRestore(state, &root->savedCallInfoTopAnchor);
    }
    if (root->hasSavedCallInfoReturn && root->callInfo != ZR_NULL) {
        root->callInfo->returnDestination =
                ZrCore_Function_StackAnchorRestore(state, &root->savedCallInfoReturnAnchor);
    }

    slot = ZrCore_Function_StackAnchorRestore(state, &root->slotAnchor);
    if (slot == ZR_NULL) {
        memset(root, 0, sizeof(*root));
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = slot + 1;
    if (root->callInfo != ZR_NULL &&
        (root->callInfo->functionTop.valuePointer == ZR_NULL ||
         root->callInfo->functionTop.valuePointer < state->stackTop.valuePointer)) {
        root->callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    native_binding_temp_root_capture_direct_pointers(root, savedStackTop, slot);
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slot));
    root->active = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_BeginTempValueRoot(const ZrLibCallContext *context,
                                             ZrLibTempValueRoot *root) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLib_TempValueRoot_Begin(context->state, root);
}

SZrTypeValue *ZrLib_TempValueRoot_Value(ZrLibTempValueRoot *root) {
    return native_binding_temp_root_value_slot(root);
}

TZrBool ZrLib_TempValueRoot_SetValue(ZrLibTempValueRoot *root, const SZrTypeValue *value) {
    TZrStackValuePointer slot;
    SZrTypeValue *slotValue;

    if (root == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    slotValue = native_binding_temp_root_value_slot(root);
    if (slotValue == ZR_NULL) {
        return ZR_FALSE;
    }

    /*
     * Temp roots exist to keep a value stable across native helper work.
     * Plain values must keep their exact identity here; semantic stack copy
     * would clone struct objects and can silently null the root slot when a
     * clone path fails under nested native calls.
     */
    if (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE) {
        *slotValue = *value;
        if (slotValue->isGarbageCollectable) {
            ZrCore_Gc_ValueStaticAssertIsAlive(root->state, slotValue);
        }
        return ZR_TRUE;
    }

    slot = native_binding_temp_root_slot(root);
    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Stack_CopyValue(root->state, slot, value);
    return ZR_TRUE;
}

TZrBool ZrLib_TempValueRoot_SetObject(ZrLibTempValueRoot *root,
                                      SZrObject *object,
                                      EZrValueType type) {
    SZrTypeValue *slotValue;

    if (root == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    slotValue = native_binding_temp_root_value_slot(root);
    if (slotValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(root->state, slotValue, object, type);
    return ZR_TRUE;
}

void ZrLib_TempValueRoot_SetNull(ZrLibTempValueRoot *root) {
    SZrTypeValue *slotValue = native_binding_temp_root_value_slot(root);
    if (slotValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(slotValue);
    }
}

void ZrLib_TempValueRoot_End(ZrLibTempValueRoot *root) {
    SZrState *state;
    SZrCallInfo *callInfo;
    TZrStackValuePointer restoredStackTop;

    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return;
    }

    state = root->state;
    callInfo = root->callInfo;
    if (native_binding_temp_root_direct_pointers_valid(root)) {
        restoredStackTop = root->savedStackTopPointer;
        if (root->hasSavedCallInfoBase && callInfo != ZR_NULL) {
            callInfo->functionBase.valuePointer = root->savedCallInfoBasePointer;
        }
        if (root->hasSavedCallInfoTop && callInfo != ZR_NULL) {
            callInfo->functionTop.valuePointer = root->savedCallInfoTopPointer;
        } else if (root->restoreCallInfoTopFromSavedStackTop && callInfo != ZR_NULL) {
            callInfo->functionTop.valuePointer = restoredStackTop;
        }
        if (root->hasSavedCallInfoReturn && callInfo != ZR_NULL) {
            callInfo->returnDestination = root->savedCallInfoReturnPointer;
        }
    } else {
        restoredStackTop = ZrCore_Function_StackAnchorRestore(state, &root->savedStackTopAnchor);

        if (root->hasSavedCallInfoBase && callInfo != ZR_NULL) {
            callInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &root->savedCallInfoBaseAnchor);
        }
        if (root->hasSavedCallInfoTop && callInfo != ZR_NULL) {
            callInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &root->savedCallInfoTopAnchor);
        } else if (root->restoreCallInfoTopFromSavedStackTop && callInfo != ZR_NULL) {
            callInfo->functionTop.valuePointer = restoredStackTop;
        }
        if (root->hasSavedCallInfoReturn && callInfo != ZR_NULL) {
            callInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &root->savedCallInfoReturnAnchor);
        }
    }
    state->stackTop.valuePointer = restoredStackTop;
    root->active = ZR_FALSE;
    root->state = ZR_NULL;
    root->callInfo = ZR_NULL;
    root->savedStackTopPointer = ZR_NULL;
    root->savedCallInfoBasePointer = ZR_NULL;
    root->savedCallInfoTopPointer = ZR_NULL;
    root->savedCallInfoReturnPointer = ZR_NULL;
    root->slotPointer = ZR_NULL;
    root->stackBasePointer = ZR_NULL;
    root->hasSavedCallInfoBase = ZR_FALSE;
    root->hasSavedCallInfoTop = ZR_FALSE;
    root->hasSavedCallInfoReturn = ZR_FALSE;
    root->restoreCallInfoTopFromSavedStackTop = ZR_FALSE;
    root->usesDirectPointers = ZR_FALSE;
}

TZrBool ZrLib_CallContext_ReadInt(const ZrLibCallContext *context, TZrSize index, TZrInt64 *outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            if (outValue != ZR_NULL) {
                *outValue = value->value.nativeObject.nativeInt64;
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            if (outValue != ZR_NULL) {
                *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            if (outValue != ZR_NULL) {
                *outValue = (TZrInt64)value->value.nativeObject.nativeDouble;
            }
            return ZR_TRUE;
        default:
            ZrLib_CallContext_RaiseTypeError(context, index, "int");
    }
}

TZrBool ZrLib_CallContext_ReadFloat(const ZrLibCallContext *context, TZrSize index, TZrFloat64 *outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            if (outValue != ZR_NULL) {
                *outValue = (TZrFloat64)value->value.nativeObject.nativeInt64;
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            if (outValue != ZR_NULL) {
                *outValue = (TZrFloat64)value->value.nativeObject.nativeUInt64;
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            if (outValue != ZR_NULL) {
                *outValue = value->value.nativeObject.nativeDouble;
            }
            return ZR_TRUE;
        default:
            ZrLib_CallContext_RaiseTypeError(context, index, "float");
    }
}

TZrBool ZrLib_CallContext_ReadBool(const ZrLibCallContext *context, TZrSize index, TZrBool *outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
    }

    if (value->type != ZR_VALUE_TYPE_BOOL) {
        ZrLib_CallContext_RaiseTypeError(context, index, "bool");
    }

    if (outValue != ZR_NULL) {
        *outValue = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_ReadString(const ZrLibCallContext *context, TZrSize index, SZrString **outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
    }

    if (value->type != ZR_VALUE_TYPE_STRING) {
        ZrLib_CallContext_RaiseTypeError(context, index, "string");
    }

    if (outValue != ZR_NULL) {
        *outValue = ZR_CAST_STRING(context->state, value->value.object);
    }
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_ReadObject(const ZrLibCallContext *context, TZrSize index, SZrObject **outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
    }

    if (value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) {
        ZrLib_CallContext_RaiseTypeError(context, index, "object");
    }

    if (outValue != ZR_NULL) {
        *outValue = ZR_CAST_OBJECT(context->state, value->value.object);
    }
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_ReadArray(const ZrLibCallContext *context, TZrSize index, SZrObject **outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
    }

    if (value->type != ZR_VALUE_TYPE_ARRAY) {
        ZrLib_CallContext_RaiseTypeError(context, index, "array");
    }

    if (outValue != ZR_NULL) {
        *outValue = ZR_CAST_OBJECT(context->state, value->value.object);
    }
    return ZR_TRUE;
}

TZrBool ZrLib_CallContext_ReadFunction(const ZrLibCallContext *context, TZrSize index, SZrTypeValue **outValue) {
    SZrTypeValue *value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL) {
        ZrLib_CallContext_RaiseArityError(context, index + 1, UINT16_MAX);
    }

    if (value->type != ZR_VALUE_TYPE_FUNCTION &&
        value->type != ZR_VALUE_TYPE_CLOSURE &&
        value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        ZrLib_CallContext_RaiseTypeError(context, index, "function");
    }

    if (outValue != ZR_NULL) {
        *outValue = value;
    }
    return ZR_TRUE;
}

void ZrLib_Value_SetNull(SZrTypeValue *value) {
    if (value != ZR_NULL) {
        ZrCore_Value_ResetAsNull(value);
    }
}

void ZrLib_Value_SetBool(SZrState *state, SZrTypeValue *value, TZrBool boolValue) {
    ZR_UNUSED_PARAMETER(state);
    if (value != ZR_NULL) {
        ZR_VALUE_FAST_SET(value, nativeBool, boolValue, ZR_VALUE_TYPE_BOOL);
    }
}

void ZrLib_Value_SetInt(SZrState *state, SZrTypeValue *value, TZrInt64 intValue) {
    if (value != ZR_NULL) {
        ZrCore_Value_InitAsInt(state, value, intValue);
    }
}

void ZrLib_Value_SetFloat(SZrState *state, SZrTypeValue *value, TZrFloat64 floatValue) {
    if (value != ZR_NULL) {
        ZrCore_Value_InitAsFloat(state, value, floatValue);
    }
}

void ZrLib_Value_SetString(SZrState *state, SZrTypeValue *value, const TZrChar *stringValue) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrLib_Value_SetStringObject(state, value, native_binding_create_string(state, stringValue != ZR_NULL ? stringValue : ""));
}

void ZrLib_Value_SetStringObject(SZrState *state, SZrTypeValue *value, SZrString *stringObject) {
    if (state == ZR_NULL || value == ZR_NULL || stringObject == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    value->type = ZR_VALUE_TYPE_STRING;
}

void ZrLib_Value_SetObject(SZrState *state, SZrTypeValue *value, SZrObject *object, EZrValueType type) {
    if (state == ZR_NULL || value == ZR_NULL || object == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value->type = type;
}

void ZrLib_Value_SetNativePointer(SZrState *state, SZrTypeValue *value, TZrPtr pointerValue) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsNativePointer(state, value, pointerValue);
}

static EZrValueType native_binding_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY
                                                                                       : ZR_VALUE_TYPE_OBJECT;
}

SZrObject *ZrLib_Object_New(SZrState *state) {
    SZrObject *object;
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    object = ZrCore_Object_New(state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(state, object);
    }
    return object;
}

SZrObject *ZrLib_Array_New(SZrState *state) {
    SZrObject *array;
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(state, array);
    }
    return array;
}

void ZrLib_Object_SetFieldCString(SZrState *state,
                                  SZrObject *object,
                                  const TZrChar *fieldName,
                                  const SZrTypeValue *value) {
    SZrTypeValue keyValue;
    SZrString *fieldString;
    TZrBool objectPinAdded = ZR_FALSE;
    TZrBool valuePinAdded = ZR_FALSE;
    TZrBool keyPinAdded = ZR_FALSE;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    if (!native_binding_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &objectPinAdded)) {
        return;
    }
    if (!native_binding_pin_value_object(state, value, &valuePinAdded)) {
        native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinAdded);
        return;
    }

    fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldString == ZR_NULL) {
        native_binding_unpin_value_object(state->global, value, valuePinAdded);
        native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinAdded);
        return;
    }

    if (!native_binding_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), &keyPinAdded)) {
        native_binding_unpin_value_object(state->global, value, valuePinAdded);
        native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinAdded);
        return;
    }
    ZrCore_Value_InitAsRawObject(state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    keyValue.type = ZR_VALUE_TYPE_STRING;

    ZrCore_Object_SetValue(state, object, &keyValue, value);

    native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), keyPinAdded);
    native_binding_unpin_value_object(state->global, value, valuePinAdded);
    native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinAdded);
}

const SZrTypeValue *ZrLib_Object_GetFieldCString(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName) {
    SZrTypeValue keyValue;
    SZrString *fieldString;
    TZrBool objectPinAdded = ZR_FALSE;
    TZrBool keyPinAdded = ZR_FALSE;
    const SZrTypeValue *result = ZR_NULL;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    if (!native_binding_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &objectPinAdded)) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldString == ZR_NULL) {
        native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinAdded);
        return ZR_NULL;
    }

    if (!native_binding_pin_raw_object(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), &keyPinAdded)) {
        native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinAdded);
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    keyValue.type = ZR_VALUE_TYPE_STRING;

    result = ZrCore_Object_GetValue(state, object, &keyValue);

    native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), keyPinAdded);
    native_binding_unpin_raw_object(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinAdded);
    return result;
}

TZrBool ZrLib_Array_PushValue(SZrState *state, SZrObject *array, const SZrTypeValue *value) {
    SZrTypeValue arrayValue;
    TZrBool arrayPinAdded = ZR_FALSE;
    TZrBool valuePinAdded = ZR_FALSE;
    SZrTypeValue key;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &arrayValue, array, native_binding_value_type_for_object(array));
    if (!native_binding_pin_value_object(state, &arrayValue, &arrayPinAdded)) {
        return ZR_FALSE;
    }
    if (!native_binding_pin_value_object(state, value, &valuePinAdded)) {
        native_binding_unpin_value_object(state->global, &arrayValue, arrayPinAdded);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)ZrLib_Array_Length(array));
    ZrCore_Object_SetValue(state, array, &key, value);
    success = state->threadStatus == ZR_THREAD_STATUS_FINE;

    native_binding_unpin_value_object(state->global, value, valuePinAdded);
    native_binding_unpin_value_object(state->global, &arrayValue, arrayPinAdded);
    return success;
}

TZrSize ZrLib_Array_Length(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }
    return array->nodeMap.elementCount;
}

const SZrTypeValue *ZrLib_Array_Get(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

static SZrString *native_binding_extract_open_generic_base_name(SZrState *state, const TZrChar *typeName) {
    const TZrChar *genericStart;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    genericStart = strchr(typeName, '<');
    if (genericStart == ZR_NULL || genericStart == typeName) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)typeName, (TZrSize)(genericStart - typeName));
}

static SZrObjectPrototype *native_binding_find_qualified_module_export_prototype(SZrState *state, const TZrChar *typeName) {
    const TZrChar *genericStart;
    const TZrChar *lastDot;
    TZrSize moduleNameLength;
    TZrSize exportNameLength;
    TZrChar moduleNameBuffer[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
    TZrChar exportNameBuffer[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
    SZrObjectModule *module;
    const SZrTypeValue *exportedValue;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    genericStart = strchr(typeName, '<');
    lastDot = strrchr(typeName, '.');
    if (lastDot == ZR_NULL || lastDot == typeName || lastDot[1] == '\0' ||
        (genericStart != ZR_NULL && lastDot > genericStart)) {
        return ZR_NULL;
    }

    moduleNameLength = (TZrSize)(lastDot - typeName);
    exportNameLength = genericStart != ZR_NULL
                               ? (TZrSize)(genericStart - (lastDot + 1))
                               : strlen(lastDot + 1);
    if (moduleNameLength == 0 || exportNameLength == 0 ||
        moduleNameLength >= sizeof(moduleNameBuffer) ||
        exportNameLength >= sizeof(exportNameBuffer)) {
        return ZR_NULL;
    }

    memcpy(moduleNameBuffer, typeName, moduleNameLength);
    moduleNameBuffer[moduleNameLength] = '\0';
    memcpy(exportNameBuffer, lastDot + 1, exportNameLength);
    exportNameBuffer[exportNameLength] = '\0';

    module = ZrLib_Module_GetLoaded(state, moduleNameBuffer);
    if (module == ZR_NULL) {
        module = native_binding_import_module(state, moduleNameBuffer);
    }
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    exportedValue = ZrLib_Module_GetExport(state, moduleNameBuffer, exportNameBuffer);
    if (exportedValue == ZR_NULL || exportedValue->type != ZR_VALUE_TYPE_OBJECT || exportedValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exportedValue->value.object);
}

SZrObjectPrototype *ZrLib_Type_FindPrototype(SZrState *state, const TZrChar *typeName) {
    SZrTypeValue key;
    SZrString *typeString;
    const SZrTypeValue *value;
    SZrObjectPrototype *qualifiedPrototype;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL ||
        state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    qualifiedPrototype = native_binding_find_qualified_module_export_prototype(state, typeName);
    if (qualifiedPrototype != ZR_NULL) {
        return qualifiedPrototype;
    }

    typeString = native_binding_create_string(state, typeName);
    if (typeString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(typeString));
    key.type = ZR_VALUE_TYPE_STRING;
    value = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, state->global->zrObject.value.object), &key);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT) {
        SZrString *openBaseName = native_binding_extract_open_generic_base_name(state, typeName);
        if (openBaseName == ZR_NULL) {
            return ZR_NULL;
        }

        ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(openBaseName));
        key.type = ZR_VALUE_TYPE_STRING;
        value = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, state->global->zrObject.value.object), &key);
        if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT) {
            return ZR_NULL;
        }
    }

    return (SZrObjectPrototype *)ZR_CAST_OBJECT(state, value->value.object);
}

SZrObject *ZrLib_Type_NewInstance(SZrState *state, const TZrChar *typeName) {
    return native_binding_new_instance_with_prototype(state, ZrLib_Type_FindPrototype(state, typeName));
}

SZrObject *ZrLib_Type_NewInstanceWithPrototype(SZrState *state, SZrObjectPrototype *prototype) {
    return native_binding_new_instance_with_prototype(state, prototype);
}

SZrObjectModule *ZrLib_Module_GetLoaded(SZrState *state, const TZrChar *moduleName) {
    SZrString *moduleString;
    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }
    moduleString = native_binding_create_string(state, moduleName);
    if (moduleString == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_Module_GetFromCache(state, moduleString);
}

const SZrTypeValue *ZrLib_Module_GetExport(SZrState *state,
                                           const TZrChar *moduleName,
                                           const TZrChar *exportName) {
    SZrObjectModule *module;
    SZrString *exportString;

    if (state == ZR_NULL || moduleName == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    module = ZrLib_Module_GetLoaded(state, moduleName);
    if (module == ZR_NULL) {
        module = native_binding_import_module(state, moduleName);
    }
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    exportString = native_binding_create_string(state, exportName);
    if (exportString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportString);
}

typedef struct ZrLibPanicRecoverContext {
    jmp_buf jumpBuffer;
    FZrPanicHandlingFunction previousHandler;
    SZrState *state;
    EZrThreadStatus status;
    struct ZrLibPanicRecoverContext *previous;
    TZrBool triggered;
} ZrLibPanicRecoverContext;

static ZR_LIB_THREAD_LOCAL ZrLibPanicRecoverContext *g_zr_lib_panic_recover_context = ZR_NULL;

static EZrThreadStatus native_binding_call_normalize_failure(SZrState *state, EZrThreadStatus status) {
    EZrThreadStatus effectiveStatus;

    if (state == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    effectiveStatus = status;
    if (effectiveStatus == ZR_THREAD_STATUS_FINE && state->threadStatus != ZR_THREAD_STATUS_FINE) {
        effectiveStatus = state->threadStatus;
    }
    if (effectiveStatus == ZR_THREAD_STATUS_FINE) {
        effectiveStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    if (!state->hasCurrentException) {
        (void)ZrCore_Exception_NormalizeStatus(state, effectiveStatus);
    }
    state->threadStatus = effectiveStatus;
    return effectiveStatus;
}

static void native_binding_call_panic_handler(SZrState *state) {
    ZrLibPanicRecoverContext *context = g_zr_lib_panic_recover_context;

    if (context == ZR_NULL || context->state != state) {
        if (state != ZR_NULL && state->global != ZR_NULL &&
            state->global->panicHandlingFunction != native_binding_call_panic_handler &&
            state->global->panicHandlingFunction != ZR_NULL) {
            state->global->panicHandlingFunction(state);
        }
        return;
    }

    context->triggered = ZR_TRUE;
    context->status = (state != ZR_NULL && state->threadStatus != ZR_THREAD_STATUS_FINE)
                              ? state->threadStatus
                              : (state != ZR_NULL && state->hasCurrentException) ? state->currentExceptionStatus
                                                                                 : ZR_THREAD_STATUS_RUNTIME_ERROR;
    longjmp(context->jumpBuffer, 1);
}

TZrBool ZrLib_CallValue(SZrState *state,
                        const SZrTypeValue *callable,
                        const SZrTypeValue *receiver,
                        const SZrTypeValue *arguments,
                        TZrSize argumentCount,
                        SZrTypeValue *result) {
    SZrTypeValue stableCallable;
    SZrTypeValue stableReceiver;
    SZrTypeValue inlineArguments[ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY];
    SZrTypeValue *stableArguments = ZR_NULL;
    TZrBool freeStableArguments = ZR_FALSE;
    TZrSize stableArgumentsBytes = 0;
    TZrStackValuePointer savedStackTop;
    SZrCallInfo *savedCallInfo;
    TZrSize totalArguments;
    TZrSize scratchSlots;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor originalCallInfoTopAnchor;
    SZrFunctionStackAnchor activeCallInfoTopAnchor;
    SZrFunctionStackAnchor callInfoReturnAnchor;
    TZrBool hasAnchoredReturnDestination = ZR_FALSE;
    TZrBool hasCallInfoAnchors = ZR_FALSE;
    TZrBool hasActiveCallInfoTopAnchor = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || callable == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    stableCallable = *callable;
    if (receiver != ZR_NULL) {
        stableReceiver = *receiver;
    }
    if (argumentCount > 0) {
        if (arguments == ZR_NULL) {
            return ZR_FALSE;
        }

        if (argumentCount <= ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY) {
            stableArguments = inlineArguments;
        } else {
            stableArgumentsBytes = argumentCount * sizeof(SZrTypeValue);
            stableArguments = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                              stableArgumentsBytes,
                                                                              ZR_MEMORY_NATIVE_TYPE_OBJECT);
            if (stableArguments == ZR_NULL) {
                return ZR_FALSE;
            }
            freeStableArguments = ZR_TRUE;
        }

        for (index = 0; index < argumentCount; index++) {
            stableArguments[index] = arguments[index];
        }
    }
    ZrLib_Value_SetNull(result);
    savedStackTop = state->stackTop.valuePointer;
    savedCallInfo = state->callInfoList;
    totalArguments = argumentCount + (receiver != ZR_NULL ? 1 : 0);
    scratchSlots = 1 + totalArguments;
    base = native_binding_resolve_call_scratch_base(savedStackTop, savedCallInfo);

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
        base = native_binding_resolve_call_scratch_base(savedStackTop, savedCallInfo);
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
    base = ZrCore_Function_CallWithoutYieldKnownValueAndRestoreAnchor(state, &baseAnchor, &stableCallable, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (savedCallInfo != ZR_NULL && hasCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &originalCallInfoTopAnchor);
        if (hasAnchoredReturnDestination) {
            savedCallInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
        }
    }

    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        SZrTypeValue *stackResult = ZrCore_Stack_GetValue(base);
        ZrCore_Value_Copy(state, result, stackResult);
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
        if (freeStableArguments) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          stableArguments,
                                          stableArgumentsBytes,
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
    return ZR_FALSE;
}

TZrBool ZrLib_CallModuleExport(SZrState *state,
                               const TZrChar *moduleName,
                               const TZrChar *exportName,
                               const SZrTypeValue *arguments,
                               TZrSize argumentCount,
                               SZrTypeValue *result) {
    const SZrTypeValue *exportValue = ZrLib_Module_GetExport(state, moduleName, exportName);
    ZrLibPanicRecoverContext context;
    EZrThreadStatus status = ZR_THREAD_STATUS_FINE;
    TZrBool callCompleted = ZR_FALSE;

    if (exportValue == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&context, 0, sizeof(context));
    context.state = state;
    context.previousHandler = state != ZR_NULL && state->global != ZR_NULL ? state->global->panicHandlingFunction : ZR_NULL;
    context.previous = g_zr_lib_panic_recover_context;
    g_zr_lib_panic_recover_context = &context;
    if (state != ZR_NULL && state->global != ZR_NULL) {
        state->global->panicHandlingFunction = native_binding_call_panic_handler;
    }

    if (setjmp(context.jumpBuffer) == 0) {
        callCompleted = ZrLib_CallValue(state, exportValue, ZR_NULL, arguments, argumentCount, result);
        if (state != ZR_NULL) {
            status = state->threadStatus;
        }
    } else {
        status = context.status;
    }

    if (state != ZR_NULL && state->global != ZR_NULL) {
        state->global->panicHandlingFunction = context.previousHandler;
    }
    g_zr_lib_panic_recover_context = context.previous;

    if (context.triggered || !callCompleted || status != ZR_THREAD_STATUS_FINE) {
        (void)native_binding_call_normalize_failure(state, status);
        return ZR_FALSE;
    }

    state->threadStatus = ZR_THREAD_STATUS_FINE;
    return ZR_TRUE;
}
