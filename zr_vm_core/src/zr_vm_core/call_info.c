//
// Created by HeJiahui on 2025/6/15.
//

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

