//
// Created by HeJiahui on 2025/7/15.
//
#include "zr_vm_core/closure.h"

#include "closure_close_proxy_token.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/state.h"
/* 待关闭栈槽链的 offset 字段所能表示的最大向后跨度。 */
#define MAX_DELTA ((256UL << ((sizeof(state->stackBase.valuePointer->toBeClosedValueOffset) - 1) * 8)) - 1)
/* 无待关闭登记项时的统一返回值。 */
#define ZR_CLOSURE_CLOSED_COUNT_NONE ((TZrSize)0)

/* 同一捕获被多个闭包引用时，保留覆盖范围最外层的有效深度。 */
static TZrUInt32 closure_merge_scope_depth(TZrUInt32 currentScopeDepth, TZrUInt32 incomingScopeDepth) {
    if (currentScopeDepth == ZR_GC_SCOPE_DEPTH_NONE) {
        return incomingScopeDepth;
    }
    if (incomingScopeDepth == ZR_GC_SCOPE_DEPTH_NONE) {
        return currentScopeDepth;
    }
    return currentScopeDepth < incomingScopeDepth ? currentScopeDepth : incomingScopeDepth;
}

/* 分配或 GC 后刷新可能已搬迁对象的临时原始指针。 */
static ZR_FORCE_INLINE SZrRawObject *closure_refresh_forwarded_raw_object(SZrRawObject *rawObject) {
    SZrRawObject *forwardedObject;

    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    forwardedObject = (SZrRawObject *)rawObject->garbageCollectMark.forwardingAddress;
    return forwardedObject != ZR_NULL ? forwardedObject : rawObject;
}

/* 函数元数据随对象搬迁时，以转发地址恢复借用指针。 */
static ZR_FORCE_INLINE SZrFunction *closure_refresh_forwarded_function(SZrFunction *function) {
    return function != ZR_NULL ? (SZrFunction *)closure_refresh_forwarded_raw_object(
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(function))
                               : ZR_NULL;
}

/* 闭包分配后恢复局部引用；调用方随后再发布到栈根。 */
static ZR_FORCE_INLINE SZrClosure *closure_refresh_forwarded_closure(SZrClosure *closure) {
    return closure != ZR_NULL ? (SZrClosure *)closure_refresh_forwarded_raw_object(
                                        ZR_CAST_RAW_OBJECT_AS_SUPER(closure))
                              : ZR_NULL;
}

/* 捕获单元被搬迁后恢复从父闭包借出的指针。 */
static ZR_FORCE_INLINE SZrClosureValue *closure_refresh_forwarded_closure_value(SZrClosureValue *closureValue) {
    return closureValue != ZR_NULL ? (SZrClosureValue *)closure_refresh_forwarded_raw_object(
                                             ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue))
                                   : ZR_NULL;
}

/* 通过父帧的 callable 槽重新取得捕获数组，避免保存搬迁前的内存地址。 */
static ZR_FORCE_INLINE SZrClosureValue **closure_refresh_parent_closure_values_from_base(SZrState *state,
                                                                                          TZrStackValuePointer base) {
    SZrTypeValue *ownerValue;
    SZrClosure *ownerClosure;

    if (state == ZR_NULL || base == ZR_NULL || base <= state->stackBase.valuePointer) {
        return ZR_NULL;
    }

    ownerValue = ZrCore_Stack_GetValueNoProfile(base - 1);
    if (ownerValue == ZR_NULL ||
        ownerValue->type != ZR_VALUE_TYPE_CLOSURE ||
        ownerValue->isNative ||
        ownerValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    ownerClosure = closure_refresh_forwarded_closure(ZR_CAST_VM_CLOSURE(state, ownerValue->value.object));
    return ownerClosure != ZR_NULL ? ownerClosure->closureValuesExtend : ZR_NULL;
}

/* 闭包创建时用父帧 callable 取得槽布局元数据。 */
static ZR_FORCE_INLINE SZrFunction *closure_metadata_function_from_frame_base(SZrState *state,
                                                                              TZrStackValuePointer base) {
    if (state == ZR_NULL || base == ZR_NULL || base <= state->stackBase.valuePointer) {
        return ZR_NULL;
    }

    return closure_refresh_forwarded_function(
            ZrCore_Closure_GetMetadataFunctionFromValue(state, ZrCore_Stack_GetValueNoProfile(base - 1)));
}

/* 生成帧的逻辑栈槽可映射到独立物理存储；无布局时沿用普通栈。 */
static TZrStackValuePointer closure_value_pointer_for_frame_slot(SZrState *state,
                                                                 const SZrFunction *function,
                                                                 TZrStackValuePointer base,
                                                                 TZrUInt32 stackSlot) {
    const SZrFunctionFrameSlotLayout *slotLayout;
    SZrStackFramePlace place;

    if (base == ZR_NULL) {
        return ZR_NULL;
    }
    if (state == ZR_NULL ||
        function == ZR_NULL ||
        function->frameSlotLayouts == ZR_NULL ||
        function->frameSlotLayoutLength == 0u) {
        return base + stackSlot;
    }

    slotLayout = ZrCore_Function_FindFrameSlotLayout(function, stackSlot);
    if (slotLayout != ZR_NULL &&
        slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
        slotLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue) &&
        ZrCore_Function_MakeFrameSlotPlace(state, function, base, stackSlot, &place)) {
        return ZR_CAST_STACK_VALUE(place.address);
    }

    return base + stackSlot;
}

