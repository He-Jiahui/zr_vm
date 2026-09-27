//
// Internal native binding dispatch-lane helpers.
//

#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_DISPATCH_LANES_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_DISPATCH_LANES_H

#include "native_binding/native_binding_internal.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/ownership.h"

/* 已由所有权控制块固定的对象无需再注册一次 GC 忽略。 */
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
    return control != ZR_NULL &&
           control->object == object &&
           control->objectIsAlive &&
           !control->dropInProgress &&
           control->ownsGcIgnore;
}

/* 稳定副本离开 VM 栈根后，由调用者记住新增 pin 并在回调结束时撤销。 */
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

/* 只释放本轮调用新加的 GC 忽略，保留已有的宿主或所有权固定。 */
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

/* 需要语义复制时检查 VM 状态，阻止带异常的值进入 native 回调。 */
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

/* 非纯堆对象的复制可能有对象语义，不允许只复制指针位。 */
static ZR_FORCE_INLINE TZrBool native_binding_value_requires_cloned_stable_copy_inline(SZrState *state,
                                                                                        const SZrTypeValue *value) {
    SZrObject *object;

    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || !value->isGarbageCollectable ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    return object != ZR_NULL &&
           !ZrCore_Value_CanFastCopyPlainHeapObject(state, value);
}

/* 所有权句柄和带复制语义的对象必须走可释放的稳定副本路径。 */
static ZR_FORCE_INLINE TZrBool native_binding_value_can_shallow_stable_copy_inline(SZrState *state,
                                                                                    const SZrTypeValue *value) {
    if (value == ZR_NULL || value->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE || value->ownershipControl != ZR_NULL ||
        value->ownershipWeakRef != ZR_NULL) {
        return ZR_FALSE;
    }

    return !native_binding_value_requires_cloned_stable_copy_inline(state, value);
}

/* 回调期间提供稳定值视图，needsRelease 标明是否生成独立的所有权引用。 */
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

/* 与 prepare 的语义复制成对，浅副本仍由原 VM 栈槽持有。 */
static ZR_FORCE_INLINE void native_binding_release_stable_value_raw_inline(SZrState *state,
                                                                           SZrTypeValue *value,
                                                                           TZrBool *needsRelease) {
    if (state == ZR_NULL || value == ZR_NULL || needsRelease == ZR_NULL || !*needsRelease) {
        return;
    }

    ZrCore_Ownership_ReleaseValue(state, value);
    *needsRelease = ZR_FALSE;
}

/* self 与参数共用相同稳定副本协议，供外部注册入口和 lane 调用。 */
static ZR_FORCE_INLINE TZrBool native_binding_prepare_stable_value_inline(SZrState *state,
                                                                          ZrLibStableValueCopy *copy,
                                                                          const SZrTypeValue *source) {
    if (copy == ZR_NULL) {
        return ZR_FALSE;
    }

    return native_binding_prepare_stable_value_raw_inline(state, &copy->value, &copy->needsRelease, source);
}

/* 释放临时稳定副本持有的所有权，不改变原 VM 参数。 */
static ZR_FORCE_INLINE void native_binding_release_stable_value_inline(SZrState *state, ZrLibStableValueCopy *copy) {
    if (copy == ZR_NULL) {
        return;
    }

    native_binding_release_stable_value_raw_inline(state, &copy->value, &copy->needsRelease);
}

/* 浅副本继续由原调用栈保活，只有克隆值需要额外 GC pin。 */
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

/* 按解析后的绑定种类取描述符回调，供所有 lane 共享。 */
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

/* 描述符声明的调度标志决定 safepoint 模式及 lane 准入条件。 */
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

