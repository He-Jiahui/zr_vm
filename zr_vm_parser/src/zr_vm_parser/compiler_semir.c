//
// Minimal SemIR metadata builder for ownership-aware and dynamic bytecode.
//

#include <string.h>

#include "compiler_internal.h"

#define ZR_SEMIR_TYPE_TABLE_DEFAULT_ENTRY_COUNT 1U
#define ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX 0U
#define ZR_SEMIR_OWNERSHIP_STATE_TABLE_CAPACITY 8U
#define ZR_SEMIR_OWNERSHIP_STATE_INDEX_FIRST 0U
#define ZR_SEMIR_DEOPT_ID_FIRST (ZR_RUNTIME_SEMIR_DEOPT_ID_NONE + 1U)

typedef struct SZrSemIrMappedInstruction {
    TZrUInt32 opcode;
    TZrUInt32 effectKind;
    TZrUInt32 ownershipInput;
    TZrUInt32 ownershipOutput;
    TZrBool needsDeopt;
    TZrBool hasExplicitOperands;
    TZrUInt32 destinationSlot;
    TZrUInt32 operand0;
    TZrUInt32 operand1;
} SZrSemIrMappedInstruction;

static SZrString *semir_resolve_member_symbol(const SZrFunction *function, TZrUInt16 memberId) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberId >= function->memberEntryLength) {
        return ZR_NULL;
    }

    return function->memberEntries[memberId].symbol;
}

static TZrBool semir_string_has_prefix(SZrString *stringValue, const TZrChar *prefix) {
    TZrNativeString text;
    TZrSize prefixLength;

    if (stringValue == ZR_NULL || prefix == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(stringValue);
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    prefixLength = strlen(prefix);
    return strncmp(text, prefix, prefixLength) == 0;
}

static TZrBool semir_match_meta_accessor_call(const SZrFunction *function,
                                              TZrUInt32 instructionIndex,
                                              SZrSemIrMappedInstruction *outMapped) {
    const TZrInstruction *callInstruction;
    const TZrInstruction *getMemberInstruction;
    const TZrInstruction *nextInstruction;
    SZrString *memberName;
    TZrUInt32 parameterCount;

    if (function == ZR_NULL || outMapped == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionIndex >= function->instructionsLength || instructionIndex == 0) {
        return ZR_FALSE;
    }

    callInstruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)callInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_CALL)) {
        return ZR_FALSE;
    }

    getMemberInstruction = &function->instructionsList[instructionIndex - 1];
    if ((EZrInstructionCode)getMemberInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_MEMBER)) {
        return ZR_FALSE;
    }
    if (getMemberInstruction->instruction.operandExtra != callInstruction->instruction.operandExtra ||
        getMemberInstruction->instruction.operand.operand1[0] != callInstruction->instruction.operand.operand1[0]) {
        return ZR_FALSE;
    }

    memberName = semir_resolve_member_symbol(function, getMemberInstruction->instruction.operand.operand1[1]);
    if (memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    parameterCount = callInstruction->instruction.operand.operand1[1];
    if (semir_string_has_prefix(memberName, "__get_")) {
        if (parameterCount > 1) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(outMapped, 0, sizeof(*outMapped));
        outMapped->opcode = ZR_SEMIR_OPCODE_META_GET;
        outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
        outMapped->needsDeopt = ZR_TRUE;
        outMapped->hasExplicitOperands = ZR_TRUE;
        outMapped->destinationSlot = callInstruction->instruction.operandExtra;
        outMapped->operand0 = callInstruction->instruction.operand.operand1[0];
        outMapped->operand1 = getMemberInstruction->instruction.operand.operand1[1];
        return ZR_TRUE;
    }

    if (!semir_string_has_prefix(memberName, "__set_")) {
        return ZR_FALSE;
    }
    if (parameterCount == 0 || instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    nextInstruction = &function->instructionsList[instructionIndex + 1];
    if ((EZrInstructionCode)nextInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        nextInstruction->instruction.operandExtra != callInstruction->instruction.operandExtra) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outMapped, 0, sizeof(*outMapped));
    outMapped->opcode = ZR_SEMIR_OPCODE_META_SET;
    outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
    outMapped->needsDeopt = ZR_TRUE;
    outMapped->hasExplicitOperands = ZR_TRUE;
    outMapped->destinationSlot = nextInstruction->instruction.operandExtra;
    outMapped->operand0 = callInstruction->instruction.operand.operand1[0];
    outMapped->operand1 = getMemberInstruction->instruction.operand.operand1[1];
    return ZR_TRUE;
}

static void semir_init_default_type_ref(SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
}

