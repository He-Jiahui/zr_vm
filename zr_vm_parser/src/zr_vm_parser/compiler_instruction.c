//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

TZrInstruction create_instruction_0(EZrInstructionCode opcode, TZrUInt16 operandExtra) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = 0;
    return instruction;
}

TZrInstruction create_instruction_1(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operand) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

TZrInstruction create_instruction_2(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt16 operand1,
                                    TZrUInt16 operand2) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operand1;
    instruction.instruction.operand.operand1[1] = operand2;
    return instruction;
}

TZrInstruction create_instruction_4(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt8 op0, TZrUInt8 op1,
                                           TZrUInt8 op2, TZrUInt8 op3) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand0[0] = op0;
    instruction.instruction.operand.operand0[1] = op1;
    instruction.instruction.operand.operand0[2] = op2;
    instruction.instruction.operand.operand0[3] = op3;
    return instruction;
}

TZrBool compiler_copy_range_to_raw(SZrCompilerState *cs,
                                          TZrPtr *outMemory,
                                          const TZrPtr source,
                                          TZrSize count,
                                          TZrSize elementSize) {
    TZrSize bytes;

    if (outMemory == ZR_NULL) {
        return ZR_FALSE;
    }

    *outMemory = ZR_NULL;
    if (cs == ZR_NULL || cs->state == ZR_NULL || count == 0 || source == ZR_NULL || elementSize == 0) {
        return ZR_TRUE;
    }

    bytes = count * elementSize;
    *outMemory = ZrCore_Memory_RawMallocWithType(cs->state->global, bytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (*outMemory == ZR_NULL) {
        return ZR_FALSE;
    }

    memcpy(*outMemory, source, bytes);
    return ZR_TRUE;
}

TZrBool compiler_copy_function_exception_metadata_slice(SZrCompilerState *cs,
                                                               SZrFunction *function,
                                                               TZrSize executionStart,
                                                               TZrSize catchStart,
                                                               TZrSize handlerStart,
                                                               SZrAstNode *sourceNode) {
    TZrSize executionCount;
    TZrSize catchCount;
    TZrSize handlerCount;

    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    executionCount = (cs->executionLocations.length > executionStart)
                             ? (cs->executionLocations.length - executionStart)
                             : 0;
    catchCount = (cs->catchClauseInfos.length > catchStart)
                         ? (cs->catchClauseInfos.length - catchStart)
                         : 0;
    handlerCount = (cs->exceptionHandlerInfos.length > handlerStart)
                           ? (cs->exceptionHandlerInfos.length - handlerStart)
                           : 0;

    function->executionLocationInfoList = ZR_NULL;
    function->executionLocationInfoLength = 0;
    function->catchClauseList = ZR_NULL;
    function->catchClauseCount = 0;
    function->exceptionHandlerList = ZR_NULL;
    function->exceptionHandlerCount = 0;
    function->sourceCodeList = (sourceNode != ZR_NULL) ? sourceNode->location.source : ZR_NULL;

    if (executionCount > 0) {
        SZrFunctionExecutionLocationInfo *src =
                (SZrFunctionExecutionLocationInfo *)ZrCore_Array_Get(&cs->executionLocations, executionStart);
        TZrPtr copied = ZR_NULL;
        if (!compiler_copy_range_to_raw(cs,
                                        &copied,
                                        src,
                                        executionCount,
                                        sizeof(SZrFunctionExecutionLocationInfo))) {
            return ZR_FALSE;
        }
        function->executionLocationInfoList = (SZrFunctionExecutionLocationInfo *)copied;
        function->executionLocationInfoLength = (TZrUInt32)executionCount;
    }

    if (catchCount > 0) {
        function->catchClauseList = (SZrFunctionCatchClauseInfo *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                catchCount * sizeof(SZrFunctionCatchClauseInfo),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->catchClauseList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < catchCount; index++) {
            SZrCompilerCatchClauseInfo *src =
                    (SZrCompilerCatchClauseInfo *)ZrCore_Array_Get(&cs->catchClauseInfos, catchStart + index);
            SZrFunctionCatchClauseInfo *dst = &function->catchClauseList[index];
            SZrLabel *targetLabel = (src != ZR_NULL && src->targetLabelId < cs->labels.length)
                                            ? (SZrLabel *)ZrCore_Array_Get(&cs->labels, src->targetLabelId)
                                            : ZR_NULL;

            dst->typeName = (src != ZR_NULL) ? src->typeName : ZR_NULL;
            dst->targetInstructionOffset =
                    (targetLabel != ZR_NULL) ? (TZrMemoryOffset)targetLabel->instructionIndex : 0;
        }
        function->catchClauseCount = (TZrUInt32)catchCount;
    }

    if (handlerCount > 0) {
        function->exceptionHandlerList = (SZrFunctionExceptionHandlerInfo *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                handlerCount * sizeof(SZrFunctionExceptionHandlerInfo),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->exceptionHandlerList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < handlerCount; index++) {
            SZrCompilerExceptionHandlerInfo *src =
                    (SZrCompilerExceptionHandlerInfo *)ZrCore_Array_Get(&cs->exceptionHandlerInfos,
                                                                        handlerStart + index);
            SZrFunctionExceptionHandlerInfo *dst = &function->exceptionHandlerList[index];
            SZrLabel *finallyLabel = (src != ZR_NULL && src->finallyLabelId < cs->labels.length)
                                             ? (SZrLabel *)ZrCore_Array_Get(&cs->labels, src->finallyLabelId)
                                             : ZR_NULL;
            SZrLabel *afterFinallyLabel = (src != ZR_NULL && src->afterFinallyLabelId < cs->labels.length)
                                                  ? (SZrLabel *)ZrCore_Array_Get(&cs->labels, src->afterFinallyLabelId)
                                                  : ZR_NULL;

            if (src == ZR_NULL) {
                memset(dst, 0, sizeof(*dst));
                continue;
            }

            dst->protectedStartInstructionOffset = src->protectedStartInstructionOffset;
            dst->finallyTargetInstructionOffset =
                    (finallyLabel != ZR_NULL) ? (TZrMemoryOffset)finallyLabel->instructionIndex : 0;
            dst->afterFinallyInstructionOffset =
                    (afterFinallyLabel != ZR_NULL) ? (TZrMemoryOffset)afterFinallyLabel->instructionIndex : 0;
            dst->catchClauseStartIndex = (TZrUInt32)(src->catchClauseStartIndex - catchStart);
            dst->catchClauseCount = src->catchClauseCount;
            dst->hasFinally = src->hasFinally;
        }
        function->exceptionHandlerCount = (TZrUInt32)handlerCount;
    }

    return ZR_TRUE;
}

// 添加指令到当前函数
void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction) {
    SZrFunctionExecutionLocationInfo locationInfo;

    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    ZrCore_Array_Push(cs->state, &cs->instructions, &instruction);
    // instructionCount 应该与 instructions.length 保持同步
    cs->instructionCount = cs->instructions.length;

    locationInfo.currentInstructionOffset = (TZrMemoryOffset)(cs->instructionCount - 1);
    locationInfo.lineInSource = (cs->currentAst != ZR_NULL && cs->currentAst->location.start.line > 0)
                                        ? (TZrUInt32)cs->currentAst->location.start.line
                                        : 0;
    ZrCore_Array_Push(cs->state, &cs->executionLocations, &locationInfo);
}