/* 回调的 GC 域模式只能取一个有效 safepoint 策略。 */
static ZR_FORCE_INLINE TZrBool native_binding_dispatch_native_mode_inline(
        TZrUInt32 dispatchFlags,
        EZrGcNativeSafepointMode *outMode) {
    TZrUInt32 modeFlags;

    if (outMode == ZR_NULL) {
        return ZR_FALSE;
    }
    modeFlags = dispatchFlags &
                (TZrUInt32)ZR_LIB_NATIVE_DISPATCH_FLAG_SAFEPOINT_MODE_MASK;
    if (modeFlags == 0u) {
        *outMode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
        return ZR_TRUE;
    }
    if (modeFlags ==
        (TZrUInt32)ZR_LIB_NATIVE_DISPATCH_FLAG_BLOCKING_DETACHED) {
        *outMode = ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED;
        return ZR_TRUE;
    }
    if (modeFlags ==
        (TZrUInt32)ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SAFEPOINT_CRITICAL) {
        *outMode = ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

/* 所有 lane 经此进入/退出 GC native 域，回调不得在域外访问受约束的 VM 状态。 */
static ZR_FORCE_INLINE TZrBool native_binding_invoke_callback_inline(
        SZrState *state,
        FZrLibBoundCallback callback,
        TZrUInt32 dispatchFlags,
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    EZrGcNativeSafepointMode mode;
    TZrBool success;

    if (state == ZR_NULL || callback == ZR_NULL || context == ZR_NULL ||
        !native_binding_dispatch_native_mode_inline(dispatchFlags, &mode) ||
        !ZrCore_GcDomain_NativeEnter(state, mode)) {
        return ZR_FALSE;
    }
    success = callback(context, result);
    ZrCore_GcDomain_NativeLeave(state);
    return success;
}

/* 从绑定条目一并传入回调和 safepoint 标志，避免 lane 自行解释描述符。 */
static ZR_FORCE_INLINE TZrBool native_binding_invoke_entry_callback_inline(
        SZrState *state,
        const ZrLibBindingEntry *entry,
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    return native_binding_invoke_callback_inline(
            state,
            native_binding_entry_callback_inline(entry),
            native_binding_entry_dispatch_flags_inline(entry),
            context,
            result);
}

/* 固定参数数目是免分配 lane 的前提；可变范围留给通用分派。 */
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

/* self 无外部所有权句柄时可由栈根配合浅拷贝进入最快路径。 */
static ZR_FORCE_INLINE TZrBool native_binding_value_can_use_fast_lane_self_inline(const SZrTypeValue *value) {
    return value == ZR_NULL ||
           (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
            value->ownershipControl == ZR_NULL &&
            value->ownershipWeakRef == ZR_NULL);
}

/* 快路只接受无 GC/所有权处理的标量参数。 */
static ZR_FORCE_INLINE TZrBool native_binding_value_can_use_fast_lane_argument_inline(const SZrTypeValue *value) {
    return value != ZR_NULL &&
           !value->isGarbageCollectable &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
           value->ownershipControl == ZR_NULL &&
           value->ownershipWeakRef == ZR_NULL;
}

/* TODO: 该栈锚点初始化 helper 当前无调用点；核查是否仍属于计划中的 lane 接口。
 * 实际栈根入口使用 adopt_stack_layout_anchor_inline 复用已有锚点。 */
static ZR_FORCE_INLINE void native_binding_context_enable_stack_layout_anchor_inline(ZrLibCallContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL || context->functionBase == ZR_NULL) {
        return;
    }

    ZrCore_Function_StackAnchorInit(context->state, context->functionBase, &context->functionBaseAnchor);
    context->stackBasePointer = context->state->stackBase.valuePointer;
    if (context->inlineFrameBase == ZR_NULL) {
        context->inlineFrameBase = context->functionBase + 1;
    }
    context->stackLayoutAnchored = ZR_TRUE;
}

/* lane 已有锚点时复用它，减少热路径重复建立锚点。 */
static ZR_FORCE_INLINE void native_binding_context_adopt_stack_layout_anchor_inline(
        ZrLibCallContext *context,
        const SZrFunctionStackAnchor *functionBaseAnchor) {
    if (context == ZR_NULL || context->state == ZR_NULL || context->functionBase == ZR_NULL || functionBaseAnchor == ZR_NULL) {
        return;
    }

    context->functionBaseAnchor = *functionBaseAnchor;
    context->stackBasePointer = context->state->stackBase.valuePointer;
    if (context->inlineFrameBase == ZR_NULL) {
        context->inlineFrameBase = context->functionBase + 1;
    }
    context->stackLayoutAnchored = ZR_TRUE;
}

/* inline 参数帧借用函数基址，但不把稳定副本布局误标为原栈参数布局。 */
static ZR_FORCE_INLINE void native_binding_context_adopt_inline_frame_anchor_inline(
        ZrLibCallContext *context,
        const SZrFunctionStackAnchor *functionBaseAnchor) {
    if (context == ZR_NULL || context->state == ZR_NULL || context->functionBase == ZR_NULL ||
        functionBaseAnchor == ZR_NULL) {
        return;
    }

    context->functionBaseAnchor = *functionBaseAnchor;
    context->stackBasePointer = context->state->stackBase.valuePointer;
    if (context->inlineFrameBase == ZR_NULL) {
        context->inlineFrameBase = context->functionBase + 1;
    }
}

/* 栈基址变化后重新解析 self/参数位置，作废旧的原栈视图。 */
static ZR_FORCE_INLINE void native_binding_context_refresh_stack_layout_inline(ZrLibCallContext *context) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer oldFunctionBase;

    if (context == ZR_NULL || !context->stackLayoutAnchored || context->state == ZR_NULL ||
        context->stackBasePointer == context->state->stackBase.valuePointer) {
        return;
    }

    oldFunctionBase = context->functionBase;
    functionBase = ZrCore_Function_StackAnchorRestore(context->state, &context->functionBaseAnchor);
    if (functionBase == ZR_NULL) {
        return;
    }

    context->functionBase = functionBase;
    if (context->inlineFrameBase == oldFunctionBase + 1) {
        context->inlineFrameBase = functionBase + 1;
    }
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

/* 即使参数已复制，inline struct 原始帧跨度也必须随扩栈恢复。 */
static ZR_FORCE_INLINE void native_binding_context_refresh_inline_frame_layout_inline(ZrLibCallContext *context) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer oldFunctionBase;

    if (context == ZR_NULL || context->stackLayoutAnchored || context->state == ZR_NULL ||
        context->stackBasePointer == ZR_NULL || context->stackBasePointer == context->state->stackBase.valuePointer ||
        context->inlineFrameFunction == ZR_NULL) {
        return;
    }

    oldFunctionBase = context->functionBase;
    functionBase = ZrCore_Function_StackAnchorRestore(context->state, &context->functionBaseAnchor);
    if (functionBase == ZR_NULL) {
        return;
    }

    context->functionBase = functionBase;
    if (context->inlineFrameBase == ZR_NULL || context->inlineFrameBase == oldFunctionBase + 1) {
        context->inlineFrameBase = functionBase + 1;
    }
    context->stackBasePointer = context->state->stackBase.valuePointer;
}

