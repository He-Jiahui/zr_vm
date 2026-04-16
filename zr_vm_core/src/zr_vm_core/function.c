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
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#include "zr_vm_core/call_info.h"

/*
 * VM call setup/teardown stays on the main interpreter path. Keep the public
 * APIs intact while switching this file to raw stack access and no-profile
 * copy/reset helpers.
 */
static ZR_FORCE_INLINE void function_stack_copy_value_no_profile(struct SZrState *state,
                                                                 SZrTypeValueOnStack *destination,
                                                                 const SZrTypeValue *source) {
    SZrTypeValue *destinationValue = ZrCore_Stack_GetValueNoProfile(destination);

    if (ZR_LIKELY(ZrCore_Value_TryCopyFastNoProfile(state, destinationValue, source))) {
        return;
    }

    ZrCore_Value_CopySlow(state, destinationValue, source);
}

#define ZrCore_Stack_GetValue ZrCore_Stack_GetValueNoProfile
#define ZrCore_Value_ResetAsNull ZrCore_Value_ResetAsNullNoProfile
#define ZrCore_Value_Copy ZrCore_Value_CopyNoProfile
#define ZrCore_Stack_CopyValue function_stack_copy_value_no_profile

static ZR_FORCE_INLINE TZrBool function_prepare_vm_callable_value(SZrState *state,
                                                                  TZrStackValuePointer stackPointer,
                                                                  SZrTypeValue **ioValue,
                                                                  SZrFunction **outFunction);

static ZR_FORCE_INLINE void function_write_vm_closure_value(SZrState *state,
                                                            SZrTypeValue *value,
                                                            SZrClosure *closure);

static ZR_FORCE_INLINE SZrFunction *function_try_get_vm_closure_function(SZrState *state,
                                                                         const SZrTypeValue *value);

static ZR_FORCE_INLINE SZrFunction *function_try_prepare_cached_stateless_function_value(
        SZrState *state,
        SZrTypeValue *value);

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_known_or_generic(SZrState *state,
                                                                       TZrStackValuePointer stackPointer,
                                                                       const SZrTypeValue *knownCallable,
                                                                       TZrSize resultCount);