/* 定位待关闭登记槽对应的物理 owner，避免镜像值与物理值重复释放。 */
static SZrTypeValue *closure_registered_mirror_frame_value(
        SZrState *state,
        TZrStackValuePointer registeredPointer,
        TZrBool vmFramesOnly) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || registeredPointer == ZR_NULL) {
        return ZR_NULL;
    }

    for (callInfo = state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        SZrFunction *function;
        TZrStackValuePointer frameBase;
        TZrStackValuePointer physicalPointer;
        TZrSize stackSlot;

        if (callInfo->functionBase.valuePointer == ZR_NULL ||
            (vmFramesOnly && !ZR_CALL_INFO_IS_VM(callInfo))) {
            continue;
        }
        function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
        if (function == ZR_NULL) {
            continue;
        }
        frameBase = callInfo->functionBase.valuePointer + 1;
        if (registeredPointer < frameBase ||
            registeredPointer >= callInfo->functionTop.valuePointer) {
            continue;
        }

        /* Generated frames register their separate physical owner storage. */
        if (registeredPointer >= frameBase + function->stackSize) {
            for (TZrUInt32 index = 0u; index < function->frameSlotLayoutLength; ++index) {
                const SZrFunctionFrameSlotLayout *layout = &function->frameSlotLayouts[index];
                if (layout->stackSlot < function->stackSize &&
                    closure_value_pointer_for_frame_slot(
                            state, function, frameBase, layout->stackSlot) == registeredPointer) {
                    return ZrCore_Stack_GetValueNoProfile(frameBase + layout->stackSlot);
                }
            }
            continue;
        }

        stackSlot = (TZrSize)(registeredPointer - frameBase);
        if (stackSlot > UINT32_MAX) {
            return ZR_NULL;
        }
        physicalPointer = closure_value_pointer_for_frame_slot(
                state, function, frameBase, (TZrUInt32)stackSlot);
        return physicalPointer != ZR_NULL
                       ? ZrCore_Stack_GetValueNoProfile(physicalPointer)
                       : ZR_NULL;
    }

    return ZR_NULL;
}

/* 所有权值即使没有 CLOSE 元方法也必须走退出作用域清理。 */
static ZR_FORCE_INLINE TZrBool closure_value_is_ownership_cleanup_value(
        const SZrTypeValue *value) {
    return (TZrBool)(value != ZR_NULL &&
                     (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
                      value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED ||
                      value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK ||
                      value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_LOANED));
}

static TZrBool closure_value_needs_proxy_cleanup(SZrState *state,
                                                 SZrTypeValue *value) {
    const SZrMeta *meta;

    if (value == ZR_NULL || ZR_VALUE_IS_TYPE_NULL(value->type)) {
        return ZR_FALSE;
    }
    if (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_BORROWED ||
        closure_value_is_ownership_cleanup_value(value)) {
        return ZR_TRUE;
    }
    meta = ZrCore_Value_GetMeta(state, value, ZR_META_CLOSE);
    return (TZrBool)(meta != ZR_NULL && meta->function != ZR_NULL);
}

/* 无控制块的 UNIQUE/LOANED 镜像可通过对象地址判定直接别名。 */
static ZR_FORCE_INLINE TZrBool closure_value_is_direct_owner_alias(
        const SZrTypeValue *value) {
    return (TZrBool)(value != ZR_NULL &&
                     value->ownershipControl == ZR_NULL &&
                     value->value.object != ZR_NULL &&
                     (value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
                      value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_LOANED));
}

/* 单元关闭后才有内嵌值；锚定逃逸需在此时补传给该值。 */
static void closure_value_apply_anchored_escape_to_closed_value(SZrState *state, SZrClosureValue *closureValue) {
    TZrUInt32 propagatedEscapeFlags;

    if (state == ZR_NULL || closureValue == ZR_NULL || !ZrCore_ClosureValue_IsClosed(closureValue) ||
        closureValue->anchoredEscapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return;
    }

    propagatedEscapeFlags = closureValue->captureEscapeFlags | closureValue->anchoredEscapeFlags;
    ZrCore_GarbageCollector_MarkValueEscaped(state,
                                             &closureValue->link.closedValue,
                                             propagatedEscapeFlags,
                                             closureValue->captureScopeDepth,
                                             (EZrGarbageCollectPromotionReason)closureValue->anchoredPromotionReason);
}

/* 两组尾数组共享一次 GC 对象分配，初始化的 null 槽不形成 GC 边。 */
SZrClosureNative *ZrCore_ClosureNative_New(struct SZrState *state, TZrSize closureValueCount) {
    TZrSize extraCaptureCount = closureValueCount > 1 ? closureValueCount - 1 : 0;
    TZrSize extraOwnerBytes = closureValueCount * sizeof(SZrRawObject *);
    SZrRawObject *object =
            ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE,
                                 sizeof(SZrClosureNative) + extraCaptureCount * sizeof(SZrTypeValue *) + extraOwnerBytes,
                                 ZR_TRUE);
    SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
    closure->nativeFunction = ZR_NULL;
    closure->aotShimFunction = ZR_NULL;
    closure->nativeBindingLookupIndex = ZR_MAX_SIZE;
    closure->callBindingGeneration = 1u;
    closure->nativeBindingDescriptor = ZR_NULL;
    closure->nativeBindingModuleDescriptor = ZR_NULL;
    closure->nativeBindingTypeDescriptor = ZR_NULL;
    closure->nativeBindingOwnerPrototype = ZR_NULL;
    closure->nativeBindingKind = 0u;
    closure->nativeBindingUsesReceiver = ZR_NATIVE_BINDING_RECEIVER_NONE;
    ZrCore_Memory_RawSet(&closure->nativeBindingDirectDispatch, 0, sizeof(closure->nativeBindingDirectDispatch));
    closure->closureValueCount = closureValueCount;
    if (closureValueCount > 0) {
        ZrCore_Memory_RawSet(closure->closureValuesExtend, 0, sizeof(SZrClosureValue *) * closureValueCount);
        ZrCore_Memory_RawSet(ZrCore_ClosureNative_GetCaptureOwners(closure), 0, extraOwnerBytes);
    }
    return closure;
}

