//
// Dispatch loop for VM execution.
//

#include "execution_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static SZrString *execution_resolve_member_symbol(SZrClosure *closure, TZrUInt16 memberId) {
    if (closure == ZR_NULL || closure->function == ZR_NULL ||
        closure->function->memberEntries == ZR_NULL ||
        memberId >= closure->function->memberEntryLength) {
        return ZR_NULL;
    }

    return closure->function->memberEntries[memberId].symbol;
}

static TZrBool execution_make_member_key(SZrState *state, SZrString *memberName, SZrTypeValue *outKey) {
    if (state == ZR_NULL || memberName == ZR_NULL || outKey == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, outKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    outKey->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static TZrBool execution_raise_vm_runtime_error(SZrState *state,
                                                SZrCallInfo **ioCallInfo,
                                                const TZrChar *format,
                                                ...) {
    TZrChar errorBuffer[ZR_RUNTIME_ERROR_BUFFER_LENGTH];
    SZrString *errorString;
    SZrTypeValue payload;
    va_list args;
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || ioCallInfo == ZR_NULL || *ioCallInfo == ZR_NULL || format == ZR_NULL) {
        return ZR_FALSE;
    }

    va_start(args, format);
    vsnprintf(errorBuffer, sizeof(errorBuffer), format, args);
    va_end(args);
    errorBuffer[sizeof(errorBuffer) - 1] = '\0';

    errorString = ZrCore_String_CreateFromNative(state, errorBuffer[0] != '\0' ? errorBuffer : "Runtime error");
    if (errorString == ZR_NULL) {
        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
        }
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
        ZR_ABORT();
    }

    callInfo = *ioCallInfo;
    execution_clear_pending_control(state);
    ZrCore_Value_InitAsRawObject(state, &payload, ZR_CAST_RAW_OBJECT_AS_SUPER(errorString));
    payload.type = ZR_VALUE_TYPE_STRING;
    payload.isGarbageCollectable = ZR_TRUE;
    payload.isNative = ZR_FALSE;

    if (!ZrCore_Exception_NormalizeThrownValue(state, &payload, callInfo, ZR_THREAD_STATUS_RUNTIME_ERROR)) {
        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
        }
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
        ZR_ABORT();
    }

    if (execution_unwind_exception_to_handler(state, ioCallInfo)) {
        return ZR_TRUE;
    }

    ZrCore_Exception_Throw(state, state->currentExceptionStatus);
    ZR_ABORT();
}

