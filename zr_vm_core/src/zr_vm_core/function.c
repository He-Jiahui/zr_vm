//
// Created by HeJiahui on 2025/7/20.
//

#include "zr_vm_core/function.h"

#include "zr_vm_core/execution.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

TZrStackValuePointer ZrFunctionCheckStack(struct SZrState *state, TZrSize size, TZrStackValuePointer stackPointer) {
    if (ZR_UNLIKELY(state->stackTail.valuePointer - state->stackTop.valuePointer < (TZrMemoryOffset) size)) {
        TZrMemoryOffset relative = ZrStackSavePointerAsOffset(state, stackPointer);
        ZrStackGrow(state, size, ZR_TRUE);
        TZrStackValuePointer restoredStackPointer = ZrStackLoadOffsetToPointer(state, relative);
        return restoredStackPointer;
    }
    return stackPointer;
}

SZrFunction *ZrFunctionNew(struct SZrState *state) {
    SZrRawObject *newObject = ZrRawObjectNew(state, ZR_RAW_OBJECT_TYPE_FUNCTION, sizeof(SZrFunction), ZR_FALSE);
    SZrFunction *function = ZR_CAST_FUNCTION(state, newObject);
    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0;
    function->childFunctionList = ZR_NULL;
    function->childFunctionLength = 0;
    function->instructionsList = ZR_NULL;
    function->instructionsLength = 0;
    function->executionLocationInfoList = ZR_NULL;
    function->executionLocationInfoLength = 0;
    function->closureValueList = ZR_NULL;
    function->closureValueLength = 0;
    function->parameterCount = 0;
    function->hasVariableArguments = ZR_FALSE;
    function->stackSize = 0;
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;
    function->lineInSourceStart = 0;
    function->lineInSourceEnd = 0;
    function->sourceCodeList = ZR_NULL;
    return function;
}

void ZrFunctionFree(struct SZrState *state, SZrFunction *function) {
    SZrGlobalState *global = state->global;
    ZR_ASSERT(function != ZR_NULL);
    ZR_MEMORY_RAW_FREE_LIST(global, function->instructionsList, function->instructionsLength);
    ZR_MEMORY_RAW_FREE_LIST(global, function->childFunctionList, function->childFunctionLength);
    ZR_MEMORY_RAW_FREE_LIST(global, function->constantValueList, function->constantValueLength);
    ZR_MEMORY_RAW_FREE_LIST(global, function->localVariableList, function->localVariableLength);
    ZR_MEMORY_RAW_FREE_LIST(global, function->closureValueList, function->closureValueLength);
    ZR_MEMORY_RAW_FREE_LIST(global, function->executionLocationInfoList, function->executionLocationInfoLength);

    // ZrMemoryRawFreeWithType(global, function, sizeof(SZrFunction), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    // function is object, gc free it automatically.
}

SZrString *ZrFunctionGetLocalVariableName(SZrFunction *function, TUInt32 index, TUInt32 programCounter) {
    for (TUInt32 i = 0;
         i < function->localVariableLength && function->localVariableList[i].offsetActivate <= programCounter; i++) {
        if (programCounter < function->localVariableList[i].offsetDead) {
            index--;
            if (index == 0) {
                return function->localVariableList[i].name;
            }
        }
    }
    return ZR_NULL;
}

void ZrFunctionCheckNativeStack(struct SZrState *state) {
    if (state->nestedNativeCalls == ZR_VM_MAX_NATIVE_CALL_STACK) {
        ZrDebugRunError(state, "C stack overflow");
    } else if (state->nestedNativeCalls >= (ZR_VM_MAX_NATIVE_CALL_STACK) / 10 * 11) {
        ZrDebugErrorWhenHandlingError(state);
    }
}

TZrStackValuePointer ZrFunctionCheckStackAndGc(struct SZrState *state, TZrSize size,
                                               TZrStackValuePointer stackPointer) {
    if (ZR_UNLIKELY(state->stackTail.valuePointer - state->stackTop.valuePointer < size)) {
        TZrMemoryOffset relative = ZrStackSavePointerAsOffset(state, stackPointer);
        // todo: check gc
        ZrStackGrow(state, size, ZR_TRUE);
        TZrStackValuePointer restoredStackPointer = ZrStackLoadOffsetToPointer(state, relative);
        return restoredStackPointer;
    }
    return stackPointer;
}

