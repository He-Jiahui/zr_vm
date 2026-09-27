//
// Created by HeJiahui on 2025/6/15.
//

#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

/* 入口帧借用 state 的基础栈槽；清零上下文后建立 native 边界，供异常回溯在此停止。 */
void ZrCore_CallInfo_EntryNativeInit(SZrState *state, SZrCallInfo *callInfo, TZrStackPointer functionIndex,
                               TZrStackPointer functionTop, SZrCallInfo *previous) {
    ZR_UNUSED_PARAMETER(state);
    memset(callInfo, 0, sizeof(*callInfo));
    // ready to call native function
    callInfo->next = ZR_NULL;
    callInfo->previous = previous;
    callInfo->callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    ZrCore_Value_ResetAsNull(&callInfo->interpreterGenericContext);
    ZrCore_Value_ResetAsNull(&callInfo->interpreterGenericMethodContext);
    callInfo->functionBase.valuePointer = functionIndex.valuePointer;
    // null function index means it is the entry call of thread
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
    // ready to call native function
    // TODO: 同一入口栈槽已在上方 ResetAsNull，第二次清零会重复记录 profile helper；
    // 核对历史入口约定与 profile 计数要求后决定是否保留两次调用。
    ZrCore_Value_ResetAsNull(&functionIndex.valuePointer->value);
    callInfo->functionTop.valuePointer = functionTop.valuePointer;
}


/* 帧只在缓存链尾追加；分配失败时不得发布 next 或推进帧链长度。 */
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
