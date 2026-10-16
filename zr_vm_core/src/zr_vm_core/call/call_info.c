//
// Created by HeJiahui on 2025/6/15.
//

#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

/* 入口或关闭回调边界借用调用者的栈槽；整体重置不释放旧资源，也不发布活动帧。
 * 复用缓存边界的调用者先保存后继，再恢复 next，避免丢失可复用子链。 */
void ZrCore_CallInfo_EntryNativeInit(SZrState *state, SZrCallInfo *callInfo, TZrStackPointer functionIndex,
                               TZrStackPointer functionTop, SZrCallInfo *previous) {
    ZR_UNUSED_PARAMETER(state);
    memset(callInfo, 0, sizeof(*callInfo));
    // native 边界阻止 VM 异常回溯越过本次 C 调用。
    callInfo->next = ZR_NULL;
    callInfo->previous = previous;
    callInfo->callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    ZrCore_Value_ResetAsNull(&callInfo->interpreterGenericContext);
    ZrCore_Value_ResetAsNull(&callInfo->interpreterGenericMethodContext);
    callInfo->functionBase.valuePointer = functionIndex.valuePointer;
    // 清空函数槽的值作为入口标记；槽地址本身仍必须有效。
    ZrCore_Value_ResetAsNull(&functionIndex.valuePointer->value);
    callInfo->context.nativeContext.continuationFunction = ZR_NULL;
    callInfo->expectedReturnCount = 0;
    callInfo->returnDestination = ZR_NULL;
    callInfo->returnDestinationReusableOffset = 0;
    callInfo->hasReturnDestination = ZR_FALSE;
    callInfo->argumentSourceFrameBase.valuePointer = ZR_NULL;
    callInfo->argumentSourceFrameBaseReusableOffset = 0;
    callInfo->argumentSourceStartSlot = 0;
    callInfo->hasArgumentSourceFrame = ZR_FALSE;
    // 保留现有重复重置，相关 profile 计数要求见下方 TODO。
    // TODO: 同一入口栈槽已在上方 ResetAsNull，第二次清零会重复记录 profile helper；
    // 核对历史入口约定与 profile 计数要求后决定是否保留两次调用。
    ZrCore_Value_ResetAsNull(&functionIndex.valuePointer->value);
    callInfo->functionTop.valuePointer = functionTop.valuePointer;
}


/* 分配可能触发 GC 或抛错；仅在拿到新记录后发布缓存链接和链长，不在此激活帧。 */
SZrCallInfo *ZrCore_CallInfo_Extend(struct SZrState *state) {
    SZrCallInfo *callInfo = ZR_NULL;
    ZR_ASSERT(state->callInfoList->next == ZR_NULL);
    callInfo = ZR_CAST_CALL_INFO(ZrCore_Memory_GcMalloc(state, ZR_MEMORY_NATIVE_TYPE_CALL_INFO, sizeof(SZrCallInfo)));
    if (callInfo == ZR_NULL) {
        return ZR_NULL;
    }
    memset(callInfo, 0, sizeof(*callInfo));
    state->callInfoList->next = callInfo;
    callInfo->previous = state->callInfoList;
    callInfo->next = ZR_NULL;
    callInfo->context.context.trap = 0;
    state->callInfoListLength++;
    return callInfo;
}