/* 最少保留一个尾槽的结构体布局；零捕获也可创建 stateless 闭包。 */
SZrClosure *ZrCore_Closure_New(struct SZrState *state, TZrSize closureValueCount) {
    // SZrClosure 已经包含了 closureValuesExtend[1]，所以只需要分配 (closureValueCount - 1) 个额外的指针
    TZrSize extraSize = closureValueCount > 1 ? (closureValueCount - 1) * sizeof(SZrClosureValue *) : 0;
    SZrRawObject *object = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE,
                                          sizeof(SZrClosure) + extraSize, ZR_FALSE);
    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);
    closure->closureValueCount = closureValueCount;
    closure->function = ZR_NULL;
    if (closureValueCount > 0) {
        ZrCore_Memory_RawSet(closure->closureValuesExtend, 0, sizeof(SZrClosureValue *) * closureValueCount);
    }
    return closure;
}

/* 每个槽获得独立的已关闭单元，并以写屏障建立闭包到单元的 GC 边。 */
void ZrCore_Closure_InitValue(struct SZrState *state, SZrClosure *closure) {
    /* TODO: AOT shim 投影在创建 closure 后立即调用此处，发布到 projectedSelfValue 之前
     * 尚无显式 GC 根；分配失败可触发完整 GC。需用分配失败注入核实该路径能否回收未锚定闭包。 */
    for (TZrSize i = 0; i < closure->closureValueCount; i++) {
        SZrRawObject *rawObject = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
        SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
        // if value is on stack
        closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(&closureValue->link.closedValue);
        ZrCore_Value_ResetAsNull(&closureValue->value.valuePointer->value);
        closureValue->captureScopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
        closureValue->captureEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
        closureValue->anchoredEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
        closureValue->anchoredPromotionReason = (TZrUInt32)ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;
        closure->closureValuesExtend[i] = closureValue;
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure), ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue));
    }
}

/* 插入线程开放链表；GC 只搬迁已关闭单元，链内 previous 地址保持有效。 */
static SZrClosureValue *closure_value_new(struct SZrState *state, TZrStackValuePointer stackPointer,
                                          SZrClosureValue **previous) {
    SZrRawObject *rawObject = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
    SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
    SZrClosureValue *next = *previous;
    closureValue->value.valuePointer = stackPointer;
    closureValue->link.next = next;
    closureValue->link.previous = previous;
    closureValue->captureScopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
    closureValue->captureEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
    closureValue->anchoredEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
    closureValue->anchoredPromotionReason = (TZrUInt32)ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;
    if (next) {
        next->link.previous = &closureValue->link.next;
    }
    *previous = closureValue;
    if (!ZrCore_State_IsInClosureValueThreadList(state)) {
        state->threadWithStackClosures = state->global->threadWithStackClosures;
        state->global->threadWithStackClosures = state;
    }
    return closureValue;
}

/* 共享同一栈槽的 upvalue；链表按栈地址降序，以便退栈时从头关闭。 */
SZrClosureValue *ZrCore_Closure_FindOrCreateValue(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrClosureValue **closureValues = &state->stackClosureValueList;
    SZrClosureValue *closureValue = ZR_NULL;
    ZR_ASSERT(ZrCore_State_IsInClosureValueThreadList(state) || state->stackClosureValueList == ZR_NULL);
    while (ZR_TRUE) {
        closureValue = *closureValues;
        if (closureValue == ZR_NULL) {
            break;
        }
        ZR_ASSERT(!ZrCore_ClosureValue_IsClosed(closureValue));
        if (closureValue->value.valuePointer < stackPointer) {
            break;
        }
        // Open upvalues are anchored by the thread closure list and may survive GC cycles.
        ZR_ASSERT(!ZrCore_Gc_RawObjectIsDead(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue)));
        if (closureValue->value.valuePointer == stackPointer) {
            return closureValue;
        }
        closureValues = &closureValue->link.next;
    }
    return closure_value_new(state, stackPointer, closureValues);
}

/* 有序链表的第一个低于范围起点的槽之后无需继续扫描。 */
TZrBool ZrCore_Closure_HasOpenStackValueInRange(const struct SZrState *state,
                                                TZrStackValuePointer stackStart,
                                                TZrStackValuePointer stackEnd) {
    SZrClosureValue *closureValue;

    if (state == ZR_NULL || stackStart == ZR_NULL || stackEnd == ZR_NULL || stackStart >= stackEnd) {
        return ZR_FALSE;
    }

    closureValue = state->stackClosureValueList;
    while (closureValue != ZR_NULL) {
        TZrStackValuePointer valuePointer = closureValue->value.valuePointer;

        ZR_ASSERT(!ZrCore_ClosureValue_IsClosed(closureValue));
        if (valuePointer < stackStart) {
            break;
        }
        if (valuePointer < stackEnd) {
            return ZR_TRUE;
        }
        closureValue = closureValue->link.next;
    }

    return ZR_FALSE;
}

