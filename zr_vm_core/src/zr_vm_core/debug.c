//
// Created by HeJiahui on 2025/6/26.
//
#include "zr_vm_core/debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_debug_conf.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_runtime_limits_conf.h"
#include "zr_vm_common/zr_runtime_sentinel_conf.h"
#include "zr_vm_common/zr_string_conf.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#define ZR_DEBUG_INFO_ALL_BITS                                                                                         \
    (ZR_DEBUG_INFO_SOURCE_FILE | ZR_DEBUG_INFO_LINE_NUMBER | ZR_DEBUG_INFO_CLOSURE | ZR_DEBUG_INFO_TAIL_CALL |         \
     ZR_DEBUG_INFO_FUNCTION_NAME | ZR_DEBUG_INFO_RETURN_VALUE | ZR_DEBUG_INFO_LINE_TABLE | ZR_DEBUG_INFO_PUSH_FUNCTION)

static TZrNativeString debug_get_string_native(SZrString *stringValue, TZrSize *outLength) {
    TZrNativeString nativeString;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (stringValue == ZR_NULL) {
        return ZR_NULL;
    }

    nativeString = ZrCore_String_GetNativeString(stringValue);
    if (nativeString != ZR_NULL && outLength != ZR_NULL) {
        *outLength = strlen(nativeString);
    }
    return nativeString;
}

static TZrUInt32 debug_get_current_instruction_offset(SZrCallInfo *callInfo, SZrFunction *function) {
    if (callInfo == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        callInfo->context.context.programCounter == ZR_NULL) {
        return 0;
    }

    if (callInfo->context.context.programCounter < function->instructionsList) {
        return 0;
    }

    return (TZrUInt32)(callInfo->context.context.programCounter - function->instructionsList);
}

static const SZrFunctionLocalVariable *debug_find_active_local(const SZrFunction *function,
                                                               TZrUInt32 programCounter,
                                                               TZrInt32 localIndex) {
    TZrInt32 activeIndex = 0;
    TZrUInt32 index;

    if (function == ZR_NULL || function->localVariableList == ZR_NULL || localIndex <= 0) {
        return ZR_NULL;
    }

    for (index = 0;
         index < function->localVariableLength && function->localVariableList[index].offsetActivate <= programCounter;
         index++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[index];
        if (local->name == ZR_NULL || programCounter >= local->offsetDead) {
            continue;
        }
        activeIndex++;
        if (activeIndex == localIndex) {
            return local;
        }
    }

    return ZR_NULL;
}

static TZrStackValuePointer debug_get_frame_base(SZrCallInfo *callInfo) {
    if (callInfo == ZR_NULL || !ZR_CALL_INFO_IS_VM(callInfo) || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }

    return callInfo->functionBase.valuePointer + 1;
}

static TZrBool debug_frame_slot_is_inline_struct(const SZrFunction *function, TZrUInt32 stackSlot) {
    const SZrFunctionFrameSlotLayout *slotLayout;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    slotLayout = ZrCore_Function_FindFrameSlotLayout(function, stackSlot);
    return (TZrBool)(slotLayout != ZR_NULL &&
                     slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT);
}

static SZrTypeValue *debug_get_frame_value_slot(SZrState *state,
                                                const SZrFunction *function,
                                                SZrCallInfo *callInfo,
                                                TZrUInt32 stackSlot) {
    TZrStackValuePointer frameBase;
    const SZrFunctionFrameSlotLayout *slotLayout;
    SZrStackFramePlace place;

    frameBase = debug_get_frame_base(callInfo);
    if (frameBase == ZR_NULL) {
        return ZR_NULL;
    }

    if (function != ZR_NULL && function->frameSlotLayouts != ZR_NULL && function->frameSlotLayoutLength > 0u) {
        slotLayout = ZrCore_Function_FindFrameSlotLayout(function, stackSlot);
        if (slotLayout != ZR_NULL &&
            slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
            slotLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue) &&
            ZrCore_Function_MakeFrameSlotPlace(state, function, frameBase, stackSlot, &place)) {
            return (SZrTypeValue *)place.address;
        }
        if (slotLayout != ZR_NULL &&
            slotLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            return ZR_NULL;
        }
    }

    return ZrCore_Stack_GetValue(frameBase + stackSlot);
}

static void debug_snapshot_value(SZrState *state, SZrTypeValue *destination, SZrTypeValue *source) {
    ZR_UNUSED_PARAMETER(state);

    if (destination == ZR_NULL || source == ZR_NULL) {
        return;
    }

    *destination = *source;
    if (!destination->isGarbageCollectable ||
        ZR_VALUE_IS_TYPE_NULL(destination->type) ||
        destination->value.object == ZR_NULL) {
        destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
        destination->ownershipControl = ZR_NULL;
        destination->ownershipWeakRef = ZR_NULL;
    } else if (destination->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE) {
        destination->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_BORROWED;
        destination->ownershipWeakRef = ZR_NULL;
    }
}

static TZrDebugSignal debug_instruction_trap_from_hook_signal(TZrUInt32 hookSignal) {
    return (TZrDebugSignal)(((hookSignal & (ZR_DEBUG_HOOK_MASK_LINE | ZR_DEBUG_HOOK_MASK_COUNT)) != 0u)
                                    ? hookSignal
                                    : ZR_DEBUG_SIGNAL_NONE);
}

static void debug_reset_hook_count(SZrState *state) {
    if (state != ZR_NULL) {
        state->debugHookCount = state->baseDebugHookCount;
    }
}

static void debug_settraps(SZrCallInfo *callInfo, TZrDebugSignal signal) {
    for (; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        if (ZR_CALL_INFO_IS_VM(callInfo)) {
            callInfo->context.context.trap = signal;
        }
    }
}

TZrBool ZrCore_Debug_GetStack(struct SZrState *state, TZrUInt32 level, SZrDebugActivation *outActivation) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || outActivation == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outActivation, 0, sizeof(*outActivation));
    callInfo = state->callInfoList;

    for (; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
        if (function == ZR_NULL) {
            continue;
        }
        if (level == 0u) {
            outActivation->callInfo = callInfo;
            outActivation->function = function;
            return ZR_TRUE;
        }
        level--;
    }

    return ZR_FALSE;
}

