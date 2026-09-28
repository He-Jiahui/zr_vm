//
// Created by HeJiahui on 2025/6/18.
//

#include "zr_vm_core/stack.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/type_layout.h"

/*
 * 栈扩容和槽间搬运属于 VM 热路径；本文件将访问器和 value 操作映射到
 * no-profile 版本，避免重复检查 TLS helper。
 */
#define ZrCore_Stack_GetValue ZrCore_Stack_GetValueNoProfile
#define ZrCore_Value_ResetAsNull ZrCore_Value_ResetAsNullNoProfile
#define ZrCore_Value_Copy ZrCore_Value_CopyNoProfile
/* 栈槽指针同样编码为相对 stackBase 的字节偏移，便于搬迁后用新基址恢复。 */
ZR_FORCE_INLINE TZrMemoryOffset ZrStackSaveAsOffset(SZrState *state, TZrStackValuePointer pointer) {
    return (TZrBytePtr) pointer - (TZrBytePtr) state->stackBase.valuePointer;
}
/* offset 必须来自同一 state 的有效栈槽指针；恢复后旧指针不再可用。 */
ZR_FORCE_INLINE TZrStackValuePointer ZrStackLoadAsOffset(SZrState *state, TZrMemoryOffset offset) {
    return ZR_CAST_STACK_VALUE((TZrBytePtr) state->stackBase.valuePointer + offset);
}
/* 分配器可能搬移整段栈，须先保存栈内借用指针；BUG: 内层 AOT 入口扩栈时，活动外层 GC root frame 的 frameBase 未迁移，FRAME_BYTE_OFFSET 根会被新栈范围检查跳过。 */
static void stack_mark_stack_as_relative(SZrState *state) {
    state->stackTop.reusableValueOffset = ZrStackSaveAsOffset(state, state->stackTop.valuePointer);
    state->toBeClosedValueList.reusableValueOffset =
            ZrStackSaveAsOffset(state, state->toBeClosedValueList.valuePointer);
    // 开放闭包捕获的栈槽地址也随底层数组一起搬移。
    for (SZrClosureValue *closureValue = state->stackClosureValueList; closureValue != ZR_NULL;
         closureValue = closureValue->link.next) {
        closureValue->value.reusableValueOffset = ZrStackSaveAsOffset(state, closureValue->value.valuePointer);
    }
    // 调用帧的边界及可选返回/实参帧地址都必须一并转为偏移。
    for (SZrCallInfo *callInfo = state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        callInfo->functionBase.reusableValueOffset = ZrStackSaveAsOffset(state, callInfo->functionBase.valuePointer);
        callInfo->functionTop.reusableValueOffset = ZrStackSaveAsOffset(state, callInfo->functionTop.valuePointer);
        if (callInfo->hasReturnDestination) {
            callInfo->returnDestinationReusableOffset = ZrStackSaveAsOffset(state, callInfo->returnDestination);
        }
        if (callInfo->hasArgumentSourceFrame) {
            callInfo->argumentSourceFrameBaseReusableOffset =
                    ZrStackSaveAsOffset(state, callInfo->argumentSourceFrameBase.valuePointer);
        }
    }
}
/* 以新 stackBase 恢复相对化字段；VM 帧设 trap 以退出快速分派路径。 */
static void stack_mark_stack_as_absolute(SZrState *state) {
    state->stackTop.valuePointer = ZrStackLoadAsOffset(state, state->stackTop.reusableValueOffset);
    state->toBeClosedValueList.valuePointer =
            ZrStackLoadAsOffset(state, state->toBeClosedValueList.reusableValueOffset);
    for (SZrClosureValue *closureValue = state->stackClosureValueList; closureValue != ZR_NULL;
         closureValue = closureValue->link.next) {
        closureValue->value.valuePointer = ZrStackLoadAsOffset(state, closureValue->value.reusableValueOffset);
    }
    for (SZrCallInfo *callInfo = state->callInfoList; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        callInfo->functionBase.valuePointer = ZrStackLoadAsOffset(state, callInfo->functionBase.reusableValueOffset);
        callInfo->functionTop.valuePointer = ZrStackLoadAsOffset(state, callInfo->functionTop.reusableValueOffset);
        if (callInfo->hasReturnDestination) {
            callInfo->returnDestination = ZrStackLoadAsOffset(state, callInfo->returnDestinationReusableOffset);
        } else {
            callInfo->returnDestination = ZR_NULL;
        }
        if (callInfo->hasArgumentSourceFrame) {
            callInfo->argumentSourceFrameBase.valuePointer =
                    ZrStackLoadAsOffset(state, callInfo->argumentSourceFrameBaseReusableOffset);
        } else {
            callInfo->argumentSourceFrameBase.valuePointer = ZR_NULL;
        }
        if (!ZrCore_CallInfo_IsNative(callInfo)) {
            callInfo->context.context.trap = 1;
        }
    }
}
/* 先保存所有栈内指针，再暂停 GC 跨过宿主 realloc，成功或失败都恢复栈状态。 */
static TZrBool stack_realloc_internal(SZrState *state, TZrUInt64 newSize, TZrBool throwError) {
    SZrGlobalState *global = state->global;
    TZrSize previousStackSize = ZrCore_State_StackGetSize(state);
    TZrSize previousStackByteSize =
            sizeof(SZrTypeValueOnStack) * (previousStackSize + ZR_THREAD_STACK_SIZE_EXTRA);
    TZrSize newStackByteSize = sizeof(SZrTypeValueOnStack) * (newSize + ZR_THREAD_STACK_SIZE_EXTRA);
    TZrBool previousStopGcFlag = state->global->garbageCollector->stopGcFlag;
    ZR_ASSERT(newSize <= ZR_VM_MAX_STACK || newSize == ZR_VM_ERROR_STACK);
    stack_mark_stack_as_relative(state);
    state->global->garbageCollector->stopGcFlag = ZR_TRUE;
    TZrStackValuePointer newStackPointer = ZR_CAST_STACK_VALUE(
            ZrCore_Memory_Allocate(global, state->stackBase.valuePointer, previousStackByteSize,
                             newStackByteSize, ZR_MEMORY_NATIVE_TYPE_STACK));
    state->global->garbageCollector->stopGcFlag = previousStopGcFlag;
    if (ZR_UNLIKELY(newStackPointer == ZR_NULL)) {
        // TODO: 失败后会用旧 stackBase 恢复指针；FZrAllocator 的失败保留契约未统一，需核验所有当前宿主 allocator。
        stack_mark_stack_as_absolute(state);
        if (throwError) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        }
        return ZR_FALSE;
    }
    state->stackBase.valuePointer = newStackPointer;
    stack_mark_stack_as_absolute(state);
    state->stackTail.valuePointer = newStackPointer + newSize;
    /*
     * 初始栈分配在逻辑容量后保留额外尾部空间。
     * 逻辑容量增长时，原来隐藏的槽首次进入可用范围。
     * 因此这些新增槽也要先初始化，避免暴露脏内存。
     */
    for (TZrSize i = previousStackSize; i < newSize + ZR_THREAD_STACK_SIZE_EXTRA; i++) {
        SZrTypeValueOnStack *slot = newStackPointer + i;
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slot));
        slot->toBeClosedValueOffset = 0u;
    }
    return ZR_TRUE;
}
/* 按 stackTop 所在槽计算所需容量，并保留当前逻辑容量以避免意外缩栈。 */
static TZrSize stack_required_size_from_pointer(SZrState *state, TZrStackValuePointer stackPointer, TZrSize extraSpace) {
    TZrSize currentSize = ZrCore_State_StackGetSize(state);
    TZrSize requiredSize = (TZrSize) (stackPointer - state->stackBase.valuePointer) + extraSpace;
    return requiredSize > currentSize ? requiredSize : currentSize;
}
/* BUG: 原生分配返回 null 时仍计算尾地址；state_stack_init 忽略失败并对空基址运算、初始化槽，启动 OOM 会触发未定义行为。 */
TZrPtr ZrCore_Stack_Construct(SZrState *state, TZrStackPointer *stack, TZrSize stackLength) {
    ZR_ASSERT(stackLength > 0);
    SZrGlobalState *global = state->global;
    TZrSize stackByteSize = sizeof(SZrTypeValueOnStack) * stackLength;
    stack->valuePointer =
            ZR_CAST_STACK_VALUE(ZrCore_Memory_RawMallocWithType(global, stackByteSize, ZR_MEMORY_NATIVE_TYPE_STACK));
    return ZR_CAST_PTR(stack->valuePointer + stackLength);
}