/* 捕获元数据来自函数声明，多个闭包可共享并合并到一个单元。 */
void ZrCore_ClosureValue_SetCaptureMetadata(SZrClosureValue *closureValue,
                                            TZrUInt32 scopeDepth,
                                            TZrUInt32 escapeFlags) {
    if (closureValue == ZR_NULL) {
        return;
    }

    closureValue->captureScopeDepth = closure_merge_scope_depth(closureValue->captureScopeDepth, scopeDepth);
    closureValue->captureEscapeFlags |= escapeFlags;
}

/* 对开放单元记下逃逸要求，待关闭时再应用于复制出的内嵌值。 */
void ZrCore_ClosureValue_AnchorEscape(SZrState *state,
                                      SZrClosureValue *closureValue,
                                      TZrUInt32 escapeFlags,
                                      EZrGarbageCollectPromotionReason promotionReason) {
    if (state == ZR_NULL || closureValue == ZR_NULL || escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return;
    }

    closureValue->anchoredEscapeFlags |= escapeFlags;
    if (closureValue->anchoredPromotionReason == (TZrUInt32)ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE ||
        closureValue->anchoredPromotionReason == (TZrUInt32)ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL) {
        closureValue->anchoredPromotionReason = (TZrUInt32)promotionReason;
    }

    ZrCore_GarbageCollector_MarkRawObjectEscaped(state,
                                                 ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue),
                                                 escapeFlags,
                                                 closureValue->captureScopeDepth,
                                                 promotionReason);
    closure_value_apply_anchored_escape_to_closed_value(state, closureValue);
}

/* VM 捕获与原生 owner 捕获均传播到共享单元；直接捕获则标记其值。 */
void ZrCore_Closure_PropagateEscapeFromObject(SZrState *state,
                                              SZrRawObject *closureObject,
                                              TZrUInt32 escapeFlags,
                                              EZrGarbageCollectPromotionReason promotionReason) {
    if (state == ZR_NULL || closureObject == ZR_NULL || escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE ||
        closureObject->type != ZR_RAW_OBJECT_TYPE_CLOSURE) {
        return;
    }

    if (!closureObject->isNative) {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, closureObject);
        if (closure == ZR_NULL) {
            return;
        }

        for (TZrUInt32 captureIndex = 0; captureIndex < closure->closureValueCount; captureIndex++) {
            SZrClosureValue *closureValue = closure->closureValuesExtend[captureIndex];

            if (closureValue == ZR_NULL) {
                continue;
            }

            if (closure->function != ZR_NULL &&
                closure->function->closureValueList != ZR_NULL &&
                captureIndex < closure->function->closureValueLength) {
                const SZrFunctionClosureVariable *closureVariable = &closure->function->closureValueList[captureIndex];
                ZrCore_ClosureValue_SetCaptureMetadata(closureValue,
                                                      closureVariable->scopeDepth,
                                                      closureVariable->escapeFlags);
            }

            ZrCore_ClosureValue_AnchorEscape(state, closureValue, escapeFlags, promotionReason);
        }
        return;
    }

    {
        SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, closureObject);
        SZrFunction *metadataFunction = closure != ZR_NULL ? closure->aotShimFunction : ZR_NULL;
        TZrUInt32 captureCount = closure != ZR_NULL ? (TZrUInt32)closure->closureValueCount : 0u;

        for (TZrUInt32 captureIndex = 0; captureIndex < captureCount; captureIndex++) {
            TZrUInt32 captureScopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
            TZrUInt32 captureEscapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
            SZrRawObject *captureOwner;
            SZrTypeValue *captureValue;

            if (metadataFunction != ZR_NULL &&
                metadataFunction->closureValueList != ZR_NULL &&
                captureIndex < metadataFunction->closureValueLength) {
                captureScopeDepth = metadataFunction->closureValueList[captureIndex].scopeDepth;
                captureEscapeFlags = metadataFunction->closureValueList[captureIndex].escapeFlags;
            }

            captureOwner = ZrCore_ClosureNative_GetCaptureOwner(closure, captureIndex);
            captureValue = ZrCore_ClosureNative_GetCaptureValue(closure, captureIndex);
            if (captureOwner != ZR_NULL && captureOwner->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE) {
                SZrClosureValue *closureValue = (SZrClosureValue *)captureOwner;
                ZrCore_ClosureValue_SetCaptureMetadata(closureValue, captureScopeDepth, captureEscapeFlags);
                ZrCore_ClosureValue_AnchorEscape(state, closureValue, escapeFlags, promotionReason);
            } else if (captureValue != ZR_NULL) {
                ZrCore_GarbageCollector_MarkValueEscaped(state,
                                                         captureValue,
                                                         escapeFlags | captureEscapeFlags,
                                                         captureScopeDepth,
                                                         promotionReason);
            }
        }
    }
}