TZrBool ZrCore_Debug_GetInfo(struct SZrState *state,
                             const SZrDebugActivation *activation,
                             EZrDebugInfoType type,
                             SZrDebugInfo *outInfo) {
    SZrCallInfo *callInfo;
    SZrFunction *function;

    if (state == ZR_NULL || activation == ZR_NULL || outInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outInfo, 0, sizeof(*outInfo));
    callInfo = activation->callInfo;
    function = activation->function;
    if (callInfo == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    outInfo->callInfo = callInfo;

    if ((type & ZR_DEBUG_INFO_FUNCTION_NAME) != 0) {
        outInfo->name = debug_get_string_native(function->functionName, ZR_NULL);
        outInfo->nameWhat = ZR_DEBUG_NAMEWHAT_UNKNOWN;
        outInfo->scope = ZR_DEBUG_SCOPE_FUNCTION;
    }

    if ((type & ZR_DEBUG_INFO_SOURCE_FILE) != 0) {
        outInfo->isNative = ZrCore_CallInfo_IsNative(callInfo);
        outInfo->source = debug_get_string_native(function->sourceCodeList, &outInfo->sourceLength);
        outInfo->definedLineStart = function->lineInSourceStart;
        outInfo->definedLineEnd = function->lineInSourceEnd;
    }

    if ((type & ZR_DEBUG_INFO_CLOSURE) != 0) {
        outInfo->closureValuesCount = function->closureValueLength;
        outInfo->parametersCount = function->parameterCount;
        outInfo->hasVariableParameters = function->hasVariableArguments;
    }

    if ((type & ZR_DEBUG_INFO_TAIL_CALL) != 0) {
        outInfo->isTailCall = (TZrBool)((callInfo->callStatus & ZR_CALL_STATUS_TAIL_CALL) != 0);
    }

    if ((type & ZR_DEBUG_INFO_RETURN_VALUE) != 0) {
        outInfo->transferStart = callInfo->yieldContext.transferStart;
        outInfo->transferCount = callInfo->yieldContext.transferCount;
    }

    if ((type & ZR_DEBUG_INFO_LINE_NUMBER) != 0) {
        outInfo->currentLine = ZrCore_Exception_FindSourceLine(function,
                                                               debug_get_current_instruction_offset(callInfo, function));
        if (outInfo->currentLine == 0 && state->debugLastFunction == function &&
            state->debugLastLine != ZR_RUNTIME_DEBUG_HOOK_LINE_NONE) {
            outInfo->currentLine = state->debugLastLine;
        }
    }

    if ((type & ZR_DEBUG_INFO_PUSH_FUNCTION) != 0) {
        SZrTypeValue *callableValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
        SZrTypeValue *stackValue;

        if (callableValue == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Function_CheckStackAndGc(state, 1, state->stackTop.valuePointer);
        stackValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
        if (stackValue == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Value_Copy(state, stackValue, callableValue);
        state->stackTop.valuePointer++;
    }

    return ZR_TRUE;
}

TZrBool ZrCore_DebugInfo_Get(struct SZrState *state, EZrDebugInfoType type, SZrDebugInfo *debugInfo) {
    SZrDebugActivation activation;
    EZrDebugInfoType effectiveType = type != 0 ? type : (EZrDebugInfoType)ZR_DEBUG_INFO_ALL_BITS;

    if (!ZrCore_Debug_GetStack(state, 0, &activation)) {
        if (debugInfo != ZR_NULL) {
            memset(debugInfo, 0, sizeof(*debugInfo));
        }
        return ZR_FALSE;
    }

    return ZrCore_Debug_GetInfo(state, &activation, effectiveType, debugInfo);
}

void ZrCore_Debug_SetHook(struct SZrState *state, FZrDebugHook hook, TZrUInt32 mask, TZrUInt32 count) {
    TZrUInt32 effectiveMask = mask;
    TZrDebugSignal trapSignal;

    if (state == ZR_NULL) {
        return;
    }

    if (count == 0u) {
        effectiveMask &= ~ZR_DEBUG_HOOK_MASK_COUNT;
    }
    if (hook == ZR_NULL || effectiveMask == 0u) {
        hook = ZR_NULL;
        effectiveMask = 0u;
        count = 0u;
    }

    state->debugHook = hook;
    state->baseDebugHookCount = count;
    debug_reset_hook_count(state);
    state->debugHookSignal = (TZrDebugSignal)effectiveMask;
    state->allowDebugHook = ZR_TRUE;
    state->debugLastFunction = ZR_NULL;
    state->debugLastLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;

    trapSignal = debug_instruction_trap_from_hook_signal(effectiveMask);
    debug_settraps(state->callInfoList, trapSignal);
}

FZrDebugHook ZrCore_Debug_GetHook(struct SZrState *state) {
    return state != ZR_NULL ? state->debugHook : ZR_NULL;
}

TZrUInt32 ZrCore_Debug_GetHookMask(struct SZrState *state) {
    return state != ZR_NULL ? state->debugHookSignal : 0u;
}

TZrUInt32 ZrCore_Debug_GetHookCount(struct SZrState *state) {
    return state != ZR_NULL ? state->baseDebugHookCount : 0u;
}

TZrNativeString ZrCore_Debug_GetLocal(struct SZrState *state,
                                      const SZrDebugActivation *activation,
                                      TZrInt32 localIndex,
                                      struct SZrTypeValue *outValue) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    const SZrFunctionLocalVariable *local;
    TZrStackValuePointer frameBase;
    TZrUInt32 programCounter;
    TZrNativeString name;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (state == ZR_NULL || activation == ZR_NULL || localIndex <= 0) {
        return ZR_NULL;
    }

    callInfo = activation->callInfo;
    function = activation->function;
    if (callInfo == ZR_NULL || function == ZR_NULL || !ZR_CALL_INFO_IS_VM(callInfo)) {
        return ZR_NULL;
    }

    programCounter = debug_get_current_instruction_offset(callInfo, function);
    local = debug_find_active_local(function, programCounter, localIndex);
    if (local == ZR_NULL) {
        return ZR_NULL;
    }

    name = debug_get_string_native(local->name, ZR_NULL);
    if (outValue == ZR_NULL) {
        return name;
    }

    frameBase = debug_get_frame_base(callInfo);
    if (frameBase == ZR_NULL) {
        return ZR_NULL;
    }

    if (debug_frame_slot_is_inline_struct(function, local->stackSlot)) {
        (void)ZrCore_Function_CopyFrameSlotInlineToObjectValue(state, function, frameBase, local->stackSlot, outValue);
        return name;
    }

    {
        SZrTypeValue *slot = debug_get_frame_value_slot(state, function, callInfo, local->stackSlot);
        if (slot == ZR_NULL) {
            return ZR_NULL;
        }
        debug_snapshot_value(state, outValue, slot);
    }

    return name;
}

TZrNativeString ZrCore_Debug_SetLocal(struct SZrState *state,
                                      const SZrDebugActivation *activation,
                                      TZrInt32 localIndex,
                                      const struct SZrTypeValue *value) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    const SZrFunctionLocalVariable *local;
    TZrUInt32 programCounter;
    SZrTypeValue *slot;

    if (state == ZR_NULL || activation == ZR_NULL || value == ZR_NULL || localIndex <= 0) {
        return ZR_NULL;
    }

    callInfo = activation->callInfo;
    function = activation->function;
    if (callInfo == ZR_NULL || function == ZR_NULL || !ZR_CALL_INFO_IS_VM(callInfo)) {
        return ZR_NULL;
    }

    programCounter = debug_get_current_instruction_offset(callInfo, function);
    local = debug_find_active_local(function, programCounter, localIndex);
    if (local == ZR_NULL || debug_frame_slot_is_inline_struct(function, local->stackSlot)) {
        return ZR_NULL;
    }

    slot = debug_get_frame_value_slot(state, function, callInfo, local->stackSlot);
    if (slot == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_Copy(state, slot, value);
    return debug_get_string_native(local->name, ZR_NULL);
}

TZrNativeString ZrCore_Debug_GetUpvalue(struct SZrState *state,
                                        struct SZrClosure *closure,
                                        TZrInt32 upvalueIndex,
                                        struct SZrTypeValue *outValue) {
    TZrUInt32 index;
    SZrClosureValue *closureValue;
    SZrTypeValue *value;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (state == ZR_NULL || closure == ZR_NULL || closure->function == ZR_NULL || upvalueIndex <= 0) {
        return ZR_NULL;
    }

    index = (TZrUInt32)(upvalueIndex - 1);
    if (index >= closure->closureValueCount || index >= closure->function->closureValueLength ||
        closure->closureValuesExtend[index] == ZR_NULL || closure->function->closureValueList == ZR_NULL) {
        return ZR_NULL;
    }

    closureValue = closure->closureValuesExtend[index];
    value = ZrCore_ClosureValue_GetValue(closureValue);
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    if (outValue != ZR_NULL) {
        debug_snapshot_value(state, outValue, value);
    }
    return debug_get_string_native(closure->function->closureValueList[index].name, ZR_NULL);
}

TZrNativeString ZrCore_Debug_SetUpvalue(struct SZrState *state,
                                        struct SZrClosure *closure,
                                        TZrInt32 upvalueIndex,
                                        const struct SZrTypeValue *value) {
    TZrUInt32 index;
    SZrClosureValue *closureValue;
    SZrTypeValue *targetValue;

    if (state == ZR_NULL || closure == ZR_NULL || closure->function == ZR_NULL || value == ZR_NULL ||
        upvalueIndex <= 0) {
        return ZR_NULL;
    }

    index = (TZrUInt32)(upvalueIndex - 1);
    if (index >= closure->closureValueCount || index >= closure->function->closureValueLength ||
        closure->closureValuesExtend[index] == ZR_NULL || closure->function->closureValueList == ZR_NULL) {
        return ZR_NULL;
    }

    closureValue = closure->closureValuesExtend[index];
    targetValue = ZrCore_ClosureValue_GetValue(closureValue);
    if (targetValue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_Copy(state, targetValue, value);
    return debug_get_string_native(closure->function->closureValueList[index].name, ZR_NULL);
}

TZrPtr ZrCore_Debug_GetUpvalueId(struct SZrState *state, struct SZrClosure *closure, TZrInt32 upvalueIndex) {
    TZrUInt32 index;

    ZR_UNUSED_PARAMETER(state);

    if (closure == ZR_NULL || closure->function == ZR_NULL || upvalueIndex <= 0) {
        return ZR_NULL;
    }

    index = (TZrUInt32)(upvalueIndex - 1);
    if (index >= closure->closureValueCount || index >= closure->function->closureValueLength ||
        closure->closureValuesExtend[index] == ZR_NULL) {
        return ZR_NULL;
    }

    return closure->closureValuesExtend[index];
}

void ZrCore_Debug_SetTraceObserver(struct SZrState *state,
                                   FZrDebugTraceObserver observer,
                                   TZrPtr userData) {
    if (state == ZR_NULL) {
        return;
    }

    state->debugTraceObserver = observer;
    state->debugTraceUserData = userData;
}

void ZrCore_Debug_CallError(struct SZrState *state, struct SZrTypeValue *value) {
    ZrCore_Debug_RunError(state,
                          "Attempted to call non-callable value (type=%d)",
                          value != ZR_NULL ? (int)value->type : -1);
}

TZrDebugSignal ZrCore_Debug_TraceExecution(struct SZrState *state, const TZrInstruction *programCounter) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    const TZrInstruction *currentProgramCounter;
    TZrUInt32 currentInstructionOffset;
    TZrUInt32 currentLine;
    FZrDebugTraceObserver traceObserver;
    TZrDebugSignal observerTrap = ZR_DEBUG_SIGNAL_NONE;
    TZrDebugSignal trap = ZR_DEBUG_SIGNAL_NONE;

    if (state == ZR_NULL || programCounter == ZR_NULL) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    callInfo = state->callInfoList;
    if (callInfo == ZR_NULL || ZrCore_CallInfo_IsNative(callInfo)) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        callInfo->context.context.trap = ZR_DEBUG_SIGNAL_NONE;
        return ZR_DEBUG_SIGNAL_NONE;
    }

    currentProgramCounter = programCounter + 1;
    if (currentProgramCounter < function->instructionsList ||
        currentProgramCounter >= function->instructionsList + function->instructionsLength) {
        callInfo->context.context.trap = ZR_DEBUG_SIGNAL_NONE;
        return ZR_DEBUG_SIGNAL_NONE;
    }

    callInfo->context.context.programCounter = currentProgramCounter;
    currentInstructionOffset = (TZrUInt32)(currentProgramCounter - function->instructionsList);
    state->previousProgramCounter = currentInstructionOffset;

    currentLine = ZrCore_Exception_FindSourceLine(function, currentInstructionOffset);

    if ((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0) {
        if (currentLine != 0 &&
            (state->debugLastFunction != function || state->debugLastLine != currentLine)) {
            state->debugLastFunction = function;
            state->debugLastLine = currentLine;
            ZrCore_Debug_Hook(state, ZR_DEBUG_HOOK_EVENT_LINE, currentLine, 0, 0);
        } else if (currentLine != 0) {
            state->debugLastFunction = function;
            state->debugLastLine = currentLine;
        } else {
            state->debugLastFunction = function;
            state->debugLastLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;
        }
        trap = state->debugHookSignal;
    }

    if ((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_COUNT) != 0 && state->baseDebugHookCount > 0u) {
        if (state->debugHookCount > 0u) {
            state->debugHookCount--;
        }
        if (state->debugHookCount == 0u) {
            debug_reset_hook_count(state);
            ZrCore_Debug_Hook(state,
                              ZR_DEBUG_HOOK_EVENT_COUNT,
                              ZR_RUNTIME_DEBUG_HOOK_LINE_NONE,
                              0,
                              0);
        }
        trap = state->debugHookSignal;
    }

    traceObserver = state->debugTraceObserver;
    if (traceObserver != ZR_NULL) {
        observerTrap = traceObserver(state,
                                     function,
                                     currentProgramCounter,
                                     currentInstructionOffset,
                                     currentLine,
                                     state->debugTraceUserData);
        if (observerTrap != ZR_DEBUG_SIGNAL_NONE) {
            trap = observerTrap;
        }
    }

    callInfo->context.context.trap = trap;
    return trap;
}

ZR_NO_RETURN void ZrCore_Debug_RunError(struct SZrState *state, TZrNativeString format, ...) {
    TZrChar errorBuffer[ZR_RUNTIME_ERROR_BUFFER_LENGTH];
    TZrNativeString errorMessage;
    va_list args;

    if (state == ZR_NULL || format == ZR_NULL) {
        ZR_ABORT();
    }

    va_start(args, format);
    vsnprintf(errorBuffer, sizeof(errorBuffer), format, args);
    va_end(args);

    errorBuffer[sizeof(errorBuffer) - 1] = '\0';
    errorMessage = errorBuffer[0] != '\0' ? errorBuffer : "Runtime error";

    // 创建错误消息字符串对象
    SZrString *errorString = ZrCore_String_CreateFromNative(state, errorMessage);
    if (errorString == ZR_NULL) {
        // 如果创建字符串失败，检查是否有 panic handling function
        SZrGlobalState *global = state->global;
        if (global != ZR_NULL && global->panicHandlingFunction != ZR_NULL) {
            ZR_THREAD_UNLOCK(state);
            global->panicHandlingFunction(state);
        }
        ZR_ABORT();
    }

    // 确保栈有足够空间
    ZrCore_Function_CheckStackAndGc(state, 1, state->stackTop.valuePointer);

    // 将错误消息字符串放到栈上
    SZrTypeValue *errorValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    ZrCore_Value_InitAsRawObject(state, errorValue, ZR_CAST_RAW_OBJECT_AS_SUPER(errorString));
    errorValue->type = ZR_VALUE_TYPE_STRING;
    errorValue->isGarbageCollectable = ZR_TRUE;
    errorValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    // 运行时错误必须先规范化为 Error/RuntimeError 对象，这样 VM catch 路径才能稳定读取
    // message/exception/stacks，而不是拿到一个裸字符串 payload。
    if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_RUNTIME_ERROR)) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
    }

    // 运行时错误统一进入 VM 异常链路；是否 panic 由未捕获边界决定。
    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    ZR_ABORT();
}