void ZrCore_Stack_Deconstruct(struct SZrState *state, TZrStackPointer *stack, TZrSize stackLength) {
    SZrGlobalState *global = state->global;
    TZrSize stackByteSize = sizeof(SZrTypeValueOnStack) * stackLength;
    ZrCore_Memory_RawFreeWithType(global, stack->valuePointer, stackByteSize, ZR_MEMORY_NATIVE_TYPE_STACK);
}

TZrStackValuePointer ZrCore_Stack_GetAddressFromOffset(struct SZrState *state, TZrMemoryOffset offset) {
    SZrCallInfo *callInfoTop = state->callInfoList;
    if (offset > 0) {
        TZrStackValuePointer address = callInfoTop->functionBase.valuePointer + offset;
        ZR_CHECK(state, address < state->stackTop.valuePointer, "stack index overflow from function base to stack top");
        return address;
    }
    // 负偏移不允许进入全局模块或闭包保留区。
    ZR_CHECK(state, offset <= ZR_VM_STACK_GLOBAL_MODULE_REGISTRY,
             "cannot access global module registry or closure offset");
    // 负索引从当前 stackTop 向栈底方向计数。
    ZR_CHECK(state,
             offset != 0 && -offset <= state->stackTop.valuePointer - (callInfoTop->functionBase.valuePointer + 1),
             "stack index overflow from stack top to function base");
    return state->stackTop.valuePointer + offset;
}

