#ifndef ZR_VM_CORE_FUNCTION_PRECALL_INTERNAL_H
#define ZR_VM_CORE_FUNCTION_PRECALL_INTERNAL_H

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"

static ZR_FORCE_INLINE void function_init_vm_call_info_exact_args_steady_state_inline(
        SZrCallInfo *callInfo,
        SZrCallInfo *previous,
        TZrStackValuePointer basePointer,
        TZrStackValuePointer topPointer,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination,
        const TZrInstruction *programCounter) {
    ZR_ASSERT(callInfo != ZR_NULL);

    /*
     * Hot KNOWN_VM call sites repeatedly land on the same exact-args, no-entry-
     * clear, no-debug-hook shape. Keep the reused call info writes as narrow as
     * possible so the dispatch loop can inline this setup directly.
     */
    callInfo->functionBase.valuePointer = basePointer;
    callInfo->functionTop.valuePointer = topPointer;
    callInfo->previous = previous;
    callInfo->expectedReturnCount = resultCount;
    callInfo->callStatus = ZR_CALL_STATUS_NONE;
    callInfo->context.context.programCounter = programCounter;
    callInfo->context.context.trap = ZR_DEBUG_SIGNAL_NONE;
    callInfo->context.context.variableArgumentCount = 0u;
    callInfo->yieldContext.returnValueCount = 0u;
    callInfo->returnDestination = returnDestination;
    callInfo->returnDestinationReusableOffset = 0;
    callInfo->hasReturnDestination = returnDestination != ZR_NULL;
}

static ZR_FORCE_INLINE SZrCallInfo *function_try_pre_call_prepared_resolved_vm_exact_args_steady_state_inline(
        struct SZrState *state,
        TZrStackValuePointer stackPointer,
        struct SZrFunction *function,
        TZrSize argumentsCount,
        TZrSize resultCount,
        TZrStackValuePointer returnDestination) {
    SZrCallInfo *callInfo;
    SZrCallInfo *previousCallInfo;
    TZrSize stackSize;
    TZrUInt32 exactArgsClearStackSizePlusOne;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(function != ZR_NULL);

    exactArgsClearStackSizePlusOne = (TZrUInt32)argumentsCount + 1u;
    if (ZR_UNLIKELY(state->debugHookSignal != 0u ||
                    function->parameterCount != argumentsCount ||
                    function->vmEntryClearStackSizePlusOne != exactArgsClearStackSizePlusOne)) {
        return ZR_NULL;
    }

    stackSize = function->stackSize;
    ZR_ASSERT(state->stackTop.valuePointer == stackPointer + 1 + argumentsCount);
    if (ZR_UNLIKELY(state->stackTail.valuePointer - stackPointer < (TZrMemoryOffset)(stackSize + 1u))) {
        return ZR_NULL;
    }

    previousCallInfo = state->callInfoList;
    callInfo = previousCallInfo->next;
    if (ZR_UNLIKELY(callInfo == ZR_NULL)) {
        callInfo = ZrCore_CallInfo_Extend(state);
    }
    if (ZR_UNLIKELY(callInfo == ZR_NULL)) {
        return ZR_NULL;
    }

    function_init_vm_call_info_exact_args_steady_state_inline(callInfo,
                                                              previousCallInfo,
                                                              stackPointer,
                                                              stackPointer + 1 + stackSize,
                                                              resultCount,
                                                              returnDestination,
                                                              function->instructionsList);
    state->callInfoList = callInfo;
    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
    return callInfo;
}

#endif // ZR_VM_CORE_FUNCTION_PRECALL_INTERNAL_H