void ZrCore_Debug_ErrorWhenHandlingError(struct SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL && state->global->panicHandlingFunction != ZR_NULL) {
        ZR_THREAD_UNLOCK(state);
        state->global->panicHandlingFunction(state);
    }
    ZR_ABORT();
}


void ZrCore_Debug_Hook(struct SZrState *state, EZrDebugHookEvent event, TZrUInt32 line, TZrUInt32 transferStart,
                 TZrUInt32 transferCount) {
    FZrDebugHook hook = state->debugHook;
    if (hook && state->allowDebugHook) {
        EZrCallStatus mask = ZR_CALL_STATUS_DEBUG_HOOK;
        SZrCallInfo *callInfo = state->callInfoList;
        TZrMemoryOffset top = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
        TZrMemoryOffset callInfoTop = ZrCore_Stack_SavePointerAsOffset(state, callInfo->functionTop.valuePointer);
        SZrDebugInfo debugInfo;
        memset(&debugInfo, 0, sizeof(debugInfo));
        debugInfo.event = event;
        debugInfo.currentLine = line;
        debugInfo.callInfo = callInfo;
        if (transferCount != 0) {
            mask |= ZR_CALL_STATUS_CALL_INFO_TRANSFER;
            callInfo->yieldContext.transferStart = transferStart;
            callInfo->yieldContext.transferCount = transferCount;
        }
        if (ZR_CALL_INFO_IS_VM(callInfo) && state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
            state->stackTop.valuePointer = state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_RESERVED_MIN;
        }
        state->allowDebugHook = ZR_FALSE;
        callInfo->callStatus |= mask;
        ZR_THREAD_UNLOCK(state);
        hook(state, &debugInfo);
        ZR_THREAD_LOCK(state);

        ZR_ASSERT(!state->allowDebugHook);
        state->allowDebugHook = ZR_TRUE;
        callInfo->functionTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, callInfoTop);
        state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, top);
        callInfo->callStatus &= ~mask;
    }
}