/* 待关闭登记接受所有权值，其他值需要具备 CLOSE 元方法。 */
static TZrBool closure_value_check_close_meta(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrTypeValue *stackValue = ZrCore_Stack_GetValue(stackPointer);
    TZrMemoryOffset ignoredSourceOffset;

    if (ZrCore_ClosureProxyToken_GetSourceOffset(state, stackPointer, &ignoredSourceOffset)) {
        return ZR_TRUE;
    }
    if (stackValue != ZR_NULL &&
        (stackValue->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE ||
         stackValue->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_SHARED ||
         stackValue->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_WEAK ||
         stackValue->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_LOANED)) {
        return ZR_TRUE;
    }
    // todo: if it is a basic type
    SZrMeta *meta = ZrCore_Value_GetMeta(state, stackValue, ZR_META_CLOSE);
    return meta != ZR_NULL;
}

/* 先释放镜像帧的物理 owner，再处理登记槽；普通值调用 CLOSE 元方法。 */
static void closure_value_call_close_meta(SZrState *state,
                                          TZrStackPointer stackPointer,
                                          EZrThreadStatus errorStatus,
                                          TZrBool isYield,
                                          TZrBool consumeStagedReceiver) {
    TZrStackPointer top = state->stackTop;
    SZrCallInfo *callInfo = state->callInfoList;
    TZrMemoryOffset valueOffset = ZrCore_Stack_SavePointerAsOffset(
            state, stackPointer.valuePointer);
    SZrTypeValue *registeredValue = &stackPointer.valuePointer->value;
    SZrTypeValue *physicalValue = consumeStagedReceiver
                                          ? ZR_NULL
                                          : closure_registered_mirror_frame_value(
                                                    state, stackPointer.valuePointer, ZR_FALSE);
    TZrBool hasDistinctPhysicalValue =
            (TZrBool)(physicalValue != ZR_NULL &&
                      physicalValue != registeredValue &&
                      !ZrCore_Value_SlotsOverlapNoProfile(
                              physicalValue, registeredValue));
    SZrTypeValue *value = registeredValue;

    /* 同一控制块的两个槽各自持有引用；直接别名只释放物理 owner 一次。 */
    if (closure_value_is_ownership_cleanup_value(registeredValue)) {
        TZrBool sharesRetainedOwnershipControl =
                (TZrBool)(hasDistinctPhysicalValue &&
                          closure_value_is_ownership_cleanup_value(physicalValue) &&
                          registeredValue->ownershipControl != ZR_NULL &&
                          physicalValue->ownershipControl == registeredValue->ownershipControl);
        TZrBool aliasesDirectOwner =
                (TZrBool)(hasDistinctPhysicalValue &&
                          closure_value_is_direct_owner_alias(registeredValue) &&
                          closure_value_is_direct_owner_alias(physicalValue) &&
                          physicalValue->value.object == registeredValue->value.object);

        if (sharesRetainedOwnershipControl) {
            ZrCore_Ownership_ReleaseValue(state, physicalValue);
        } else if (aliasesDirectOwner) {
            ZrCore_Value_ResetAsNullNoProfile(registeredValue);
            ZrCore_Ownership_ReleaseValue(state, physicalValue);
            return;
        }
        ZrCore_Ownership_ReleaseValue(state, registeredValue);
        return;
    }
    if (ZR_VALUE_IS_TYPE_NULL(value->type)) {
        return;
    }
    const SZrMeta *meta = ZrCore_Value_GetMeta(state, value, ZR_META_CLOSE);
    if (meta == ZR_NULL || meta->function == ZR_NULL) {
        if (consumeStagedReceiver) {
            ZrCore_Value_ResetAsNullNoProfile(value);
        }
        return;
    }
    top.valuePointer = ZrCore_Function_ReserveScratchSlots(state, 3, top.valuePointer);
    stackPointer.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, valueOffset);
    value = ZrCore_Stack_GetValue(stackPointer.valuePointer);
    ZrCore_Stack_SetRawObjectValue(state, top.valuePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
    ZrCore_Stack_CopyValue(state, top.valuePointer + 1, value);
    if (consumeStagedReceiver) {
        stackPointer.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, valueOffset);
        ZrCore_Value_ResetAsNullNoProfile(ZrCore_Stack_GetValueNoProfile(stackPointer.valuePointer));
    }
    if (errorStatus == ZR_THREAD_STATUS_INVALID) {
        ZrCore_Stack_CopyValue(state, top.valuePointer + 2, &state->global->nullValue);
    } else {
        ZrCore_Exception_MarkError(state, errorStatus, top.valuePointer + 2);
    }
    state->stackTop.valuePointer = top.valuePointer + 3;
    if (callInfo != ZR_NULL && callInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }
    if (isYield) {
        ZrCore_Function_Call(state, top.valuePointer, 0);
    } else {
        ZrCore_Function_CallWithoutYield(state, top.valuePointer, 0);
    }
}

