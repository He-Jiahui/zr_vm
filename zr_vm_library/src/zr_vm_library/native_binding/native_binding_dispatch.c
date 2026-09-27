#include "native_binding/native_binding_internal.h"

#include <setjmp.h>

#include "zr_vm_core/exception.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/property_reference.h"
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
#define native_binding_context_adopt_inline_frame_anchor native_binding_context_adopt_inline_frame_anchor_inline

#define ZR_LIB_THREAD_LOCAL ZR_THREAD_LOCAL

/* 由通用分派入口消费描述符的参数范围；回调只接收通过声明约束的调用。 */
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

/* 临时根优先借用现有 VM 栈空间，只有仍属于当前栈的指针才可直接使用。 */
static ZR_FORCE_INLINE TZrBool native_binding_stack_pointer_in_current_range(const SZrState *state,
                                                                             TZrStackValuePointer pointer) {
    return state != ZR_NULL && pointer != ZR_NULL && state->stackBase.valuePointer != ZR_NULL &&
           state->stackTail.valuePointer != ZR_NULL && pointer >= state->stackBase.valuePointer &&
           pointer <= state->stackTail.valuePointer;
}

/* 调用信息可能暂时持有比 state->stackTop 更可靠的栈顶，供临时根恢复。 */
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

/* 热路径在剩余栈槽足够时直接建立 GC 可见的临时根，并保存回退位置。 */
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

/* Begin 仅在直接栈槽无法使用时进入会扩栈的通用路径。 */
static ZR_FORCE_INLINE TZrBool native_binding_temp_root_try_begin_direct(SZrState *state, ZrLibTempValueRoot *root) {
    return native_binding_temp_root_try_begin_direct_slot(state, root) != ZR_NULL;
}

/* 扩栈后原始地址失效，End/Value 必须回退到偏移锚点。 */
static ZR_FORCE_INLINE TZrBool native_binding_temp_root_direct_pointers_valid(const ZrLibTempValueRoot *root) {
    return root != ZR_NULL && root->active && root->state != ZR_NULL && root->usesDirectPointers &&
           root->stackBasePointer != ZR_NULL && root->state->stackBase.valuePointer == root->stackBasePointer;
}

/* 扩栈完成后重新记录快路径指针，避免每次读取临时根都恢复锚点。 */
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

/* 回调只在根处于活动期时可获取值槽；栈移动时统一从锚点重建地址。 */
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

/* 字段/数组辅助函数跨分配边界保活裸对象，addedByCaller 决定谁负责解除忽略。 */
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

/* 只撤销本调用新增的 GC 忽略，避免破坏宿主已持有的固定状态。 */
static ZR_FORCE_INLINE void native_binding_unpin_raw_object(SZrGlobalState *global,
                                                            SZrRawObject *object,
                                                            TZrBool addedByCaller) {
    if (!addedByCaller || global == ZR_NULL || object == ZR_NULL) {
        return;
    }

    ZrCore_GarbageCollector_UnignoreObject(global, object);
}

/* 构造回调优先保持派生实例的实际原型，避免继承调用退回描述符的基类。 */
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

/* 回调可能触发扩栈，返回值必须通过函数基址锚点写回闭包槽。 */
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

