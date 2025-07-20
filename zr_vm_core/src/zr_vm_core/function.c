//
// Created by HeJiahui on 2025/7/20.
//

#include "zr_vm_core/function.h"

#include "zr_vm_core/execution.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"

TZrStackValuePointer ZrFunctionCheckStack(struct SZrState *state, TZrSize size, TZrStackValuePointer stackPointer) {
    if (ZR_UNLIKELY(state->stackTail.valuePointer - state->stackTop.valuePointer < size)) {
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

    ZrMemoryRawFreeWithType(global, function, sizeof(SZrFunction), ZR_VALUE_TYPE_FUNCTION);
}

TZrString *ZrFunctionGetLocalVariableName(SZrFunction *function, TUInt32 index, TUInt32 programCounter) {
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

SZrCallInfo *ZrFunctionPreCall(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount) {
    do {
        SZrTypeValue *value = ZrStackGetValue(stackPointer);
        EZrValueType type = value->type;
        TBool isNative = value->isNative;
        switch (type) {
            case ZR_VALUE_TYPE_FUNCTION: {
                // todo:
                return ZR_NULL;
            } break;
            case ZR_VALUE_TYPE_NATIVE_POINTER: {
                return ZR_NULL;
            } break;
            case ZR_VALUE_TYPE_NATIVE_DATA: {
                return ZR_NULL;
            } break;
            default: {
                // todo: use CALL meta
                return ZR_NULL;
            }
        }

    } while (ZR_TRUE);
}