void ZrCore_Debug_HookReturn(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount) {
    if (state->debugHookSignal & ZR_DEBUG_HOOK_MASK_RETURN) {
        TZrStackValuePointer stackPointer = callInfo->functionTop.valuePointer - resultCount;
        TZrUInt32 totalArgumentsCount = 0;
        TZrInt32 transferStart = 0;
        if (ZR_CALL_INFO_IS_VM(callInfo)) {
            SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
            if (function != ZR_NULL && function->hasVariableArguments) {
                totalArgumentsCount = (TZrUInt32)(callInfo->context.context.variableArgumentCount +
                                                  function->parameterCount + 1);
            }
        }
        callInfo->functionBase.valuePointer += totalArgumentsCount;
        transferStart = (TZrUInt32)(stackPointer - callInfo->functionBase.valuePointer);
        ZrCore_Debug_Hook(state,
                          ZR_DEBUG_HOOK_EVENT_RETURN,
                          ZR_RUNTIME_DEBUG_HOOK_LINE_NONE,
                          transferStart,
                          (TZrUInt32)resultCount);
        callInfo->functionBase.valuePointer -= totalArgumentsCount;
    }
    callInfo = callInfo->previous;
    if (callInfo != ZR_NULL && ZR_CALL_INFO_IS_VM(callInfo)) {
        SZrFunction *function = ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
        if (function != ZR_NULL && function->instructionsList != ZR_NULL &&
            callInfo->context.context.programCounter >= function->instructionsList) {
            state->previousProgramCounter =
                    ZR_CAST_INT64(callInfo->context.context.programCounter - function->instructionsList) - 1;
        }
    }
}

// 获取prototype类型名称字符串
static const char *get_prototype_type_name(EZrObjectPrototypeType type) {
    switch (type) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return "class";
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "struct";
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return "interface";
        case ZR_OBJECT_PROTOTYPE_TYPE_ENUM:
            return "enum";
        case ZR_OBJECT_PROTOTYPE_TYPE_UNION:
            return "union";
        case ZR_OBJECT_PROTOTYPE_TYPE_MODULE:
            return "module";
        case ZR_OBJECT_PROTOTYPE_TYPE_NATIVE:
            return "native";
        case ZR_OBJECT_PROTOTYPE_TYPE_INVALID:
        default:
            return "unknown";
    }
}

