//
// Created by HeJiahui on 2025/6/15.
//

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

void ZrCallInfoEntryNativeInit(SZrState *state, SZrCallInfo *callInfo, TZrStackPointer functionIndex,
                               TZrStackPointer functionTop, SZrCallInfo *previous) {
    ZR_UNUSED_PARAMETER(state);
    // ready to call native function
    callInfo->next = ZR_NULL;
    callInfo->previous = previous;
    callInfo->callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    callInfo->functionBase.valuePointer = functionIndex.valuePointer;
    // null function index means it is the entry call of thread
    ZrValueResetAsNull(&functionIndex.valuePointer->value);
    callInfo->context.nativeContext.continuationFunction = ZR_NULL;
    callInfo->expectedReturnCount = 0;
    // ready to call native function
    ZrValueResetAsNull(&functionIndex.valuePointer->value);
    callInfo->functionTop.valuePointer = functionTop.valuePointer;
}


SZrCallInfo *ZrCallInfoExtend(struct SZrState *state) {
    SZrCallInfo *callInfo = ZR_NULL;
    ZR_ASSERT(state->callInfoList->next == ZR_NULL);
    callInfo = ZR_CAST_CALL_INFO(ZrMemoryGcMalloc(state, ZR_VALUE_TYPE_VM_MEMORY, sizeof(SZrCallInfo)));
    state->callInfoList->next = callInfo;
    callInfo->previous = state->callInfoList;
    callInfo->next = ZR_NULL;
    callInfo->context.context.trap = 0;
    state->callInfoListLength++;
    return callInfo;
}