/* VM 调用闭包的总入口：优先使用栈根/标量快路，再退到有稳定副本的通用路径。 */
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
                                                   state,
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
    /* BUG: 实例调用已复制带所有权的 self 后，超过内联容量的参数缓冲区分配失败会直接返回，
     * 未释放 stableSelfCopy 增加的强引用；见 Value_CopySlow -> Ownership_AssignValue。 */
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

    /* 栈临时槽使稳定副本对 GC 可见；扩栈前后的调用帧指针均须由锚点恢复。 */
    if (stableSlotCount > 0) {
        enteredStableScratchLayout = ZR_TRUE;
        /* BUG: 这里保存原始栈地址，后续 CheckStackAndAnchor 可调用 Stack_GrowTo 移动栈；
         * 回调报错后 cleanup 用旧地址恢复 stackTop，留下悬空栈指针。 */
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
    native_binding_context_adopt_inline_frame_anchor(
            &context, &functionBaseAnchor);
    success = native_binding_invoke_entry_callback_inline(
            state, entryView, &context, &result);

    /* false 且无线程错误表示回调选择返回 null；有错误时保持异常并走清理路径。 */
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

/* 返回活动临时根的 VM 栈槽；调用方不得在 End 后继续保存此地址。 */
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

/* 描述符参数数目不含实例 receiver，供绑定回调选择业务参数。 */
TZrSize ZrLib_CallContext_ArgumentCount(const ZrLibCallContext *context) {
    return context != ZR_NULL ? context->argumentCount : 0;
}

/* inline struct 参数存于专门帧布局；普通 SZrTypeValue 视图不能代表其存储。 */
static TZrBool native_binding_context_argument_is_inline_struct_parameter(ZrLibCallContext *context,
                                                                          TZrSize index) {
    const SZrFunctionFrameSlotLayout *slotLayout;
    TZrUInt32 stackSlot;

    if (context == ZR_NULL || index >= context->argumentCount ||
        context->inlineFrameFunction == ZR_NULL ||
        context->inlineFrameBase == ZR_NULL ||
        index > (TZrSize)(UINT32_MAX - context->inlineArgumentStartSlot)) {
        return ZR_FALSE;
    }

    stackSlot = context->inlineArgumentStartSlot + (TZrUInt32)index;
    slotLayout = ZrCore_Function_FindFrameSlotLayout(context->inlineFrameFunction, stackSlot);
    return slotLayout != ZR_NULL &&
           slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT &&
           slotLayout->isParameter;
}

/* 回调访问 receiver 前刷新栈地址，支持回调期间的扩栈。 */
SZrTypeValue *ZrLib_CallContext_Self(const ZrLibCallContext *context) {
    ZrLibCallContext *mutableContext = (ZrLibCallContext *)context;
    native_binding_context_refresh_stack_layout_inline(mutableContext);
    return mutableContext != ZR_NULL ? mutableContext->selfValue : ZR_NULL;
}

/* 按 lane 选择稳定副本或 VM 栈槽；inline struct 参数须改用 InlineArgumentSpan。 */
SZrTypeValue *ZrLib_CallContext_Argument(const ZrLibCallContext *context, TZrSize index) {
    ZrLibCallContext *mutableContext = (ZrLibCallContext *)context;

    native_binding_context_refresh_stack_layout_inline(mutableContext);
    native_binding_context_refresh_inline_frame_layout_inline(mutableContext);
    if (mutableContext == ZR_NULL || index >= mutableContext->argumentCount) {
        return ZR_NULL;
    }
    if (native_binding_context_argument_is_inline_struct_parameter(mutableContext, index)) {
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

/* FFI ref/out 回写优先遵守属性引用语义，其他参数同步到原调用栈槽。 */
TZrBool ZrLib_CallContext_WriteBackArgument(ZrLibCallContext *context,
                                            TZrSize index,
                                            const SZrTypeValue *value) {
    SZrTypeValue *callbackArgument;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer originalArgumentBase;
    SZrTypeValue *originalArgument;

    if (context == ZR_NULL || context->state == ZR_NULL || value == ZR_NULL ||
        index >= context->argumentCount) {
        return ZR_FALSE;
    }

    callbackArgument = ZrLib_CallContext_Argument(context, index);
    if (callbackArgument == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrCore_PropertyReference_IsValid(context->state, callbackArgument)) {
        return ZrCore_PropertyReference_Store(
                context->state, callbackArgument, value);
    }
    /* TODO: Value_Copy 是 void，下面两次复制后均未核查线程状态；
     * FFI ref/out 调用者只看本函数的 bool。注入结构体克隆或所有权复制失败，
     * 核实是否会把空值/部分写回误报为成功。 */
    if (callbackArgument != value) {
        ZrCore_Value_Copy(context->state, callbackArgument, value);
    }

    functionBase = context->stackBasePointer != ZR_NULL
            ? ZrCore_Function_StackAnchorRestore(context->state, &context->functionBaseAnchor)
            : context->functionBase;
    if (functionBase == ZR_NULL) {
        return ZR_FALSE;
    }
    originalArgumentBase = functionBase + (context->stackLayoutUsesReceiver ? 2u : 1u);
    originalArgument = ZrCore_Stack_GetValueNoProfile(originalArgumentBase + index);
    if (originalArgument == ZR_NULL) {
        return ZR_FALSE;
    }
    if (originalArgument != callbackArgument) {
        ZrCore_Value_Copy(context->state, originalArgument, callbackArgument);
    }
    return ZR_TRUE;
}

/* 借用当前调用帧内的 inline struct 字节区；地址只在帧和当前栈布局有效。 */
TZrBool ZrLib_CallContext_InlineArgumentSpan(const ZrLibCallContext *context,
                                             TZrSize index,
                                             ZrLibInlineSpan *outSpan) {
    ZrLibCallContext *mutableContext = (ZrLibCallContext *)context;
    const SZrFunctionFrameSlotLayout *slotLayout;
    SZrStackFramePlace place;
    TZrUInt32 stackSlot;

    if (outSpan != ZR_NULL) {
        memset(outSpan, 0, sizeof(*outSpan));
    }

    native_binding_context_refresh_stack_layout_inline(mutableContext);
    native_binding_context_refresh_inline_frame_layout_inline(mutableContext);
    if (mutableContext == ZR_NULL || outSpan == ZR_NULL ||
        index >= mutableContext->argumentCount ||
        mutableContext->inlineFrameFunction == ZR_NULL ||
        mutableContext->inlineFrameBase == ZR_NULL ||
        index > (TZrSize)(UINT32_MAX - mutableContext->inlineArgumentStartSlot)) {
        return ZR_FALSE;
    }

    stackSlot = mutableContext->inlineArgumentStartSlot + (TZrUInt32)index;
    slotLayout = ZrCore_Function_FindFrameSlotLayout(mutableContext->inlineFrameFunction, stackSlot);
    if (slotLayout == ZR_NULL ||
        slotLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT ||
        !slotLayout->isParameter ||
        !ZrCore_Function_MakeFrameSlotPlace(mutableContext->state,
                                            mutableContext->inlineFrameFunction,
                                            mutableContext->inlineFrameBase,
                                            stackSlot,
                                            &place)) {
        return ZR_FALSE;
    }

    outSpan->address = place.address;
    outSpan->byteSize = place.byteSize;
    outSpan->byteAlign = place.byteAlign;
    outSpan->typeLayoutId = slotLayout->typeLayoutId;
    outSpan->available = ZR_TRUE;
    return ZR_TRUE;
}

/* 返回注册该原生成员的声明原型，供描述符驱动的构造/反射回调使用。 */
SZrObjectPrototype *ZrLib_CallContext_OwnerPrototype(const ZrLibCallContext *context) {
    return context != ZR_NULL ? context->ownerPrototype : ZR_NULL;
}

/* 构造回调拿到实际目标原型，保留派生类型的实例身份。 */
SZrObjectPrototype *ZrLib_CallContext_GetConstructTargetPrototype(const ZrLibCallContext *context) {
    return native_binding_context_resolve_construct_target_prototype((ZrLibCallContext *)context);
}

/* 描述符和回调共用同一参数计数约定；上界 UINT16_MAX 表示不设上限。 */
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

/* 原生回调统一通过 VM 异常入口上报类型不匹配，要求 context->state 有效。 */
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

/* 参数范围错误携带描述符调用名，避免各模块重复拼接错误上下文。 */
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

/* 为 native helper 暂存跨分配使用的 VM 值；成功后必须成对 End。 */
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

/* 从回调上下文建立临时 GC 根，继承正在执行的 state 与调用帧。 */
TZrBool ZrLib_CallContext_BeginTempValueRoot(const ZrLibCallContext *context,
                                             ZrLibTempValueRoot *root) {
    if (context == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLib_TempValueRoot_Begin(context->state, root);
}

/* 取得可写根值槽；每次访问重新解析，不能跨扩栈缓存返回指针。 */
SZrTypeValue *ZrLib_TempValueRoot_Value(ZrLibTempValueRoot *root) {
    return native_binding_temp_root_value_slot(root);
}

/* 无所有权值保持对象身份；带所有权值走栈赋值语义以维护引用计数。 */
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

/* 将新建对象直接放入活动根，供后续可能触发 GC 的构造过程使用。 */
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

/* 提前解除根槽的对象引用，End 之前也可让其参与回收。 */
void ZrLib_TempValueRoot_SetNull(ZrLibTempValueRoot *root) {
    SZrTypeValue *slotValue = native_binding_temp_root_value_slot(root);
    if (slotValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(slotValue);
    }
}

/* 恢复进入时的栈与调用信息；Begin 成功后即使回调失败也需执行。 */
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

/* 原生模块复用此入口读取整数类参数；目前也接受浮点并执行窄化。 */
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
        /* BUG: 浮点参数为 NaN、无穷或超出 int64 范围时，C 转换行为未定义；
         * FFI Buffer/Pointer 等公开回调可直接传入此类值。 */
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

/* 将 VM 数值扩展为回调需要的 double；大整数调用方须自行考虑精度。 */
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

/* 布尔读取保留严格类型约束，供原生描述符避免隐式真值转换。 */
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

/* 返回当前调用期有效的 VM 字符串对象，回调不可在无根条件下跨 GC 保存。 */
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

/* 对象读取同时容纳数组值，供共用对象存储接口的原生模块使用。 */
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

/* 仅接受数组值，避免调用方把普通对象误当连续下标容器。 */
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

/* FFI 回调接口允许 VM 可调用值及原生函数指针，返回视图只在调用期有效。 */
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

/* 原生回调构造返回值前的通用空值初始化入口。 */
void ZrLib_Value_SetNull(SZrTypeValue *value) {
    if (value != ZR_NULL) {
        ZrCore_Value_ResetAsNull(value);
    }
}

/* 把 C 布尔结果封装为 VM 标量，不产生额外 GC 所有权。 */
void ZrLib_Value_SetBool(SZrState *state, SZrTypeValue *value, TZrBool boolValue) {
    ZR_UNUSED_PARAMETER(state);
    if (value != ZR_NULL) {
        ZR_VALUE_FAST_SET(value, nativeBool, boolValue, ZR_VALUE_TYPE_BOOL);
    }
}

/* 将原生模块整数结果交给 VM 值初始化规则。 */
void ZrLib_Value_SetInt(SZrState *state, SZrTypeValue *value, TZrInt64 intValue) {
    if (value != ZR_NULL) {
        ZrCore_Value_InitAsInt(state, value, intValue);
    }
}

/* 将原生浮点结果交给 VM 值初始化规则。 */
void ZrLib_Value_SetFloat(SZrState *state, SZrTypeValue *value, TZrFloat64 floatValue) {
    if (value != ZR_NULL) {
        ZrCore_Value_InitAsFloat(state, value, floatValue);
    }
}

/* 为回调结果创建受 VM 管理的字符串；分配可能触发 GC。 */
void ZrLib_Value_SetString(SZrState *state, SZrTypeValue *value, const TZrChar *stringValue) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrLib_Value_SetStringObject(state, value, native_binding_create_string(state, stringValue != ZR_NULL ? stringValue : ""));
}

/* 将已有 VM 字符串交给结果槽；调用方须保证对象在此处仍有效。 */
void ZrLib_Value_SetStringObject(SZrState *state, SZrTypeValue *value, SZrString *stringObject) {
    if (state == ZR_NULL || value == ZR_NULL || stringObject == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    value->type = ZR_VALUE_TYPE_STRING;
}

/* 为新建对象/数组赋予 VM 值类型，后续由调用栈或临时根负责可达性。 */
void ZrLib_Value_SetObject(SZrState *state, SZrTypeValue *value, SZrObject *object, EZrValueType type) {
    if (state == ZR_NULL || value == ZR_NULL || object == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value->type = type;
}

/* 封装外部地址；本接口本身不管理地址指向内存的生命周期。 */
void ZrLib_Value_SetNativePointer(SZrState *state, SZrTypeValue *value, TZrPtr pointerValue) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return;
    }
    ZrCore_Value_InitAsNativePointer(state, value, pointerValue);
}

/* 数组与普通对象共享底层对象指针，固定 GC 根时保留其值标签。 */
static EZrValueType native_binding_value_type_for_object(SZrObject *object) {
    return object != ZR_NULL && object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY ? ZR_VALUE_TYPE_ARRAY
                                                                                       : ZR_VALUE_TYPE_OBJECT;
}

/* 原生模块创建普通 VM 对象；返回裸指针后须在后续分配前建立根。 */
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

/* 创建使用 VM 数组内部类型的对象，供 FFI 和反射回调组装结果。 */
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

/* 用临时字符串键设置对象字段；内部临时固定键、值和宿主对象。 */
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

    /* BUG: 此 void API 在固定对象/值或创建键失败时静默返回；FFI handle 的
     * 隐藏 owner 字段及结构体结果字段调用者无法得知写入失败，仍会发布不完整对象。 */
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

/* 借用对象字段值；返回后目标对象必须仍有根，调用方不可跨修改长期保存指针。 */
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

/* 数组为顺序整数键时直接使用预留 pair pool，避免通常的通用哈希写入开销。 */
static TZrBool native_binding_array_try_push_dense_pair_pool_pinned(SZrState *state,
                                                                    SZrObject *array,
                                                                    const SZrTypeValue *value) {
    SZrHashSet *nodeMap;
    SZrHashKeyValuePair *pair;
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || value == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        array->superArrayStorageMode == ZR_SUPER_ARRAY_STORAGE_MODE_RAW_CANONICAL) {
        return ZR_FALSE;
    }

    nodeMap = &array->nodeMap;
    if (!nodeMap->isValid || nodeMap->buckets == ZR_NULL || nodeMap->capacity == 0) {
        return ZR_FALSE;
    }

    index = nodeMap->elementCount;
    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacity(state, nodeMap, index + 1) ||
        !ZrCore_HashSet_EnsurePairPoolForElementCount(state, nodeMap, nodeMap->pairPoolUsed + 1) ||
        index >= nodeMap->capacity ||
        nodeMap->buckets[index] != ZR_NULL) {
        return ZR_FALSE;
    }

    pair = ZrCore_HashSet_TakeReservedPair(nodeMap);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    pair->next = ZR_NULL;
    ZR_VALUE_FAST_SET(&pair->key, nativeInt64, (TZrInt64)index, ZR_VALUE_TYPE_INT64);
    ZrCore_Value_ResetAsNull(&pair->value);
    /* BUG: Copy 可在克隆对象或取得所有权引用时失败并设置线程错误，
     * 此处仍插入 pair 并增加数组长度；PushValue 随后返回 false，却留下部分写入。 */
    ZrCore_Value_Copy(state, &pair->value, value);
    nodeMap->buckets[index] = pair;
    nodeMap->elementCount++;
    array->superArrayStorageMode = ZR_SUPER_ARRAY_STORAGE_MODE_NODE_CANONICAL;
    array->superArrayStorageGeneration++;
    if (ZrCore_Value_IsGarbageCollectable(&pair->value)) {
        ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(array), &pair->value);
    }
    array->memberVersion++;
    return ZR_TRUE;
}