TZrBool ZrCore_Stack_GrowTo(struct SZrState *state, TZrSize requiredSize, TZrBool canThrowError) {
    return stack_realloc_internal(state, requiredSize, canThrowError);
}

TZrBool ZrCore_Stack_Grow(struct SZrState *state, TZrSize space, TZrBool canThrowError) {
    TZrSize requiredSize = stack_required_size_from_pointer(state, state->stackTop.valuePointer, space);
    return stack_realloc_internal(state, requiredSize, canThrowError);
}

TZrBool ZrCore_Stack_CheckFullAndGrow(SZrState *state, TZrSize space, TZrNativeString errorMessage) {
    TZrBool result = ZR_FALSE;
    ZR_THREAD_LOCK(state);
    SZrCallInfo *callInfoTop = state->callInfoList;
    ZR_CHECK(state, space > 0, "stack space to grow must be positive");
    if (state->stackTail.valuePointer - state->stackTop.valuePointer > (TZrMemoryOffset) space) {
        result = ZR_TRUE;
    } else {
        TZrSize requiredSize = stack_required_size_from_pointer(state, state->stackTop.valuePointer, space);
        result = stack_realloc_internal(state, requiredSize, ZR_FALSE);
    }
    if (result && callInfoTop->functionTop.valuePointer < state->stackTop.valuePointer + space) {
        callInfoTop->functionTop.valuePointer = state->stackTop.valuePointer + space;
    }
    ZR_THREAD_UNLOCK(state);
    if (ZR_UNLIKELY(!result)) {
        if (errorMessage) {
            ZrCore_Log_Error(state, "stack overflow: %s", errorMessage);
        } else {
            ZrCore_Log_Error(state, "stack overflow");
        }
    }
    return result;
}

void ZrCore_Stack_SetRawObjectValue(struct SZrState *state, SZrTypeValueOnStack *destination, SZrRawObject *object) {
    SZrTypeValue *destinationValue = ZrCore_Stack_GetValue(destination);
    ZrCore_Value_PrepareDestinationForOverwriteNoProfile(state, destinationValue);
    ZrCore_Value_InitAsRawObject(state, destinationValue, object);
    destinationValue->isGarbageCollectable = ZR_TRUE;
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destinationValue);
}


void ZrCore_Stack_CopyValue(SZrState *state, SZrTypeValueOnStack *destination, const SZrTypeValue *source) {
    SZrTypeValue *destinationValue = ZrCore_Stack_GetValue(destination);
    ZrCore_Value_Copy(state, destinationValue, source);
}

TZrMemoryOffset ZrCore_Stack_SavePointerAsOffset(struct SZrState *state, TZrStackValuePointer stackPointer) {
    return ZrStackSaveAsOffset(state, stackPointer);
}

TZrStackValuePointer ZrCore_Stack_LoadOffsetToPointer(struct SZrState *state, TZrMemoryOffset offset) {
    return ZrStackLoadAsOffset(state, offset);
}

TZrMemoryOffset ZrCore_Stack_SaveByteAddressAsOffset(struct SZrState *state, TZrPtr stackAddress) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(state->stackBase.valuePointer != ZR_NULL);
    ZR_ASSERT(stackAddress != ZR_NULL);
    return (TZrBytePtr)stackAddress - (TZrBytePtr)state->stackBase.valuePointer;
}

TZrPtr ZrCore_Stack_LoadByteOffsetToAddress(struct SZrState *state, TZrMemoryOffset offset) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(state->stackBase.valuePointer != ZR_NULL);
    ZR_ASSERT(offset >= 0);
    return (TZrBytePtr)state->stackBase.valuePointer + offset;
}
/* 只把 stackBase 到逻辑 stackTail 视为可用范围，不含额外尾部缓冲。 */
static TZrBool stack_try_get_byte_size(struct SZrState *state, TZrMemoryOffset *outByteSize) {
    if (state == ZR_NULL || state->stackBase.valuePointer == ZR_NULL || state->stackTail.valuePointer == ZR_NULL ||
        outByteSize == ZR_NULL) {
        return ZR_FALSE;
    }

    *outByteSize = (TZrMemoryOffset)((state->stackTail.valuePointer - state->stackBase.valuePointer) *
                                     (TZrMemoryOffset)sizeof(SZrTypeValueOnStack));
    return ZR_TRUE;
}
/* 以减法比较界限，避免 offset + byteSize 在范围检查时溢出。 */
static TZrBool stack_byte_range_is_available(struct SZrState *state, TZrMemoryOffset offset, TZrUInt32 byteSize) {
    TZrMemoryOffset stackByteSize;

    if (offset < 0 || !stack_try_get_byte_size(state, &stackByteSize)) {
        return ZR_FALSE;
    }

    return (TZrBool)((TZrMemoryOffset)byteSize <= stackByteSize &&
                     offset <= stackByteSize - (TZrMemoryOffset)byteSize);
}
/* 零对齐在 stack-place 接口中按无额外对齐要求处理。 */
static TZrUInt32 stack_normalize_byte_align(TZrUInt32 byteAlign) {
    return byteAlign > 0u ? byteAlign : 1u;
}

