//
// ExecBC quickening.
// Name-based access rewrites are intentionally disabled: access semantics must
// remain explicit in emitted instructions and artifacts.
//

#include <limits.h>

#include "compiler_internal.h"

static TZrUInt32 compiler_quickening_find_deopt_id(const SZrFunction *function, TZrUInt32 instructionIndex) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];
        if (instruction->execInstructionIndex == instructionIndex) {
            return instruction->deoptId;
        }
    }

    return 0;
}

static TZrUInt8 compiler_quickening_member_entry_flags(const SZrFunction *function, TZrUInt32 memberEntryIndex) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return 0;
    }

    return function->memberEntries[memberEntryIndex].reserved0;
}

static TZrBool compiler_quickening_function_matches_inline_child(const SZrFunction *left, const SZrFunction *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    return left->functionName == right->functionName &&
           left->parameterCount == right->parameterCount &&
           left->instructionsLength == right->instructionsLength &&
           left->lineInSourceStart == right->lineInSourceStart &&
           left->lineInSourceEnd == right->lineInSourceEnd;
}

static void compiler_quickening_rebind_constant_function_values_to_children(SZrFunction *function) {
    if (function == ZR_NULL || function->constantValueList == ZR_NULL || function->childFunctionList == ZR_NULL ||
        function->childFunctionLength == 0) {
        return;
    }

    for (TZrUInt32 constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        SZrTypeValue *constant = &function->constantValueList[constantIndex];
        SZrRawObject *rawObject;
        SZrFunction *constantFunction = ZR_NULL;

        if ((constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE) ||
            constant->value.object == ZR_NULL) {
            continue;
        }

        rawObject = constant->value.object;
        if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
            constantFunction = ZR_CAST(SZrFunction *, rawObject);
        } else if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE && !constant->isNative) {
            SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
            if (closure != ZR_NULL) {
                constantFunction = closure->function;
            }
        }

        if (constantFunction == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            SZrFunction *childFunction = &function->childFunctionList[childIndex];
            if (!compiler_quickening_function_matches_inline_child(constantFunction, childFunction)) {
                continue;
            }

            if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                constant->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(childFunction);
            } else if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE && !constant->isNative) {
                SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
                if (closure != ZR_NULL) {
                    closure->function = childFunction;
                }
            }
            break;
        }
    }
}