static ZR_FORCE_INLINE void ZrFunctionCallInternal(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                   TZrSize resultCount, TUInt32 callIncremental, TBool isYield) {
    state->nestedNativeCalls += callIncremental;
    state->nestedNativeCallYieldFlag += isYield ? 1 : 0;
    if (ZR_UNLIKELY(state->nestedNativeCalls > ZR_VM_MAX_NATIVE_CALL_STACK)) {
        ZrFunctionCheckStack(state, 0, stackPointer);
        ZrFunctionCheckNativeStack(state);
    }
    // todo:
    SZrCallInfo *callInfo = ZrFunctionPreCall(state, stackPointer, resultCount);
    if (callInfo != ZR_NULL) {
        callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
        ZrExecute(state, callInfo);
    }
    state->nestedNativeCallYieldFlag -= isYield ? 1 : 0;
    state->nestedNativeCalls -= callIncremental;
}

void ZrFunctionCall(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount) {
    ZrFunctionCallInternal(state, stackPointer, resultCount, 1, ZR_FALSE);
}

void ZrFunctionCallWithoutYield(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount) {
    ZrFunctionCallInternal(state, stackPointer, resultCount, 1, ZR_TRUE);
}

static ZR_FORCE_INLINE SZrCallInfo *ZrFunctionPreCallNativeCallInfo(struct SZrState *state,
                                                                    TZrStackValuePointer basePointer,
                                                                    TZrSize resultCount, EZrCallStatus mask,
                                                                    TZrStackValuePointer topPointer) {
    SZrCallInfo *callInfo = state->callInfoList->next ? state->callInfoList->next : ZrCallInfoExtend(state);
    callInfo->functionBase.valuePointer = basePointer;
    callInfo->expectedReturnCount = resultCount;
    callInfo->callStatus = mask;
    callInfo->functionTop.valuePointer = topPointer;
    return callInfo;
}


static ZR_FORCE_INLINE TZrSize ZrFunctionPreCallNative(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                       TZrSize resultCount, FZrNativeFunction function) {
    TZrSize returnCount = 0;
    SZrCallInfo *callInfo = ZR_NULL;
    ZrFunctionCheckStackAndGc(state, ZR_STACK_NATIVE_CALL_MIN, stackPointer);
    callInfo = ZrFunctionPreCallNativeCallInfo(state, stackPointer, resultCount, ZR_CALL_STATUS_NATIVE_CALL,
                                               state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_MIN);
    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
    if (ZR_UNLIKELY(state->debugHookSignal & ZR_DEBUG_HOOK_MASK_CALL)) {
        TInt32 argumentsCount = ZR_CAST_INT(state->stackTop.valuePointer - stackPointer);
        ZrDebugHook(state, ZR_DEBUG_HOOK_EVENT_CALL, -1, 1, argumentsCount);
    }
    ZR_THREAD_UNLOCK(state);
    returnCount = function(state);
    ZR_THREAD_LOCK(state);
    ZR_STACK_CHECK_CALL_INFO_STACK_COUNT(state, returnCount);
    ZrFunctionPostCall(state, callInfo, returnCount);
    return returnCount;
}

static TZrStackValuePointer ZrFunctionGetMetaCall(SZrState *state, TZrStackValuePointer stackPointer) {
    ZrFunctionCheckStackAndGc(state, 1, stackPointer);
    SZrTypeValue *value = ZrStackGetValue(stackPointer);
    SZrMeta *metaValue = ZrValueGetMeta(state, value, ZR_META_CALL);
    if (ZR_UNLIKELY(metaValue == ZR_NULL)) {
        // todo: throw error: no call meta found
        ZrDebugCallError(state, value);
    }
    for (TZrStackValuePointer p = state->stackTop.valuePointer; p > stackPointer; p--) {
        ZrStackCopyValue(state, p, ZrStackGetValue(p - 1));
    }
    state->stackTop.valuePointer++;
    ZrValueInitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(metaValue->function));
    return stackPointer;
}