/* 待关闭链表只需转发到统一的元方法与所有权清理路径。 */
/* Consume the logical local and physical mirror before any close callback can re-enter. */
static void closure_value_close_proxy(SZrState *state,
                                      TZrStackPointer proxyPointer,
                                      TZrMemoryOffset sourceOffset,
                                      EZrThreadStatus errorStatus,
                                      TZrBool isYield) {
    TZrMemoryOffset proxyOffset = ZrCore_Stack_SavePointerAsOffset(state, proxyPointer.valuePointer);
    TZrStackValuePointer sourcePointer = ZrCore_Stack_LoadOffsetToPointer(state, sourceOffset);
    SZrTypeValue *source = ZrCore_Stack_GetValueNoProfile(sourcePointer);
    SZrTypeValue *mirror = closure_registered_mirror_frame_value(state, sourcePointer, ZR_TRUE);
    TZrBool distinctMirror = (TZrBool)(mirror != ZR_NULL && mirror != source &&
                                      !ZrCore_Value_SlotsOverlapNoProfile(mirror, source));
    SZrTypeValue *chosen = source;
    SZrTypeValue *other;
    SZrTypeValue *staged = ZrCore_Stack_GetValueNoProfile(proxyPointer.valuePointer);

    if (ZR_VALUE_IS_TYPE_NULL(source->type) && distinctMirror &&
        !ZR_VALUE_IS_TYPE_NULL(mirror->type)) {
        chosen = mirror;
    }
    if (ZR_VALUE_IS_TYPE_NULL(chosen->type)) {
        ZrCore_Value_ResetAsNullNoProfile(staged);
        return;
    }
    other = distinctMirror ? (chosen == source ? mirror : source) : ZR_NULL;
    if (chosen->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_BORROWED) {
        /* OWN_DROP on a borrowed local clears its view without calling @close. */
        ZrCore_Ownership_ReleaseValue(state, chosen);
        if (other != ZR_NULL && !ZR_VALUE_IS_TYPE_NULL(other->type)) {
            ZrCore_Ownership_ReleaseValue(state, other);
        }
        ZrCore_Value_ResetAsNullNoProfile(staged);
        return;
    }
    if (!closure_value_needs_proxy_cleanup(state, chosen) &&
        !closure_value_needs_proxy_cleanup(state, other)) {
        /* A plain using value has no cleanup and stays readable after the scope. */
        ZrCore_Value_ResetAsNullNoProfile(staged);
        return;
    }
    *staged = *chosen;
    ZrCore_Value_ResetAsNullNoProfile(chosen);
    if (other != ZR_NULL && !ZR_VALUE_IS_TYPE_NULL(other->type)) {
        if (closure_value_is_ownership_cleanup_value(other) &&
            !(closure_value_is_direct_owner_alias(other) &&
              closure_value_is_direct_owner_alias(staged) &&
              other->value.object == staged->value.object)) {
            ZrCore_Ownership_ReleaseValue(state, other);
        } else {
            ZrCore_Value_ResetAsNullNoProfile(other);
        }
    }
    proxyPointer.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, proxyOffset);
    closure_value_call_close_meta(state, proxyPointer, errorStatus, isYield, ZR_TRUE);
}

static void closure_value_pre_call_close_meta(SZrState *state, TZrStackPointer stackPointer, EZrThreadStatus errorStatus,
                                            TZrBool isYield) {
    TZrMemoryOffset sourceOffset;

    if (ZrCore_ClosureProxyToken_GetSourceOffset(state, stackPointer.valuePointer, &sourceOffset)) {
        closure_value_close_proxy(state, stackPointer, sourceOffset, errorStatus, isYield);
        return;
    }
    closure_value_call_close_meta(state, stackPointer, errorStatus, isYield, ZR_FALSE);
}


/* 长跨度以零 offset 桥接，非零 offset 指向前一登记值。 */
void ZrCore_Closure_ToBeClosedValueClosureNew(struct SZrState *state, TZrStackValuePointer stackPointer) {
    ZR_ASSERT(stackPointer > state->toBeClosedValueList.valuePointer);
    SZrTypeValue *stackValue = ZrCore_Stack_GetValue(stackPointer);
    if (!ZR_VALUE_IS_TYPE_NULL(stackValue->type) &&
        !closure_value_check_close_meta(state, stackPointer)) {
        return;
    }


    // extends to be closed value list
    while ((TZrSize)(stackPointer - state->toBeClosedValueList.valuePointer) > (TZrSize)MAX_DELTA) {
        state->toBeClosedValueList.valuePointer += MAX_DELTA;
        state->toBeClosedValueList.valuePointer->toBeClosedValueOffset = 0;
    }
    stackPointer->toBeClosedValueOffset = ZR_CAST(TZrUInt32, stackPointer - state->toBeClosedValueList.valuePointer);
    state->toBeClosedValueList.valuePointer = stackPointer;
}