static TZrBool compiler_quickening_append_callsite_cache(SZrState *state,
                                                         SZrFunction *function,
                                                         EZrFunctionCallSiteCacheKind kind,
                                                         TZrUInt32 instructionIndex,
                                                         TZrUInt32 memberEntryIndex,
                                                         TZrUInt32 deoptId,
                                                         TZrUInt32 argumentCount,
                                                         TZrUInt16 *outCacheIndex) {
    SZrFunctionCallSiteCacheEntry *newEntries;
    TZrSize newCount;
    TZrSize copyBytes;

    if (outCacheIndex != ZR_NULL) {
        *outCacheIndex = 0;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outCacheIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    newCount = (TZrSize)function->callSiteCacheLength + 1;
    if (newCount > UINT16_MAX) {
        return ZR_FALSE;
    }

    newEntries = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionCallSiteCacheEntry) * newCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newEntries == ZR_NULL) {
        return ZR_FALSE;
    }

    copyBytes = sizeof(SZrFunctionCallSiteCacheEntry) * function->callSiteCacheLength;
    if (function->callSiteCaches != ZR_NULL && copyBytes > 0) {
        memcpy(newEntries, function->callSiteCaches, copyBytes);
        ZrCore_Memory_RawFreeWithType(state->global,
                                      function->callSiteCaches,
                                      sizeof(SZrFunctionCallSiteCacheEntry) * function->callSiteCacheLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    memset(&newEntries[newCount - 1], 0, sizeof(SZrFunctionCallSiteCacheEntry));
    newEntries[newCount - 1].kind = (TZrUInt32)kind;
    newEntries[newCount - 1].instructionIndex = instructionIndex;
    newEntries[newCount - 1].memberEntryIndex = memberEntryIndex;
    newEntries[newCount - 1].deoptId = deoptId;
    newEntries[newCount - 1].argumentCount = argumentCount;

    function->callSiteCaches = newEntries;
    function->callSiteCacheLength = (TZrUInt32)newCount;
    *outCacheIndex = (TZrUInt16)(newCount - 1);
    return ZR_TRUE;
}

static TZrBool compiler_quicken_cached_calls(SZrState *state, SZrFunction *function) {
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrFunctionCallSiteCacheKind cacheKind;
        EZrInstructionCode quickenedOpcode;
        TZrUInt16 cacheIndex;
        TZrUInt32 argumentCount;

        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(META_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED);
                break;
            default:
                continue;
        }

        argumentCount = instruction->instruction.operand.operand1[1];
        if (argumentCount == 0) {
            continue;
        }

        if (!compiler_quickening_append_callsite_cache(state,
                                                       function,
                                                       cacheKind,
                                                       index,
                                                       ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE,
                                                       compiler_quickening_find_deopt_id(function, index),
                                                       argumentCount,
                                                       &cacheIndex)) {
            return ZR_FALSE;
        }

        instruction->instruction.operationCode = (TZrUInt16)quickenedOpcode;
        instruction->instruction.operand.operand1[1] = cacheIndex;
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_dynamic_iter_loop_guards(SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        TZrInstruction *iterMoveNextInst = &function->instructionsList[index];
        TZrInstruction *jumpIfInst = &function->instructionsList[index + 1];
        TZrInt32 jumpOffset;

        if ((EZrInstructionCode)iterMoveNextInst->instruction.operationCode != ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT)) {
            continue;
        }
        if ((EZrInstructionCode)jumpIfInst->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF)) {
            continue;
        }
        if (jumpIfInst->instruction.operandExtra != iterMoveNextInst->instruction.operandExtra) {
            continue;
        }

        jumpOffset = jumpIfInst->instruction.operand.operand2[0];
        if (jumpOffset < INT16_MIN || jumpOffset > INT16_MAX) {
            continue;
        }

        iterMoveNextInst->instruction.operationCode =
                (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE);
        iterMoveNextInst->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)jumpOffset);
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_zero_arg_calls(SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *callInst = &function->instructionsList[index];

        if (callInst->instruction.operand.operand1[1] != 0) {
            continue;
        }

        switch ((EZrInstructionCode)callInst->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(META_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS);
                break;
            default:
                break;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_meta_access(SZrState *state, SZrFunction *function) {
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrFunctionCallSiteCacheKind cacheKind;
        EZrInstructionCode quickenedOpcode;
        TZrUInt16 cacheIndex;
        TZrUInt32 memberEntryIndex;
        TZrUInt8 memberFlags;
        TZrBool isStaticAccessor;

        if (opcode == ZR_INSTRUCTION_ENUM(META_GET)) {
            memberEntryIndex = instruction->instruction.operand.operand1[1];
            memberFlags = compiler_quickening_member_entry_flags(function, memberEntryIndex);
            isStaticAccessor =
                    (TZrBool)((memberFlags & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0);
            cacheKind = isStaticAccessor ? ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC
                                         : ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET;
            quickenedOpcode = isStaticAccessor ? ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED)
                                               : ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED);
        } else if (opcode == ZR_INSTRUCTION_ENUM(META_SET)) {
            memberEntryIndex = instruction->instruction.operand.operand1[1];
            memberFlags = compiler_quickening_member_entry_flags(function, memberEntryIndex);
            isStaticAccessor =
                    (TZrBool)((memberFlags & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0);
            cacheKind = isStaticAccessor ? ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC
                                         : ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET;
            quickenedOpcode = isStaticAccessor ? ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED)
                                               : ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED);
        } else {
            continue;
        }

        if (!compiler_quickening_append_callsite_cache(state,
                                                       function,
                                                       cacheKind,
                                                       index,
                                                       memberEntryIndex,
                                                       compiler_quickening_find_deopt_id(function, index),
                                                       0,
                                                       &cacheIndex)) {
            return ZR_FALSE;
        }

        instruction->instruction.operationCode = (TZrUInt16)quickenedOpcode;
        instruction->instruction.operand.operand1[1] = cacheIndex;
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_child_functions(SZrState *state, SZrFunction *function) {
    TZrUInt32 childIndex;

    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!compiler_quicken_meta_access(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_cached_calls(state, function)) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_dynamic_iter_loop_guards(function)) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_zero_arg_calls(function)) {
        return ZR_FALSE;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (!compiler_quicken_child_functions(state, &function->childFunctionList[childIndex])) {
            return ZR_FALSE;
        }
    }

    compiler_quickening_rebind_constant_function_values_to_children(function);

    return ZR_TRUE;
}

TZrBool compiler_quicken_execbc_function(SZrState *state, SZrFunction *function) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    return compiler_quicken_child_functions(state, function);
}