// 添加常量到常量池
TZrUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value) {
    if (cs == ZR_NULL || cs->hasError || value == ZR_NULL) {
        return 0;
    }

    if (cs->constantCount != cs->constants.length) {
        cs->constantCount = cs->constants.length;
    }

    // 检查常量是否已存在（常量去重）
    // 遍历已有的常量，查找相同的值
    for (TZrSize i = 0; i < cs->constants.length; i++) {
        SZrTypeValue *existingValue = (SZrTypeValue *)ZrCore_Array_Get(&cs->constants, i);
        if (existingValue != ZR_NULL) {
            // 使用 ZrCore_Value_Equal 比较常量值
            if (ZrCore_Value_Equal(cs->state, existingValue, value)) {
                // 找到相同的常量，返回已有常量的索引
                return (TZrUInt32)i;
            }
        }
    }

    // 常量不存在，添加新常量
    ZrCore_Array_Push(cs->state, &cs->constants, value);
    TZrUInt32 index = (TZrUInt32)(cs->constants.length - 1);
    cs->constantCount = cs->constants.length;
    return index;
}

TZrUInt32 compiler_get_or_add_member_entry_with_flags(SZrCompilerState *cs,
                                                      SZrString *memberName,
                                                      TZrUInt8 flags) {
    SZrFunction *function;
    SZrFunctionMemberEntry *newEntries;
    TZrSize newCount;
    TZrSize copyBytes;

    if (cs == ZR_NULL || cs->currentFunction == ZR_NULL || memberName == ZR_NULL) {
        return ZR_PARSER_MEMBER_ID_NONE;
    }

    function = cs->currentFunction;
    for (TZrUInt32 index = 0; index < function->memberEntryLength; index++) {
        SZrFunctionMemberEntry *entry = &function->memberEntries[index];
        if (entry->entryKind == ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL &&
            entry->symbol != ZR_NULL &&
            entry->reserved0 == flags &&
            ZrCore_String_Equal(entry->symbol, memberName)) {
            return index;
        }
    }

    newCount = (TZrSize)function->memberEntryLength + 1;
    newEntries = (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionMemberEntry) * newCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newEntries == ZR_NULL) {
        return ZR_PARSER_MEMBER_ID_NONE;
    }

    copyBytes = sizeof(SZrFunctionMemberEntry) * function->memberEntryLength;
    if (function->memberEntries != ZR_NULL && copyBytes > 0) {
        memcpy(newEntries, function->memberEntries, copyBytes);
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      function->memberEntries,
                                      sizeof(SZrFunctionMemberEntry) * function->memberEntryLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    memset(&newEntries[newCount - 1], 0, sizeof(SZrFunctionMemberEntry));
    newEntries[newCount - 1].symbol = memberName;
    newEntries[newCount - 1].entryKind = ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL;
    newEntries[newCount - 1].reserved0 = flags;

    function->memberEntries = newEntries;
    function->memberEntryLength = (TZrUInt32)newCount;
    return (TZrUInt32)(newCount - 1);
}