static const SZrTypeValueOnStack CZrFunctionNullStackSlotTemplate = {
        .value =
                {
                        .type = ZR_VALUE_TYPE_NULL,
                        .value.nativeObject.nativeUInt64 = 0,
                        .isGarbageCollectable = ZR_FALSE,
                        .isNative = ZR_TRUE,
                        .ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE,
                        .ownershipControl = ZR_NULL,
                        .ownershipWeakRef = ZR_NULL,
                },
        .toBeClosedValueOffset = 0u,
};

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_vm(struct SZrState *state,
                                                         TZrStackValuePointer stackPointer,
                                                         TZrSize resultCount,
                                                         TZrStackValuePointer returnDestination,
                                                         SZrFunction *function);

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_resolved_vm(struct SZrState *state,
                                                                  TZrStackValuePointer stackPointer,
                                                                  SZrFunction *function,
                                                                  TZrSize argumentsCount,
                                                                  TZrSize resultCount,
                                                                  TZrStackValuePointer returnDestination);

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
    function->childFunctionGraphIsBorrowed = ZR_FALSE;
    function->ownerFunction = ZR_NULL;
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
    function->vmEntryClearStackSizePlusOne = 0;
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
    function->staticImports = ZR_NULL;
    function->staticImportLength = 0;
    function->moduleEntryEffects = ZR_NULL;
    function->moduleEntryEffectLength = 0;
    function->exportedCallableSummaries = ZR_NULL;
    function->exportedCallableSummaryLength = 0;
    function->topLevelCallableBindings = ZR_NULL;
    function->topLevelCallableBindingLength = 0;
    function->parameterMetadata = ZR_NULL;
    function->parameterMetadataCount = 0;
    function->hasCallableReturnType = ZR_FALSE;
    ZrCore_Memory_RawSet(&function->callableReturnType, 0, sizeof(function->callableReturnType));
    function->callableReturnType.baseType = ZR_VALUE_TYPE_OBJECT;
    function->callableReturnType.elementBaseType = ZR_VALUE_TYPE_OBJECT;
    function->compileTimeVariableInfos = ZR_NULL;
    function->compileTimeVariableInfoLength = 0;
    function->compileTimeFunctionInfos = ZR_NULL;
    function->compileTimeFunctionInfoLength = 0;
    function->escapeBindings = ZR_NULL;
    function->escapeBindingLength = 0;
    function->returnEscapeSlots = ZR_NULL;
    function->returnEscapeSlotCount = 0;
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
    function->cachedStatelessClosure = ZR_NULL;
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
                    ZrCore_Log_Diagnosticf(ZR_NULL,
                                           ZR_LOG_LEVEL_ERROR,
                                           ZR_OUTPUT_CHANNEL_STDERR,
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
                ZrCore_Log_Diagnosticf(ZR_NULL,
                                       ZR_LOG_LEVEL_ERROR,
                                       ZR_OUTPUT_CHANNEL_STDERR,
                                       "[closure-graph] function=%s ptr=%p instr=%u const=%u target=%p targetName=%s params=%u instrs=%u lines=%u-%u childCount=%u\n",
                                       function->functionName != ZR_NULL
                                               ? ZrCore_String_GetNativeString(function->functionName)
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
            case ZR_INSTRUCTION_ENUM(OWN_BORROW):
            case ZR_INSTRUCTION_ENUM(OWN_LOAN):
            case ZR_INSTRUCTION_ENUM(OWN_SHARE):
            case ZR_INSTRUCTION_ENUM(OWN_WEAK):
            case ZR_INSTRUCTION_ENUM(OWN_DETACH):
            case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
            case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
            case ZR_INSTRUCTION_ENUM(TYPEOF):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
            case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            case ZR_INSTRUCTION_ENUM(META_GET):
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
            case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
            case ZR_INSTRUCTION_ENUM(META_SET):
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
            case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
                function_note_generated_frame_slot(operandB1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(ADD):
            case ZR_INSTRUCTION_ENUM(ADD_INT):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
            case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
            case ZR_INSTRUCTION_ENUM(SUB):
            case ZR_INSTRUCTION_ENUM(SUB_INT):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
            case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
            case ZR_INSTRUCTION_ENUM(MUL):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
            case ZR_INSTRUCTION_ENUM(DIV):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
            case ZR_INSTRUCTION_ENUM(MOD):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
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
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
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
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(META_CALL):
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_call_span(operandA1, operandB1, &slotCount);
                break;

            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
            case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
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
            case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
                function_note_generated_frame_slot(destinationSlot, &slotCount);
                function_note_generated_frame_slot(operandA1, &slotCount);
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

static TZrBool function_has_return_escape_slot(const SZrFunction *function, TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->returnEscapeSlots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->returnEscapeSlotCount; index++) {
        if (function->returnEscapeSlots[index] == stackSlot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const SZrFunctionLocalVariable *function_find_local_escape_variable(const SZrFunction *function,
                                                                           TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->localVariableList == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[index];
        if (local->stackSlot == stackSlot &&
            (local->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u) {
            return local;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionEscapeBinding *function_find_return_escape_binding(const SZrFunction *function,
                                                                           TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->escapeBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->escapeBindingLength; index++) {
        const SZrFunctionEscapeBinding *binding = &function->escapeBindings[index];
        EZrFunctionEscapeBindingKind bindingKind = (EZrFunctionEscapeBindingKind)binding->bindingKind;

        if (binding->slotOrIndex != stackSlot) {
            continue;
        }

        if ((bindingKind == ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL ||
             bindingKind == ZR_FUNCTION_ESCAPE_BINDING_KIND_RETURN_SLOT) &&
            (binding->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN) != 0u) {
            return binding;
        }
    }

    return ZR_NULL;
}

TZrBool ZrCore_Function_ApplyReturnEscape(struct SZrState *state,
                                          const SZrFunction *function,
                                          TZrUInt32 stackSlot,
                                          const SZrTypeValue *value) {
    TZrUInt32 escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN;
    TZrUInt32 scopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
    const SZrFunctionLocalVariable *localVariable;
    const SZrFunctionEscapeBinding *binding;

    if (state == ZR_NULL || function == ZR_NULL || value == ZR_NULL || !function_has_return_escape_slot(function, stackSlot)) {
        return ZR_FALSE;
    }

    localVariable = function_find_local_escape_variable(function, stackSlot);
    if (localVariable != ZR_NULL) {
        escapeFlags |= localVariable->escapeFlags;
        scopeDepth = localVariable->scopeDepth;
    } else {
        binding = function_find_return_escape_binding(function, stackSlot);
        if (binding != ZR_NULL) {
            escapeFlags |= binding->escapeFlags;
            scopeDepth = binding->scopeDepth;
        }
    }

    ZrCore_GarbageCollector_MarkValueEscaped(state,
                                             value,
                                             escapeFlags,
                                             scopeDepth,
                                             ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE);
    return ZR_TRUE;
}

void ZrCore_Function_RebindConstantFunctionValuesToChildren(SZrFunction *function) {
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL || function->childFunctionLength == 0) {
        return;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        SZrFunction *childFunction = &function->childFunctionList[childIndex];

        childFunction->ownerFunction = function;
        ZrCore_Function_RebindConstantFunctionValuesToChildren(childFunction);
    }

    if (function->constantValueList == ZR_NULL || function->constantValueLength == 0) {
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

void ZrCore_Function_ClearChildOwnerLinks(SZrFunction *function) {
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL || function->childFunctionLength == 0) {
        return;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        SZrFunction *childFunction = &function->childFunctionList[childIndex];

        childFunction->ownerFunction = ZR_NULL;
        ZrCore_Function_ClearChildOwnerLinks(childFunction);
    }
}

TZrBool ZrCore_Function_ValidateCreateClosureTargetsInChildGraph(const SZrFunction *function) {
    return function_validate_create_closure_targets_in_child_graph_recursive(function);
}

static void function_reset_to_tombstone(SZrFunction *function) {
    if (function == ZR_NULL) {
        return;
    }

    function->instructionsList = ZR_NULL;
    function->instructionsLength = 0;
    function->childFunctionList = ZR_NULL;
    function->childFunctionLength = 0;
    function->childFunctionGraphIsBorrowed = ZR_FALSE;
    function->ownerFunction = ZR_NULL;
    function->constantValueList = ZR_NULL;
    function->constantValueLength = 0;
    function->localVariableList = ZR_NULL;
    function->localVariableLength = 0;
    function->closureValueList = ZR_NULL;
    function->closureValueLength = 0;
    function->executionLocationInfoList = ZR_NULL;
    function->executionLocationInfoLength = 0;
    function->lineInSourceList = ZR_NULL;
    function->catchClauseList = ZR_NULL;
    function->catchClauseCount = 0;
    function->exceptionHandlerList = ZR_NULL;
    function->exceptionHandlerCount = 0;
    function->exportedVariables = ZR_NULL;
    function->exportedVariableLength = 0;
    function->typedExportedSymbols = ZR_NULL;
    function->typedExportedSymbolLength = 0;
    function->typedLocalBindings = ZR_NULL;
    function->typedLocalBindingLength = 0;
    function->staticImports = ZR_NULL;
    function->staticImportLength = 0;
    function->moduleEntryEffects = ZR_NULL;
    function->moduleEntryEffectLength = 0;
    function->exportedCallableSummaries = ZR_NULL;
    function->exportedCallableSummaryLength = 0;
    function->topLevelCallableBindings = ZR_NULL;
    function->topLevelCallableBindingLength = 0;
    function->parameterMetadata = ZR_NULL;
    function->parameterMetadataCount = 0;
    function->hasCallableReturnType = ZR_FALSE;
    ZrCore_Memory_RawSet(&function->callableReturnType, 0, sizeof(function->callableReturnType));
    function->callableReturnType.baseType = ZR_VALUE_TYPE_OBJECT;
    function->callableReturnType.elementBaseType = ZR_VALUE_TYPE_OBJECT;
    function->compileTimeFunctionInfos = ZR_NULL;
    function->compileTimeFunctionInfoLength = 0;
    function->compileTimeVariableInfos = ZR_NULL;
    function->compileTimeVariableInfoLength = 0;
    function->escapeBindings = ZR_NULL;
    function->escapeBindingLength = 0;
    function->returnEscapeSlots = ZR_NULL;
    function->returnEscapeSlotCount = 0;
    function->testInfos = ZR_NULL;
    function->testInfoLength = 0;
    function->decoratorNames = ZR_NULL;
    function->decoratorCount = 0;
    function->memberEntries = ZR_NULL;
    function->memberEntryLength = 0;
    function->prototypeData = ZR_NULL;
    function->prototypeDataLength = 0;
    function->prototypeCount = 0;
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
    function->prototypeInstances = ZR_NULL;
    function->prototypeInstancesLength = 0;
    function->functionName = ZR_NULL;
    function->sourceCodeList = ZR_NULL;
    function->sourceHash = ZR_NULL;
    function->runtimeDecoratorMetadata = ZR_NULL;
    function->runtimeDecoratorDecorators = ZR_NULL;
    function->hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&function->decoratorMetadataValue);
    function->parameterCount = 0;
    function->hasVariableArguments = ZR_FALSE;
    function->stackSize = 0;
    function->vmEntryClearStackSizePlusOne = 0;
    function->lineInSourceStart = 0;
    function->lineInSourceEnd = 0;
    function->cachedStatelessClosure = ZR_NULL;
}

void ZrCore_Function_DetachOwnedBuffers(SZrFunction *function) {
    function_reset_to_tombstone(function);
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

    if (function->cachedStatelessClosure != ZR_NULL) {
        function->cachedStatelessClosure->function = ZR_NULL;
        function->cachedStatelessClosure = ZR_NULL;
    }
    if (!function->childFunctionGraphIsBorrowed &&
        function->childFunctionList != ZR_NULL &&
        function->childFunctionLength > 0) {
        for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            ZrCore_Function_Free(state, &function->childFunctionList[childIndex]);
        }
    }
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
    if (function->staticImports != ZR_NULL && function->staticImportLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->staticImports,
                                      sizeof(SZrString *) * function->staticImportLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->moduleEntryEffects != ZR_NULL && function->moduleEntryEffectLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->moduleEntryEffects,
                                      sizeof(SZrFunctionModuleEffect) * function->moduleEntryEffectLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->exportedCallableSummaries != ZR_NULL && function->exportedCallableSummaryLength > 0) {
        for (TZrUInt32 i = 0; i < function->exportedCallableSummaryLength; i++) {
            SZrFunctionCallableSummary *summary = &function->exportedCallableSummaries[i];
            if (summary->effects != ZR_NULL && summary->effectCount > 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              summary->effects,
                                              sizeof(SZrFunctionModuleEffect) * summary->effectCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      function->exportedCallableSummaries,
                                      sizeof(SZrFunctionCallableSummary) * function->exportedCallableSummaryLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->topLevelCallableBindings != ZR_NULL && function->topLevelCallableBindingLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->topLevelCallableBindings,
                                      sizeof(SZrFunctionTopLevelCallableBinding) * function->topLevelCallableBindingLength,
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
    if (function->escapeBindings != ZR_NULL && function->escapeBindingLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->escapeBindings,
                                      sizeof(SZrFunctionEscapeBinding) * function->escapeBindingLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->returnEscapeSlots != ZR_NULL && function->returnEscapeSlotCount > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->returnEscapeSlots,
                                      sizeof(TZrUInt32) * function->returnEscapeSlotCount,
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
    // prototypeInstances 不需要手动释放，它们由GC管理（作为对象引用）。
    // Reset the function to a GC-safe tombstone because the raw object itself
    // stays on the GC list until the collector later reclaims it.
    function_reset_to_tombstone(function);

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

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_known_or_generic(SZrState *state,
                                                                       TZrStackValuePointer stackPointer,
                                                                       const SZrTypeValue *knownCallable,
                                                                       TZrSize resultCount) {
    SZrTypeValue *stackCallable = ZrCore_Stack_GetValue(stackPointer);

    if (knownCallable != ZR_NULL) {
        if (knownCallable->isNative) {
            switch (knownCallable->type) {
                case ZR_VALUE_TYPE_FUNCTION:
                case ZR_VALUE_TYPE_CLOSURE:
                case ZR_VALUE_TYPE_NATIVE_POINTER:
                    return ZrCore_Function_PreCallKnownNativeValue(
                            state,
                            stackPointer,
                            ZR_CAST(SZrTypeValue *, knownCallable),
                            resultCount,
                            ZR_NULL);
                default:
                    break;
            }
        } else {
            switch (knownCallable->type) {
                case ZR_VALUE_TYPE_FUNCTION:
                case ZR_VALUE_TYPE_CLOSURE:
                    return ZrCore_Function_PreCallKnownVmValue(
                            state,
                            stackPointer,
                            stackCallable != ZR_NULL ? stackCallable : ZR_CAST(SZrTypeValue *, knownCallable),
                            resultCount,
                            ZR_NULL);
                default:
                    break;
            }
        }
    }

    return ZrCore_Function_PreCall(state, stackPointer, resultCount, ZR_NULL);
}

static ZR_FORCE_INLINE void function_call_internal_known(struct SZrState *state,
                                                         TZrStackValuePointer stackPointer,
                                                         const SZrTypeValue *knownCallable,
                                                         TZrSize resultCount,
                                                         TZrUInt32 callIncremental,
                                                         TZrBool isYield) {
    state->nestedNativeCalls += callIncremental;
    state->nestedNativeCallYieldFlag += isYield ? 1 : 0;
    if (ZR_UNLIKELY(state->nestedNativeCalls > ZR_VM_MAX_NATIVE_CALL_STACK)) {
        ZrCore_Function_CheckStack(state, 0, stackPointer);
        ZrCore_Function_CheckNativeStack(state);
    }

    {
        SZrCallInfo *callInfo = function_pre_call_known_or_generic(state, stackPointer, knownCallable, resultCount);
        if (callInfo != ZR_NULL) {
            callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
            ZrCore_Execute(state, callInfo);
        }
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
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(anchor != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    anchor->offset = ZrCore_Stack_SavePointerAsOffset(state, stackPointer);
}

TZrStackValuePointer ZrCore_Function_StackAnchorRestore(struct SZrState *state,
                                                  const SZrFunctionStackAnchor *anchor) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(anchor != ZR_NULL);
    return ZrCore_Stack_LoadOffsetToPointer(state, anchor->offset);
}

TZrStackValuePointer ZrCore_Function_CheckStackAndAnchor(struct SZrState *state,
                                                   TZrSize size,
                                                   TZrStackValuePointer checkPointer,
                                                   TZrStackValuePointer stackPointer,
                                                   SZrFunctionStackAnchor *anchor) {
    TZrStackValuePointer effectiveCheckPointer;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(anchor != ZR_NULL);
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

TZrStackValuePointer ZrCore_Function_CallWithoutYieldKnownValueAndRestore(struct SZrState *state,
                                                                          TZrStackValuePointer stackPointer,
                                                                          const SZrTypeValue *callableValue,
                                                                          TZrSize resultCount) {
    SZrFunctionStackAnchor anchor;

    if (state == ZR_NULL || stackPointer == ZR_NULL) {
        return stackPointer;
    }

    ZrCore_Function_StackAnchorInit(state, stackPointer, &anchor);
    return ZrCore_Function_CallWithoutYieldKnownValueAndRestoreAnchor(state, &anchor, callableValue, resultCount);
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

TZrStackValuePointer ZrCore_Function_CallWithoutYieldKnownValueAndRestoreAnchor(struct SZrState *state,
                                                                                const SZrFunctionStackAnchor *anchor,
                                                                                const SZrTypeValue *callableValue,
                                                                                TZrSize resultCount) {
    TZrStackValuePointer stackPointer;

    if (state == ZR_NULL || anchor == ZR_NULL) {
        return ZR_NULL;
    }

    stackPointer = ZrCore_Function_StackAnchorRestore(state, anchor);
    if (stackPointer == ZR_NULL) {
        return ZR_NULL;
    }

    function_call_internal_known(state, stackPointer, callableValue, resultCount, 1, ZR_TRUE);
    return ZrCore_Function_StackAnchorRestore(state, anchor);
}

static ZR_FORCE_INLINE SZrCallInfo *function_acquire_call_info(struct SZrState *state) {
    SZrCallInfo *callInfo;

    ZR_ASSERT(state != ZR_NULL);

    callInfo = state->callInfoList->next;
    if (ZR_LIKELY(callInfo != ZR_NULL)) {
        return callInfo;
    }

    return ZrCore_CallInfo_Extend(state);
}

static ZR_FORCE_INLINE TZrDebugSignal function_debug_trap_from_hook_signal(TZrUInt32 debugHookSignal) {
    return (TZrDebugSignal)(((debugHookSignal & ZR_DEBUG_HOOK_MASK_LINE) != 0) ? debugHookSignal
                                                                                : ZR_DEBUG_SIGNAL_NONE);
}

static ZR_FORCE_INLINE void function_init_call_info_common(SZrCallInfo *callInfo,
                                                           SZrCallInfo *previous,
                                                           TZrStackValuePointer basePointer,
                                                           TZrStackValuePointer topPointer,
                                                           TZrSize resultCount) {
    ZR_ASSERT(callInfo != ZR_NULL);

    callInfo->functionBase.valuePointer = basePointer;
    callInfo->functionTop.valuePointer = topPointer;
    callInfo->previous = previous;
    callInfo->expectedReturnCount = resultCount;
}

static ZR_FORCE_INLINE void function_reinitialize_native_call_info_runtime_state(SZrCallInfo *callInfo,
                                                                                 TZrStackValuePointer returnDestination) {
    ZR_ASSERT(callInfo != ZR_NULL);

    callInfo->callStatus = ZR_CALL_STATUS_NATIVE_CALL;
    callInfo->context = (TZrCallInfoContext) {0};
    callInfo->yieldContext = (TZrCallInfoYieldContext) {0};
    callInfo->returnDestination = returnDestination;
    callInfo->returnDestinationReusableOffset = 0;
    callInfo->hasReturnDestination = returnDestination != ZR_NULL;
}

static ZR_FORCE_INLINE void function_reinitialize_vm_call_info_runtime_state(SZrCallInfo *callInfo,
                                                                             EZrCallStatus callStatus,
                                                                             TZrStackValuePointer returnDestination,
                                                                             const TZrInstruction *programCounter,
                                                                             TZrDebugSignal trap) {
    ZR_ASSERT(callInfo != ZR_NULL);

    callInfo->callStatus = callStatus;
    callInfo->context = (TZrCallInfoContext) {0};
    callInfo->context.context.programCounter = programCounter;
    callInfo->context.context.trap = trap;
    callInfo->yieldContext = (TZrCallInfoYieldContext) {0};
    callInfo->returnDestination = returnDestination;
    callInfo->returnDestinationReusableOffset = 0;
    callInfo->hasReturnDestination = returnDestination != ZR_NULL;
}

static ZR_FORCE_INLINE void function_init_native_call_info(SZrCallInfo *callInfo,
                                                           SZrCallInfo *previous,
                                                           TZrStackValuePointer basePointer,
                                                           TZrStackValuePointer topPointer,
                                                           TZrSize resultCount,
                                                           TZrStackValuePointer returnDestination) {
    function_init_call_info_common(callInfo,
                                   previous,
                                   basePointer,
                                   topPointer,
                                   resultCount);
    function_reinitialize_native_call_info_runtime_state(callInfo, returnDestination);
}

static ZR_FORCE_INLINE void function_init_vm_call_info(SZrCallInfo *callInfo,
                                                       SZrCallInfo *previous,
                                                       TZrStackValuePointer basePointer,
                                                       TZrStackValuePointer topPointer,
                                                       TZrSize resultCount,
                                                       TZrStackValuePointer returnDestination,
                                                       const TZrInstruction *programCounter,
                                                       TZrDebugSignal trap) {
    function_init_call_info_common(callInfo,
                                   previous,
                                   basePointer,
                                   topPointer,
                                   resultCount);
    function_reinitialize_vm_call_info_runtime_state(callInfo,
                                                     ZR_CALL_STATUS_NONE,
                                                     returnDestination,
                                                     programCounter,
                                                     trap);
}

static ZR_FORCE_INLINE void function_reset_stack_slots_to_null(TZrStackValuePointer start,
                                                               TZrStackValuePointer end) {
    ZR_ASSERT(start != ZR_NULL);
    ZR_ASSERT(end != ZR_NULL);
    ZR_ASSERT(end >= start);

    for (; start < end; start++) {
        *start = CZrFunctionNullStackSlotTemplate;
    }
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

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(checkPointer != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(*stackPointer != ZR_NULL);

    if (ZR_LIKELY(state->stackTail.valuePointer - checkPointer >= (TZrMemoryOffset) size)) {
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

static ZR_FORCE_INLINE TZrSize function_resolve_vm_entry_clear_stack_size(const SZrFunction *function) {
    TZrSize clearStackSize;
    SZrFunction *mutableFunction;

    ZR_ASSERT(function != ZR_NULL);

    if (function->vmEntryClearStackSizePlusOne != 0u) {
        return (TZrSize)(function->vmEntryClearStackSizePlusOne - 1u);
    }

    clearStackSize = function->parameterCount;
    if (function->localVariableList != ZR_NULL && function->localVariableLength > 0) {
        for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
            const SZrFunctionLocalVariable *localVariable = &function->localVariableList[index];
            TZrSize localNextSlot;

            if (localVariable->offsetActivate != 0) {
                continue;
            }

            localNextSlot = (TZrSize)localVariable->stackSlot + 1;
            if (localNextSlot > clearStackSize) {
                clearStackSize = localNextSlot;
            }
        }
    }

    if (clearStackSize > function->stackSize) {
        clearStackSize = function->stackSize;
    }

    mutableFunction = ZR_CAST(SZrFunction *, function);
    ZR_ASSERT(clearStackSize < UINT32_MAX);
    mutableFunction->vmEntryClearStackSizePlusOne = (TZrUInt32)clearStackSize + 1u;
    return clearStackSize;
}

static ZR_FORCE_INLINE void function_initialize_vm_frame_slots(TZrStackValuePointer functionBase,
                                                               TZrSize preservedArgumentCount,
                                                               TZrSize stackSize) {
    ZR_ASSERT(functionBase != ZR_NULL);

    if (preservedArgumentCount > stackSize) {
        preservedArgumentCount = stackSize;
    }

    function_reset_stack_slots_to_null(functionBase + 1 + preservedArgumentCount, functionBase + 1 + stackSize);
}

static ZR_FORCE_INLINE void function_reuse_vm_frame_slots(struct SZrState *state,
                                                          TZrStackValuePointer functionBase,
                                                          TZrSize preservedArgumentCount,
                                                          TZrSize previousStackSize,
                                                          TZrSize nextStackSize) {
    TZrSize slotIndex;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(functionBase != ZR_NULL);

    if (preservedArgumentCount > previousStackSize) {
        preservedArgumentCount = previousStackSize;
    }

    for (slotIndex = preservedArgumentCount; slotIndex < previousStackSize; slotIndex++) {
        ZrCore_Ownership_ReleaseValue(state, ZrCore_Stack_GetValue(functionBase + 1 + slotIndex));
    }

    if (nextStackSize <= previousStackSize) {
        return;
    }

    function_reset_stack_slots_to_null(functionBase + 1 + previousStackSize, functionBase + 1 + nextStackSize);
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
    TZrUInt32 debugHookSignal;

    function_restore_call_pointers_after_stack_check(
            state,
            ZR_STACK_NATIVE_CALL_RESERVED_MIN,
            state->stackTop.valuePointer,
            &stackPointer,
            &returnDestination);
    previousTop = state != ZR_NULL ? state->stackTop.valuePointer : ZR_NULL;
    debugHookSignal = state->debugHookSignal;
    callInfo = function_acquire_call_info(state);
    if (ZR_UNLIKELY(callInfo == ZR_NULL)) {
        return 0;
    }
    function_init_native_call_info(callInfo,
                                   state->callInfoList,
                                   stackPointer,
                                   state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_RESERVED_MIN,
                                   resultCount,
                                   returnDestination);
    if (previousTop != ZR_NULL) {
        function_reset_stack_slots_to_null(previousTop, callInfo->functionTop.valuePointer);
    }
    state->callInfoList = callInfo;
    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
    ZrCore_Function_StackAnchorInit(state, callInfo->functionBase.valuePointer, &callInfoBaseAnchor);
    ZrCore_Function_StackAnchorInit(state, callInfo->functionTop.valuePointer, &callInfoTopAnchor);
    if (callInfo->hasReturnDestination && callInfo->returnDestination != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, callInfo->returnDestination, &callInfoReturnAnchor);
        hasReturnDestinationAnchor = ZR_TRUE;
    }
    if (ZR_UNLIKELY(debugHookSignal & ZR_DEBUG_HOOK_MASK_CALL)) {
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
    if (state->threadStatus == ZR_THREAD_STATUS_FINE && callInfo->functionBase.valuePointer != ZR_NULL) {
        TZrStackValuePointer returnTop = state->stackTop.valuePointer;
        if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
            state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
        }
        // Native thunks can synthesize VM-like closures that capture stack slots.
        // Close any remaining open upvalues here before PostCall recycles the frame.
        ZrCore_Closure_CloseClosure(state,
                                    callInfo->functionBase.valuePointer + 1,
                                    ZR_THREAD_STATUS_INVALID,
                                    ZR_FALSE);
        state->stackTop.valuePointer = returnTop;
    }
    ZR_STACK_CHECK_CALL_INFO_STACK_COUNT(state, returnCount);
    ZrCore_Function_PostCall(state, callInfo, returnCount);
    return returnCount;
}

static ZR_FORCE_INLINE TZrBool function_prepare_vm_callable_value(SZrState *state,
                                                                  TZrStackValuePointer stackPointer,
                                                                  SZrTypeValue **ioValue,
                                                                  SZrFunction **outFunction);

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_vm(struct SZrState *state,
                                                         TZrStackValuePointer stackPointer,
                                                         TZrSize resultCount,
                                                         TZrStackValuePointer returnDestination,
                                                         SZrFunction *function);

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
    TZrSize previousStackSize;
    TZrSize stackSize;
    TZrStackValuePointer effectiveReturnDestination;
    EZrCallStatus preservedCallStatus;
    TZrUInt32 debugHookSignal;

    if (state == ZR_NULL || callInfo == ZR_NULL || stackPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    value = ZrCore_Stack_GetValue(stackPointer);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_LIKELY(value->type == ZR_VALUE_TYPE_CLOSURE)) {
        if (value->isNative) {
            return ZR_FALSE;
        }
        function = function_try_get_vm_closure_function(state, value);
        if (function == ZR_NULL) {
            return ZR_FALSE;
        }
    } else if (value->type == ZR_VALUE_TYPE_FUNCTION && !value->isNative) {
        function = function_try_prepare_cached_stateless_function_value(state, value);
        if (function == ZR_NULL &&
            !function_prepare_vm_callable_value(state, stackPointer, &value, &function)) {
            return ZR_FALSE;
        }
    } else if (value->isNative || !function_prepare_vm_callable_value(state, stackPointer, &value, &function)) {
        return ZR_FALSE;
    }

    reuseBase = callInfo->functionBase.valuePointer;
    callValueCount = ZR_CAST_INT64(state->stackTop.valuePointer - stackPointer);
    argumentsCount = callValueCount - 1;
    parametersCount = function->parameterCount;
    previousStackSize = (TZrSize)(callInfo->functionTop.valuePointer - reuseBase - 1);
    stackSize = function->stackSize;
    effectiveReturnDestination = callInfo->hasReturnDestination
                                         ? callInfo->returnDestination
                                         : callInfo->functionBase.valuePointer;
    preservedCallStatus = callInfo->callStatus;
    debugHookSignal = state->debugHookSignal;

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
    function_reinitialize_vm_call_info_runtime_state(callInfo,
                                                     preservedCallStatus,
                                                     effectiveReturnDestination,
                                                     function->instructionsList,
                                                     function_debug_trap_from_hook_signal(debugHookSignal));
    state->callInfoList = callInfo;
    function_reuse_vm_frame_slots(state, stackPointer, argumentsCount, previousStackSize, stackSize);
    state->stackTop.valuePointer = stackPointer + 1 + parametersCount;

    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool function_materialize_function_value_as_closure(SZrState *state,
                                                                               TZrStackValuePointer stackPointer,
                                                                               SZrTypeValue **ioValue,
                                                                               SZrFunction *function) {
    SZrClosure *closure;
    SZrTypeValue *value;
    SZrFunctionStackAnchor stackPointerAnchor;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(ioValue != ZR_NULL);
    ZR_ASSERT(*ioValue != ZR_NULL);
    ZR_ASSERT(function != ZR_NULL);

    ZrCore_Function_StackAnchorInit(state, stackPointer, &stackPointerAnchor);
    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    stackPointer = ZrCore_Function_StackAnchorRestore(state, &stackPointerAnchor);
    value = ZrCore_Stack_GetValue(stackPointer);
    ZR_ASSERT(value != ZR_NULL);
    closure->function = function;
    function_write_vm_closure_value(state, value, closure);
    *ioValue = value;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE TZrBool function_materialize_zero_capture_function_value_as_cached_closure(
        SZrState *state,
        TZrStackValuePointer stackPointer,
        SZrTypeValue **ioValue,
        SZrFunction *function) {
    SZrClosure *closure;
    SZrTypeValue *value;
    SZrFunctionStackAnchor stackPointerAnchor;
    TZrBool hasStackPointerAnchor = ZR_FALSE;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(ioValue != ZR_NULL);
    ZR_ASSERT(*ioValue != ZR_NULL);
    ZR_ASSERT(function != ZR_NULL);
    ZR_ASSERT(function->closureValueLength == 0);

    closure = function->cachedStatelessClosure;
    if (closure == ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, stackPointer, &stackPointerAnchor);
        hasStackPointerAnchor = ZR_TRUE;

        closure = ZrCore_Closure_New(state, 0);
        if (closure == ZR_NULL) {
            return ZR_FALSE;
        }

        closure->function = function;
        function->cachedStatelessClosure = closure;
        ZrCore_RawObject_Barrier(state,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    }

    if (hasStackPointerAnchor) {
        stackPointer = ZrCore_Function_StackAnchorRestore(state, &stackPointerAnchor);
    }

    value = ZrCore_Stack_GetValue(stackPointer);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    function_write_vm_closure_value(state, value, closure);
    *ioValue = value;
    return ZR_TRUE;
}

static ZR_FORCE_INLINE void function_write_vm_closure_value(SZrState *state,
                                                            SZrTypeValue *value,
                                                            SZrClosure *closure) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(closure != ZR_NULL);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value->type = ZR_VALUE_TYPE_CLOSURE;
    value->isGarbageCollectable = ZR_TRUE;
    value->isNative = ZR_FALSE;
}

static ZR_FORCE_INLINE SZrFunction *function_try_get_vm_closure_function(SZrState *state,
                                                                         const SZrTypeValue *value) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(value->type == ZR_VALUE_TYPE_CLOSURE);
    ZR_ASSERT(!value->isNative);

    return ZrCore_Closure_GetMetadataFunctionFromValue(state, value);
}

static ZR_FORCE_INLINE SZrFunction *function_try_prepare_cached_stateless_function_value(
        SZrState *state,
        SZrTypeValue *value) {
    SZrFunction *function;
    SZrClosure *closure;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);
    ZR_ASSERT(value->type == ZR_VALUE_TYPE_FUNCTION);
    ZR_ASSERT(!value->isNative);

    function = ZR_CAST_FUNCTION(state, value->value.object);
    if (function == ZR_NULL || function->closureValueLength != 0) {
        return ZR_NULL;
    }

    closure = function->cachedStatelessClosure;
    if (closure == ZR_NULL || closure->function != function) {
        return ZR_NULL;
    }

    function_write_vm_closure_value(state, value, closure);
    return function;
}

static ZR_FORCE_INLINE TZrBool function_prepare_vm_callable_value(SZrState *state,
                                                                  TZrStackValuePointer stackPointer,
                                                                  SZrTypeValue **ioValue,
                                                                  SZrFunction **outFunction) {
    SZrTypeValue *value;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(ioValue != ZR_NULL);
    ZR_ASSERT(*ioValue != ZR_NULL);
    ZR_ASSERT(outFunction != ZR_NULL);

    *outFunction = ZR_NULL;
    value = *ioValue;

    switch (value->type) {
        case ZR_VALUE_TYPE_FUNCTION: {
            SZrFunction *function;

            ZR_ASSERT(!value->isNative);
            function = function_try_prepare_cached_stateless_function_value(state, value);
            if (function != ZR_NULL) {
                *ioValue = value;
                *outFunction = function;
                return ZR_TRUE;
            }

            function = ZR_CAST_FUNCTION(state, value->value.object);
            if (function == ZR_NULL) {
                return ZR_FALSE;
            }
            if (function->closureValueLength == 0) {
                if (!function_materialize_zero_capture_function_value_as_cached_closure(
                            state,
                            stackPointer,
                            ioValue,
                            function)) {
                    return ZR_FALSE;
                }
            } else if (!function_materialize_function_value_as_closure(state, stackPointer, ioValue, function)) {
                return ZR_FALSE;
            }
            *outFunction = function;
            return ZR_TRUE;
        }

        case ZR_VALUE_TYPE_CLOSURE: {
            ZR_ASSERT(!value->isNative);
            *outFunction = function_try_get_vm_closure_function(state, value);
            if (*outFunction == ZR_NULL) {
                return ZR_FALSE;
            }

            *ioValue = value;
            return ZR_TRUE;
        }

        default:
            return ZR_FALSE;
    }
}

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_dispatch(struct SZrState *state,
                                                               TZrStackValuePointer stackPointer,
                                                               SZrTypeValue *value,
                                                               TZrSize resultCount,
                                                               TZrStackValuePointer returnDestination) {
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(value != ZR_NULL);

    if (ZR_LIKELY(value->type == ZR_VALUE_TYPE_CLOSURE)) {
        SZrFunction *function;

        if (value->isNative) {
            SZrClosureNative *native = ZR_CAST_NATIVE_CLOSURE(state, value->value.object);
            function_pre_call_native(state,
                                     stackPointer,
                                     resultCount,
                                     native->nativeFunction,
                                     returnDestination);
            return ZR_NULL;
        }

        function = function_try_get_vm_closure_function(state, value);
        if (function == ZR_NULL) {
            return ZR_NULL;
        }
        return function_pre_call_vm(state, stackPointer, resultCount, returnDestination, function);
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION && !value->isNative) {
        SZrFunction *function = function_try_prepare_cached_stateless_function_value(state, value);
        if (ZR_LIKELY(function != ZR_NULL)) {
            return function_pre_call_vm(state, stackPointer, resultCount, returnDestination, function);
        }
    }

    do {
        switch (value->type) {
            case ZR_VALUE_TYPE_FUNCTION:
            case ZR_VALUE_TYPE_CLOSURE: {
                if (value->isNative) {
                    SZrClosureNative *native = ZR_CAST_NATIVE_CLOSURE(state, value->value.object);
                    function_pre_call_native(state,
                                             stackPointer,
                                             resultCount,
                                             native->nativeFunction,
                                             returnDestination);
                    return ZR_NULL;
                }
                {
                    SZrFunction *function = ZR_NULL;
                    if (!function_prepare_vm_callable_value(state, stackPointer, &value, &function)) {
                        return ZR_NULL;
                    }
                    return function_pre_call_vm(state, stackPointer, resultCount, returnDestination, function);
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
                value = ZrCore_Stack_GetValue(stackPointer);
            } break;
        }
    } while (ZR_TRUE);
}

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_vm(struct SZrState *state,
                                                         TZrStackValuePointer stackPointer,
                                                         TZrSize resultCount,
                                                         TZrStackValuePointer returnDestination,
                                                         SZrFunction *function) {
    TZrSize argumentsCount;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(function != ZR_NULL);

    argumentsCount = ZR_CAST_INT64(state->stackTop.valuePointer - stackPointer) - 1;
    return function_pre_call_resolved_vm(
            state,
            stackPointer,
            function,
            argumentsCount,
            resultCount,
            returnDestination);
}

static ZR_FORCE_INLINE SZrCallInfo *function_pre_call_resolved_vm(struct SZrState *state,
                                                                  TZrStackValuePointer stackPointer,
                                                                  SZrFunction *function,
                                                                  TZrSize argumentsCount,
                                                                  TZrSize resultCount,
                                                                  TZrStackValuePointer returnDestination) {
    SZrCallInfo *callInfo;
    TZrSize parametersCount;
    TZrSize stackSize;
    TZrSize entryClearStackSize;
    TZrUInt32 debugHookSignal;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(function != ZR_NULL);

    parametersCount = function->parameterCount;
    stackSize = function->stackSize;
    entryClearStackSize = function_resolve_vm_entry_clear_stack_size(function);
    debugHookSignal = state->debugHookSignal;

    state->stackTop.valuePointer = stackPointer + 1 + argumentsCount;
    function_restore_call_pointers_after_stack_check(state, stackSize + 1, stackPointer, &stackPointer, &returnDestination);
    callInfo = function_acquire_call_info(state);
    if (ZR_UNLIKELY(callInfo == ZR_NULL)) {
        return ZR_NULL;
    }

    function_init_vm_call_info(callInfo,
                               state->callInfoList,
                               stackPointer,
                               stackPointer + 1 + stackSize,
                               resultCount,
                               returnDestination,
                               function->instructionsList,
                               function_debug_trap_from_hook_signal(debugHookSignal));
    state->callInfoList = callInfo;
    if (entryClearStackSize > argumentsCount) {
        function_initialize_vm_frame_slots(stackPointer, argumentsCount, entryClearStackSize);
    }
    if (parametersCount != argumentsCount) {
        state->stackTop.valuePointer = stackPointer + 1 + parametersCount;
    }
    if (ZR_UNLIKELY(debugHookSignal & ZR_DEBUG_HOOK_MASK_CALL)) {
        ZrCore_Debug_Hook(state,
                          ZR_DEBUG_HOOK_EVENT_CALL,
                          ZR_RUNTIME_DEBUG_HOOK_LINE_NONE,
                          1,
                          (TZrUInt32)argumentsCount);
    }
    ZR_ASSERT(callInfo->functionTop.valuePointer <= state->stackTail.valuePointer);
    return callInfo;
}

SZrCallInfo *ZrCore_Function_PreCallKnownValue(struct SZrState *state,
                                          TZrStackValuePointer stackPointer,
                                          SZrTypeValue *callableValue,
                                          TZrSize resultCount,
                                          TZrStackValuePointer returnDestination) {
    return function_pre_call_dispatch(state, stackPointer, callableValue, resultCount, returnDestination);
}

SZrCallInfo *ZrCore_Function_PreCallKnownVmValue(struct SZrState *state,
                                            TZrStackValuePointer stackPointer,
                                            SZrTypeValue *callableValue,
                                            TZrSize resultCount,
                                            TZrStackValuePointer returnDestination) {
    SZrFunction *function = ZR_NULL;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(callableValue != ZR_NULL);

    /*
     * KNOWN_VM_CALL already proves the callee family, so zero-capture VM
     * functions do not need to be re-materialized as closures on every hot
     * dispatch. Keeping the base slot as FUNCTION trims the steady-state path
     * while still preserving closure materialization for capturing functions.
     */
    if (callableValue->type == ZR_VALUE_TYPE_FUNCTION && !callableValue->isNative) {
        function = ZR_CAST_FUNCTION(state, callableValue->value.object);
        if (function != ZR_NULL && function->closureValueLength == 0) {
            return function_pre_call_vm(state, stackPointer, resultCount, returnDestination, function);
        }
    }

    if (!function_prepare_vm_callable_value(state, stackPointer, &callableValue, &function)) {
        return ZR_NULL;
    }

    ZR_ASSERT(function != ZR_NULL);
    return function_pre_call_vm(state, stackPointer, resultCount, returnDestination, function);
}

SZrCallInfo *ZrCore_Function_PreCallResolvedVmFunction(struct SZrState *state,
                                                  TZrStackValuePointer stackPointer,
                                                  SZrFunction *function,
                                                  TZrSize argumentsCount,
                                                  TZrSize resultCount,
                                                  TZrStackValuePointer returnDestination) {
    return function_pre_call_resolved_vm(
            state,
            stackPointer,
            function,
            argumentsCount,
            resultCount,
            returnDestination);
}

SZrCallInfo *ZrCore_Function_PreCallKnownNativeValue(struct SZrState *state,
                                                TZrStackValuePointer stackPointer,
                                                SZrTypeValue *callableValue,
                                                TZrSize resultCount,
                                                TZrStackValuePointer returnDestination) {
    FZrNativeFunction nativeFunction = ZR_NULL;

    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(stackPointer != ZR_NULL);
    ZR_ASSERT(callableValue != ZR_NULL);

    switch (callableValue->type) {
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE: {
            SZrClosureNative *nativeClosure;

            ZR_ASSERT(callableValue->isNative);
            nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, callableValue->value.object);
            ZR_ASSERT(nativeClosure != ZR_NULL);
            nativeFunction = nativeClosure->nativeFunction;
        } break;
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            nativeFunction = ZR_CAST_FUNCTION_POINTER(callableValue);
            break;
        default:
            ZR_ASSERT(ZR_FALSE && "known native call requires native callable");
            return ZR_NULL;
    }

    ZR_ASSERT(nativeFunction != ZR_NULL);
    function_pre_call_native(state, stackPointer, resultCount, nativeFunction, returnDestination);
    return ZR_NULL;
}

SZrCallInfo *ZrCore_Function_PreCall(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount,
                                TZrStackValuePointer returnDestination) {
    SZrProfileRuntime *profileRuntime =
            (state != ZR_NULL && state->global != ZR_NULL) ? state->global->profileRuntime : ZR_NULL;

    if (ZR_UNLIKELY(profileRuntime != ZR_NULL && profileRuntime->recordHelpers)) {
        profileRuntime->helperCounts[ZR_PROFILE_HELPER_PRECALL]++;
    }
    return ZrCore_Function_PreCallKnownValue(
            state,
            stackPointer,
            ZrCore_Stack_GetValue(stackPointer),
            resultCount,
            returnDestination);
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