// 输出Prototype信息（增强版本，显示详细信息）
ZR_CORE_API void ZrCore_Debug_PrintPrototype(struct SZrState *state, struct SZrObjectPrototype *prototype, FILE *output) {
    if (state == ZR_NULL || prototype == ZR_NULL || output == ZR_NULL) {
        return;
    }

    // 输出名称和类型
    const char *typeName = get_prototype_type_name(prototype->type);
    TZrNativeString nameStr = ZR_NULL;
    if (prototype->name != ZR_NULL) {
        nameStr = ZrCore_String_GetNativeStringShort(prototype->name);
        if (nameStr == ZR_NULL) {
            nameStr = *ZrCore_String_GetNativeStringLong(prototype->name);
        }
    }
    fprintf(output, "%s %s", typeName, nameStr != ZR_NULL ? nameStr : "<unnamed>");

    // 输出继承链（递归显示所有基类）
    fprintf(output, " ");
    TZrBool hasInheritance = ZR_FALSE;
    struct SZrObjectPrototype *current = prototype->superPrototype;
    while (current != ZR_NULL) {
        if (hasInheritance) {
            fprintf(output, ", ");
        } else {
            fprintf(output, ": ");
        }
        if (current->name != ZR_NULL) {
            TZrNativeString superName = ZrCore_String_GetNativeStringShort(current->name);
            if (superName == ZR_NULL) {
                superName = *ZrCore_String_GetNativeStringLong(current->name);
            }
            fprintf(output, "%s", superName != ZR_NULL ? superName : "<unknown>");
        } else {
            fprintf(output, "<unnamed>");
        }
        hasInheritance = ZR_TRUE;
        current = current->superPrototype;
    }
    fprintf(output, "\n");
    fprintf(output, "{\n");

    // 输出Struct的字段信息（包含偏移量）
    if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        SZrStructPrototype *structProto = (SZrStructPrototype *) prototype;
        if (structProto->keyOffsetMap.isValid && structProto->keyOffsetMap.buckets != ZR_NULL &&
            structProto->keyOffsetMap.elementCount > 0) {
            fprintf(output, "  // Fields (with offsets):\n");
            for (TZrSize i = 0; i < structProto->keyOffsetMap.capacity; i++) {
                SZrHashKeyValuePair *pair = structProto->keyOffsetMap.buckets[i];
                while (pair != ZR_NULL) {
                    if (pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL) {
                        SZrString *fieldName = ZR_CAST_STRING(state, pair->key.value.object);
                        TZrNativeString fieldNameStr = ZrCore_String_GetNativeStringShort(fieldName);
                        if (fieldNameStr == ZR_NULL) {
                            fieldNameStr = *ZrCore_String_GetNativeStringLong(fieldName);
                        }
                        TZrUInt64 offset = pair->value.value.nativeObject.nativeUInt64;
                        fprintf(output, "    %s (offset: %llu bytes)\n",
                                fieldNameStr != ZR_NULL ? fieldNameStr : "<unknown>", (unsigned long long) offset);
                    }
                    pair = pair->next;
                }
            }
        } else {
            fprintf(output, "  // Fields: (none)\n");
        }
    }

    // 输出Meta方法（包含函数信息）
    fprintf(output, "  // Meta Methods:\n");
    TZrBool hasMeta = ZR_FALSE;
    for (EZrMetaType metaType = 0; metaType < ZR_META_ENUM_MAX; metaType++) {
        if (prototype->metaTable.metas[metaType] != ZR_NULL) {
            hasMeta = ZR_TRUE;
            const char *metaName = CZrMetaName[metaType];
            SZrMeta *meta = prototype->metaTable.metas[metaType];
            if (meta->function != ZR_NULL && meta->function->functionName != ZR_NULL) {
                TZrNativeString funcName = ZrCore_String_GetNativeStringShort(meta->function->functionName);
                if (funcName == ZR_NULL) {
                    funcName = *ZrCore_String_GetNativeStringLong(meta->function->functionName);
                }
                fprintf(output, "    @%s -> %s (params: %u)\n", metaName != ZR_NULL ? metaName : "<unknown>",
                        funcName != ZR_NULL ? funcName : "<anonymous>", (unsigned int) meta->function->parameterCount);
            } else {
                fprintf(output, "    @%s -> <anonymous> (params: %u)\n", metaName != ZR_NULL ? metaName : "<unknown>",
                        meta->function != ZR_NULL ? (unsigned int) meta->function->parameterCount : 0);
            }
        }
    }
    if (!hasMeta) {
        fprintf(output, "    (none)\n");
    }

    fprintf(output, "}\n");
}

// 输出Object信息
ZR_CORE_API void ZrCore_Debug_PrintObject(struct SZrState *state, struct SZrObject *object, FILE *output) {
    if (state == ZR_NULL || object == ZR_NULL || output == ZR_NULL) {
        return;
    }

    // 输出prototype信息
    if (object->prototype != ZR_NULL && object->prototype->name != ZR_NULL) {
        TZrNativeString protoName = ZrCore_String_GetNativeStringShort(object->prototype->name);
        if (protoName == ZR_NULL) {
            protoName = *ZrCore_String_GetNativeStringLong(object->prototype->name);
        }
        const char *typeName = get_prototype_type_name(object->prototype->type);
        fprintf(output, "%s %s", typeName, protoName != ZR_NULL ? protoName : "<unnamed>");
    } else {
        fprintf(output, "object");
    }

    fprintf(output, " {\n");

    // 输出字段值
    if (object->nodeMap.isValid && object->nodeMap.buckets != ZR_NULL && object->nodeMap.elementCount > 0) {
        fprintf(output, "  // Fields:\n");
        TZrSize count = 0;

        for (TZrSize i = 0; i < object->nodeMap.capacity && count < ZR_DEBUG_MAX_FIELDS; i++) {
            SZrHashKeyValuePair *pair = object->nodeMap.buckets[i];
            while (pair != ZR_NULL && count < ZR_DEBUG_MAX_FIELDS) {
                // 输出键名
                if (pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL) {
                    SZrString *keyStr = ZR_CAST_STRING(state, pair->key.value.object);
                    TZrNativeString keyName = ZrCore_String_GetNativeStringShort(keyStr);
                    if (keyName == ZR_NULL) {
                        keyName = *ZrCore_String_GetNativeStringLong(keyStr);
                    }
                    fprintf(output, "    %s = ", keyName != ZR_NULL ? keyName : "<unknown>");

                    // 输出值
                    SZrString *valueStr = ZrCore_Value_ConvertToString(state, &pair->value);
                    if (valueStr != ZR_NULL) {
                        TZrNativeString valueName = ZrCore_String_GetNativeStringShort(valueStr);
                        if (valueName == ZR_NULL) {
                            valueName = *ZrCore_String_GetNativeStringLong(valueStr);
                        }
                        fprintf(output, "%s", valueName != ZR_NULL ? valueName : "<unknown>");
                    } else {
                        // 如果无法转换为字符串，输出类型信息
                        fprintf(output, "<%s>", pair->value.type < ZR_VALUE_TYPE_ENUM_MAX ? "value" : "unknown");
                    }
                    fprintf(output, "\n");
                    count++;
                }
                pair = pair->next;
            }
        }

        if (count >= ZR_DEBUG_MAX_FIELDS && object->nodeMap.elementCount > ZR_DEBUG_MAX_FIELDS) {
            fprintf(output, "    ... (and %zu more fields)\n",
                    (size_t) (object->nodeMap.elementCount - ZR_DEBUG_MAX_FIELDS));
        }
    } else {
        fprintf(output, "  // (no fields)\n");
    }

    fprintf(output, "}\n");
}