TZrUInt32 compiler_get_or_add_member_entry(SZrCompilerState *cs, SZrString *memberName) {
    return compiler_get_or_add_member_entry_with_flags(cs, memberName, 0);
}

TZrBool compiler_value_is_compile_time_function_pointer(SZrCompilerState *cs, const SZrTypeValue *value) {
    TZrPtr pointerValue;

    if (cs == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_FALSE;
    }

    pointerValue = value->value.nativeObject.nativePointer;
    if (pointerValue == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, i);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (TZrPtr)(*funcPtr) == pointerValue) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool compiler_is_runtime_safe_compile_time_value_internal(SZrCompilerState *cs,
                                                                  const SZrTypeValue *value,
                                                                  SZrRawObject **visitedObjects,
                                                                  TZrSize visitedCount,
                                                                  TZrUInt32 depth) {
    if (cs == ZR_NULL || value == ZR_NULL || visitedObjects == ZR_NULL) {
        return ZR_FALSE;
    }

    if (depth > ZR_PARSER_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH) {
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return ZR_FALSE;
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY: {
            SZrRawObject *rawObject = ZrCore_Value_GetRawObject(value);
            SZrObject *object;

            if (rawObject == ZR_NULL) {
                return ZR_TRUE;
            }

            for (TZrSize i = 0; i < visitedCount; i++) {
                if (visitedObjects[i] == rawObject) {
                    return ZR_TRUE;
                }
            }

            if (visitedCount >= ZR_PARSER_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH) {
                return ZR_FALSE;
            }

            object = ZR_CAST_OBJECT(cs->state, rawObject);
            if (object == ZR_NULL || !object->nodeMap.isValid || object->nodeMap.buckets == ZR_NULL) {
                return ZR_TRUE;
            }

            visitedObjects[visitedCount] = rawObject;
            for (TZrSize bucketIndex = 0; bucketIndex < object->nodeMap.capacity; bucketIndex++) {
                SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucketIndex];
                while (pair != ZR_NULL) {
                    if (!compiler_is_runtime_safe_compile_time_value_internal(cs, &pair->key, visitedObjects,
                                                                              visitedCount + 1, depth + 1) ||
                        !compiler_is_runtime_safe_compile_time_value_internal(cs, &pair->value, visitedObjects,
                                                                              visitedCount + 1, depth + 1)) {
                        return ZR_FALSE;
                    }
                    pair = pair->next;
                }
            }
            return ZR_TRUE;
        }
        default:
            return ZR_TRUE;
    }
}

ZR_PARSER_API TZrBool ZrParser_Compiler_ValidateRuntimeProjectionValue(SZrCompilerState *cs,
                                                             const SZrTypeValue *value,
                                                             SZrFileRange location) {
    SZrRawObject *visitedObjects[ZR_PARSER_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH];
    const TZrChar *message;

    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < ZR_PARSER_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH; i++) {
        visitedObjects[i] = ZR_NULL;
    }

    if (compiler_is_runtime_safe_compile_time_value_internal(cs, value, visitedObjects, 0, 0)) {
        return ZR_TRUE;
    }

    cs->hasCompileTimeError = ZR_TRUE;
    cs->hasFatalError = ZR_TRUE;
    message = compiler_value_is_compile_time_function_pointer(cs, value)
                      ? "Compile-time value cannot be projected to runtime because it is a compile-time-only function reference"
                      : "Compile-time value cannot be projected to runtime because it contains native pointer values such as compile-time-only function references";
    ZrParser_Compiler_Error(cs, message, location);
    return ZR_FALSE;
}

// 分配局部变量槽位