TZrBool ZrCore_Stack_MakeFramePlace(struct SZrState *state,
                                    TZrStackValuePointer frameBase,
                                    TZrUInt32 frameByteOffset,
                                    TZrUInt32 byteSize,
                                    TZrUInt32 byteAlign,
                                    SZrStackFramePlace *outPlace) {
    TZrMemoryOffset stackByteSize;
    TZrMemoryOffset frameBaseOffset;
    TZrMemoryOffset absoluteOffset;
    TZrUInt32 normalizedAlign;

    if (frameBase == ZR_NULL || outPlace == ZR_NULL || !stack_try_get_byte_size(state, &stackByteSize)) {
        return ZR_FALSE;
    }

    normalizedAlign = stack_normalize_byte_align(byteAlign);
    if (frameByteOffset % normalizedAlign != 0u) {
        return ZR_FALSE;
    }

    frameBaseOffset = ZrStackSaveAsOffset(state, frameBase);
    if (frameBaseOffset < 0 ||
        frameBaseOffset > stackByteSize ||
        (TZrMemoryOffset)frameByteOffset > stackByteSize - frameBaseOffset) {
        return ZR_FALSE;
    }

    absoluteOffset = frameBaseOffset + (TZrMemoryOffset)frameByteOffset;
    if (!stack_byte_range_is_available(state, absoluteOffset, byteSize)) {
        return ZR_FALSE;
    }

    outPlace->address = ZrCore_Stack_LoadByteOffsetToAddress(state, absoluteOffset);
    outPlace->byteOffset = absoluteOffset;
    outPlace->byteSize = byteSize;
    outPlace->byteAlign = normalizedAlign;
    return ZR_TRUE;
}
/* place 必须仍指向当前栈基址下的同一范围，且容量/对齐覆盖完整布局。 */
static TZrBool stack_place_covers_layout(struct SZrState *state,
                                         const SZrStackFramePlace *place,
                                         const SZrTypeLayout *layout) {
    TZrUInt32 layoutAlign;

    if (place == ZR_NULL || place->address == ZR_NULL || layout == ZR_NULL) {
        return ZR_FALSE;
    }

    layoutAlign = stack_normalize_byte_align(layout->byteAlign);
    if (place->byteSize < layout->byteSize ||
        stack_normalize_byte_align(place->byteAlign) < layoutAlign ||
        !stack_byte_range_is_available(state, place->byteOffset, layout->byteSize)) {
        return ZR_FALSE;
    }

    return (TZrBool)(place->address == ZrCore_Stack_LoadByteOffsetToAddress(state, place->byteOffset));
}

TZrBool ZrCore_Stack_CopyInline(struct SZrState *state,
                                const SZrTypeLayout *layout,
                                TZrMemoryOffset destinationOffset,
                                TZrMemoryOffset sourceOffset) {
    TZrPtr destination;
    TZrPtr source;

    if (layout == ZR_NULL ||
        !stack_byte_range_is_available(state, destinationOffset, layout->byteSize) ||
        !stack_byte_range_is_available(state, sourceOffset, layout->byteSize)) {
        return ZR_FALSE;
    }

    destination = ZrCore_Stack_LoadByteOffsetToAddress(state, destinationOffset);
    source = ZrCore_Stack_LoadByteOffsetToAddress(state, sourceOffset);
    return ZrCore_TypeLayout_CopyInline(state, layout, destination, source);
}

TZrBool ZrCore_Stack_CopyInlinePlace(struct SZrState *state,
                                     const SZrTypeLayout *layout,
                                     const SZrStackFramePlace *destination,
                                     const SZrStackFramePlace *source) {
    if (!stack_place_covers_layout(state, destination, layout) ||
        !stack_place_covers_layout(state, source, layout)) {
        return ZR_FALSE;
    }

    return ZrCore_TypeLayout_CopyInline(state, layout, destination->address, source->address);
}
