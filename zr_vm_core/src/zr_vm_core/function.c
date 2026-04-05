//
// Created by HeJiahui on 2025/7/20.
//

#include "zr_vm_core/function.h"

#include <stdlib.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
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
    SZrRawObject *newObject = ZrCore_RawObject_New(state, ZR_VALUE_TYPE_FUNCTION, sizeof(SZrFunction), ZR_FALSE);
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
    function->lineInSourceList = ZR_NULL;
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
    function->sourceHash = ZR_NULL;
    function->exportedVariables = ZR_NULL;
    function->exportedVariableLength = 0;
    function->typedLocalBindings = ZR_NULL;
    function->typedLocalBindingLength = 0;
    function->typedExportedSymbols = ZR_NULL;
    function->typedExportedSymbolLength = 0;
    function->parameterMetadata = ZR_NULL;
    function->parameterMetadataCount = 0;
    function->compileTimeVariableInfos = ZR_NULL;
    function->compileTimeVariableInfoLength = 0;
    function->compileTimeFunctionInfos = ZR_NULL;
    function->compileTimeFunctionInfoLength = 0;
    function->testInfos = ZR_NULL;
    function->testInfoLength = 0;
    function->hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&function->decoratorMetadataValue);
    function->decoratorNames = ZR_NULL;
    function->decoratorCount = 0;
    function->memberEntries = ZR_NULL;
    function->memberEntryLength = 0;
    function->functionName = ZR_NULL;  // 函数名，匿名函数为 ZR_NULL
    function->prototypeData = ZR_NULL;
    function->prototypeDataLength = 0;
    function->prototypeCount = 0;
    function->prototypeInstances = ZR_NULL;
    function->prototypeInstancesLength = 0;
    function->semIrTypeTable = ZR_NULL;
    function->semIrTypeTableLength = 0;
    function->semIrOwnershipTable = ZR_NULL;
    function->semIrOwnershipTableLength = 0;
    function->semIrEffectTable = ZR_NULL;
    function->semIrEffectTableLength = 0;
    function->semIrBlockTable = ZR_NULL;
    function->semIrBlockTableLength = 0;
    function->semIrInstructions = ZR_NULL;
    function->semIrInstructionLength = 0;
    function->semIrDeoptTable = ZR_NULL;
    function->semIrDeoptTableLength = 0;
    function->callSiteCaches = ZR_NULL;
    function->callSiteCacheLength = 0;
    function->runtimeDecoratorMetadata = ZR_NULL;
    function->runtimeDecoratorDecorators = ZR_NULL;
    return function;
}

static TZrBool function_matches_inline_child(const SZrFunction *left, const SZrFunction *right) {
    TZrBool sameFunctionName;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    sameFunctionName = left->functionName == right->functionName ||
                       (left->functionName != ZR_NULL && right->functionName != ZR_NULL &&
                        ZrCore_String_Equal(left->functionName, right->functionName));

    return sameFunctionName &&
           left->parameterCount == right->parameterCount &&
           left->instructionsLength == right->instructionsLength &&
           left->lineInSourceStart == right->lineInSourceStart &&
           left->lineInSourceEnd == right->lineInSourceEnd;
}

static const SZrFunction *function_resolve_closure_target_from_constant(const SZrTypeValue *constant) {
    SZrRawObject *rawObject;

    if (constant == ZR_NULL || constant->value.object == ZR_NULL ||
        (constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE)) {
        return ZR_NULL;
    }

    rawObject = constant->value.object;
    if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST(const SZrFunction *, rawObject);
    }

    if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE && !constant->isNative) {
        const SZrClosure *closure = ZR_CAST(const SZrClosure *, rawObject);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }

    return ZR_NULL;
}