/* 组装返回数组时固定容器和值；快路不适用则回落到 VM 对象写入语义。 */
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

    if (native_binding_array_try_push_dense_pair_pool_pinned(state, array, value)) {
        success = state->threadStatus == ZR_THREAD_STATUS_FINE;
    } else {
        ZrCore_Value_InitAsInt(state, &key, (TZrInt64)ZrLib_Array_Length(array));
        ZrCore_Object_SetValue(state, array, &key, value);
        success = state->threadStatus == ZR_THREAD_STATUS_FINE;
    }

    native_binding_unpin_value_object(state->global, value, valuePinAdded);
    native_binding_unpin_value_object(state->global, &arrayValue, arrayPinAdded);
    return success;
}

/* 与 VM 超级数组的当前存储模式共享长度定义。 */
TZrSize ZrLib_Array_Length(SZrObject *array) {
    return ZrCore_Object_SuperArrayLength(array);
}

/* 借用数组元素值；后续修改数组或 GC 前由调用方负责可达性。 */
const SZrTypeValue *ZrLib_Array_Get(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

/* 类型查询先尝试完整名，再允许开放泛型落回全局基类原型。 */
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

/* 模块导出与全局作用域使用相同的对象值表示，集中验证原型对象。 */
static SZrObjectPrototype *native_binding_value_as_object_prototype(SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)object;
}