// 获取访问修饰符名称
static const char *get_access_modifier_name(TZrUInt32 modifier) {
    switch (modifier) {
        case ZR_ACCESS_CONSTANT_PUBLIC:
            return "public";
        case ZR_ACCESS_CONSTANT_PRIVATE:
            return "private";
        case ZR_ACCESS_CONSTANT_PROTECTED:
            return "protected";
        default:
            return "unknown";
    }
}

// 从常量池中解析并字符串化prototype信息
ZR_CORE_API void ZrCore_Debug_PrintPrototypesFromData(struct SZrState *state, struct SZrFunction *entryFunction,
                                                FILE *output) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || output == ZR_NULL) {
        return;
    }

    // 检查prototypeData是否有效
    if (entryFunction->prototypeData == ZR_NULL || entryFunction->prototypeDataLength == 0 || 
        entryFunction->prototypeCount == 0) {
        fprintf(output, "// No prototype data found\n");
        return;
    }

    // 检查常量池是否有效（用于字符串索引解析）
    if (entryFunction->constantValueList == ZR_NULL || entryFunction->constantValueLength == 0) {
        fprintf(output, "// Error: No constants found (needed for string index resolution)\n");
        return;
    }

    TZrUInt32 prototypeCount = entryFunction->prototypeCount;
    fprintf(output, "// ========== PROTOTYPES FROM DATA (count: %u) ==========\n",
            (unsigned int) prototypeCount);

    // 读取prototype数据（跳过头部的prototypeCount）
    const TZrByte *prototypeData = entryFunction->prototypeData + sizeof(TZrUInt32);
    TZrSize remainingDataSize = entryFunction->prototypeDataLength - sizeof(TZrUInt32);
    const TZrByte *currentPos = prototypeData;

    // 遍历每个prototype
    for (TZrUInt32 i = 0; i < prototypeCount; i++) {
        if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
            fprintf(output, "// Error: Insufficient data for prototype %u\n", (unsigned int) (i + 1));
            break;
        }

        // 解析SZrCompiledPrototypeInfo
        const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
        TZrUInt32 nameStringIndex = protoInfo->nameStringIndex;
        TZrUInt32 type = protoInfo->type;
        TZrUInt32 accessModifier = protoInfo->accessModifier;
        TZrUInt32 inheritsCount = protoInfo->inheritsCount;
        TZrUInt32 membersCount = protoInfo->membersCount;
        TZrUInt32 decoratorsCount = protoInfo->decoratorsCount;

        // 计算当前prototype数据的大小
        TZrSize inheritArraySize = inheritsCount * sizeof(TZrUInt32);
        TZrSize decoratorArraySize = decoratorsCount * sizeof(TZrUInt32);
        TZrSize membersArraySize = membersCount * sizeof(SZrCompiledMemberInfo);
        TZrSize currentPrototypeSize =
                sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + decoratorArraySize + membersArraySize;

        if (remainingDataSize < currentPrototypeSize) {
            fprintf(output, "// Error: Insufficient data for prototype %u (size: %zu, expected: %zu)\n",
                    (unsigned int) (i + 1), (TZrSize) remainingDataSize, currentPrototypeSize);
            break;
        }

        // 读取类型名称
        TZrNativeString typeNameStr = "<unknown>";
        if (nameStringIndex < entryFunction->constantValueLength) {
            const SZrTypeValue *nameConstant = &entryFunction->constantValueList[nameStringIndex];
            if (nameConstant->type == ZR_VALUE_TYPE_STRING) {
                SZrString *typeName = ZR_CAST_STRING(state, nameConstant->value.object);
                if (typeName != ZR_NULL) {
                    TZrNativeString tmpStr = ZrCore_String_GetNativeStringShort(typeName);
                    if (tmpStr == ZR_NULL) {
                        tmpStr = *ZrCore_String_GetNativeStringLong(typeName);
                    }
                    if (tmpStr != ZR_NULL) {
                        typeNameStr = tmpStr;
                    }
                }
            }
        }

        const char *prototypeTypeName = "unknown";
        if (type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS)
            prototypeTypeName = "class";
        else if (type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT)
            prototypeTypeName = "struct";
        else if (type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE)
            prototypeTypeName = "interface";
        else if (type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE)
            prototypeTypeName = "module";
        else if (type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM)
            prototypeTypeName = "enum";
        else if (type == ZR_OBJECT_PROTOTYPE_TYPE_UNION)
            prototypeTypeName = "union";
        else if (type == ZR_OBJECT_PROTOTYPE_TYPE_NATIVE)
            prototypeTypeName = "native";

        fprintf(output, "// --- Prototype %u ---\n", (unsigned int) (i + 1));
        fprintf(output, "%s %s", prototypeTypeName, typeNameStr);

        // 读取继承类型
        if (inheritsCount > 0) {
            fprintf(output, " : ");
            const TZrUInt32 *inheritIndices = (const TZrUInt32 *) (currentPos + sizeof(SZrCompiledPrototypeInfo));
            for (TZrUInt32 j = 0; j < inheritsCount; j++) {
                if (j > 0)
                    fprintf(output, ", ");
                TZrUInt32 inheritStringIndex = inheritIndices[j];
                if (inheritStringIndex < entryFunction->constantValueLength) {
                    const SZrTypeValue *inheritConstant = &entryFunction->constantValueList[inheritStringIndex];
                    if (inheritConstant->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *inheritTypeName = ZR_CAST_STRING(state, inheritConstant->value.object);
                        if (inheritTypeName != ZR_NULL) {
                            TZrNativeString inheritStr = ZrCore_String_GetNativeStringShort(inheritTypeName);
                            if (inheritStr == ZR_NULL) {
                                inheritStr = *ZrCore_String_GetNativeStringLong(inheritTypeName);
                            }
                            fprintf(output, "%s", inheritStr != ZR_NULL ? inheritStr : "<unknown>");
                        }
                    }
                }
            }
        }

        fprintf(output, " {\n");

        // 输出访问修饰符名称
        const char *accessName = get_access_modifier_name(accessModifier);
        fprintf(output, "  access: %s (%u),\n", accessName, (unsigned int) accessModifier);
        fprintf(output, "  layoutByteSize: %u,\n", (unsigned int) protoInfo->layoutByteSize);
        fprintf(output, "  layoutByteAlign: %u,\n", (unsigned int) protoInfo->layoutByteAlign);

        // 计算成员数据的起始位置
        const TZrByte *membersData =
                (const TZrByte *)(currentPos + sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + decoratorArraySize);

        fprintf(output, "  members: [\n");

        // 遍历每个成员
        for (TZrUInt32 j = 0; j < membersCount; j++) {
            const SZrCompiledMemberInfo *memberInfo =
                    (const SZrCompiledMemberInfo *) (membersData + j * sizeof(SZrCompiledMemberInfo));

                TZrUInt32 memberType = memberInfo->memberType;
                TZrUInt32 memberNameStringIndex = memberInfo->nameStringIndex;
                TZrUInt32 memberAccess = memberInfo->accessModifier;
                TZrUInt32 isStatic = memberInfo->isStatic;
                TZrUInt32 fieldTypeNameStringIndex = memberInfo->fieldTypeNameStringIndex;
                TZrUInt32 fieldOffset = memberInfo->fieldOffset;
                TZrUInt32 fieldSize = memberInfo->fieldSize;
                TZrUInt32 isMetaMethod = memberInfo->isMetaMethod;
                TZrUInt32 metaType = memberInfo->metaType;
                TZrUInt32 functionConstantIndex = memberInfo->functionConstantIndex;
                TZrUInt32 parameterCount = memberInfo->parameterCount;
                TZrUInt32 returnTypeNameStringIndex = memberInfo->returnTypeNameStringIndex;

                // 读取成员名称
                TZrNativeString memberNameStr = "<unnamed>";
                if (memberNameStringIndex > 0 && memberNameStringIndex < entryFunction->constantValueLength) {
                    const SZrTypeValue *nameConstant = &entryFunction->constantValueList[memberNameStringIndex];
                    if (nameConstant->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *memberName = ZR_CAST_STRING(state, nameConstant->value.object);
                        if (memberName != ZR_NULL) {
                            TZrNativeString tmpStr = ZrCore_String_GetNativeStringShort(memberName);
                            if (tmpStr == ZR_NULL) {
                                tmpStr = *ZrCore_String_GetNativeStringLong(memberName);
                            }
                            if (tmpStr != ZR_NULL) {
                                memberNameStr = tmpStr;
                            }
                        }
                    }
                }

                // 根据成员类型输出信息（使用EZrAstNodeType枚举值）
                // 注意：实际存储的是EZrAstNodeType枚举值，使用ZR_AST_CONSTANT_*常量进行匹配
                const char *memberTypeName = "unknown";
                // 使用常量匹配所有可能的类型
                if (memberType == ZR_AST_CONSTANT_STRUCT_FIELD)
                    memberTypeName = "STRUCT_FIELD";
                else if (memberType == ZR_AST_CONSTANT_STRUCT_METHOD)
                    memberTypeName = "STRUCT_METHOD";
                else if (memberType == ZR_AST_CONSTANT_STRUCT_META_FUNCTION)
                    memberTypeName = "STRUCT_META_FUNCTION";
                else if (memberType == ZR_AST_CONSTANT_CLASS_FIELD)
                    memberTypeName = "CLASS_FIELD";
                else if (memberType == ZR_AST_CONSTANT_CLASS_METHOD)
                    memberTypeName = "CLASS_METHOD";
                else if (memberType == ZR_AST_CONSTANT_CLASS_PROPERTY)
                    memberTypeName = "CLASS_PROPERTY";
                else if (memberType == ZR_AST_CONSTANT_CLASS_META_FUNCTION)
                    memberTypeName = "CLASS_META_FUNCTION";

                fprintf(output, "    {\n");
                fprintf(output, "      type: %s (%u),\n", memberTypeName, (unsigned int) memberType);
                fprintf(output, "      name: \"%s\",\n", memberNameStr);

                // 访问修饰符
                const char *memberAccessName = "unknown";
                if (memberAccess == ZR_ACCESS_CONSTANT_PUBLIC)
                    memberAccessName = "public";
                else if (memberAccess == ZR_ACCESS_CONSTANT_PRIVATE)
                    memberAccessName = "private";
                else if (memberAccess == ZR_ACCESS_CONSTANT_PROTECTED)
                    memberAccessName = "protected";
                fprintf(output, "      access: %s (%u),\n", memberAccessName, (unsigned int) memberAccess);
                fprintf(output, "      static: %s,\n", isStatic ? "true" : "false");

                // 判断是否为字段类型（使用常量）
                TZrBool isFieldType = (memberType == ZR_AST_CONSTANT_STRUCT_FIELD ||
                                     memberType == ZR_AST_CONSTANT_CLASS_FIELD);
                // 判断是否为方法类型（使用常量）
                TZrBool isMethodType = (memberType == ZR_AST_CONSTANT_STRUCT_METHOD ||
                                      memberType == ZR_AST_CONSTANT_CLASS_METHOD);
                // 判断是否为元方法类型（使用常量）
                TZrBool isMetaFunctionType = (memberType == ZR_AST_CONSTANT_STRUCT_META_FUNCTION ||
                                            memberType == ZR_AST_CONSTANT_CLASS_META_FUNCTION);

                // 如果是字段
                if (isFieldType) {
                    // 读取字段类型名称
                    TZrNativeString fieldTypeNameStr = "<unknown>";
                    if (fieldTypeNameStringIndex > 0 && fieldTypeNameStringIndex < entryFunction->constantValueLength) {
                        const SZrTypeValue *fieldTypeConstant =
                                &entryFunction->constantValueList[fieldTypeNameStringIndex];
                        if (fieldTypeConstant->type == ZR_VALUE_TYPE_STRING) {
                            SZrString *fieldTypeName = ZR_CAST_STRING(state, fieldTypeConstant->value.object);
                            if (fieldTypeName != ZR_NULL) {
                                TZrNativeString tmpStr = ZrCore_String_GetNativeStringShort(fieldTypeName);
                                if (tmpStr == ZR_NULL) {
                                    tmpStr = *ZrCore_String_GetNativeStringLong(fieldTypeName);
                                }
                                if (tmpStr != ZR_NULL) {
                                    fieldTypeNameStr = tmpStr;
                                }
                            }
                        }
                    }
                    fprintf(output, "      fieldType: \"%s\",\n", fieldTypeNameStr);
                    fprintf(output, "      fieldOffset: %u,\n", (unsigned int) fieldOffset);
                    fprintf(output, "      fieldSize: %u\n", (unsigned int) fieldSize);
                }
                // 如果是方法
                else if (isMethodType) {
                    fprintf(output, "      isMetaMethod: false,\n");
                    fprintf(output, "      functionConstantIndex: %u,\n", (unsigned int) functionConstantIndex);
                    fprintf(output, "      parameterCount: %u,\n", (unsigned int) parameterCount);
                    // 读取返回类型名称
                    TZrNativeString returnTypeNameStr = "<void>";
                    if (returnTypeNameStringIndex > 0 && returnTypeNameStringIndex < entryFunction->constantValueLength) {
                        const SZrTypeValue *returnTypeConstant =
                                &entryFunction->constantValueList[returnTypeNameStringIndex];
                        if (returnTypeConstant->type == ZR_VALUE_TYPE_STRING) {
                            SZrString *returnTypeName = ZR_CAST_STRING(state, returnTypeConstant->value.object);
                            if (returnTypeName != ZR_NULL) {
                                TZrNativeString tmpStr = ZrCore_String_GetNativeStringShort(returnTypeName);
                                if (tmpStr == ZR_NULL) {
                                    tmpStr = *ZrCore_String_GetNativeStringLong(returnTypeName);
                                }
                                if (tmpStr != ZR_NULL) {
                                    returnTypeNameStr = tmpStr;
                                }
                            }
                        }
                    }
                    fprintf(output, "      returnType: \"%s\"\n", returnTypeNameStr);
                }
                // 如果是元方法
                else if (isMetaFunctionType) {
                    fprintf(output, "      isMetaMethod: true,\n");
                    // 输出元方法类型名称（使用CZrMetaName数组）
                    const char *metaTypeName = "unknown";
                    if (metaType < ZR_META_ENUM_MAX) {
                        metaTypeName = CZrMetaName[metaType];
                    }
                    fprintf(output, "      metaType: %s (%u),\n", metaTypeName != ZR_NULL ? metaTypeName : "unknown",
                            (unsigned int) metaType);
                    fprintf(output, "      functionConstantIndex: %u,\n", (unsigned int) functionConstantIndex);
                    fprintf(output, "      parameterCount: %u,\n", (unsigned int) parameterCount);
                    // 读取返回类型名称
                    TZrNativeString returnTypeNameStr = "<void>";
                    if (returnTypeNameStringIndex > 0 && returnTypeNameStringIndex < entryFunction->constantValueLength) {
                        const SZrTypeValue *returnTypeConstant =
                                &entryFunction->constantValueList[returnTypeNameStringIndex];
                        if (returnTypeConstant->type == ZR_VALUE_TYPE_STRING) {
                            SZrString *returnTypeName = ZR_CAST_STRING(state, returnTypeConstant->value.object);
                            if (returnTypeName != ZR_NULL) {
                                TZrNativeString tmpStr = ZrCore_String_GetNativeStringShort(returnTypeName);
                                if (tmpStr == ZR_NULL) {
                                    tmpStr = *ZrCore_String_GetNativeStringLong(returnTypeName);
                                }
                                if (tmpStr != ZR_NULL) {
                                    returnTypeNameStr = tmpStr;
                                }
                            }
                        }
                    }
                    fprintf(output, "      returnType: \"%s\"\n", returnTypeNameStr);
                }
                // 如果是属性
                else if (memberType == ZR_AST_CONSTANT_CLASS_PROPERTY) {
                    fprintf(output,
                            "      // Property: getter/setter function indices not yet stored in member info\n");
                } else {
                    // 未知类型，尝试读取类型名称（如果有fieldTypeNameStringIndex）
                    TZrNativeString fieldTypeNameStr = "<unknown>";
                    if (fieldTypeNameStringIndex > 0 && fieldTypeNameStringIndex < entryFunction->constantValueLength) {
                        const SZrTypeValue *fieldTypeConstant =
                                &entryFunction->constantValueList[fieldTypeNameStringIndex];
                        if (fieldTypeConstant->type == ZR_VALUE_TYPE_STRING) {
                            SZrString *fieldTypeName = ZR_CAST_STRING(state, fieldTypeConstant->value.object);
                            if (fieldTypeName != ZR_NULL) {
                                TZrNativeString tmpStr = ZrCore_String_GetNativeStringShort(fieldTypeName);
                                if (tmpStr == ZR_NULL) {
                                    tmpStr = *ZrCore_String_GetNativeStringLong(fieldTypeName);
                                }
                                if (tmpStr != ZR_NULL) {
                                    fieldTypeNameStr = tmpStr;
                                }
                            }
                        }
                    }
                    
                    // 输出所有可用信息
                    fprintf(output, "      // Unknown member type, showing all fields:\n");
                    if (fieldTypeNameStringIndex > 0) {
                        fprintf(output, "      fieldType: \"%s\",\n", fieldTypeNameStr);
                        fprintf(output, "      fieldTypeNameStringIndex: %u,\n", (unsigned int) fieldTypeNameStringIndex);
                    } else {
                        fprintf(output, "      fieldTypeNameStringIndex: %u,\n", (unsigned int) fieldTypeNameStringIndex);
                    }
                    fprintf(output, "      fieldOffset: %u,\n", (unsigned int) fieldOffset);
                    fprintf(output, "      fieldSize: %u,\n", (unsigned int) fieldSize);
                    fprintf(output, "      isMetaMethod: %s,\n", isMetaMethod ? "true" : "false");
                    fprintf(output, "      metaType: %u,\n", (unsigned int) metaType);
                    fprintf(output, "      functionConstantIndex: %u,\n", (unsigned int) functionConstantIndex);
                    fprintf(output, "      parameterCount: %u\n", (unsigned int) parameterCount);
                }

            fprintf(output, "    }%s\n", (j < membersCount - 1) ? "," : "");
        }

        fprintf(output, "  ]\n");
        fprintf(output, "}\n\n");
        
        // 移动到下一个prototype
        currentPos += currentPrototypeSize;
        remainingDataSize -= currentPrototypeSize;
    }

    fprintf(output, "// ========== END OF PROTOTYPES ==========\n");
}
