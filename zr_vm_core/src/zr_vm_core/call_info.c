//
// Created by HeJiahui on 2025/6/15.
//

#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

void ZrCore_CallInfo_EntryNativeInit(SZrState *state, SZrCallInfo *callInfo, TZrStackPointer functionIndex,
                               TZrStackPointer functionTop, SZrCallInfo *previous) {
    ZR_UNUSED_PARAMETER(state);
    memset(callInfo, 0, sizeof(*callInfo));
    // ready to call native function
    callInfo->next = ZR_NULL;
    callInfo->previous = previous;
    callInfo->callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    callInfo->functionBase.valuePointer = functionIndex.valuePointer;
    // null function index means it is the entry call of thread
    ZrCore_Value_ResetAsNull(&functionIndex.valuePointer->value);
    callInfo->context.nativeContext.continuationFunction = ZR_NULL;
    callInfo->expectedReturnCount = 0;
    callInfo->returnDestination = ZR_NULL;
    // ready to call native function
    ZrCore_Value_ResetAsNull(&functionIndex.valuePointer->value);
    callInfo->functionTop.valuePointer = functionTop.valuePointer;
}


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