/* 以模块限定名解析类型时可按需导入提供模块，再检查其公开导出。 */
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
    return native_binding_value_as_object_prototype(state, exportedValue);
}

/* 原生模块按名称找类型：优先模块限定导出，其次全局原型与开放泛型基类。 */
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
    qualifiedPrototype = native_binding_value_as_object_prototype(state, value);
    if (qualifiedPrototype == ZR_NULL) {
        SZrString *openBaseName = native_binding_extract_open_generic_base_name(state, typeName);
        if (openBaseName == ZR_NULL) {
            return ZR_NULL;
        }

        ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(openBaseName));
        key.type = ZR_VALUE_TYPE_STRING;
        value = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, state->global->zrObject.value.object), &key);
        qualifiedPrototype = native_binding_value_as_object_prototype(state, value);
    }

    return qualifiedPrototype;
}

/* 按名称构造声明类型的实例；原型解析失败时由底层构造器返回空。 */
SZrObject *ZrLib_Type_NewInstance(SZrState *state, const TZrChar *typeName) {
    return native_binding_new_instance_with_prototype(state, ZrLib_Type_FindPrototype(state, typeName));
}

/* 已有目标原型的回调绕过名称查询，以保留继承链上的实际构造目标。 */
SZrObject *ZrLib_Type_NewInstanceWithPrototype(SZrState *state, SZrObjectPrototype *prototype) {
    return native_binding_new_instance_with_prototype(state, prototype);
}