TZrBool ZrCore_Closure_MarkCloseProxy(struct SZrState *state,
                                     TZrStackValuePointer proxySlot,
                                     TZrStackValuePointer sourceSlot) {
    TZrMemoryOffset proxyOffset;

    if (state == ZR_NULL || state->stackBase.valuePointer == ZR_NULL ||
        state->toBeClosedValueList.valuePointer == ZR_NULL ||
        sourceSlot == ZR_NULL || proxySlot == ZR_NULL ||
        sourceSlot < state->stackBase.valuePointer ||
        sourceSlot >= state->stackTop.valuePointer ||
        proxySlot <= sourceSlot ||
        proxySlot <= state->toBeClosedValueList.valuePointer ||
        proxySlot >= state->stackTop.valuePointer ||
        !ZR_VALUE_IS_TYPE_NULL(ZrCore_Stack_GetValueNoProfile(proxySlot)->type)) {
        return ZR_FALSE;
    }

    proxyOffset = ZrCore_Stack_SavePointerAsOffset(state, proxySlot);
    if (!ZrCore_ClosureProxyToken_Install(state, proxySlot, sourceSlot)) {
        return ZR_FALSE;
    }
    proxySlot = ZrCore_Stack_LoadOffsetToPointer(state, proxyOffset);
    ZrCore_Closure_ToBeClosedValueClosureNew(state, proxySlot);
    if (state->toBeClosedValueList.valuePointer != proxySlot) {
        ZrCore_Value_ResetAsNullNoProfile(ZrCore_Stack_GetValueNoProfile(proxySlot));
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* 摘链前单元仍指向活栈槽；调用方负责将其转为闭合值。 */
void ZrCore_Closure_UnlinkValue(SZrClosureValue *closureValue) {
    ZR_ASSERT(!ZrCore_ClosureValue_IsClosed(closureValue));
    *closureValue->link.previous = closureValue->link.next;
    if (closureValue->link.next) {
        closureValue->link.next->link.previous = closureValue->link.previous;
    }
}

/* 从高栈槽依次关停开放捕获；栈顶之外的登记暂不拷贝。 */
void ZrCore_Closure_CloseStackValue(struct SZrState *state, TZrStackValuePointer stackPointer) {
    SZrClosureValue **cursor = &state->stackClosureValueList;

    while (*cursor != ZR_NULL) {
        SZrClosureValue *closureValue = *cursor;
        TZrStackValuePointer valuePointer = closureValue->value.valuePointer;

        ZR_ASSERT(!ZrCore_ClosureValue_IsClosed(closureValue));
        // Open upvalues are kept in descending stack-slot order. Once we move
        // below the closing threshold, the remaining entries belong to older frames.
        if (valuePointer < stackPointer) {
            break;
        }
        if (valuePointer >= state->stackTop.valuePointer) {
            cursor = &closureValue->link.next;
            continue;
        }
        SZrTypeValue *slot = &closureValue->link.closedValue;
        TZrStackValuePointer sourcePointer = valuePointer;
        SZrTypeValue *sourceValue = ZR_CAST_FROM_STACK_VALUE(sourcePointer);
        ZR_ASSERT(valuePointer < state->stackTop.valuePointer);
        ZrCore_Closure_UnlinkValue(closureValue);
        ZrCore_Value_ResetAsNull(slot);
        ZrCore_Value_Copy(state, slot, sourceValue);
        closureValue->value.valuePointer = ZR_CAST_STACK_VALUE(slot);
        closure_value_apply_anchored_escape_to_closed_value(state, closureValue);
        SZrRawObject *rawObject = ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue);
        if (ZrCore_RawObject_IsWaitToScan(rawObject) || ZrCore_RawObject_IsReferenced(rawObject)) {
            ZrCore_RawObject_MarkAsReferenced(rawObject);
            if (slot->isGarbageCollectable && !ZR_VALUE_IS_TYPE_NULL(slot->type) && slot->value.object != ZR_NULL) {
                ZrCore_RawObject_Barrier(state, rawObject, slot->value.object);
            }
        }
    }
}

/* 跳过用于编码大跨度的零 offset 桥接槽，回到上一登记节点。 */
static void closure_pop_to_be_closed_list(SZrState *state) {
    TZrStackValuePointer toBeClosed = state->toBeClosedValueList.valuePointer;
    ZR_ASSERT(toBeClosed->toBeClosedValueOffset > 0);
    toBeClosed -= toBeClosed->toBeClosedValueOffset;
    while (toBeClosed > state->stackBase.valuePointer && toBeClosed->toBeClosedValueOffset == 0) {
        toBeClosed -= MAX_DELTA;
    }
    state->toBeClosedValueList.valuePointer = toBeClosed;
}

/* 先冻结捕获再执行可调用的清理逻辑，每轮重取可能搬迁的阈值栈指针。 */
TZrStackValuePointer ZrCore_Closure_CloseClosure(struct SZrState *state, TZrStackValuePointer stackPointer,
                                           EZrThreadStatus errorStatus, TZrBool isYield) {
    TZrMemoryOffset offset = ZrCore_Stack_SavePointerAsOffset(state, stackPointer);
    ZrCore_Closure_CloseStackValue(state, stackPointer);
    while (state->toBeClosedValueList.valuePointer >= stackPointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        closure_pop_to_be_closed_list(state);
        closure_value_pre_call_close_meta(state, toBeClosed, errorStatus, isYield);
        TZrStackValuePointer pointer = ZrCore_Stack_LoadOffsetToPointer(state, offset);
        stackPointer = pointer;
    }
    return stackPointer;
}

/* AOT 退出路径按登记数关闭，实际处理数可小于请求数。 */
TZrSize ZrCore_Closure_CloseRegisteredValues(struct SZrState *state,
                                       TZrSize count,
                                       EZrThreadStatus errorStatus,
                                       TZrBool isYield) {
    TZrSize closedCount = ZR_CLOSURE_CLOSED_COUNT_NONE;

    if (state == ZR_NULL || count == 0) {
        return ZR_CLOSURE_CLOSED_COUNT_NONE;
    }

    while (closedCount < count &&
           state->toBeClosedValueList.valuePointer > state->stackBase.valuePointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        closure_pop_to_be_closed_list(state);
        closure_value_pre_call_close_meta(state, toBeClosed, errorStatus, isYield);
        closedCount++;
    }

    return closedCount;
}

/* 分配与捕获查找均可能触发 GC，发布到栈后按父帧元数据连接共享单元。 */
void ZrCore_Closure_PushToStack(struct SZrState *state, struct SZrFunction *function, SZrClosureValue **closureValueList,
                           TZrStackValuePointer base, TZrStackValuePointer closurePointer) {
    SZrFunctionStackAnchor baseAnchor;
    TZrMemoryOffset closurePointerOffset;
    TZrBool hasBaseAnchor = ZR_FALSE;

    if (state == ZR_NULL || closurePointer == ZR_NULL) {
        return;
    }

    function = closure_refresh_forwarded_function(function);
    if (function == ZR_NULL) {
        return;
    }

    if (base != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, base, &baseAnchor);
        hasBaseAnchor = ZR_TRUE;
    }
    closurePointerOffset = ZrCore_Stack_SaveByteAddressAsOffset(state, closurePointer);

    TZrSize closureSize = function->closureValueLength;
    SZrFunction *parentFunction = closure_metadata_function_from_frame_base(state, base);
    SZrClosure *closure = ZrCore_Closure_New(state, closureSize);

    if (hasBaseAnchor) {
        base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    }
    closurePointer = ZR_CAST_STACK_VALUE(ZrCore_Stack_LoadByteOffsetToAddress(state, closurePointerOffset));
    closure = closure_refresh_forwarded_closure(closure);
    function = closure_refresh_forwarded_function(function);
    parentFunction = closure_refresh_forwarded_function(parentFunction);
    if (function == ZR_NULL || closure == ZR_NULL) {
        return;
    }

    SZrFunctionClosureVariable *closureVariables = function->closureValueList;
    closure->function = function;
    ZrCore_Stack_SetRawObjectValue(state, closurePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    /* 传入父闭包捕获列表时，重新从仍在栈上的父 callable 获取数组。 */
    if (closureValueList != ZR_NULL) {
        closureValueList = closure_refresh_parent_closure_values_from_base(state, base);
    }
    for (TZrSize i = 0; i < closureSize; i++) {
        SZrFunctionClosureVariable closureValue = closureVariables[i];
        SZrClosureValue *capturedValue;

        if (closureValue.inStack) {
            TZrStackValuePointer capturePointer =
                    closure_value_pointer_for_frame_slot(state, parentFunction, base, closureValue.index);
            capturedValue = capturePointer != ZR_NULL
                                    ? ZrCore_Closure_FindOrCreateValue(state, capturePointer)
                                    : ZR_NULL;
            if (hasBaseAnchor) {
                base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
            }
            closure = closure_refresh_forwarded_closure(closure);
            function = closure_refresh_forwarded_function(function);
            parentFunction = closure_refresh_forwarded_function(parentFunction);
            closureVariables = function != ZR_NULL ? function->closureValueList : ZR_NULL;
            if (closureValueList != ZR_NULL) {
                closureValueList = closure_refresh_parent_closure_values_from_base(state, base);
            }
        } else {
            capturedValue = closureValueList != ZR_NULL ? closureValueList[closureValue.index] : ZR_NULL;
            capturedValue = closure_refresh_forwarded_closure_value(capturedValue);
        }
        if (closure == ZR_NULL || function == ZR_NULL || closureVariables == ZR_NULL) {
            return;
        }
        closure->closureValuesExtend[i] = capturedValue;
        if (closure->closureValuesExtend[i] != ZR_NULL) {
            ZrCore_ClosureValue_SetCaptureMetadata(closure->closureValuesExtend[i],
                                                  closureValue.scopeDepth,
                                                  closureValue.escapeFlags);
        }
        ZrCore_RawObject_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                            ZR_CAST_RAW_OBJECT_AS_SUPER(closure->closureValuesExtend[i]));
    }
}