static TZrBool function_child_graph_contains_target(const SZrFunction *function, const SZrFunction *target) {
    if (function == ZR_NULL || target == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        const SZrFunction *childFunction = &function->childFunctionList[childIndex];
        if (childFunction == target || function_child_graph_contains_target(childFunction, target)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_validate_create_closure_targets_in_child_graph_recursive(const SZrFunction *function) {
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode) instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(CREATE_CLOSURE)) {
            TZrUInt32 constantIndex = instruction->instruction.operand.operand1[0];
            const SZrFunction *targetFunction;

            if (function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
                return ZR_FALSE;
            }

            targetFunction = function_resolve_closure_target_from_constant(&function->constantValueList[constantIndex]);
            if (targetFunction == ZR_NULL || !function_child_graph_contains_target(function, targetFunction)) {
                for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
                    const SZrFunction *childFunction = &function->childFunctionList[childIndex];
                    fprintf(stderr,
                            "[closure-graph]   child[%u]=%p name=%s params=%u instr=%u lines=%u-%u\n",
                            (unsigned)childIndex,
                            (const void *)childFunction,
                            childFunction->functionName != ZR_NULL
                                    ? ZrCore_String_GetNativeString(childFunction->functionName)
                                    : "<anonymous>",
                            (unsigned)childFunction->parameterCount,
                            (unsigned)childFunction->instructionsLength,
                            (unsigned)childFunction->lineInSourceStart,
                            (unsigned)childFunction->lineInSourceEnd);
                }
                fprintf(stderr,
                        "[closure-graph] function=%s ptr=%p instr=%u const=%u target=%p targetName=%s params=%u instrs=%u lines=%u-%u childCount=%u\n",
                        function->functionName != ZR_NULL ? ZrCore_String_GetNativeString(function->functionName)
                                                         : "<anonymous>",
                        (const void *)function,
                        (unsigned)instructionIndex,
                        (unsigned)constantIndex,
                        (const void *)targetFunction,
                        (targetFunction != ZR_NULL && targetFunction->functionName != ZR_NULL)
                                ? ZrCore_String_GetNativeString(targetFunction->functionName)
                                : "<anonymous>",
                        targetFunction != ZR_NULL ? (unsigned)targetFunction->parameterCount : 0u,
                        targetFunction != ZR_NULL ? (unsigned)targetFunction->instructionsLength : 0u,
                        targetFunction != ZR_NULL ? (unsigned)targetFunction->lineInSourceStart : 0u,
                        targetFunction != ZR_NULL ? (unsigned)targetFunction->lineInSourceEnd : 0u,
                        (unsigned)function->childFunctionLength);
                return ZR_FALSE;
            }
        }
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (!function_validate_create_closure_targets_in_child_graph_recursive(&function->childFunctionList[childIndex])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void function_note_generated_frame_slot(TZrUInt32 slotIndex, TZrUInt32 *ioSlotCount) {
    TZrUInt32 requiredSlotCount;

    if (ioSlotCount == ZR_NULL) {
        return;
    }

    requiredSlotCount = slotIndex + 1u;
    if (requiredSlotCount > *ioSlotCount) {
        *ioSlotCount = requiredSlotCount;
    }
}

static void function_note_generated_call_span(TZrUInt32 callBaseSlot, TZrUInt32 argumentCount, TZrUInt32 *ioSlotCount) {
    function_note_generated_frame_slot(callBaseSlot, ioSlotCount);
    if (argumentCount > 0u && callBaseSlot <= UINT32_MAX - argumentCount) {
        function_note_generated_frame_slot(callBaseSlot + argumentCount, ioSlotCount);
    }
}

TZrUInt32 ZrCore_Function_GetGeneratedFrameSlotCount(const SZrFunction *function) {
    TZrUInt32 slotCount;

    if (function == ZR_NULL) {
        return 0u;
    }

    slotCount = function->stackSize;
    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;
        TZrUInt32 operandA1 = instruction->instruction.operand.operand1[0];
        TZrUInt32 operandB1 = instruction->instruction.operand.operand1[1];

        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            case ZR_INSTRUCTION_ENUM(CATCH):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot((TZrUInt32)instruction->instruction.operand.operand2[0], &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
            case ZR_INSTRUCTION_ENUM(TO_BOOL):
            case ZR_INSTRUCTION_ENUM(TO_INT):
            case ZR_INSTRUCTION_ENUM(TO_UINT):
            case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            case ZR_INSTRUCTION_ENUM(TO_STRING):
            case ZR_INSTRUCTION_ENUM(TO_STRUCT):
            case ZR_INSTRUCTION_ENUM(TO_OBJECT):
            case ZR_INSTRUCTION_ENUM(NEG):
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
            case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
            case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
            case ZR_INSTRUCTION_ENUM(OWN_USING):
            case ZR_INSTRUCTION_ENUM(OWN_SHARE):
            case ZR_INSTRUCTION_ENUM(OWN_WEAK):
            case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
            case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
            case ZR_INSTRUCTION_ENUM(TYPEOF):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            case ZR_INSTRUCTION_ENUM(META_GET):
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
            case ZR_INSTRUCTION_ENUM(META_SET):
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                function_note_generated_frame_slot(operandB1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(ADD):
            case ZR_INSTRUCTION_ENUM(ADD_INT):
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
            case ZR_INSTRUCTION_ENUM(SUB):
            case ZR_INSTRUCTION_ENUM(SUB_INT):
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
            case ZR_INSTRUCTION_ENUM(MUL):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
            case ZR_INSTRUCTION_ENUM(DIV):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
            case ZR_INSTRUCTION_ENUM(MOD):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            case ZR_INSTRUCTION_ENUM(POW):
            case ZR_INSTRUCTION_ENUM(POW_SIGNED):
            case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(POW_FLOAT):
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
            case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(BITWISE_AND):
            case ZR_INSTRUCTION_ENUM(BITWISE_OR):
            case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                function_note_generated_frame_slot(operandB1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(META_CALL):
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_call_span(operandA1, operandB1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(JUMP_IF):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(THROW):
            case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                function_note_generated_frame_slot(operandA1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(JUMP):
            case ZR_INSTRUCTION_ENUM(TRY):
            case ZR_INSTRUCTION_ENUM(END_TRY):
            case ZR_INSTRUCTION_ENUM(END_FINALLY):
            case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
            case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
            case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
            case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            default:
                break;
        }
    }

    return slotCount;
}

void ZrCore_Function_RebindConstantFunctionValuesToChildren(SZrFunction *function) {
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
            if (!function_matches_inline_child(constantFunction, childFunction)) {
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

TZrBool ZrCore_Function_ValidateCreateClosureTargetsInChildGraph(const SZrFunction *function) {
    return function_validate_create_closure_targets_in_child_graph_recursive(function);
}

void ZrCore_Function_Free(struct SZrState *state, SZrFunction *function) {
    SZrGlobalState *global = state->global;
    ZR_ASSERT(function != ZR_NULL);
#define ZR_FUNCTION_FREE_METADATA_PARAMETERS(PARAMETERS, COUNT)                                                      \
    do {                                                                                                             \
        if ((PARAMETERS) != ZR_NULL && (COUNT) > 0) {                                                                \
            for (TZrUInt32 metadataIndex = 0; metadataIndex < (COUNT); metadataIndex++) {                           \
                if ((PARAMETERS)[metadataIndex].decoratorNames != ZR_NULL &&                                         \
                    (PARAMETERS)[metadataIndex].decoratorCount > 0) {                                                \
                    ZrCore_Memory_RawFreeWithType(global,                                                            \
                                                  (PARAMETERS)[metadataIndex].decoratorNames,                        \
                                                  sizeof(SZrString *) * (PARAMETERS)[metadataIndex].decoratorCount,  \
                                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);                                   \
                }                                                                                                    \
            }                                                                                                        \
            ZrCore_Memory_RawFreeWithType(global,                                                                    \
                                          (PARAMETERS),                                                              \
                                          sizeof(SZrFunctionMetadataParameter) * (COUNT),                            \
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);                                           \
        }                                                                                                            \
    } while (0)

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
    if (function->lineInSourceList != ZR_NULL && function->instructionsLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->lineInSourceList, function->instructionsLength);
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
    if (function->typedExportedSymbols != ZR_NULL && function->typedExportedSymbolLength > 0) {
        for (TZrUInt32 i = 0; i < function->typedExportedSymbolLength; i++) {
            SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[i];
            if (symbol->parameterTypes != ZR_NULL && symbol->parameterCount > 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              symbol->parameterTypes,
                                              sizeof(SZrFunctionTypedTypeRef) * symbol->parameterCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      function->typedExportedSymbols,
                                      sizeof(SZrFunctionTypedExportSymbol) * function->typedExportedSymbolLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->typedLocalBindings != ZR_NULL && function->typedLocalBindingLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->typedLocalBindings,
                                      sizeof(SZrFunctionTypedLocalBinding) * function->typedLocalBindingLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    ZR_FUNCTION_FREE_METADATA_PARAMETERS(function->parameterMetadata, function->parameterMetadataCount);
    if (function->compileTimeFunctionInfos != ZR_NULL && function->compileTimeFunctionInfoLength > 0) {
        for (TZrUInt32 i = 0; i < function->compileTimeFunctionInfoLength; i++) {
            SZrFunctionCompileTimeFunctionInfo *info = &function->compileTimeFunctionInfos[i];
            ZR_FUNCTION_FREE_METADATA_PARAMETERS(info->parameters, info->parameterCount);
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      function->compileTimeFunctionInfos,
                                      sizeof(SZrFunctionCompileTimeFunctionInfo) * function->compileTimeFunctionInfoLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->compileTimeVariableInfos != ZR_NULL && function->compileTimeVariableInfoLength > 0) {
        for (TZrUInt32 i = 0; i < function->compileTimeVariableInfoLength; i++) {
            SZrFunctionCompileTimeVariableInfo *info = &function->compileTimeVariableInfos[i];
            if (info->pathBindings != ZR_NULL && info->pathBindingCount > 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              info->pathBindings,
                                              sizeof(SZrFunctionCompileTimePathBinding) * info->pathBindingCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      function->compileTimeVariableInfos,
                                      sizeof(SZrFunctionCompileTimeVariableInfo) * function->compileTimeVariableInfoLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->testInfos != ZR_NULL && function->testInfoLength > 0) {
        for (TZrUInt32 i = 0; i < function->testInfoLength; i++) {
            SZrFunctionTestInfo *info = &function->testInfos[i];
            ZR_FUNCTION_FREE_METADATA_PARAMETERS(info->parameters, info->parameterCount);
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      function->testInfos,
                                      sizeof(SZrFunctionTestInfo) * function->testInfoLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->decoratorNames != ZR_NULL && function->decoratorCount > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->decoratorNames,
                                      sizeof(SZrString *) * function->decoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->memberEntries != ZR_NULL && function->memberEntryLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->memberEntries,
                                      sizeof(SZrFunctionMemberEntry) * function->memberEntryLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->prototypeData != ZR_NULL && function->prototypeDataLength > 0) {
        ZrCore_Memory_RawFreeWithType(global, function->prototypeData, function->prototypeDataLength, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrTypeTable != ZR_NULL && function->semIrTypeTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrTypeTable,
                                      sizeof(SZrFunctionTypedTypeRef) * function->semIrTypeTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrOwnershipTable != ZR_NULL && function->semIrOwnershipTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrOwnershipTable,
                                      sizeof(SZrSemIrOwnershipEntry) * function->semIrOwnershipTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrEffectTable != ZR_NULL && function->semIrEffectTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrEffectTable,
                                      sizeof(SZrSemIrEffectEntry) * function->semIrEffectTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrBlockTable != ZR_NULL && function->semIrBlockTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrBlockTable,
                                      sizeof(SZrSemIrBlockEntry) * function->semIrBlockTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrInstructions != ZR_NULL && function->semIrInstructionLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrInstructions,
                                      sizeof(SZrSemIrInstruction) * function->semIrInstructionLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->semIrDeoptTable != ZR_NULL && function->semIrDeoptTableLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->semIrDeoptTable,
                                      sizeof(SZrSemIrDeoptEntry) * function->semIrDeoptTableLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->callSiteCaches != ZR_NULL && function->callSiteCacheLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->callSiteCaches,
                                      sizeof(SZrFunctionCallSiteCacheEntry) * function->callSiteCacheLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    // prototypeInstances 不需要手动释放，它们由GC管理（作为对象引用）

    // ZrCore_Memory_RawFreeWithType(global, function, sizeof(SZrFunction), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    // function is object, gc free it automatically.
#undef ZR_FUNCTION_FREE_METADATA_PARAMETERS
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
    const TZrSize errorHandlerGuardThreshold =
            (ZR_VM_MAX_NATIVE_CALL_STACK / ZR_NATIVE_CALL_STACK_ERROR_HANDLER_GUARD_DIVISOR) *
            ZR_NATIVE_CALL_STACK_ERROR_HANDLER_GUARD_MULTIPLIER;

    if (state->nestedNativeCalls == ZR_VM_MAX_NATIVE_CALL_STACK) {
        ZrCore_Debug_RunError(state, "C stack overflow");
    } else if (state->nestedNativeCalls >= errorHandlerGuardThreshold) {
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

TZrStackValuePointer ZrCore_Function_ReserveScratchSlots(struct SZrState *state,
                                                   TZrSize size,
                                                   TZrStackValuePointer scratchBase) {
    if (state == ZR_NULL || scratchBase == ZR_NULL || size == 0) {
        return scratchBase;
    }

    scratchBase = ZrCore_Function_CheckStackAndGc(state, size, scratchBase);
    if (scratchBase == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < size; index++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(scratchBase + index));
    }

    return scratchBase;
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
    SZrCallInfo *preservedNext = ZR_NULL;

    if (callInfo == ZR_NULL) {
        return ZR_NULL;
    }

    preservedNext = callInfo->next;
    for (SZrCallInfo *activeCallInfo = state->callInfoList; activeCallInfo != ZR_NULL; activeCallInfo = activeCallInfo->previous) {
        if (preservedNext == activeCallInfo) {
            preservedNext = ZR_NULL;
            break;
        }
    }

    memset(callInfo, 0, sizeof(*callInfo));
    callInfo->next = preservedNext;
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

static ZR_FORCE_INLINE void function_initialize_vm_frame_slots(TZrStackValuePointer functionBase,
                                                               TZrSize preservedArgumentCount,
                                                               TZrSize stackSize) {
    TZrSize slotIndex;

    if (functionBase == ZR_NULL) {
        return;
    }

    if (preservedArgumentCount > stackSize) {
        preservedArgumentCount = stackSize;
    }

    for (slotIndex = preservedArgumentCount; slotIndex < stackSize; slotIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase + 1 + slotIndex));
    }
}

static ZR_FORCE_INLINE void function_reuse_vm_frame_slots(struct SZrState *state,
                                                          TZrStackValuePointer functionBase,
                                                          TZrSize preservedArgumentCount,
                                                          TZrSize stackSize) {
    TZrSize slotIndex;

    if (state == ZR_NULL || functionBase == ZR_NULL) {
        return;
    }

    if (preservedArgumentCount > stackSize) {
        preservedArgumentCount = stackSize;
    }

    for (slotIndex = preservedArgumentCount; slotIndex < stackSize; slotIndex++) {
        ZrCore_Ownership_ReleaseValue(state, ZrCore_Stack_GetValue(functionBase + 1 + slotIndex));
    }
}


static ZR_FORCE_INLINE TZrSize function_pre_call_native(struct SZrState *state,
                                                        TZrStackValuePointer stackPointer,
                                                        TZrSize resultCount,
                                                        FZrNativeFunction function,
                                                        TZrStackValuePointer returnDestination) {
    TZrSize returnCount = 0;
    SZrCallInfo *callInfo = ZR_NULL;
    TZrStackValuePointer previousTop = ZR_NULL;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor callInfoTopAnchor;
    SZrFunctionStackAnchor callInfoReturnAnchor;
    TZrBool hasReturnDestinationAnchor = ZR_FALSE;

    function_restore_call_pointers_after_stack_check(
            state,
            ZR_STACK_NATIVE_CALL_RESERVED_MIN,
            state->stackTop.valuePointer,
            &stackPointer,
            &returnDestination);
    previousTop = state != ZR_NULL ? state->stackTop.valuePointer : ZR_NULL;
    callInfo = function_pre_call_native_call_info(state, stackPointer, resultCount, ZR_CALL_STATUS_NATIVE_CALL,
                                               state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_RESERVED_MIN);
    callInfo->returnDestination = returnDestination;
    callInfo->hasReturnDestination = returnDestination != ZR_NULL;
    callInfo->previous = state->callInfoList;
    if (previousTop != ZR_NULL) {
        for (TZrStackValuePointer slot = previousTop; slot < callInfo->functionTop.valuePointer; slot++) {
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slot));
        }
    }
    state->callInfoList = callInfo;
    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
    ZrCore_Function_StackAnchorInit(state, callInfo->functionBase.valuePointer, &callInfoBaseAnchor);
    ZrCore_Function_StackAnchorInit(state, callInfo->functionTop.valuePointer, &callInfoTopAnchor);
    if (callInfo->hasReturnDestination && callInfo->returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, callInfo->returnDestination, &callInfoReturnAnchor);
        hasReturnDestinationAnchor = ZR_TRUE;
    }
    if (ZR_UNLIKELY(state->debugHookSignal & ZR_DEBUG_HOOK_MASK_CALL)) {
        TZrInt32 argumentsCount = ZR_CAST_INT(state->stackTop.valuePointer - stackPointer);
        ZrCore_Debug_Hook(state,
                          ZR_DEBUG_HOOK_EVENT_CALL,
                          ZR_RUNTIME_DEBUG_HOOK_LINE_NONE,
                          1,
                          (TZrUInt32)argumentsCount);
    }
    ZR_THREAD_UNLOCK(state);
    returnCount = function(state);
    ZR_THREAD_LOCK(state);
    callInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
    callInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
    if (hasReturnDestinationAnchor) {
        callInfo->returnDestination = ZrCore_Function_StackAnchorRestore(state, &callInfoReturnAnchor);
    }
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

TZrBool ZrCore_Function_TryReuseTailVmCall(struct SZrState *state,
                                     struct SZrCallInfo *callInfo,
                                     TZrStackValuePointer stackPointer) {
    SZrTypeValue *value;
    SZrFunction *function;
    TZrStackValuePointer reuseBase;
    TZrSize callValueCount;
    TZrSize argumentsCount;
    TZrSize parametersCount;
    TZrSize stackSize;
    TZrStackValuePointer effectiveReturnDestination;

    if (state == ZR_NULL || callInfo == ZR_NULL || stackPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    value = ZrCore_Stack_GetValue(stackPointer);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_FUNCTION: {
            SZrClosure *closure;

            if (value->isNative) {
                return ZR_FALSE;
            }

            function = ZR_CAST_FUNCTION(state, value->value.object);
            if (function == ZR_NULL) {
                return ZR_FALSE;
            }

            closure = ZrCore_Closure_New(state, 0);
            if (closure == ZR_NULL) {
                return ZR_FALSE;
            }

            closure->function = function;
            ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
            value->type = ZR_VALUE_TYPE_CLOSURE;
            value->isGarbageCollectable = ZR_TRUE;
            value->isNative = ZR_FALSE;
        } break;
        case ZR_VALUE_TYPE_CLOSURE: {
            SZrClosure *closure;

            if (value->isNative) {
                return ZR_FALSE;
            }

            closure = ZR_CAST_VM_CLOSURE(state, value->value.object);
            if (closure == ZR_NULL || closure->function == ZR_NULL) {
                return ZR_FALSE;
            }

            function = closure->function;
        } break;
        default:
            return ZR_FALSE;
    }

    reuseBase = callInfo->functionBase.valuePointer;
    callValueCount = ZR_CAST_INT64(state->stackTop.valuePointer - stackPointer);
    argumentsCount = callValueCount - 1;
    parametersCount = function->parameterCount;
    stackSize = function->stackSize;
    effectiveReturnDestination = callInfo->hasReturnDestination
                                         ? callInfo->returnDestination
                                         : callInfo->functionBase.valuePointer;

    if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    ZrCore_Closure_CloseStackValue(state, callInfo->functionBase.valuePointer + 1);

    if (stackPointer != reuseBase) {
        for (TZrSize index = 0; index < callValueCount; index++) {
            ZrCore_Stack_CopyValue(state, reuseBase + index, ZrCore_Stack_GetValue(stackPointer + index));
        }
        stackPointer = reuseBase;
    }
    state->stackTop.valuePointer = stackPointer + callValueCount;

    function_restore_call_pointers_after_stack_check(
            state,
            stackSize + 1,
            stackPointer,
            &stackPointer,
            &effectiveReturnDestination);

    callInfo->functionBase.valuePointer = stackPointer;
    callInfo->functionTop.valuePointer = stackPointer + 1 + stackSize;
    callInfo->context.context.programCounter = function->instructionsList;
    callInfo->context.context.variableArgumentCount = 0;
    callInfo->returnDestination = effectiveReturnDestination;
    callInfo->returnDestinationReusableOffset = 0;
    callInfo->hasReturnDestination = ZR_TRUE;
    state->callInfoList = callInfo;
    function_reuse_vm_frame_slots(state, stackPointer, argumentsCount, stackSize);
    state->stackTop.valuePointer = stackPointer + 1 + parametersCount;

    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
    return ZR_TRUE;
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
                    function_pre_call_native(state,
                                             stackPointer,
                                             resultCount,
                                             native->nativeFunction,
                                             returnDestination);
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
                    function_restore_call_pointers_after_stack_check(state, stackSize + 1, stackPointer, &stackPointer,
                                                                     &returnDestination);
                    callInfo = function_pre_call_native_call_info(state, stackPointer, resultCount, ZR_CALL_STATUS_NONE,
                                                               stackPointer + 1 + stackSize);
                    callInfo->returnDestination = returnDestination;
                    callInfo->hasReturnDestination = returnDestination != ZR_NULL;
                    callInfo->previous = state->callInfoList;
                    state->callInfoList = callInfo;
                    callInfo->context.context.programCounter = function->instructionsList;
                    callInfo->context.context.trap =
                            (state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0 ? state->debugHookSignal : 0;
                    function_initialize_vm_frame_slots(stackPointer, argumentsCount, stackSize);
                    state->stackTop.valuePointer = stackPointer + 1 + parametersCount;
                    if (ZR_UNLIKELY(state->debugHookSignal & ZR_DEBUG_HOOK_MASK_CALL)) {
                        ZrCore_Debug_Hook(state,
                                          ZR_DEBUG_HOOK_EVENT_CALL,
                                          ZR_RUNTIME_DEBUG_HOOK_LINE_NONE,
                                          1,
                                          (TZrUInt32)argumentsCount);
                    }
                    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
                    return callInfo;
                }
            } break;
            case ZR_VALUE_TYPE_CLOSURE: {
                // 闭包类型：与 VM 函数类似，但需要通过 ZR_CAST_VM_CLOSURE 转换
                if (isNative) {
                    // Native callables are also backed by SZrClosureNative. Native
                    // bindings and some runtime helpers currently surface them as
                    // CLOSURE + isNative, so this is a valid call path.
                    SZrClosureNative *native = ZR_CAST_NATIVE_CLOSURE(state, value->value.object);
                    function_pre_call_native(state,
                                             stackPointer,
                                             resultCount,
                                             native->nativeFunction,
                                             returnDestination);
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
                    function_restore_call_pointers_after_stack_check(state, stackSize + 1, stackPointer, &stackPointer,
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
                    callInfo->context.context.trap =
                            (state->debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0 ? state->debugHookSignal : 0;
                    function_initialize_vm_frame_slots(stackPointer, argumentsCount, stackSize);
                    state->stackTop.valuePointer = stackPointer + 1 + parametersCount;
                    if (ZR_UNLIKELY(state->debugHookSignal & ZR_DEBUG_HOOK_MASK_CALL)) {
                        ZrCore_Debug_Hook(state,
                                          ZR_DEBUG_HOOK_EVENT_CALL,
                                          ZR_RUNTIME_DEBUG_HOOK_LINE_NONE,
                                          1,
                                          (TZrUInt32)argumentsCount);
                    }
                    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
                    return callInfo;
                }
            } break;
            case ZR_VALUE_TYPE_NATIVE_POINTER: {
                FZrNativeFunction native = ZR_CAST_FUNCTION_POINTER(value);
                function_pre_call_native(state, stackPointer, resultCount, native, returnDestination);
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