SZrCallInfo *ZrFunctionPreCall(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount) {
    do {
        SZrTypeValue *value = ZrStackGetValue(stackPointer);
        EZrValueType type = value->type;
        TBool isNative = value->isNative;
        switch (type) {
            case ZR_VALUE_TYPE_FUNCTION: {
                if (isNative) {
                    SZrClosureNative *native = ZR_CAST_NATIVE_CLOSURE(state, value->value.object);
                    ZrFunctionPreCallNative(state, stackPointer, resultCount, native->nativeFunction);
                    // todo:
                    return ZR_NULL;
                } else {
                    SZrCallInfo *callInfo = ZR_NULL;
                    SZrFunction *function = ZR_CAST_VM_CLOSURE(state, value->value.object)->function;
                    TZrSize argumentsCount = ZR_CAST_INT64(state->stackTop.valuePointer - stackPointer) - 1;
                    TZrSize parametersCount = function->parameterCount;
                    TZrSize stackSize = function->stackSize;
                    ZrFunctionCheckStackAndGc(state, stackSize, stackPointer);
                    callInfo = ZrFunctionPreCallNativeCallInfo(state, stackPointer, resultCount, ZR_CALL_STATUS_NONE,
                                                               stackPointer + 1 + stackSize);
                    state->callInfoList = callInfo;
                    callInfo->context.context.programCounter = function->instructionsList;
                    for (; argumentsCount < parametersCount; argumentsCount++) {
                        SZrTypeValue *stackValue = ZrStackGetValue(state->stackTop.valuePointer++);
                        ZrValueResetAsNull(stackValue);
                    }
                    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
                    return callInfo;
                }
            } break;
            case ZR_VALUE_TYPE_NATIVE_POINTER: {
                FZrNativeFunction native = ZR_CAST_FUNCTION_POINTER(value);
                ZrFunctionPreCallNative(state, stackPointer, resultCount, native);
                return ZR_NULL;
            } break;
            case ZR_VALUE_TYPE_NATIVE_DATA: {
                return ZR_NULL;
            } break;
            default: {
                // todo: use CALL meta
                stackPointer = ZrFunctionGetMetaCall(state, stackPointer);
                // return ZR_NULL;
            } break;
        }
    } while (ZR_TRUE);
}

static ZR_FORCE_INLINE void ZrFunctionMoveReturns(SZrState *state, TZrStackValuePointer stackPointer,
                                                  TZrSize returnCount, TZrSize expectedReturnCount) {
    switch (expectedReturnCount) {
        case 0: {
            state->stackTop.valuePointer = stackPointer;
            return;
        } break;
        case 1: {
            if (returnCount == 0) {
                ZrValueResetAsNull(ZrStackGetValue(stackPointer));
            } else {
                ZrStackCopyValue(state, stackPointer, ZrStackGetValue(state->stackTop.valuePointer - returnCount));
            }
            state->stackTop.valuePointer = stackPointer + 1;
            return;
        } break;
        default: {
            // todo: if expected more than 1 results
        } break;
    }
    TZrStackValuePointer first = state->stackTop.valuePointer - returnCount;
    if (returnCount > expectedReturnCount) {
        returnCount = expectedReturnCount;
    }
    for (TZrSize i = 0; i < returnCount; i++) {
        ZrStackCopyValue(state, stackPointer + i, ZrStackGetValue(first + i));
    }
    for (TZrSize i = returnCount; i < expectedReturnCount; i++) {
        ZrValueResetAsNull(ZrStackGetValue(stackPointer + i));
    }
    state->stackTop.valuePointer = stackPointer + expectedReturnCount;
}

void ZrFunctionPostCall(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount) {
    TZrSize expectedReturnCount = callInfo->expectedReturnCount;
    if (ZR_UNLIKELY(state->debugHookSignal)) {
        ZrDebugHookReturn(state, callInfo, resultCount);
    }
    // move result
    ZrFunctionMoveReturns(state, callInfo->functionBase.valuePointer, resultCount, expectedReturnCount);

    ZR_ASSERT(!(callInfo->callStatus &
                (ZR_CALL_STATUS_DEBUG_HOOK | ZR_CALL_STATUS_HOOK_YIELD | ZR_CALL_STATUS_DECONSTRUCTOR_CALL |
                 ZR_CALL_STATUS_CALL_INFO_TRANSFER | ZR_CALL_STATUS_CLOSE_CALL)));

    state->callInfoList = callInfo->previous;
}