static TZrBool semir_type_ref_equals(const SZrFunctionTypedTypeRef *lhs, const SZrFunctionTypedTypeRef *rhs) {
    if (lhs == rhs) {
        return ZR_TRUE;
    }
    if (lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    return lhs->baseType == rhs->baseType &&
           lhs->isNullable == rhs->isNullable &&
           lhs->ownershipQualifier == rhs->ownershipQualifier &&
           lhs->isArray == rhs->isArray &&
           lhs->typeName == rhs->typeName &&
           lhs->elementBaseType == rhs->elementBaseType &&
           lhs->elementTypeName == rhs->elementTypeName;
}

static void semir_release_existing_metadata(SZrState *state, SZrFunction *function) {
    SZrGlobalState *global;

    if (state == ZR_NULL || function == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    global = state->global;
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
}

static TZrUInt32 semir_ensure_type_entry(SZrFunctionTypedTypeRef *typeTable,
                                         TZrUInt32 *ioCount,
                                         const SZrFunctionTypedTypeRef *typeRef) {
    TZrUInt32 index;

    if (typeTable == ZR_NULL || ioCount == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX;
    }

    for (index = 0; index < *ioCount; index++) {
        if (semir_type_ref_equals(&typeTable[index], typeRef)) {
            return index;
        }
    }

    typeTable[*ioCount] = *typeRef;
    (*ioCount)++;
    return (*ioCount) - 1;
}

static TZrUInt32 semir_find_type_index_for_slot(const SZrFunction *function,
                                                TZrUInt32 slot,
                                                const SZrFunctionTypedTypeRef *typeTable,
                                                TZrUInt32 typeCount) {
    TZrUInt32 bindingIndex;
    TZrUInt32 typeIndex;

    if (function == ZR_NULL || typeTable == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX;
    }

    for (bindingIndex = 0; bindingIndex < function->typedLocalBindingLength; bindingIndex++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[bindingIndex];
        if (binding->stackSlot != slot) {
            continue;
        }

        for (typeIndex = 0; typeIndex < typeCount; typeIndex++) {
            if (semir_type_ref_equals(&typeTable[typeIndex], &binding->type)) {
                return typeIndex;
            }
        }
    }

    return ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX;
}

static TZrUInt32 semir_ensure_ownership_state(TZrUInt32 *stateTable,
                                              TZrUInt32 *ioCount,
                                              TZrUInt32 stateValue) {
    TZrUInt32 index;

    if (stateTable == ZR_NULL || ioCount == ZR_NULL) {
        return ZR_SEMIR_OWNERSHIP_STATE_INDEX_FIRST;
    }

    for (index = 0; index < *ioCount; index++) {
        if (stateTable[index] == stateValue) {
            return index;
        }
    }

    stateTable[*ioCount] = stateValue;
    (*ioCount)++;
    return (*ioCount) - 1;
}

static TZrBool semir_map_exec_instruction(const TZrInstruction *instruction, SZrSemIrMappedInstruction *outMapped) {
    if (instruction == ZR_NULL || outMapped == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outMapped, 0, sizeof(*outMapped));
    switch ((EZrInstructionCode)instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_UNIQUE;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_UNIQUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_BORROW):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_BORROW;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_BORROW_SHARED;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_LOAN):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_LOAN;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_UNIQUE;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_BORROW_MUT;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_SHARE):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_SHARE;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_UNIQUE;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_WEAK):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_WEAK;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_WEAK;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_DETACH):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_DETACH;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_UPGRADE;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_WEAK;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
            outMapped->opcode = ZR_SEMIR_OPCODE_OWN_RELEASE;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION;
            outMapped->ownershipInput = ZR_SEMIR_OWNERSHIP_STATE_SHARED;
            outMapped->ownershipOutput = ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(TYPEOF):
            outMapped->opcode = ZR_SEMIR_OPCODE_TYPEOF;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_TAIL_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(META_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_TAIL_CALL;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(META_GET):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_GET;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            outMapped->hasExplicitOperands = ZR_TRUE;
            outMapped->destinationSlot = instruction->instruction.operandExtra;
            outMapped->operand0 = instruction->instruction.operand.operand1[0];
            outMapped->operand1 = instruction->instruction.operand.operand1[1];
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(META_SET):
            outMapped->opcode = ZR_SEMIR_OPCODE_META_SET;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            outMapped->hasExplicitOperands = ZR_TRUE;
            outMapped->destinationSlot = instruction->instruction.operandExtra;
            outMapped->operand0 = instruction->instruction.operandExtra;
            outMapped->operand1 = instruction->instruction.operand.operand1[1];
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_ITER_INIT;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
            outMapped->opcode = ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT;
            outMapped->effectKind = ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME;
            outMapped->needsDeopt = ZR_TRUE;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool semir_allocate_type_table(SZrState *state,
                                         SZrFunction *function,
                                         SZrFunctionTypedTypeRef **outTypeTable,
                                         TZrUInt32 *outTypeCount) {
    SZrGlobalState *global;
    TZrUInt32 typeCapacity;
    TZrUInt32 typeCount = 0;
    SZrFunctionTypedTypeRef *typeTable;
    SZrFunctionTypedTypeRef *finalTypeTable;
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || outTypeTable == ZR_NULL || outTypeCount == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    typeCapacity = function->typedLocalBindingLength + ZR_SEMIR_TYPE_TABLE_DEFAULT_ENTRY_COUNT;
    typeTable = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrFunctionTypedTypeRef) * typeCapacity,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (typeTable == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(typeTable, 0, sizeof(SZrFunctionTypedTypeRef) * typeCapacity);
    semir_init_default_type_ref(&typeTable[typeCount]);
    typeCount++;
    for (index = 0; index < function->typedLocalBindingLength; index++) {
        if (function->typedLocalBindings == ZR_NULL) {
            break;
        }
        semir_ensure_type_entry(typeTable, &typeCount, &function->typedLocalBindings[index].type);
    }

    finalTypeTable = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrFunctionTypedTypeRef) * typeCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (finalTypeTable == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      typeTable,
                                      sizeof(SZrFunctionTypedTypeRef) * typeCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawCopy(finalTypeTable, typeTable, sizeof(SZrFunctionTypedTypeRef) * typeCount);
    ZrCore_Memory_RawFreeWithType(global,
                                  typeTable,
                                  sizeof(SZrFunctionTypedTypeRef) * typeCapacity,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);

    *outTypeTable = finalTypeTable;
    *outTypeCount = typeCount;
    return ZR_TRUE;
}

static TZrBool semir_build_for_single_function(SZrState *state, SZrFunction *function) {
    SZrGlobalState *global;
    TZrUInt32 semirInstructionCount = 0;
    TZrUInt32 deoptCount = 0;
    TZrUInt32 ownershipStates[ZR_SEMIR_OWNERSHIP_STATE_TABLE_CAPACITY] = {0};
    TZrUInt32 ownershipCount = 0;
    SZrFunctionTypedTypeRef *typeTable = ZR_NULL;
    TZrUInt32 typeCount = 0;
    SZrSemIrEffectEntry *effectTable = ZR_NULL;
    SZrSemIrInstruction *instructionTable = ZR_NULL;
    SZrSemIrDeoptEntry *deoptTable = ZR_NULL;
    TZrUInt32 semirIndex = 0;
    TZrUInt32 nextDeoptId = ZR_SEMIR_DEOPT_ID_FIRST;
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    semir_release_existing_metadata(state, function);

    if (function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        SZrSemIrMappedInstruction mapped;
        if (semir_match_meta_accessor_call(function, index, &mapped) ||
            semir_map_exec_instruction(&function->instructionsList[index], &mapped)) {
            semirInstructionCount++;
            if (mapped.needsDeopt) {
                deoptCount++;
            }
        }
    }

    if (semirInstructionCount == 0) {
        return ZR_TRUE;
    }

    if (!semir_allocate_type_table(state, function, &typeTable, &typeCount)) {
        return ZR_FALSE;
    }

    effectTable = (SZrSemIrEffectEntry *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrSemIrEffectEntry) * semirInstructionCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    instructionTable = (SZrSemIrInstruction *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrSemIrInstruction) * semirInstructionCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (deoptCount > 0) {
        deoptTable = (SZrSemIrDeoptEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(SZrSemIrDeoptEntry) * deoptCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    if (effectTable == ZR_NULL || instructionTable == ZR_NULL || (deoptCount > 0 && deoptTable == ZR_NULL)) {
        if (typeTable != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          typeTable,
                                          sizeof(SZrFunctionTypedTypeRef) * typeCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (effectTable != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          effectTable,
                                          sizeof(SZrSemIrEffectEntry) * semirInstructionCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (instructionTable != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          instructionTable,
                                          sizeof(SZrSemIrInstruction) * semirInstructionCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (deoptTable != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          deoptTable,
                                          sizeof(SZrSemIrDeoptEntry) * deoptCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(effectTable, 0, sizeof(SZrSemIrEffectEntry) * semirInstructionCount);
    ZrCore_Memory_RawSet(instructionTable, 0, sizeof(SZrSemIrInstruction) * semirInstructionCount);
    if (deoptTable != ZR_NULL) {
        ZrCore_Memory_RawSet(deoptTable, 0, sizeof(SZrSemIrDeoptEntry) * deoptCount);
    }

    function->semIrTypeTable = typeTable;
    function->semIrTypeTableLength = typeCount;
    function->semIrEffectTable = effectTable;
    function->semIrEffectTableLength = semirInstructionCount;
    function->semIrInstructions = instructionTable;
    function->semIrInstructionLength = semirInstructionCount;
    function->semIrDeoptTable = deoptTable;
    function->semIrDeoptTableLength = deoptCount;

    for (index = 0; index < function->instructionsLength; index++) {
        SZrSemIrMappedInstruction mapped;
        TZrUInt32 deoptId = ZR_RUNTIME_SEMIR_DEOPT_ID_NONE;

        if (!(semir_match_meta_accessor_call(function, index, &mapped) ||
              semir_map_exec_instruction(&function->instructionsList[index], &mapped))) {
            continue;
        }

        function->semIrEffectTable[semirIndex].kind = mapped.effectKind;
        function->semIrEffectTable[semirIndex].instructionIndex = semirIndex;
        if (mapped.effectKind == ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION) {
            function->semIrEffectTable[semirIndex].ownershipInputIndex =
                    semir_ensure_ownership_state(ownershipStates, &ownershipCount, mapped.ownershipInput);
            function->semIrEffectTable[semirIndex].ownershipOutputIndex =
                    semir_ensure_ownership_state(ownershipStates, &ownershipCount, mapped.ownershipOutput);
        }

        if (mapped.needsDeopt && function->semIrDeoptTable != ZR_NULL) {
            TZrUInt32 deoptIndex = nextDeoptId - ZR_SEMIR_DEOPT_ID_FIRST;
            deoptId = nextDeoptId++;
            function->semIrDeoptTable[deoptIndex].deoptId = deoptId;
            function->semIrDeoptTable[deoptIndex].execInstructionIndex = index;
        }

        function->semIrInstructions[semirIndex].opcode = mapped.opcode;
        function->semIrInstructions[semirIndex].execInstructionIndex = index;
        function->semIrInstructions[semirIndex].typeTableIndex =
                semir_find_type_index_for_slot(function,
                                               mapped.hasExplicitOperands ? mapped.destinationSlot
                                                                          : function->instructionsList[index]
                                                                                    .instruction.operandExtra,
                                               function->semIrTypeTable,
                                               function->semIrTypeTableLength);
        function->semIrInstructions[semirIndex].effectTableIndex = semirIndex;
        function->semIrInstructions[semirIndex].destinationSlot =
                mapped.hasExplicitOperands ? mapped.destinationSlot
                                           : function->instructionsList[index].instruction.operandExtra;
        function->semIrInstructions[semirIndex].operand0 =
                mapped.hasExplicitOperands ? mapped.operand0
                                           : function->instructionsList[index].instruction.operand.operand1[0];
        function->semIrInstructions[semirIndex].operand1 =
                mapped.hasExplicitOperands ? mapped.operand1
                                           : function->instructionsList[index].instruction.operand.operand1[1];
        function->semIrInstructions[semirIndex].deoptId = deoptId;
        semirIndex++;
    }

    if (ownershipCount > 0) {
        function->semIrOwnershipTable = (SZrSemIrOwnershipEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(SZrSemIrOwnershipEntry) * ownershipCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->semIrOwnershipTable == ZR_NULL) {
            semir_release_existing_metadata(state, function);
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(function->semIrOwnershipTable,
                             0,
                             sizeof(SZrSemIrOwnershipEntry) * ownershipCount);
        for (index = 0; index < ownershipCount; index++) {
            function->semIrOwnershipTable[index].state = ownershipStates[index];
        }
        function->semIrOwnershipTableLength = ownershipCount;
    }

    function->semIrBlockTable = (SZrSemIrBlockEntry *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrSemIrBlockEntry),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->semIrBlockTable == ZR_NULL) {
        semir_release_existing_metadata(state, function);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(function->semIrBlockTable, 0, sizeof(SZrSemIrBlockEntry));
    function->semIrBlockTable[0].blockId = 0;
    function->semIrBlockTable[0].firstInstructionIndex = 0;
    function->semIrBlockTable[0].instructionCount = semirInstructionCount;
    function->semIrBlockTableLength = 1;
    return ZR_TRUE;
}

static TZrBool compiler_build_function_semir_metadata_internal(SZrState *state,
                                                               SZrFunction *function,
                                                               TZrBool recurseChildren) {
    TZrUInt32 childIndex;

    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!semir_build_for_single_function(state, function)) {
        return ZR_FALSE;
    }

    if (recurseChildren && !function->childFunctionGraphIsBorrowed) {
        for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            if (!compiler_build_function_semir_metadata_internal(state,
                                                                 &function->childFunctionList[childIndex],
                                                                 ZR_TRUE)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

TZrBool compiler_build_function_semir_metadata(SZrState *state, SZrFunction *function) {
    return compiler_build_function_semir_metadata_internal(state, function, ZR_TRUE);
}

TZrBool compiler_build_function_semir_metadata_shallow(SZrState *state, SZrFunction *function) {
    return compiler_build_function_semir_metadata_internal(state, function, ZR_FALSE);
}
