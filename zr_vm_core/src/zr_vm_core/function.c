//
// Created by HeJiahui on 2025/7/20.
//

#include "zr_vm_core/function.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

TZrStackValuePointer ZrCore_Function_CheckStack(struct SZrState *state, TZrSize size, TZrStackValuePointer stackPointer) {
    // Check capacity relative to the actual scratch base, not only the current
    // stackTop. Some meta/native paths allocate above stackTop at functionTop.
    if (ZR_UNLIKELY(state->stackTail.valuePointer - stackPointer < (TZrMemoryOffset) size)) {
        TZrMemoryOffset relative = ZrCore_Stack_SavePointerAsOffset(state, stackPointer);
        TZrSize requiredSize = (TZrSize) (stackPointer - state->stackBase.valuePointer) + size;
        ZrCore_Stack_GrowTo(state, requiredSize, ZR_TRUE);
        TZrStackValuePointer restoredStackPointer = ZrCore_Stack_LoadOffsetToPointer(state, relative);
        return restoredStackPointer;
    }
    return stackPointer;
}

SZrFunction *ZrCore_Function_New(struct SZrState *state) {
    SZrRawObject *newObject = ZrCore_RawObject_New(state, ZR_RAW_OBJECT_TYPE_FUNCTION, sizeof(SZrFunction), ZR_FALSE);
    SZrFunction *function = ZR_CAST_FUNCTION(state, newObject);
    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0;
    function->childFunctionList = ZR_NULL;
    function->childFunctionLength = 0;
    function->instructionsList = ZR_NULL;
    function->instructionsLength = 0;
    function->executionLocationInfoList = ZR_NULL;
    function->executionLocationInfoLength = 0;
    function->catchClauseList = ZR_NULL;
    function->catchClauseCount = 0;
    function->exceptionHandlerList = ZR_NULL;
    function->exceptionHandlerCount = 0;
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
    function->exportedVariables = ZR_NULL;
    function->exportedVariableLength = 0;
    function->functionName = ZR_NULL;  // 函数名，匿名函数为 ZR_NULL
    function->prototypeData = ZR_NULL;
    function->prototypeDataLength = 0;
    function->prototypeCount = 0;
    function->prototypeInstances = ZR_NULL;
    function->prototypeInstancesLength = 0;
    return function;
}