/* 某些回调重绑 self，lane 完成后把结果写回原 receiver 栈槽。 */
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

/* 固定短参数且无 GC/所有权副作用时，可跳过稳定副本与 pin。 */
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

/* 显式 opt-in 的回调直接使用可刷新 VM 栈视图作为 GC 根。 */
static ZR_FORCE_INLINE TZrBool native_binding_can_use_stack_root_lane_inline(const ZrLibBindingEntry *entry,
                                                                              const ZrLibCallContext *context) {
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);

    return native_binding_entry_callback_inline(entry) != ZR_NULL &&
           (native_binding_entry_dispatch_flags_inline(entry) & ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT) != 0U &&
           context->functionBase != ZR_NULL;
}

/* 少量复杂参数在 C 栈上构建稳定值，避免通用路径的 scratch 帧分配。 */
static ZR_FORCE_INLINE TZrBool native_binding_can_use_inline_pinned_lane_inline(const ZrLibBindingEntry *entry,
                                                                                 const ZrLibCallContext *context) {
    ZR_ASSERT(entry != ZR_NULL);
    ZR_ASSERT(context != ZR_NULL);

    return native_binding_entry_callback_inline(entry) != ZR_NULL &&
           context->argumentCount <= ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY;
}

/** @brief 在原 VM 栈上执行已声明栈根语义的原生回调。
 * @pre entry 的回调存在，且 context 的函数基址锚点对应当前调用帧。 */
TZrBool native_binding_dispatch_stack_root_lane(SZrState *state,
                                                const ZrLibBindingEntry *entry,
                                                ZrLibCallContext *context,
                                                const SZrFunctionStackAnchor *functionBaseAnchor,
                                                SZrTypeValue *result);
/** @brief 供直接回调和测试复用的 GC-aware 栈根入口。 */
ZR_LIBRARY_API TZrBool native_binding_dispatch_stack_root_callback_lane(
        SZrState *state,
        FZrLibBoundCallback callback,
        ZrLibCallContext *context,
        const SZrFunctionStackAnchor *functionBaseAnchor,
        SZrTypeValue *result);
/** @brief 调度无 GC 参数及无所有权 self 的短固定参数调用。 */
ZR_LIBRARY_API TZrBool native_binding_dispatch_fast_lane(SZrState *state,
                                          const ZrLibBindingEntry *entry,
                                          ZrLibCallContext *context,
                                          const SZrFunctionStackAnchor *functionBaseAnchor,
                                          SZrTypeValue *result);
/** @brief 调度少量需要所有权复制或 pin 的参数，并在退出前成对清理。 */
TZrBool native_binding_dispatch_inline_pinned_lane(SZrState *state,
                                                   const ZrLibBindingEntry *entry,
                                                   ZrLibCallContext *context,
                                                   const SZrFunctionStackAnchor *functionBaseAnchor,
                                                   SZrTypeValue *result);

#endif // ZR_VM_LIBRARY_NATIVE_BINDING_DISPATCH_LANES_H