/* 只查询 VM 模块缓存，不触发插件加载或模块执行。 */
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

/* 模块导出解析可按需触发原生模块导入；结果借用自模块对象。 */
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

/* 宿主调用模块导出时的线程局部 panic 边界，支持同线程嵌套恢复。 */
typedef struct ZrLibPanicRecoverContext {
    jmp_buf jumpBuffer;
    FZrPanicHandlingFunction previousHandler;
    SZrState *state;
    EZrThreadStatus status;
    struct ZrLibPanicRecoverContext *previous;
    TZrBool triggered;
} ZrLibPanicRecoverContext;

/* 仅当前线程可见；恢复时必须连同 global 的 handler 一起还原。 */
static ZR_LIB_THREAD_LOCAL ZrLibPanicRecoverContext *g_zr_lib_panic_recover_context = ZR_NULL;

/* 宿主 API 将无异常的失败转换成规范化 VM 错误，避免只返回 false 丢失诊断。 */
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

/* 只截获属于当前 state 的未处理 panic；其他状态转交原宿主 handler。 */
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

/* 从 C 回调同步调用任意 VM 可调用值；借用参数先搬入受 VM 扫描的 scratch 栈。 */
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

/* 宿主按模块/导出调用 VM；临时 panic 边界将未处理异常转为 false 与状态。 */
/* TODO: panic handler 用 longjmp 穿过可能仍活动的 native GC 域和临时 pin；
 * 核查 VM 未处理异常的展开路径是否已在进入 handler 前释放这些资源。 */
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