void ZrCore_Function_Free(struct SZrState *state, SZrFunction *function) {
    SZrGlobalState *global = state->global;
    ZR_ASSERT(function != ZR_NULL);
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->instructionsList, function->instructionsLength);
    }
    if (function->childFunctionList != ZR_NULL && function->childFunctionLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->childFunctionList, function->childFunctionLength);
    }
    if (function->constantValueList != ZR_NULL && function->constantValueLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->constantValueList, function->constantValueLength);
    }
    if (function->localVariableList != ZR_NULL && function->localVariableLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->localVariableList, function->localVariableLength);
    }
    if (function->closureValueList != ZR_NULL && function->closureValueLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->closureValueList, function->closureValueLength);
    }
    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->executionLocationInfoList, function->executionLocationInfoLength);
    }
    if (function->catchClauseList != ZR_NULL && function->catchClauseCount > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->catchClauseList, function->catchClauseCount);
    }
    if (function->exceptionHandlerList != ZR_NULL && function->exceptionHandlerCount > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->exceptionHandlerList, function->exceptionHandlerCount);
    }
    if (function->exportedVariables != ZR_NULL && function->exportedVariableLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->exportedVariables, function->exportedVariableLength);
    }
    if (function->prototypeData != ZR_NULL && function->prototypeDataLength > 0) {
        ZrCore_Memory_RawFreeWithType(global, function->prototypeData, function->prototypeDataLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    // prototypeInstances 不需要手动释放，它们由GC管理（作为对象引用）

    // ZrCore_Memory_RawFreeWithType(global, function, sizeof(SZrFunction), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    // function is object, gc free it automatically.
}

SZrString *ZrCore_Function_GetLocalVariableName(SZrFunction *function, TZrUInt32 index, TZrUInt32 programCounter) {
    for (TZrUInt32 i = 0;
         i < function->localVariableLength && function->localVariableList[i].offsetActivate <= programCounter; i++) {
        SZrFunctionLocalVariable *local = &function->localVariableList[i];
        if (programCounter < local->offsetDead && local->stackSlot == index) {
            return local->name;
        }
    }
    return ZR_NULL;
}

void ZrCore_Function_CheckNativeStack(struct SZrState *state) {
    if (state->nestedNativeCalls == ZR_VM_MAX_NATIVE_CALL_STACK) {
        ZrCore_Debug_RunError(state, "C stack overflow");
    } else if (state->nestedNativeCalls >= (ZR_VM_MAX_NATIVE_CALL_STACK) / 10 * 11) {
        ZrCore_Debug_ErrorWhenHandlingError(state);
    }
}

TZrStackValuePointer ZrCore_Function_CheckStackAndGc(struct SZrState *state, TZrSize size,
                                               TZrStackValuePointer stackPointer) {
    if (ZR_UNLIKELY(state->stackTail.valuePointer - stackPointer < (TZrMemoryOffset) size)) {
        TZrMemoryOffset relative = ZrCore_Stack_SavePointerAsOffset(state, stackPointer);
        TZrSize requiredSize = (TZrSize) (stackPointer - state->stackBase.valuePointer) + size;
        // todo: check gc
        ZrCore_Stack_GrowTo(state, requiredSize, ZR_TRUE);
        TZrStackValuePointer restoredStackPointer = ZrCore_Stack_LoadOffsetToPointer(state, relative);
        return restoredStackPointer;
    }
    return stackPointer;
}

static ZR_FORCE_INLINE void function_call_internal(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                   TZrSize resultCount, TZrUInt32 callIncremental, TZrBool isYield) {
    state->nestedNativeCalls += callIncremental;
    state->nestedNativeCallYieldFlag += isYield ? 1 : 0;
    if (ZR_UNLIKELY(state->nestedNativeCalls > ZR_VM_MAX_NATIVE_CALL_STACK)) {
        ZrCore_Function_CheckStack(state, 0, stackPointer);
        ZrCore_Function_CheckNativeStack(state);
    }
    // todo:
    SZrCallInfo *callInfo = ZrCore_Function_PreCall(state, stackPointer, resultCount, ZR_NULL);
    if (callInfo != ZR_NULL) {
        callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
        ZrCore_Execute(state, callInfo);
    }
    state->nestedNativeCallYieldFlag -= isYield ? 1 : 0;
    state->nestedNativeCalls -= callIncremental;
}

void ZrCore_Function_Call(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount) {
    function_call_internal(state, stackPointer, resultCount, 1, ZR_FALSE);
}

void ZrCore_Function_CallWithoutYield(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount) {
    function_call_internal(state, stackPointer, resultCount, 1, ZR_TRUE);
}

void ZrCore_Function_StackAnchorInit(struct SZrState *state,
                               TZrStackValuePointer stackPointer,
                               SZrFunctionStackAnchor *anchor) {
    if (state == ZR_NULL || anchor == ZR_NULL || stackPointer == ZR_NULL) {
        return;
    }

    anchor->offset = ZrCore_Stack_SavePointerAsOffset(state, stackPointer);
}

TZrStackValuePointer ZrCore_Function_StackAnchorRestore(struct SZrState *state,
                                                  const SZrFunctionStackAnchor *anchor) {
    if (state == ZR_NULL || anchor == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Stack_LoadOffsetToPointer(state, anchor->offset);
}

TZrStackValuePointer ZrCore_Function_CheckStackAndAnchor(struct SZrState *state,
                                                   TZrSize size,
                                                   TZrStackValuePointer checkPointer,
                                                   TZrStackValuePointer stackPointer,
                                                   SZrFunctionStackAnchor *anchor) {
    TZrStackValuePointer effectiveCheckPointer;

    if (state == ZR_NULL || stackPointer == ZR_NULL || anchor == ZR_NULL) {
        return stackPointer;
    }

    effectiveCheckPointer = checkPointer != ZR_NULL ? checkPointer : stackPointer;
    ZrCore_Function_StackAnchorInit(state, stackPointer, anchor);
    ZrCore_Function_CheckStackAndGc(state, size, effectiveCheckPointer);
    return ZrCore_Function_StackAnchorRestore(state, anchor);
}

TZrStackValuePointer ZrCore_Function_CallAndRestore(struct SZrState *state,
                                              TZrStackValuePointer stackPointer,
                                              TZrSize resultCount) {
    SZrFunctionStackAnchor anchor;

    if (state == ZR_NULL || stackPointer == ZR_NULL) {
        return stackPointer;
    }

    ZrCore_Function_StackAnchorInit(state, stackPointer, &anchor);
    return ZrCore_Function_CallAndRestoreAnchor(state, &anchor, resultCount);
}

TZrStackValuePointer ZrCore_Function_CallWithoutYieldAndRestore(struct SZrState *state,
                                                          TZrStackValuePointer stackPointer,
                                                          TZrSize resultCount) {
    SZrFunctionStackAnchor anchor;

    if (state == ZR_NULL || stackPointer == ZR_NULL) {
        return stackPointer;
    }

    ZrCore_Function_StackAnchorInit(state, stackPointer, &anchor);
    return ZrCore_Function_CallWithoutYieldAndRestoreAnchor(state, &anchor, resultCount);
}

TZrStackValuePointer ZrCore_Function_CallAndRestoreAnchor(struct SZrState *state,
                                                    const SZrFunctionStackAnchor *anchor,
                                                    TZrSize resultCount) {
    TZrStackValuePointer stackPointer;

    if (state == ZR_NULL || anchor == ZR_NULL) {
        return ZR_NULL;
    }

    stackPointer = ZrCore_Function_StackAnchorRestore(state, anchor);
    if (stackPointer == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Function_Call(state, stackPointer, resultCount);
    return ZrCore_Function_StackAnchorRestore(state, anchor);
}

TZrStackValuePointer ZrCore_Function_CallWithoutYieldAndRestoreAnchor(struct SZrState *state,
                                                                const SZrFunctionStackAnchor *anchor,
                                                                TZrSize resultCount) {
    TZrStackValuePointer stackPointer;

    if (state == ZR_NULL || anchor == ZR_NULL) {
        return ZR_NULL;
    }

    stackPointer = ZrCore_Function_StackAnchorRestore(state, anchor);
    if (stackPointer == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Function_CallWithoutYield(state, stackPointer, resultCount);
    return ZrCore_Function_StackAnchorRestore(state, anchor);
}

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_native_call_info(struct SZrState *state,
                                                                    TZrStackValuePointer basePointer,
                                                                    TZrSize resultCount, EZrCallStatus mask,
                                                                    TZrStackValuePointer topPointer) {
    SZrCallInfo *callInfo = state->callInfoList->next ? state->callInfoList->next : ZrCore_CallInfo_Extend(state);
    callInfo->functionBase.valuePointer = basePointer;
    callInfo->expectedReturnCount = resultCount;
    callInfo->callStatus = mask;
    callInfo->functionTop.valuePointer = topPointer;
    callInfo->returnDestination = ZR_NULL;  /* VM 分支会覆盖；Native 分支保持 ZR_NULL，PostCall 退化为 functionBase */
    callInfo->returnDestinationReusableOffset = 0;
    callInfo->hasReturnDestination = ZR_FALSE;
    return callInfo;
}

static ZR_FORCE_INLINE void function_restore_call_pointers_after_stack_check(
        struct SZrState *state,
        TZrSize size,
        TZrStackValuePointer checkPointer,
        TZrStackValuePointer *stackPointer,
        TZrStackValuePointer *returnDestination) {
    SZrFunctionStackAnchor stackPointerAnchor;
    SZrFunctionStackAnchor returnDestinationAnchor;
    TZrBool hasReturnDestination;

    if (state == ZR_NULL || stackPointer == ZR_NULL || *stackPointer == ZR_NULL) {
        return;
    }

    hasReturnDestination = returnDestination != ZR_NULL && *returnDestination != ZR_NULL;
    ZrCore_Function_StackAnchorInit(state, *stackPointer, &stackPointerAnchor);
    if (hasReturnDestination) {
        ZrCore_Function_StackAnchorInit(state, *returnDestination, &returnDestinationAnchor);
    }

    ZrCore_Function_CheckStackAndGc(state, size, checkPointer);
    *stackPointer = ZrCore_Function_StackAnchorRestore(state, &stackPointerAnchor);
    if (hasReturnDestination) {
        *returnDestination = ZrCore_Function_StackAnchorRestore(state, &returnDestinationAnchor);
    }
}


static ZR_FORCE_INLINE TZrSize function_pre_call_native(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                       TZrSize resultCount, FZrNativeFunction function) {
    TZrSize returnCount = 0;
    SZrCallInfo *callInfo = ZR_NULL;
    function_restore_call_pointers_after_stack_check(
            state, ZR_STACK_NATIVE_CALL_MIN, state->stackTop.valuePointer, &stackPointer, ZR_NULL);
    callInfo = function_pre_call_native_call_info(state, stackPointer, resultCount, ZR_CALL_STATUS_NATIVE_CALL,
                                               state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_MIN);
    callInfo->previous = state->callInfoList;
    state->callInfoList = callInfo;
    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
    if (ZR_UNLIKELY(state->debugHookSignal & ZR_DEBUG_HOOK_MASK_CALL)) {
        TZrInt32 argumentsCount = ZR_CAST_INT(state->stackTop.valuePointer - stackPointer);
        ZrCore_Debug_Hook(state, ZR_DEBUG_HOOK_EVENT_CALL, -1, 1, argumentsCount);
    }
    ZR_THREAD_UNLOCK(state);
    returnCount = function(state);
    ZR_THREAD_LOCK(state);
    ZR_STACK_CHECK_CALL_INFO_STACK_COUNT(state, returnCount);
    ZrCore_Function_PostCall(state, callInfo, returnCount);
    return returnCount;
}

static TZrStackValuePointer function_get_meta_call(SZrState *state, TZrStackValuePointer stackPointer) {
    function_restore_call_pointers_after_stack_check(state, 1, state->stackTop.valuePointer, &stackPointer, ZR_NULL);
    SZrTypeValue *value = ZrCore_Stack_GetValue(stackPointer);
    SZrMeta *metaValue = ZrCore_Value_GetMeta(state, value, ZR_META_CALL);
    if (ZR_UNLIKELY(metaValue == ZR_NULL)) {
        // todo: throw error: no call meta found
        ZrCore_Debug_CallError(state, value);
    }
    for (TZrStackValuePointer p = state->stackTop.valuePointer; p > stackPointer; p--) {
        ZrCore_Stack_CopyValue(state, p, ZrCore_Stack_GetValue(p - 1));
    }
    state->stackTop.valuePointer++;
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(metaValue->function));
    return stackPointer;
}

SZrCallInfo *ZrCore_Function_PreCall(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount,
                               TZrStackValuePointer returnDestination) {
    do {
        SZrTypeValue *value = ZrCore_Stack_GetValue(stackPointer);
        EZrValueType type = value->type;
        TZrBool isNative = value->isNative;
        switch (type) {
            case ZR_VALUE_TYPE_FUNCTION: {
                if (isNative) {
                    SZrClosureNative *native = ZR_CAST_NATIVE_CLOSURE(state, value->value.object);
                    function_pre_call_native(state, stackPointer, resultCount, native->nativeFunction);
                    // todo:
                    return ZR_NULL;
                } else {
                    SZrCallInfo *callInfo = ZR_NULL;
                    SZrFunction *function = ZR_CAST_FUNCTION(state, value->value.object);
                    if (function == ZR_NULL) {
                        return ZR_NULL;
                    }
                    SZrClosure *closure = ZrCore_Closure_New(state, 0);
                    if (closure == ZR_NULL) {
                        return ZR_NULL;
                    }
                    closure->function = function;
                    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
                    value->type = ZR_VALUE_TYPE_CLOSURE;
                    value->isGarbageCollectable = ZR_TRUE;
                    value->isNative = ZR_FALSE;
                    TZrSize argumentsCount = ZR_CAST_INT64(state->stackTop.valuePointer - stackPointer) - 1;
                    TZrSize parametersCount = function->parameterCount;
                    TZrSize stackSize = function->stackSize;
                    function_restore_call_pointers_after_stack_check(state, stackSize, stackPointer, &stackPointer,
                                                                     &returnDestination);
                    callInfo = function_pre_call_native_call_info(state, stackPointer, resultCount, ZR_CALL_STATUS_NONE,
                                                               stackPointer + 1 + stackSize);
                    callInfo->returnDestination = returnDestination;
                    callInfo->hasReturnDestination = returnDestination != ZR_NULL;
                    callInfo->previous = state->callInfoList;
                    state->callInfoList = callInfo;
                    callInfo->context.context.programCounter = function->instructionsList;
                    for (; argumentsCount < parametersCount; argumentsCount++) {
                        SZrTypeValue *stackValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer++);
                        ZrCore_Value_ResetAsNull(stackValue);
                    }
                    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
                    return callInfo;
                }
            } break;
            case ZR_VALUE_TYPE_CLOSURE: {
                // 闭包类型：与 VM 函数类似，但需要通过 ZR_CAST_VM_CLOSURE 转换
                // 闭包不是 native 的（isNative 应该是 ZR_FALSE）
                if (isNative) {
                    // Native 闭包应该使用 ZR_VALUE_TYPE_FUNCTION
                    // 这里不应该到达，但为了安全起见，我们处理它
                    SZrClosureNative *native = ZR_CAST_NATIVE_CLOSURE(state, value->value.object);
                    function_pre_call_native(state, stackPointer, resultCount, native->nativeFunction);
                    return ZR_NULL;
                } else {
                    // VM 闭包：提取其中的函数并调用
                    SZrCallInfo *callInfo = ZR_NULL;
                    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, value->value.object);
                    if (closure->function == ZR_NULL) {
                        // 闭包没有关联的函数，这不应该发生
                        // todo: throw error
                        return ZR_NULL;
                    }
                    SZrFunction *function = closure->function;
                    TZrSize argumentsCount = ZR_CAST_INT64(state->stackTop.valuePointer - stackPointer) - 1;
                    TZrSize parametersCount = function->parameterCount;
                    TZrSize stackSize = function->stackSize;
                    function_restore_call_pointers_after_stack_check(state, stackSize, stackPointer, &stackPointer,
                                                                     &returnDestination);
                    callInfo = function_pre_call_native_call_info(state, stackPointer, resultCount, ZR_CALL_STATUS_NONE,
                                                               stackPointer + 1 + stackSize);
                    callInfo->returnDestination = returnDestination;
                    callInfo->hasReturnDestination = returnDestination != ZR_NULL;
                    callInfo->previous = state->callInfoList;
                    state->callInfoList = callInfo;
                    // 验证闭包的 function 字段是否仍然有效
                    SZrTypeValue *callInfoBaseValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
                    SZrClosure *callInfoClosure = ZR_CAST_VM_CLOSURE(state, callInfoBaseValue->value.object);
                    if (ZR_UNLIKELY(callInfoClosure->function == ZR_NULL)) {
                        // 闭包的 function 字段在创建 callInfo 后变成了 NULL，这不应该发生
                        // todo: throw error
                        return ZR_NULL;
                    }
                    callInfo->context.context.programCounter = function->instructionsList;
                    for (; argumentsCount < parametersCount; argumentsCount++) {
                        SZrTypeValue *stackValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer++);
                        ZrCore_Value_ResetAsNull(stackValue);
                    }
                    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
                    return callInfo;
                }
            } break;
            case ZR_VALUE_TYPE_NATIVE_POINTER: {
                FZrNativeFunction native = ZR_CAST_FUNCTION_POINTER(value);
                function_pre_call_native(state, stackPointer, resultCount, native);
                return ZR_NULL;
            } break;
            case ZR_VALUE_TYPE_NATIVE_DATA: {
                return ZR_NULL;
            } break;
            default: {
                // todo: use CALL meta
                stackPointer = function_get_meta_call(state, stackPointer);
                // return ZR_NULL;
            } break;
        }
    } while (ZR_TRUE);
}