/* AOT 原生闭包从 shim 取得元数据；普通 native callable 不提供函数元数据。 */
SZrFunction *ZrCore_Closure_GetMetadataFunctionFromValue(struct SZrState *state, const SZrTypeValue *value) {
    SZrRawObject *rawObject;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            if (value->value.object == ZR_NULL) {
                return ZR_NULL;
            }
            break;
        default:
            return ZR_NULL;
    }

    rawObject = closure_refresh_forwarded_raw_object(value->value.object);
    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION) {
        if (value->isNative || rawObject->type != ZR_RAW_OBJECT_TYPE_FUNCTION || rawObject->isNative) {
            return ZR_NULL;
        }
        return closure_refresh_forwarded_function(ZR_CAST_FUNCTION(state, rawObject));
    }

    if (rawObject->type != ZR_RAW_OBJECT_TYPE_CLOSURE || rawObject->isNative != value->isNative) {
        return ZR_NULL;
    }
    if (value->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, rawObject);
        return nativeClosure != ZR_NULL ? closure_refresh_forwarded_function(nativeClosure->aotShimFunction) : ZR_NULL;
    }

    {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, rawObject);
        return closure != ZR_NULL ? closure_refresh_forwarded_function(closure->function) : ZR_NULL;
    }
}

/* 调用帧可在 callable 被替换后保留显式 metadataFunction，优先使用该缓存。 */
SZrFunction *ZrCore_Closure_GetMetadataFunctionFromCallInfo(struct SZrState *state, struct SZrCallInfo *callInfo) {
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }
    if (callInfo->metadataFunction != ZR_NULL) {
        return callInfo->metadataFunction;
    }

    return ZrCore_Closure_GetMetadataFunctionFromValue(state,
                                                       ZrCore_Stack_GetValueNoProfile(
                                                               callInfo->functionBase.valuePointer));
}


#undef MAX_DELTA