static TZrBool execution_is_truthy(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_INT(value->type)) {
        return value->value.nativeObject.nativeInt64 != 0;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeUInt64 != 0;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return value->value.nativeObject.nativeDouble != 0.0;
    }
    if (ZR_VALUE_IS_TYPE_NULL(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_STRING(value->type)) {
        SZrString *str = ZR_CAST_STRING(state, value->value.object);
        TZrSize len = (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? str->shortStringLength : str->longStringLength;
        return len > 0;
    }

    return ZR_TRUE;
}

static TZrBool execution_try_reuse_preinstalled_top_level_closure(SZrState *state,
                                                                  SZrClosure *ownerClosure,
                                                                  SZrFunction *function,
                                                                  TZrStackValuePointer base,
                                                                  SZrTypeValue *destination) {
    TZrUInt32 index;

    if (state == ZR_NULL || ownerClosure == ZR_NULL || ownerClosure->function == ZR_NULL || function == ZR_NULL ||
        base == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < ownerClosure->function->exportedVariableLength; ++index) {
        const SZrFunctionExportedVariable *binding = &ownerClosure->function->exportedVariables[index];
        SZrTypeValue *existingValue;
        SZrFunction *existingFunction;

        if (binding->exportKind != ZR_MODULE_EXPORT_KIND_FUNCTION ||
            binding->readiness != ZR_MODULE_EXPORT_READY_DECLARATION ||
            binding->callableChildIndex >= ownerClosure->function->childFunctionLength ||
            &ownerClosure->function->childFunctionList[binding->callableChildIndex] != function ||
            binding->stackSlot >= ownerClosure->function->stackSize) {
            continue;
        }

        existingValue = ZrCore_Stack_GetValue(base + binding->stackSlot);
        if (existingValue == ZR_NULL) {
            continue;
        }

        existingFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, existingValue);
        if (existingFunction != function) {
            continue;
        }

        ZrCore_Value_Copy(state, destination, existingValue);
        return ZR_TRUE;
    }

    for (index = 0; index < ownerClosure->function->topLevelCallableBindingLength; ++index) {
        const SZrFunctionTopLevelCallableBinding *binding = &ownerClosure->function->topLevelCallableBindings[index];
        SZrTypeValue *existingValue;
        SZrFunction *existingFunction;

        if (binding->callableChildIndex >= ownerClosure->function->childFunctionLength ||
            &ownerClosure->function->childFunctionList[binding->callableChildIndex] != function ||
            binding->stackSlot >= ownerClosure->function->stackSize) {
            continue;
        }

        existingValue = ZrCore_Stack_GetValue(base + binding->stackSlot);
        if (existingValue == ZR_NULL) {
            continue;
        }

        existingFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, existingValue);
        if (existingFunction != function) {
            continue;
        }

        ZrCore_Value_Copy(state, destination, existingValue);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

void ZrCore_Execute(SZrState *state, SZrCallInfo *callInfo) {
    SZrClosure *closure;
    SZrTypeValue *constants;
    TZrStackValuePointer base;
    SZrTypeValue ret;
    ZrCore_Value_ResetAsNull(&ret);
    const TZrInstruction *programCounter;
    TZrDebugSignal trap;
    SZrTypeValue *opA;
    SZrTypeValue *opB;
    /*
     * registers macros
     */

    /*
     *
     */
    ZR_INSTRUCTION_DISPATCH_TABLE
#define DONE(N) ZR_INSTRUCTION_DONE(instruction, programCounter, N)
#if defined(ZR_INSTRUCTION_USE_DISPATCH_TABLE) && ZR_INSTRUCTION_DISPATCH_TABLE_SUPPORTED
#define DONE_SKIP(N) DONE(N)
#else
#define DONE_SKIP(N) do { programCounter += ((N) - 1); } while (0); break
#endif
// extra operand
#define E(INSTRUCTION) INSTRUCTION.instruction.operandExtra
// 4 OPERANDS
#define A0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[0]
#define B0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[1]
#define C0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[2]
#define D0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[3]
// 2 OPERANDS
#define A1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[0]
#define B1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[1]
// 1 OPERAND
#define A2(INSTRUCTION) INSTRUCTION.instruction.operand.operand2[0]

#define BASE(OFFSET) (base + (OFFSET))
#define CONST(OFFSET) (constants + (OFFSET))
#define CLOSURE(OFFSET) (closure->closureValuesExtend[OFFSET])

#define ALGORITHM_1(REGION, OP, TYPE) ZR_VALUE_FAST_SET(destination, REGION, OP(opA->value.nativeObject.REGION), TYPE);
#define ALGORITHM_2(REGION, OP, TYPE)                                                                                  \
    ZR_VALUE_FAST_SET(destination, REGION, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CVT_2(CVT, REGION, OP, TYPE)                                                                         \
    ZR_VALUE_FAST_SET(destination, CVT, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CONST_2(REGION, OP, TYPE, RIGHT)                                                                     \
    ZR_VALUE_FAST_SET(destination, REGION, (opA->value.nativeObject.REGION) OP(RIGHT), TYPE);
#define ALGORITHM_FUNC_2(REGION, OP_FUNC, TYPE)                                                                        \
    ZR_VALUE_FAST_SET(destination, REGION, OP_FUNC(opA->value.nativeObject.REGION, opB->value.nativeObject.REGION),    \
                      TYPE);

#define UPDATE_TRAP(CALL_INFO) (trap = (CALL_INFO)->context.context.trap)
#define UPDATE_BASE(CALL_INFO) (base = (CALL_INFO)->functionBase.valuePointer + 1)
#define UPDATE_STACK(CALL_INFO)                                                                                        \
    {                                                                                                                  \
        UPDATE_BASE(CALL_INFO);                                                                                        \
    }
#define SAVE_PC(STATE, CALL_INFO) ((CALL_INFO)->context.context.programCounter = programCounter)
#define SAVE_STATE(STATE, CALL_INFO)                                                                                   \
    (SAVE_PC(STATE, CALL_INFO), ((STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer))
    // MODIFIABLE: ERROR & STACK & HOOK
#define PROTECT_ESH(STATE, CALL_INFO, EXP)                                                                             \
    do {                                                                                                               \
        SAVE_PC(STATE, CALL_INFO);                                                                                     \
        (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer;                                        \
        EXP;                                                                                                           \
        UPDATE_BASE(CALL_INFO);                                                                                        \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    } while (0)
#define PROTECT_EH(STATE, CALL_INFO, EXP)                                                                              \
    do {                                                                                                               \
        SAVE_PC(STATE, CALL_INFO);                                                                                     \
        EXP;                                                                                                           \
        UPDATE_BASE(CALL_INFO);                                                                                        \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    } while (0)
#define PROTECT_E(STATE, CALL_INFO, EXP)                                                                               \
    do {                                                                                                               \
        SAVE_PC(STATE, CALL_INFO);                                                                                     \
        (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer;                                        \
        EXP;                                                                                                           \
        UPDATE_BASE(CALL_INFO);                                                                                        \
    } while (0)

#define RELOAD_DESTINATION_AFTER_PROTECT(CALL_INFO, INSTRUCTION)                                                       \
    do {                                                                                                               \
        UPDATE_BASE(CALL_INFO);                                                                                        \
        destination = destinationIsRet ? &ret : &BASE(E(INSTRUCTION))->value;                                         \
    } while (0)

#define ZrCore_Debug_RunError(STATE, ...)                                                                              \
    do {                                                                                                               \
        SAVE_PC((STATE), callInfo);                                                                                    \
        if (execution_raise_vm_runtime_error((STATE), &callInfo, __VA_ARGS__)) {                                      \
            goto LZrReturning;                                                                                         \
        }                                                                                                              \
    } while (0)

#define RESUME_AFTER_NATIVE_CALL(STATE, CALL_INFO)                                                                     \
    do {                                                                                                               \
        if ((STATE)->hasCurrentException && execution_unwind_exception_to_handler((STATE), &(CALL_INFO))) {           \
            goto LZrReturning;                                                                                         \
        }                                                                                                              \
        if ((CALL_INFO) != ZR_NULL &&                                                                                  \
            (((STATE)->stackTop.valuePointer < (CALL_INFO)->functionBase.valuePointer + 1) ||                         \
             ((STATE)->stackTop.valuePointer > (CALL_INFO)->functionTop.valuePointer))) {                             \
            (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer;                                    \
        }                                                                                                              \
        UPDATE_BASE(CALL_INFO);                                                                                        \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    } while (0)

#define JUMP(CALL_INFO, INSTRUCTION, OFFSET)                                                                           \
    {                                                                                                                  \
        programCounter += A2(INSTRUCTION) + (OFFSET);                                                                  \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    }

LZrStart:
    trap = state->debugHookSignal;
LZrReturning: {
    SZrTypeValue *functionBaseValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    closure = ZR_CAST_VM_CLOSURE(state, functionBaseValue->value.object);
    constants = closure->function->constantValueList;
    programCounter = callInfo->context.context.programCounter - 1;
    base = callInfo->functionBase.valuePointer + 1;
}
    if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE)) {
        // todo
    }
    for (;;) {

        TZrInstruction instruction;
        /*
         * fetch instruction
         */
        ZR_INSTRUCTION_FETCH(instruction, programCounter, trap = ZrCore_Debug_TraceExecution(state, programCounter);
                             UPDATE_STACK(callInfo), 1);
        // 检查 programCounter 是否超出指令范围
        const TZrInstruction *instructionsEnd =
                closure->function->instructionsList + closure->function->instructionsLength;
        if (ZR_UNLIKELY(programCounter >= instructionsEnd)) {
            // 超出指令范围，退出循环（相当于隐式返回）
            break;
        }
        UPDATE_BASE(callInfo);
        // debug line
#if ZR_DEBUG
#endif
        ZR_ASSERT(base == callInfo->functionBase.valuePointer + 1);
        ZR_ASSERT(base <= state->stackTop.valuePointer &&
                  state->stackTop.valuePointer <= state->stackTail.valuePointer);

        TZrBool destinationIsRet = E(instruction) == ZR_INSTRUCTION_USE_RET_FLAG;
        SZrTypeValue *destination = destinationIsRet ? &ret : &BASE(E(instruction))->value;

        ZR_INSTRUCTION_DISPATCH(instruction) {
            ZR_INSTRUCTION_LABEL(GET_STACK) {
                SZrTypeValue *source = &BASE(A2(instruction))->value;
                if (destinationIsRet) {
                    *destination = *source;
                } else {
                    ZrCore_Value_Copy(state, destination, source);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_STACK) {
                // SET_STACK 指令格式：
                // operandExtra (E) = destSlot (目标栈槽)
                // operand2[0] (A2) = srcSlot (源栈槽)
                // 将 srcSlot 的值复制到 destSlot
                SZrTypeValue *srcValue = &BASE(A2(instruction))->value;
                ZrCore_Value_Copy(state, &BASE(E(instruction))->value, srcValue);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CONSTANT) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // BASE(B1(instruction))->value = *CONST(ret.value.nativeObject.nativeUInt64);
                if (destinationIsRet) {
                    *destination = *CONST(A2(instruction));
                } else {
                    ZrCore_Value_Copy(state, destination, CONST(A2(instruction)));
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CONSTANT) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                //*CONST(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                *CONST(A2(instruction)) = *destination;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CLOSURE) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // closure function to access
                ZrCore_Value_Copy(state, destination, ZrCore_ClosureValue_GetValue(CLOSURE(A2(instruction))));
                // BASE(B1(instruction))->value = CLOSURE(ret.value.nativeObject.nativeUInt64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CLOSURE) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                SZrClosureValue *closureValue = CLOSURE(A2(instruction));
                SZrTypeValue *value = ZrCore_ClosureValue_GetValue(closureValue);
                SZrTypeValue *newValue = destination;
                // closure function to access
                ZrCore_Value_Copy(state, value, newValue);
                // CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue), newValue);
                // *CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_BOOL) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_BOOL);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              callInfo->functionTop.valuePointer,
                                                                              meta,
                                                                              opA,
                                                                              ZR_NULL,
                                                                              ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (returnValue->type == ZR_VALUE_TYPE_BOOL) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeInt64 != 0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeUInt64 != 0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeDouble != 0.0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_STRING(opA->type)) {
                        SZrString *str = ZR_CAST_STRING(state, opA->value.object);
                        TZrSize len = (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? str->shortStringLength : str->longStringLength;
                        ZR_VALUE_FAST_SET(destination, nativeBool, len > 0, ZR_VALUE_TYPE_BOOL);
                    } else {
                        // 对象类型，默认返回 true
                        ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_INT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_INT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              callInfo->functionTop.valuePointer,
                                                                              meta,
                                                                              opA,
                                                                              ZR_NULL,
                                                                              ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsInt(state, destination, 0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsInt(state, destination, 0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZrCore_Value_InitAsInt(state, destination, (TZrInt64) opA->value.nativeObject.nativeUInt64);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZrCore_Value_InitAsInt(state, destination, (TZrInt64) opA->value.nativeObject.nativeDouble);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsInt(state, destination, opA->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0
                        ZrCore_Value_InitAsInt(state, destination, 0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_UINT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_UINT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              callInfo->functionTop.valuePointer,
                                                                              meta,
                                                                              opA,
                                                                              ZR_NULL,
                                                                              ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsUInt(state, destination, 0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsUInt(state, destination, 0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZrCore_Value_InitAsUInt(state, destination, (TZrUInt64) opA->value.nativeObject.nativeInt64);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZrCore_Value_InitAsUInt(state, destination, (TZrUInt64) opA->value.nativeObject.nativeDouble);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsUInt(state, destination, opA->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0
                        ZrCore_Value_InitAsUInt(state, destination, 0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_FLOAT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              callInfo->functionTop.valuePointer,
                                                                              meta,
                                                                              opA,
                                                                              ZR_NULL,
                                                                              ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_FLOAT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsFloat(state, destination, 0.0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsFloat(state, destination, 0.0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZrCore_Value_InitAsFloat(state, destination, (TZrFloat64) opA->value.nativeObject.nativeInt64);
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZrCore_Value_InitAsFloat(state, destination, (TZrFloat64) opA->value.nativeObject.nativeUInt64);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsFloat(state, destination, opA->value.nativeObject.nativeBool ? (TZrFloat64)ZR_TRUE : (TZrFloat64)ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0.0
                        ZrCore_Value_InitAsFloat(state, destination, 0.0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRING) {
                SZrString *resultString;

                opA = &BASE(A1(instruction))->value;
                resultString = ZrCore_Value_ConvertToString(state, opA);
                UPDATE_BASE(callInfo);
                destination = destinationIsRet ? &ret : &BASE(E(instruction))->value;
                if (resultString != ZR_NULL) {
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(resultString));
                } else {
                    // 转换失败，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRUCT) {
                opA = &BASE(A1(instruction))->value;
                // operand1[1] 存储类型名称常量索引
                TZrUInt16 typeNameConstantIndex = B1(instruction);
                if (closure->function != ZR_NULL && typeNameConstantIndex < closure->function->constantValueLength) {
                    SZrTypeValue *typeNameValue = CONST(typeNameConstantIndex);
                    if (typeNameValue->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
                        
                        // 1. 优先尝试通过元方法进行转换
                        // 如果定义了 ZR_META_TO_STRUCT，使用它；否则直接查找原型
                        // 注意：ZR_META_TO_STRUCT 可能不在标准元方法列表中，需要检查是否支持
                        // 如果opA是对象类型，尝试从对象的prototype中查找元方法
                        SZrMeta *meta = ZR_NULL;
                        if (opA->type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObject *obj = ZR_CAST_OBJECT(state, opA->value.object);
                            if (obj != ZR_NULL && obj->prototype != ZR_NULL) {
                                // 尝试查找TO_STRUCT元方法（如果存在）
                                // 注意：ZR_META_TO_STRUCT可能不在标准枚举中，这里先查找原型链
                                // 如果未来添加了ZR_META_TO_STRUCT，可以在这里使用
                                // TODO: 目前暂时跳过元方法查找，直接查找原型
                            }
                        }
                        if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                            TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                            SZrCallInfo *savedCallInfo = state->callInfoList;
                            PROTECT_E(state, callInfo, {
                                TZrStackValuePointer metaBase = ZR_NULL;
                                TZrStackValuePointer restoredStackTop = savedStackTop;
                                TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                      savedCallInfo,
                                                                                      savedStackTop,
                                                                                      savedStackTop,
                                                                                      meta,
                                                                                      opA,
                                                                                      typeNameValue,
                                                                                      ZR_META_CALL_MAX_ARGUMENTS,
                                                                                      &metaBase,
                                                                                      &restoredStackTop);
                                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                                if (metaCallSucceeded) {
                                    SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                    ZrCore_Value_Copy(state, destination, returnValue);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                                state->stackTop.valuePointer = restoredStackTop;
                                state->callInfoList = savedCallInfo;
                            });
                        } else {
                            // 2. 无元方法，尝试查找类型原型并执行转换
                            SZrObjectPrototype *prototype = find_type_prototype(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_STRUCT);
                            if (prototype != ZR_NULL) {
                                // 找到原型，执行转换
                                if (!convert_to_struct(state, opA, prototype, destination)) {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            } else {
                                // 3. 找不到原型，如果源值是对象，尝试直接复制
                                // 这允许在运行时动态创建 struct（如果类型系统支持）
                                if (ZR_VALUE_IS_TYPE_OBJECT(opA->type)) {
                                    // TODO: 暂时直接复制对象（后续需要设置正确的原型）
                                    ZrCore_Value_Copy(state, destination, opA);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            }
                        }
                    } else {
                        // 类型名称常量类型错误
                        ZrCore_Value_ResetAsNull(destination);
                    }
                } else {
                    // 常量索引越界
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_OBJECT) {
                opA = &BASE(A1(instruction))->value;
                // operand1[1] 存储类型名称常量索引
                TZrUInt16 typeNameConstantIndex = B1(instruction);
                if (closure->function != ZR_NULL && typeNameConstantIndex < closure->function->constantValueLength) {
                    SZrTypeValue *typeNameValue = CONST(typeNameConstantIndex);
                    if (typeNameValue->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
                        
                        // 1. 优先尝试通过元方法进行转换
                        // 如果定义了 ZR_META_TO_OBJECT，使用它；否则直接查找原型
                        // 注意：ZR_META_TO_OBJECT 可能不在标准元方法列表中，需要检查是否支持
                        // 如果opA是对象类型，尝试从对象的prototype中查找元方法
                        SZrMeta *meta = ZR_NULL;
                        if (opA->type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObject *obj = ZR_CAST_OBJECT(state, opA->value.object);
                            if (obj != ZR_NULL && obj->prototype != ZR_NULL) {
                                // 尝试查找TO_OBJECT元方法（如果存在）
                                // 注意：ZR_META_TO_OBJECT可能不在标准枚举中，这里先查找原型链
                                // 如果未来添加了ZR_META_TO_OBJECT，可以在这里使用
                                // TODO: 目前暂时跳过元方法查找，直接查找原型
                            }
                        }
                        if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                            TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                            SZrCallInfo *savedCallInfo = state->callInfoList;
                            PROTECT_E(state, callInfo, {
                                TZrStackValuePointer metaBase = ZR_NULL;
                                TZrStackValuePointer restoredStackTop = savedStackTop;
                                TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                      savedCallInfo,
                                                                                      savedStackTop,
                                                                                      savedStackTop,
                                                                                      meta,
                                                                                      opA,
                                                                                      typeNameValue,
                                                                                      ZR_META_CALL_MAX_ARGUMENTS,
                                                                                      &metaBase,
                                                                                      &restoredStackTop);
                                RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                                if (metaCallSucceeded) {
                                    SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                    ZrCore_Value_Copy(state, destination, returnValue);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                                state->stackTop.valuePointer = restoredStackTop;
                                state->callInfoList = savedCallInfo;
                            });
                        } else {
                            // 2. 无元方法，尝试查找类型原型并执行转换
                            SZrObjectPrototype *prototype = find_type_prototype(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_INVALID);
                            if (prototype != ZR_NULL) {
                                // 找到原型，执行转换
                                TZrBool converted = ZR_FALSE;

                                if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                                    converted = convert_to_class(state, opA, prototype, destination);
                                } else if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
                                    converted = convert_to_enum(state, opA, prototype, destination);
                                }

                                if (!converted) {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            } else {
                                // 3. 找不到原型，如果源值是对象，尝试直接复制
                                // 这允许在运行时动态创建 class（如果类型系统支持）
                                if (ZR_VALUE_IS_TYPE_OBJECT(opA->type)) {
                                    // TODO: 暂时直接复制对象（后续需要设置正确的原型）
                                    ZrCore_Value_Copy(state, destination, opA);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            }
                        }
                    } else {
                        // 类型名称常量类型错误
                        ZrCore_Value_ResetAsNull(destination);
                    }
                } else {
                    // 常量索引越界
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD) {
                SZrTypeValue builtinResult;

                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZrCore_Value_ResetAsNull(&builtinResult);
                if (try_builtin_add(state, &builtinResult, opA, opB)) {
                    if (destinationIsRet) {
                        ret = builtinResult;
                    } else {
                        UPDATE_BASE(callInfo);
                        destination = &BASE(E(instruction))->value;
                        ZrCore_Value_Copy(state, destination, &builtinResult);
                    }
                    // 基础数值和字符串拼接直接在运行时处理。
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_ADD);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                        PROTECT_E(state, callInfo, {
                            TZrStackValuePointer metaBase = ZR_NULL;
                            TZrStackValuePointer restoredStackTop = savedStackTop;
                            TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                  savedCallInfo,
                                                                                  savedStackTop,
                                                                                  savedStackTop,
                                                                                  meta,
                                                                                  opA,
                                                                                  opB,
                                                                                  ZR_META_CALL_MAX_ARGUMENTS,
                                                                                  &metaBase,
                                                                                  &restoredStackTop);
                            RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                            if (metaCallSucceeded) {
                                SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                ZrCore_Value_ResetAsNull(destination);
                            }
                            state->stackTop.valuePointer = restoredStackTop;
                            state->callInfoList = savedCallInfo;
                        });
                    } else {
                        // 无元方法，返回 null
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    // 根据操作数类型选择使用有符号还是无符号整数
                    if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {
                        ALGORITHM_2(nativeInt64, +, opA->type);
                    } else {
                        ALGORITHM_2(nativeUInt64, +, opA->type);
                    }
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, opA, opB, "ADD_INT");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, opA, opB, "ADD_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_STRING) {
                SZrTypeValue concatResult;

                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_STRING(opA->type) && ZR_VALUE_IS_TYPE_STRING(opB->type));
                ZrCore_Value_ResetAsNull(&concatResult);
                if (!concat_values_to_destination(state, &concatResult, opA, opB, ZR_FALSE)) {
                    UPDATE_BASE(callInfo);
                    destination = destinationIsRet ? &ret : &BASE(E(instruction))->value;
                    ZrCore_Value_ResetAsNull(destination);
                } else if (destinationIsRet) {
                    ret = concatResult;
                } else {
                    UPDATE_BASE(callInfo);
                    destination = &BASE(E(instruction))->value;
                    ZrCore_Value_Copy(state, destination, &concatResult);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SUB);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    ALGORITHM_2(nativeInt64, -, opA->type);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, opA, opB, "SUB_INT");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, opA, opB, "SUB_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_MUL);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    ALGORITHM_2(nativeInt64, *, ZR_VALUE_TYPE_INT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    ALGORITHM_2(nativeUInt64, *, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(NEG) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeInt64, -opA->value.nativeObject.nativeInt64, opA->type);
                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                    ZrCore_Value_InitAsInt(state, destination, -(TZrInt64)opA->value.nativeObject.nativeUInt64);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeDouble, -opA->value.nativeObject.nativeDouble, opA->type);
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_NEG);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                        PROTECT_E(state, callInfo, {
                            TZrStackValuePointer metaBase = ZR_NULL;
                            TZrStackValuePointer restoredStackTop = savedStackTop;
                            TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                  savedCallInfo,
                                                                                  savedStackTop,
                                                                                  savedStackTop,
                                                                                  meta,
                                                                                  opA,
                                                                                  ZR_NULL,
                                                                                  ZR_META_CALL_UNARY_ARGUMENT_COUNT,
                                                                                  &metaBase,
                                                                                  &restoredStackTop);
                            RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                            if (metaCallSucceeded) {
                                SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                ZrCore_Value_ResetAsNull(destination);
                            }
                            state->stackTop.valuePointer = restoredStackTop;
                            state->callInfoList = savedCallInfo;
                        });
                    } else {
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_DIV);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: divide by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                        // ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                        ZrCore_Debug_RunError(state, "divide by zero");
                    }
                    ALGORITHM_2(nativeInt64, /, ZR_VALUE_TYPE_INT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: divide by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeUInt64 == 0)) {
                        // ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                        ZrCore_Debug_RunError(state, "divide by zero");
                    }
                    ALGORITHM_2(nativeUInt64, /, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if ((ZR_VALUE_IS_TYPE_NUMBER(opA->type) || ZR_VALUE_IS_TYPE_BOOL(opA->type)) &&
                    (ZR_VALUE_IS_TYPE_NUMBER(opB->type) || ZR_VALUE_IS_TYPE_BOOL(opB->type))) {
                    if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type) &&
                            ZR_VALUE_IS_TYPE_UNSIGNED_INT(opB->type)) {
                            SAVE_STATE(state, callInfo); // error: modulo by zero
                            if (ZR_UNLIKELY(opB->value.nativeObject.nativeUInt64 == 0)) {
                                ZrCore_Debug_RunError(state, "modulo by zero");
                            }
                            ALGORITHM_2(nativeUInt64, %, ZR_VALUE_TYPE_UINT64);
                        } else {
                            TZrInt64 divisor;

                            SAVE_STATE(state, callInfo); // error: modulo by zero
                            divisor = value_to_int64(opB);
                            if (ZR_UNLIKELY(divisor == 0)) {
                                ZrCore_Debug_RunError(state, "modulo by zero");
                            }
                            if (ZR_UNLIKELY(divisor < 0)) {
                                divisor = -divisor;
                            }
                            ZrCore_Value_InitAsInt(state, destination, value_to_int64(opA) % divisor);
                        }
                    } else {
                        execution_try_binary_numeric_float_fallback_or_raise(
                                state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD");
                    }
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_MOD);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                        PROTECT_E(state, callInfo, {
                            TZrStackValuePointer metaBase = ZR_NULL;
                            TZrStackValuePointer restoredStackTop = savedStackTop;
                            TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                                  savedCallInfo,
                                                                                  savedStackTop,
                                                                                  savedStackTop,
                                                                                  meta,
                                                                                  opA,
                                                                                  opB,
                                                                                  ZR_META_CALL_MAX_ARGUMENTS,
                                                                                  &metaBase,
                                                                                  &restoredStackTop);
                            RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                            if (metaCallSucceeded) {
                                SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                ZrCore_Value_ResetAsNull(destination);
                            }
                            state->stackTop.valuePointer = restoredStackTop;
                            state->callInfoList = savedCallInfo;
                        });
                    } else {
                        // 无元方法，返回 null
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: modulo by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                        ZrCore_Debug_RunError(state, "modulo by zero");
                    }
                    TZrInt64 divisor = opB->value.nativeObject.nativeInt64;
                    if (ZR_UNLIKELY(divisor < 0)) {
                        divisor = -divisor;
                    }
                    ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: modulo by zero
                    if (opB->value.nativeObject.nativeUInt64 == 0) {
                        ZrCore_Debug_RunError(state, "modulo by zero");
                    }
                    ALGORITHM_2(nativeUInt64, %, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_POW);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: power domain error
                    TZrInt64 valueA = opA->value.nativeObject.nativeInt64;
                    TZrInt64 valueB = opB->value.nativeObject.nativeInt64;
                    if (ZR_UNLIKELY(valueA == 0 && valueB <= 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    if (ZR_UNLIKELY(valueA < 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    ALGORITHM_FUNC_2(nativeInt64, ZrCore_Math_IntPower, ZR_VALUE_TYPE_INT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: power domain error
                    TZrUInt64 valueA = opA->value.nativeObject.nativeUInt64;
                    TZrUInt64 valueB = opB->value.nativeObject.nativeUInt64;
                    if (ZR_UNLIKELY(valueA == 0 && valueB == 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    ALGORITHM_FUNC_2(nativeUInt64, ZrCore_Math_UIntPower, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SHIFT_LEFT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SHIFT_RIGHT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        TZrBool metaCallSucceeded = execution_invoke_meta_call(state,
                                                                              savedCallInfo,
                                                                              savedStackTop,
                                                                              savedStackTop,
                                                                              meta,
                                                                              opA,
                                                                              opB,
                                                                              ZR_META_CALL_MAX_ARGUMENTS,
                                                                              &metaBase,
                                                                              &restoredStackTop);
                        RELOAD_DESTINATION_AFTER_PROTECT(callInfo, instruction);
                        if (metaCallSucceeded) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                    ALGORITHM_1(nativeBool, !, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeInt64 == 0,
                                      ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeDouble == 0,
                                      ZR_VALUE_TYPE_BOOL);
                } else {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, &&, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, ||, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state, ZR_EXEC_NUMERIC_COMPARE_GREATER, destination, opA, opB, "LOGICAL_GREATER_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state, ZR_EXEC_NUMERIC_COMPARE_LESS, destination, opA, opB, "LOGICAL_LESS_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TZrBool result = ZrCore_Value_Equal(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TZrBool result = !ZrCore_Value_Equal(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state,
                        ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL,
                        destination,
                        opA,
                        opB,
                        "LOGICAL_GREATER_EQUAL_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state,
                        ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL,
                        destination,
                        opA,
                        opB,
                        "LOGICAL_LESS_EQUAL_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_NOT) {
                opA = &BASE(A1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type));
                ALGORITHM_1(nativeInt64, ~, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, &, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, |, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_XOR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, ^, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_FUNCTION_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in SUPER_FUNCTION_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_FUNCTION_TAIL_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) &&
                          "Function value is NULL in SUPER_FUNCTION_TAIL_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_CALL) {
                // FUNCTION_CALL 指令格式：
                // operandExtra (E) = resultSlot (返回值槽位，用于编译时；ZrCore_Function_PreCall 需要的是 expectedReturnCount)
                // operand1[0] (A1) = functionSlot (函数在栈上的槽位)
                // operand1[1] (B1) = parametersCount (参数数量，直接使用，不从栈读取)
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);  // 参数数量直接使用，不是栈槽位
                TZrSize expectedReturnCount = 1;  // 期望 1 个返回值；ZrCore_Function_PreCall 的 resultCount 表示 expectedReturnCount；E(instruction)=resultSlot 仅编译时用
                
                opA = &BASE(functionSlot)->value;
                // 这里只保证“非空可调用目标”进入统一预调用分派。
                // ZrCore_Function_PreCall 会继续分流 function/closure/native pointer，
                // 并在其它值类型上解析 @call 元方法。
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in FUNCTION_CALL");
                
                // 设置栈顶指针（函数在 functionSlot，参数在 functionSlot+1 到 functionSlot+parametersCount）
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }
                
                // save 下一条指令的地址：fetch 使用 *(PC+=1)，当前 programCounter 指向本指令，故保存 programCounter+1
                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo = ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    // NULL means native call
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    // a vm call
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in SUPER_DYN_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_CALL_CACHED) {
                TZrSize functionSlot = A1(instruction);
                const SZrFunctionCallSiteCacheEntry *cacheEntry =
                        execution_get_callsite_cache_entry(closure->function,
                                                           B1(instruction),
                                                           ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL);
                TZrSize expectedReturnCount = 1;
                SZrCallInfo *nextCallInfo;

                if (cacheEntry == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_DYN_CALL_CACHED: invalid callsite cache");
                }

                state->stackTop.valuePointer = BASE(functionSlot) + cacheEntry->argumentCount + 1;
                execution_try_prepare_dyn_call_target_cached(state,
                                                             closure->function,
                                                             B1(instruction),
                                                             BASE(functionSlot),
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL);

                callInfo->context.context.programCounter = programCounter + 1;
                nextCallInfo = ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_TAIL_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) &&
                          "Function value is NULL in SUPER_DYN_TAIL_CALL_NO_ARGS");

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_TAIL_CALL_CACHED) {
                TZrSize functionSlot = A1(instruction);
                const SZrFunctionCallSiteCacheEntry *cacheEntry =
                        execution_get_callsite_cache_entry(closure->function,
                                                           B1(instruction),
                                                           ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                if (cacheEntry == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_DYN_TAIL_CALL_CACHED: invalid callsite cache");
                }

                state->stackTop.valuePointer = BASE(functionSlot) + cacheEntry->argumentCount + 1;
                execution_try_prepare_dyn_call_target_cached(state,
                                                             closure->function,
                                                             B1(instruction),
                                                             BASE(functionSlot),
                                                             ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL);

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DYN_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in DYN_CALL");

                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }

                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DYN_TAIL_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                opA = &BASE(functionSlot)->value;
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in DYN_TAIL_CALL");

                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_META_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                if (!execution_prepare_meta_call_target(state, BASE(functionSlot))) {
                    ZrCore_Debug_RunError(state, "SUPER_META_CALL_NO_ARGS: receiver does not define @call");
                }

                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_META_CALL_CACHED) {
                TZrSize functionSlot = A1(instruction);
                const SZrFunctionCallSiteCacheEntry *cacheEntry =
                        execution_get_callsite_cache_entry(closure->function,
                                                           B1(instruction),
                                                           ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL);
                TZrSize expectedReturnCount = 1;
                SZrCallInfo *nextCallInfo;

                if (cacheEntry == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_META_CALL_CACHED: invalid callsite cache");
                }

                state->stackTop.valuePointer = BASE(functionSlot) + cacheEntry->argumentCount + 1;
                if (!execution_prepare_meta_call_target_cached(state,
                                                               closure->function,
                                                               B1(instruction),
                                                               BASE(functionSlot),
                                                               ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_CALL_CACHED: receiver does not define @call");
                }

                callInfo->context.context.programCounter = programCounter + 1;
                nextCallInfo = ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_META_TAIL_CALL_NO_ARGS) {
                TZrSize functionSlot = A1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                state->stackTop.valuePointer = BASE(functionSlot) + 1;
                if (!execution_prepare_meta_call_target(state, BASE(functionSlot))) {
                    ZrCore_Debug_RunError(state, "SUPER_META_TAIL_CALL_NO_ARGS: receiver does not define @call");
                }

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_META_TAIL_CALL_CACHED) {
                TZrSize functionSlot = A1(instruction);
                const SZrFunctionCallSiteCacheEntry *cacheEntry =
                        execution_get_callsite_cache_entry(closure->function,
                                                           B1(instruction),
                                                           ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                if (cacheEntry == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SUPER_META_TAIL_CALL_CACHED: invalid callsite cache");
                }

                state->stackTop.valuePointer = BASE(functionSlot) + cacheEntry->argumentCount + 1;
                if (!execution_prepare_meta_call_target_cached(state,
                                                               closure->function,
                                                               B1(instruction),
                                                               BASE(functionSlot),
                                                               ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_TAIL_CALL_CACHED: receiver does not define @call");
                }

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(META_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;

                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }

                if (!execution_prepare_meta_call_target(state, BASE(functionSlot))) {
                    ZrCore_Debug_RunError(state, "META_CALL: receiver does not define @call");
                }
                parametersCount++;

                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo =
                        ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(META_TAIL_CALL) {
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);
                TZrSize expectedReturnCount = 1;
                TZrStackValuePointer functionPointer;
                SZrCallInfo *nextCallInfo;

                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }

                if (!execution_prepare_meta_call_target(state, BASE(functionSlot))) {
                    ZrCore_Debug_RunError(state, "META_TAIL_CALL: receiver does not define @call");
                }
                parametersCount++;

                callInfo->context.context.programCounter = programCounter + 1;
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_TAIL_CALL) {
                // FUNCTION_TAIL_CALL 指令格式：
                // operandExtra (E) = resultSlot (返回值槽位，编译时；ZrCore_Function_PreCall 需要的是 expectedReturnCount)
                // operand1[0] (A1) = functionSlot (函数在栈上的槽位)
                // operand1[1] (B1) = parametersCount (参数数量，直接使用，不从栈读取)
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);  // 参数数量直接使用，不是栈槽位
                TZrSize expectedReturnCount = 1;  // 期望 1 个返回值；E(instruction)=resultSlot 仅编译时用
                
                opA = &BASE(functionSlot)->value;
                // 与普通调用保持一致，把实际可调用性判断交给统一预调用分派，
                // 以便对象值通过 @call 元方法进入调用链。
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in FUNCTION_CALL");
                
                // 设置栈顶指针
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }
                
                // 尾调用：重用当前调用帧
                // 保存下一条指令的地址：fetch 使用 *(PC+=1)，故保存 programCounter+1
                callInfo->context.context.programCounter = programCounter + 1;
                // 设置尾调用标志
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                // 准备调用参数（函数在BASE(functionSlot)，参数在BASE(functionSlot+1)到BASE(functionSlot+parametersCount)）
                TZrStackValuePointer functionPointer = BASE(functionSlot);
                if (execution_try_reuse_tail_call_frame(state, callInfo, functionPointer)) {
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    goto LZrStart;
                }
                // 调用函数（expectedReturnCount=1，与 FUNCTION_CALL 一致）；返回值写入 BASE(E(instruction))
                SZrCallInfo *nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    // Native调用，清除尾调用标志
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    RESUME_AFTER_NATIVE_CALL(state, callInfo);
                } else {
                    // VM调用：对于尾调用，重用当前callInfo而不是创建新的
                    // 但ZrFunctionPreCall总是创建新的callInfo，所以我们需要调整
                    // 实际上，对于真正的尾调用优化，我们需要手动设置callInfo的字段
                    // 这里先使用简单的实现：清除尾调用标志，使用普通调用
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_RETURN) {
                // FUNCTION_RETURN 指令格式：
                // operandExtra (E) = 返回值数量 (returnCount)
                // operand1[0] (A1) = 返回值槽位 (resultSlot)
                // operand1[1] (B1) = 可变参数参数数量 (variableArguments, 0 表示非可变参数函数)
                TZrSize returnCount = E(instruction);
                TZrSize resultSlot = A1(instruction);
                TZrSize variableArguments = B1(instruction);

                // save its program counter
                callInfo->context.context.programCounter = programCounter;
                execution_discard_exception_handlers_for_callinfo(state, callInfo);

                if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
                    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
                }
                // Always close open upvalues for the returning frame. The to-be-closed
                // list only tracks close metas, not ordinary captured locals.
                ZrCore_Closure_CloseClosure(state,
                                      callInfo->functionBase.valuePointer + 1,
                                      ZR_THREAD_STATUS_INVALID,
                                      ZR_FALSE);
                UPDATE_BASE(callInfo);

                // 如果是可变参数函数，需要调整 functionBase 指针
                // 参考 Lua: if (nparams1) ci->func.p -= ci->u.l.nextraargs + nparams1;
                if (variableArguments > 0) {
                    callInfo->functionBase.valuePointer -=
                            callInfo->context.context.variableArgumentCount + variableArguments;
                }
                state->stackTop.valuePointer = BASE(resultSlot) + returnCount;
                ZrCore_Function_PostCall(state, callInfo, returnCount);
                trap = callInfo->context.context.trap;
                goto LZrReturn;
            }

        LZrReturn: {
            // return from vm
            if (callInfo->callStatus & ZR_CALL_STATUS_CREATE_FRAME) {
                return;
            } else {
                callInfo = callInfo->previous;
                goto LZrReturning;
            }
        }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GETUPVAL) {
                // GETUPVAL 指令格式：
                // operandExtra (E) = destination slot
                // operand1[0] (A1) = upvalue index
                // operand1[1] (B1) = 未使用
                TZrSize upvalueIndex = A1(instruction);
                SZrClosure *currentClosure = ZR_CAST_VM_CLOSURE(state, ZrCore_Stack_GetValue(base - 1)->value.object);
                if (ZR_UNLIKELY(upvalueIndex >= currentClosure->closureValueCount)) {
                    ZrCore_Debug_RunError(state, "upvalue index out of range");
                }
                SZrClosureValue *closureValue = currentClosure->closureValuesExtend[upvalueIndex];
                if (ZR_UNLIKELY(closureValue == ZR_NULL)) {
                    // 如果闭包值为 NULL，尝试初始化（这可能是第一次访问）
                    // 注意：这不应该发生在正常执行中，但为了测试的兼容性，我们允许这种情况
                    ZrCore_Debug_RunError(state, "upvalue is null - closure values may not be initialized");
                }
                ZrCore_Value_Copy(state, destination, ZrCore_ClosureValue_GetValue(closureValue));
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SETUPVAL) {
                // SETUPVAL 指令格式：
                // operandExtra (E) = source slot (destination)
                // operand1[0] (A1) = upvalue index
                // operand1[1] (B1) = 未使用
                TZrSize upvalueIndex = A1(instruction);
                SZrClosure *currentClosure = ZR_CAST_VM_CLOSURE(state, ZrCore_Stack_GetValue(base - 1)->value.object);
                if (ZR_UNLIKELY(upvalueIndex >= currentClosure->closureValueCount)) {
                    ZrCore_Debug_RunError(state, "upvalue index out of range");
                }
                SZrClosureValue *closureValue = currentClosure->closureValuesExtend[upvalueIndex];
                if (ZR_UNLIKELY(closureValue == ZR_NULL)) {
                    ZrCore_Debug_RunError(state, "upvalue is null");
                }
                SZrTypeValue *target = ZrCore_ClosureValue_GetValue(closureValue);
                ZrCore_Value_Copy(state, target, destination);
                ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(currentClosure->closureValuesExtend[upvalueIndex]),
                               destination);
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GET_SUB_FUNCTION) {
                // GET_SUB_FUNCTION 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = childFunctionIndex (子函数在 childFunctionList 中的索引)
                // operand1[1] (B1) = 0 (未使用)
                // GET_SUB_FUNCTION 用于从父函数的 childFunctionList 中通过索引获取子函数并压入栈
                // 这是编译时确定的静态索引，运行时直接通过索引访问，无需名称查找
                // 注意：GET_SUB_FUNCTION 只操作函数类型（ZR_VALUE_TYPE_FUNCTION 或 ZR_VALUE_TYPE_CLOSURE）
                TZrSize childFunctionIndex = A1(instruction);
                
                // 获取父函数的 callInfo
                SZrCallInfo *parentCallInfo = callInfo->previous;
                TZrBool found = ZR_FALSE;
                SZrFunction *parentFunction = ZR_NULL;
                
                if (parentCallInfo != ZR_NULL) {
                    TZrBool isVM = ZR_CALL_INFO_IS_VM(parentCallInfo);
                    if (isVM) {
                        // 获取父函数的闭包和函数
                        SZrTypeValue *parentFunctionBaseValue = ZrCore_Stack_GetValue(parentCallInfo->functionBase.valuePointer);
                        if (parentFunctionBaseValue != ZR_NULL) {
                            // 类型检查：确保父函数是函数类型或闭包类型
                            if (parentFunctionBaseValue->type == ZR_VALUE_TYPE_FUNCTION || 
                                parentFunctionBaseValue->type == ZR_VALUE_TYPE_CLOSURE) {
                                SZrClosure *parentClosure = ZR_CAST_VM_CLOSURE(state, parentFunctionBaseValue->value.object);
                                if (parentClosure != ZR_NULL && parentClosure->function != ZR_NULL) {
                                    parentFunction = parentClosure->function;
                                }
                            } else {
                                ZrCore_Debug_RunError(state, "GET_SUB_FUNCTION: parent must be a function or closure");
                            }
                        }
                    } else {
                        // 如果不是 VM 调用，尝试从当前函数的闭包获取子函数
                        if (closure != ZR_NULL && closure->function != ZR_NULL) {
                            parentFunction = closure->function;
                        }
                    }
                } else if (parentCallInfo == ZR_NULL) {
                    // 如果没有父函数的 callInfo，尝试从当前函数的闭包获取子函数
                    // 这适用于顶层函数或测试函数直接调用的情况
                    if (closure != ZR_NULL && closure->function != ZR_NULL) {
                        parentFunction = closure->function;
                    }
                }
                
                // 从父函数获取子函数
                if (parentFunction != ZR_NULL) {
                    // 通过索引直接访问 childFunctionList
                    if (childFunctionIndex < parentFunction->childFunctionLength) {
                        SZrFunction *childFunction = &parentFunction->childFunctionList[childFunctionIndex];
                        if (childFunction != ZR_NULL &&
                            childFunction->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                            SZrClosureValue **parentClosureValues =
                                    closure != ZR_NULL ? closure->closureValuesExtend : ZR_NULL;
                            ZrCore_Closure_PushToStack(state, childFunction, parentClosureValues, base, BASE(E(instruction)));
                            destination->type = ZR_VALUE_TYPE_CLOSURE;
                            destination->isGarbageCollectable = ZR_TRUE;
                            destination->isNative = ZR_FALSE;
                            found = ZR_TRUE;
                        }
                    }
                }
                
                // 如果没找到，返回 null
                if (!found) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);


            ZR_INSTRUCTION_LABEL(GET_GLOBAL) {
                // GET_GLOBAL 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = 0 (未使用)
                // operand1[1] (B1) = 0 (未使用)
                // GET_GLOBAL 用于获取全局 zr 对象到堆栈
                SZrGlobalState *global = state->global;
                if (global != ZR_NULL && global->zrObject.type == ZR_VALUE_TYPE_OBJECT) {
                    ZrCore_Value_Copy(state, destination, &global->zrObject);
                } else {
                    // 如果 zr 对象未初始化，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(TYPEOF) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Reflection_TypeOfValue(state, opA, destination)) {
                    ZrCore_Debug_RunError(state, "TYPEOF: failed to materialize runtime type");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GET_MEMBER) {
                SZrString *memberName = execution_resolve_member_symbol(closure, B1(instruction));
                SZrTypeValue memberKey;
                SZrTypeValue stableReceiver;
                TZrNativeString memberNativeName;
                TZrBool resolved = ZR_FALSE;

                opA = &BASE(A1(instruction))->value;
                stableReceiver = *opA;
                if (memberName == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "GET_MEMBER: invalid member id");
                } else if (stableReceiver.type != ZR_VALUE_TYPE_OBJECT &&
                           stableReceiver.type != ZR_VALUE_TYPE_ARRAY &&
                           stableReceiver.type != ZR_VALUE_TYPE_STRING) {
                    ZrCore_Debug_RunError(state, "GET_MEMBER: receiver must be an object, array, or string");
                }

                resolved = ZrCore_Object_GetMember(state, &stableReceiver, memberName, destination);
                if (!resolved &&
                    execution_make_member_key(state, memberName, &memberKey) &&
                    execution_try_materialize_global_prototypes(state, closure, callInfo, &stableReceiver, &memberKey)) {
                    resolved = ZrCore_Object_GetMember(state, &stableReceiver, memberName, destination);
                }

                if (!resolved) {
                    memberNativeName = ZrCore_String_GetNativeString(memberName);
                    ZrCore_Debug_RunError(state,
                                          "GET_MEMBER: missing member '%s'",
                                          memberNativeName != ZR_NULL ? memberNativeName : "<unknown>");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SET_MEMBER) {
                SZrString *memberName = execution_resolve_member_symbol(closure, B1(instruction));
                opA = &BASE(A1(instruction))->value;
                if (memberName == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "SET_MEMBER: invalid member id");
                } else if (!ZrCore_Object_SetMember(state, opA, memberName, destination)) {
                    ZrCore_Debug_RunError(state, "SET_MEMBER: receiver must be a writable object member");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(META_GET) {
                SZrString *memberName = execution_resolve_member_symbol(closure, B1(instruction));
                opA = &BASE(A1(instruction))->value;
                if (memberName == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "META_GET: invalid member id");
                } else if (!execution_meta_get_member(state, opA, memberName, destination)) {
                    ZrCore_Debug_RunError(state, "META_GET: receiver must define property getter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SUPER_META_GET_CACHED) {
                opA = &BASE(A1(instruction))->value;
                if (!execution_meta_get_cached_member(state, closure->function, B1(instruction), opA, destination)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_GET_CACHED: receiver must define property getter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(META_SET) {
                SZrString *memberName = execution_resolve_member_symbol(closure, B1(instruction));
                opA = destination;
                opB = &BASE(A1(instruction))->value;
                if (memberName == ZR_NULL) {
                    ZrCore_Debug_RunError(state, "META_SET: invalid member id");
                } else if (!execution_meta_set_member(state, opA, memberName, opB)) {
                    ZrCore_Debug_RunError(state, "META_SET: receiver must define property setter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SUPER_META_SET_CACHED) {
                opA = destination;
                opB = &BASE(A1(instruction))->value;
                if (!execution_meta_set_cached_member(state, closure->function, B1(instruction), opA, opB)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_SET_CACHED: receiver must define property setter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SUPER_META_GET_STATIC_CACHED) {
                opA = &BASE(A1(instruction))->value;
                if (!execution_meta_get_cached_static_member(state, closure->function, B1(instruction), opA, destination)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_GET_STATIC_CACHED: receiver must define static property getter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SUPER_META_SET_STATIC_CACHED) {
                opA = destination;
                opB = &BASE(A1(instruction))->value;
                if (!execution_meta_set_cached_static_member(state, closure->function, B1(instruction), opA, opB)) {
                    ZrCore_Debug_RunError(state, "SUPER_META_SET_STATIC_CACHED: receiver must define static property setter");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GET_BY_INDEX) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (!ZrCore_Object_GetByIndex(state, opA, opB, destination)) {
                    ZrCore_Debug_RunError(state, "GET_BY_INDEX: receiver must be an object or array");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SET_BY_INDEX) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (!ZrCore_Object_SetByIndex(state, opA, opB, destination)) {
                    ZrCore_Debug_RunError(state, "SET_BY_INDEX: receiver must be an object or array");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(ITER_INIT) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Object_IterInit(state, opA, destination)) {
                    ZrCore_Debug_RunError(state, "ITER_INIT: receiver does not satisfy iterable contract");
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(ITER_MOVE_NEXT) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Object_IterMoveNext(state, opA, destination)) {
                    ZrCore_Debug_RunError(state, "ITER_MOVE_NEXT: receiver does not satisfy iterator contract");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DYN_ITER_INIT) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Object_IterInit(state, opA, destination)) {
                    ZrCore_Debug_RunError(state, "DYN_ITER_INIT: receiver does not satisfy dynamic iterable contract");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DYN_ITER_MOVE_NEXT) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Object_IterMoveNext(state, opA, destination)) {
                    ZrCore_Debug_RunError(state, "DYN_ITER_MOVE_NEXT: receiver does not satisfy dynamic iterator contract");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE) {
                TZrInt16 jumpOffset = (TZrInt16)B1(instruction);

                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Object_IterMoveNext(state, opA, destination)) {
                    ZrCore_Debug_RunError(state,
                                          "SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE: receiver does not satisfy dynamic "
                                          "iterator contract");
                }

                if (!execution_is_truthy(state, destination)) {
                    programCounter += jumpOffset;
                    UPDATE_TRAP(callInfo);
                }
            }
            DONE_SKIP(2);

            ZR_INSTRUCTION_LABEL(ITER_CURRENT) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Object_IterCurrent(state, opA, destination)) {
                    ZrCore_Debug_RunError(state, "ITER_CURRENT: receiver does not satisfy iterator contract");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP) { JUMP(callInfo, instruction, 0); }
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP_IF) {
                // JUMP_IF 指令格式：
                // operandExtra (E) = condSlot (条件值的栈槽)
                // operand2[0] (A2) = offset (相对跳转偏移量)
                // 如果条件为假，跳转到 else 分支；如果条件为真，继续执行 then 分支
                SZrTypeValue *condValue = &BASE(E(instruction))->value;
                TZrBool condition = execution_is_truthy(state, condValue);
                
                // 如果条件为假，跳转到 else 分支
                if (!condition) {
                    JUMP(callInfo, instruction, 0);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_CLOSURE) {
                // CREATE_CLOSURE 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = functionConstantIndex
                // operand1[1] (B1) = closureVarCount
                TZrSize functionConstantIndex = A1(instruction);
                SZrTypeValue *functionConstant = CONST(functionConstantIndex);
                // 从常量池获取函数对象
                // 注意：编译器将SZrFunction*存储为ZR_VALUE_TYPE_CLOSURE类型，但value.object实际指向SZrFunction*
                SZrFunction *function = ZR_NULL;
                if (functionConstant->type == ZR_VALUE_TYPE_CLOSURE ||
                    functionConstant->type == ZR_VALUE_TYPE_FUNCTION) {
                    // 从raw object获取实际的函数对象
                    SZrRawObject *rawObject = functionConstant->value.object;
                    if (rawObject != ZR_NULL && rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                        function = ZR_CAST(SZrFunction *, rawObject);
                    }
                }
                if (function != ZR_NULL) {
                    SZrClosureValue **parentClosureValues =
                            closure != ZR_NULL ? closure->closureValuesExtend : ZR_NULL;
                    if (!execution_try_reuse_preinstalled_top_level_closure(state,
                                                                            closure,
                                                                            function,
                                                                            base,
                                                                            destination)) {
                        ZrCore_Closure_PushToStack(state, function, parentClosureValues, base, BASE(E(instruction)));
                        destination->type = ZR_VALUE_TYPE_CLOSURE;
                        destination->isGarbageCollectable = ZR_TRUE;
                        destination->isNative = ZR_FALSE;
                    }
                } else {
                    // 类型错误或函数为NULL
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_OBJECT) {
                // 创建空对象
                SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
                if (object != ZR_NULL) {
                    ZrCore_Object_Init(state, object);
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_ARRAY) {
                // 创建空数组对象
                SZrObject *array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
                if (array != ZR_NULL) {
                    ZrCore_Object_Init(state, array);
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_UNIQUE) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_UniqueValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_USING) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_UsingValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_SHARE) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_ShareValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_WEAK) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_WeakValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_UPGRADE) {
                opA = &BASE(A1(instruction))->value;
                if (!ZrCore_Ownership_UpgradeValue(state, destination, opA)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(OWN_RELEASE) {
                opA = &BASE(A1(instruction))->value;
                ZrCore_Ownership_ReleaseValue(state, opA);
                ZrCore_Value_ResetAsNull(destination);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MARK_TO_BE_CLOSED) {
                TZrSize closeSlot = E(instruction);
                TZrStackValuePointer closePointer = BASE(closeSlot);
                ZrCore_Closure_ToBeClosedValueClosureNew(state, closePointer);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CLOSE_SCOPE) {
                close_scope_cleanup_registrations(state, E(instruction));
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TRY) {
                if (!execution_push_exception_handler(state, callInfo, E(instruction))) {
                    if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_MEMORY_ERROR)) {
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
                    }
                    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(END_TRY) {
                SZrVmExceptionHandlerState *handlerState = execution_find_handler_state(state, callInfo, E(instruction));
                SZrFunction *handlerFunction = ZR_NULL;
                const SZrFunctionExceptionHandlerInfo *handlerInfo =
                        execution_lookup_exception_handler_info(state, handlerState, &handlerFunction);

                if (handlerState != ZR_NULL) {
                    if (handlerInfo != ZR_NULL && handlerInfo->hasFinally) {
                        handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
                    } else {
                        execution_pop_exception_handler(state, handlerState);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(THROW) {
                SZrTypeValue payload;

                SAVE_PC(state, callInfo);
                execution_clear_pending_control(state);
                payload = BASE(E(instruction))->value;
                if (!ZrCore_Exception_NormalizeThrownValue(state,
                                                          &payload,
                                                          callInfo,
                                                          ZR_THREAD_STATUS_RUNTIME_ERROR)) {
                    if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                    }
                    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                }

                if (execution_unwind_exception_to_handler(state, &callInfo)) {
                    goto LZrReturning;
                }

                ZrCore_Exception_Throw(state, state->currentExceptionStatus);
                ZR_ABORT();
            }
            ZR_INSTRUCTION_LABEL(CATCH) {
                if (state->hasCurrentException) {
                    ZrCore_Value_Copy(state, destination, &state->currentException);
                    ZrCore_Exception_ClearCurrent(state);
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(END_FINALLY) {
                SZrVmExceptionHandlerState *handlerState = execution_find_handler_state(state, callInfo, E(instruction));
                SZrCallInfo *resumeCallInfo;
                TZrStackValuePointer targetSlot;

                if (handlerState != ZR_NULL) {
                    execution_pop_exception_handler(state, handlerState);
                }

                switch (state->pendingControl.kind) {
                    case ZR_VM_PENDING_CONTROL_NONE:
                        break;
                    case ZR_VM_PENDING_CONTROL_EXCEPTION:
                        resumeCallInfo = state->pendingControl.callInfo != ZR_NULL
                                                 ? state->pendingControl.callInfo
                                                 : callInfo;
                        callInfo = resumeCallInfo;
                        if (execution_unwind_exception_to_handler(state, &callInfo)) {
                            goto LZrReturning;
                        }
                        ZrCore_Exception_Throw(state, state->currentExceptionStatus);
                        break;
                    case ZR_VM_PENDING_CONTROL_RETURN:
                    case ZR_VM_PENDING_CONTROL_BREAK:
                    case ZR_VM_PENDING_CONTROL_CONTINUE:
                        resumeCallInfo = state->pendingControl.callInfo != ZR_NULL
                                                 ? state->pendingControl.callInfo
                                                 : callInfo;
                        callInfo = resumeCallInfo;
                        if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                            goto LZrReturning;
                        }

                        if (state->pendingControl.kind == ZR_VM_PENDING_CONTROL_RETURN &&
                            state->pendingControl.hasValue &&
                            resumeCallInfo != ZR_NULL &&
                            resumeCallInfo->functionBase.valuePointer != ZR_NULL) {
                            targetSlot = resumeCallInfo->functionBase.valuePointer + 1 + state->pendingControl.valueSlot;
                            ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);
                        }

                        if (execution_jump_to_instruction_offset(state,
                                                                 &callInfo,
                                                                 resumeCallInfo,
                                                                 state->pendingControl.targetInstructionOffset)) {
                            execution_clear_pending_control(state);
                            goto LZrReturning;
                        }

                        execution_clear_pending_control(state);
                        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
                            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                        }
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                        break;
                    default:
                        execution_clear_pending_control(state);
                        break;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_RETURN) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_RETURN,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              E(instruction),
                                              &BASE(E(instruction))->value);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_BREAK) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_BREAK,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              0,
                                              ZR_NULL);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_CONTINUE) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_CONTINUE,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              0,
                                              ZR_NULL);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_DEFAULT() {
                // todo: error unreachable
                char message[ZR_RUNTIME_DIAGNOSTIC_BUFFER_LENGTH];
                sprintf(message, "Not implemented op code:%d at offset %d\n", instruction.instruction.operationCode,
                        (int) (instructionsEnd - programCounter));
                ZrCore_Debug_RunError(state, message);
            }
            DONE(1);
        }
    }

#undef DONE
#undef ZrCore_Debug_RunError
}