static ZR_FORCE_INLINE void function_move_returns(SZrState *state, TZrStackValuePointer stackPointer,
                                                  TZrSize returnCount, TZrSize expectedReturnCount) {
    switch (expectedReturnCount) {
        case 0: {
            state->stackTop.valuePointer = stackPointer;
            return;
        } break;
        case 1: {
            if (returnCount == 0) {
                ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(stackPointer));
            } else {
                ZrCore_Stack_CopyValue(state, stackPointer, ZrCore_Stack_GetValue(state->stackTop.valuePointer - returnCount));
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
        ZrCore_Stack_CopyValue(state, stackPointer + i, ZrCore_Stack_GetValue(first + i));
    }
    for (TZrSize i = returnCount; i < expectedReturnCount; i++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(stackPointer + i));
    }
    state->stackTop.valuePointer = stackPointer + expectedReturnCount;
}

void ZrCore_Function_PostCall(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount) {
    TZrSize expectedReturnCount = callInfo->expectedReturnCount;
    if (ZR_UNLIKELY(state->debugHookSignal)) {
        ZrCore_Debug_HookReturn(state, callInfo, resultCount);
    }
    // move result
    TZrStackValuePointer dest = (callInfo->hasReturnDestination)
                                    ? callInfo->returnDestination
                                    : callInfo->functionBase.valuePointer;
    function_move_returns(state, dest, resultCount, expectedReturnCount);

    ZR_ASSERT(!(callInfo->callStatus &
                (ZR_CALL_STATUS_DEBUG_HOOK | ZR_CALL_STATUS_HOOK_YIELD | ZR_CALL_STATUS_DECONSTRUCTOR_CALL |
                 ZR_CALL_STATUS_CALL_INFO_TRANSFER | ZR_CALL_STATUS_CLOSE_CALL)));

    state->callInfoList = callInfo->previous;
}
