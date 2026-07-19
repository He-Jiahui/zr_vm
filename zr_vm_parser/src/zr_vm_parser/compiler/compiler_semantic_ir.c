#include "compiler_internal.h"

typedef struct SZrCompilerSemanticIrSlot {
    TZrUInt32 stackSlot;
    TZrPlaceId placeId;
    TZrValueId valueId;
    TZrTypeId typeId;
    TZrSymbolId symbolId;
} SZrCompilerSemanticIrSlot;

static SZrCompilerSemanticIrSlot *compiler_semantic_ir_find_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot) {
    TZrSize index;

    if (cs == ZR_NULL || !cs->preSemanticIrSlots.isValid) {
        return ZR_NULL;
    }
    for (index = cs->preSemanticIrSlots.length; index > 0U; index--) {
        SZrCompilerSemanticIrSlot *slot =
                (SZrCompilerSemanticIrSlot *)ZrCore_Array_Get(
                        &cs->preSemanticIrSlots, index - 1U);
        if (slot != ZR_NULL && slot->stackSlot == stackSlot) {
            return slot;
        }
    }
    return ZR_NULL;
}

static TZrBool compiler_semantic_ir_emit(
        SZrCompilerState *cs,
        const SZrSemanticIrInstructionSpec *spec) {
    if (cs == ZR_NULL || spec == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_FALSE;
    }
    cs->preSemanticIrValidated = ZR_FALSE;
    return (TZrBool)(ZrParser_SemanticIr_Emit(&cs->preSemanticIr, spec) !=
                     ZR_SEMANTIC_INSTRUCTION_ID_INVALID);
}

static const SZrSemanticIrInstruction *compiler_semantic_ir_last_instruction(
        const SZrCompilerState *cs) {
    if (cs == ZR_NULL || !cs->preSemanticIrInitialized ||
        cs->preSemanticIr.instructions.length == 0U) {
        return ZR_NULL;
    }
    return ZrParser_SemanticIr_InstructionAt(
            &cs->preSemanticIr,
            cs->preSemanticIr.instructions.length - 1U);
}

static EZrInstructionCode compiler_semantic_ir_exec_opcode(
        const SZrSemanticIrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
    switch (instruction->opcode) {
        case ZR_SEMANTIC_IR_LOAD:
            return ZR_INSTRUCTION_ENUM(GET_STACK);
        case ZR_SEMANTIC_IR_STORE:
            return ZR_INSTRUCTION_ENUM(SET_STACK);
        case ZR_SEMANTIC_IR_MOVE:
            return ZR_INSTRUCTION_ENUM(OWN_DETACH);
        case ZR_SEMANTIC_IR_DROP:
            return ZR_INSTRUCTION_ENUM(OWN_RELEASE);
        case ZR_SEMANTIC_IR_BORROW_SHARED:
            return ZR_INSTRUCTION_ENUM(OWN_BORROW);
        case ZR_SEMANTIC_IR_BORROW_MUT:
            return ZR_INSTRUCTION_ENUM(OWN_LOAN);
        case ZR_SEMANTIC_IR_OWN_CONSTRUCT:
            switch (instruction->ownershipOperation) {
                case ZR_SEMANTIC_OWNERSHIP_UNIQUE:
                    return ZR_INSTRUCTION_ENUM(OWN_UNIQUE);
                case ZR_SEMANTIC_OWNERSHIP_SHARE:
                    return ZR_INSTRUCTION_ENUM(OWN_SHARE);
                case ZR_SEMANTIC_OWNERSHIP_WEAK:
                    return ZR_INSTRUCTION_ENUM(OWN_WEAK);
                case ZR_SEMANTIC_OWNERSHIP_UPGRADE:
                    return ZR_INSTRUCTION_ENUM(OWN_UPGRADE);
                case ZR_SEMANTIC_OWNERSHIP_NONE:
                case ZR_SEMANTIC_OWNERSHIP_ENUM_MAX:
                default:
                    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
            }
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static SZrCompilerSemanticIrSlot *compiler_semantic_ir_add_temporary_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot slot;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_NULL;
    }
    memset(&slot, 0, sizeof(slot));
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_TEMPORARY;
    base.identity = stackSlot;
    slot.stackSlot = stackSlot;
    slot.placeId = ZrParser_PlaceGraph_AddBase(
            &cs->preSemanticIr.places,
            &base,
            ZR_SEMANTIC_ID_INVALID,
            sourceRange);
    if (slot.placeId == ZR_PLACE_ID_INVALID) {
        return ZR_NULL;
    }
    slot.valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, ZR_SEMANTIC_ID_INVALID, sourceRange);
    if (slot.valueId == ZR_VALUE_ID_INVALID) {
        return ZR_NULL;
    }
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    spec.placeId = slot.placeId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_NULL;
    }
    spec.opcode = ZR_SEMANTIC_IR_INITIALIZE;
    spec.valueId = slot.valueId;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_NULL;
    }
    return (SZrCompilerSemanticIrSlot *)ZrCore_Array_Get(
            &cs->preSemanticIrSlots, cs->preSemanticIrSlots.length - 1U);
}

static SZrCompilerSemanticIrSlot *compiler_semantic_ir_materialize_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_find_slot(cs, stackSlot);
    TZrSize index;

    if (slot != ZR_NULL || cs == ZR_NULL || !cs->localVars.isValid) {
        return slot;
    }
    for (index = cs->localVars.length; index > 0U; index--) {
        const SZrFunctionLocalVariable *local =
                (const SZrFunctionLocalVariable *)ZrCore_Array_Get(
                        &cs->localVars, index - 1U);
        if (local != ZR_NULL && local->stackSlot == stackSlot &&
            local->name != ZR_NULL) {
            if (compiler_semantic_ir_register_local(
                        cs,
                        local->name,
                        stackSlot,
                        sourceRange,
                        ZR_TRUE)) {
                return compiler_semantic_ir_find_slot(cs, stackSlot);
            }
            break;
        }
    }
    return compiler_semantic_ir_add_temporary_slot(
            cs, stackSlot, sourceRange);
}

void compiler_semantic_ir_init(SZrCompilerState *cs) {
    SZrFileRange emptyRange;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }
    memset(&emptyRange, 0, sizeof(emptyRange));
    ZrParser_SemanticIrFunction_Init(
            cs->state,
            &cs->preSemanticIr,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID);
    ZrCore_Array_Init(
            cs->state,
            &cs->preSemanticIrSlots,
            sizeof(SZrCompilerSemanticIrSlot),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cs->preSemanticIrInitialized = ZR_TRUE;
    cs->preSemanticIrValidated = ZR_FALSE;
    (void)ZrParser_SemanticIr_AddRegion(
            &cs->preSemanticIr,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            emptyRange);
}

void compiler_semantic_ir_reset(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }
    compiler_semantic_ir_free(cs);
    compiler_semantic_ir_init(cs);
}

void compiler_semantic_ir_free(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !cs->preSemanticIrInitialized) {
        return;
    }
    ZrParser_SemanticIrFunction_Free(cs->state, &cs->preSemanticIr);
    ZrCore_Array_Free(cs->state, &cs->preSemanticIrSlots);
    cs->preSemanticIrInitialized = ZR_FALSE;
    cs->preSemanticIrValidated = ZR_FALSE;
}

const SZrSemanticIrFunction *ZrParser_Compiler_PreSemanticIr(
        const SZrCompilerState *cs) {
    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_NULL;
    }
    return &cs->preSemanticIr;
}

TZrBool ZrParser_Compiler_PreSemanticIrIsValidated(
        const SZrCompilerState *cs) {
    return (TZrBool)(cs != ZR_NULL && cs->preSemanticIrInitialized &&
                     cs->preSemanticIrValidated);
}

TZrBool ZrParser_Compiler_ValidatePreSemanticIr(SZrCompilerState *cs) {
    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_FALSE;
    }
    cs->preSemanticIrValidated =
            ZrParser_SemanticIr_Validate(&cs->preSemanticIr);
    return cs->preSemanticIrValidated;
}

TZrBool compiler_semantic_ir_register_local(SZrCompilerState *cs,
                                             SZrString *name,
                                             TZrUInt32 stackSlot,
                                             SZrFileRange sourceRange,
                                             TZrBool initialized) {
    const SZrTypeBinding *binding;
    SZrCompilerSemanticIrSlot slot;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || name == ZR_NULL || cs->typeEnv == ZR_NULL ||
        stackSlot == ZR_PARSER_SLOT_NONE || !cs->preSemanticIrInitialized) {
        return ZR_FALSE;
    }
    binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
    if (binding == ZR_NULL || binding->typeId == ZR_SEMANTIC_ID_INVALID ||
        binding->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&slot, 0, sizeof(slot));
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = stackSlot;
    slot.stackSlot = stackSlot;
    slot.typeId = binding->typeId;
    slot.symbolId = binding->symbolId;
    slot.placeId = ZrParser_SemanticIr_AddLocal(
            &cs->preSemanticIr,
            slot.symbolId,
            &base,
            slot.typeId,
            sourceRange,
            ZR_FALSE);
    if (slot.placeId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    spec.typeId = slot.typeId;
    spec.placeId = slot.placeId;
    spec.symbolId = slot.symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    if (!initialized) {
        return ZR_TRUE;
    }

    slot.valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, slot.typeId, sourceRange);
    if (slot.valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    *(SZrCompilerSemanticIrSlot *)ZrCore_Array_Get(
            &cs->preSemanticIrSlots, cs->preSemanticIrSlots.length - 1U) = slot;
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_INITIALIZE;
    spec.typeId = slot.typeId;
    spec.placeId = slot.placeId;
    spec.valueId = slot.valueId;
    spec.symbolId = slot.symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

static TZrBool compiler_semantic_ir_emit_load(SZrCompilerState *cs,
                                              TZrUInt32 stackSlot,
                                              SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_materialize_slot(cs, stackSlot, sourceRange);
    SZrSemanticIrInstructionSpec spec;
    TZrValueId resultValueId;

    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    resultValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, slot->typeId, sourceRange);
    if (resultValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_LOAD;
    spec.typeId = slot->typeId;
    spec.placeId = slot->placeId;
    spec.resultValueId = resultValueId;
    spec.symbolId = slot->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

static TZrBool compiler_semantic_ir_emit_store(SZrCompilerState *cs,
                                               TZrUInt32 stackSlot,
                                               SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_materialize_slot(cs, stackSlot, sourceRange);
    SZrSemanticIrInstructionSpec spec;
    TZrValueId valueId;

    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, slot->typeId, sourceRange);
    if (valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    slot->valueId = valueId;
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_STORE;
    spec.typeId = slot->typeId;
    spec.placeId = slot->placeId;
    spec.valueId = valueId;
    spec.symbolId = slot->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

static TZrBool compiler_semantic_ir_emit_ownership(
        SZrCompilerState *cs,
        EZrOwnershipBuiltinKind builtinKind,
        TZrUInt32 sourceSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_materialize_slot(cs, sourceSlot, sourceRange);
    SZrSemanticIrInstructionSpec spec;
    TZrValueId resultValueId = ZR_VALUE_ID_INVALID;

    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.typeId = slot->typeId;
    spec.placeId = slot->placeId;
    spec.valueId = slot->valueId;
    spec.symbolId = slot->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    switch (builtinKind) {
        case ZR_OWNERSHIP_BUILTIN_KIND_RELEASE:
            spec.opcode = ZR_SEMANTIC_IR_DROP;
            slot->valueId = ZR_VALUE_ID_INVALID;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_DETACH:
            spec.opcode = ZR_SEMANTIC_IR_MOVE;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            slot->valueId = ZR_VALUE_ID_INVALID;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
            spec.opcode = ZR_SEMANTIC_IR_BORROW_SHARED;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            spec.regionId = 1U;
            spec.loanId = ZrParser_SemanticIr_AddLoan(
                    &cs->preSemanticIr,
                    slot->placeId,
                    ZR_SEMANTIC_LOAN_SHARED,
                    spec.regionId,
                    sourceRange,
                    sourceRange,
                    resultValueId);
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
            spec.opcode = ZR_SEMANTIC_IR_BORROW_MUT;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            spec.regionId = 1U;
            spec.loanId = ZrParser_SemanticIr_AddLoan(
                    &cs->preSemanticIr,
                    slot->placeId,
                    ZR_SEMANTIC_LOAN_MUTABLE,
                    spec.regionId,
                    sourceRange,
                    sourceRange,
                    resultValueId);
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_UNIQUE;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARED:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_SHARE;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_WEAK:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_WEAK;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_UPGRADE;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        case ZR_OWNERSHIP_BUILTIN_KIND_RETURN_LOAN:
        default:
            return ZR_FALSE;
    }
    if (((spec.opcode == ZR_SEMANTIC_IR_MOVE ||
          spec.opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
          spec.opcode == ZR_SEMANTIC_IR_BORROW_MUT ||
          spec.opcode == ZR_SEMANTIC_IR_OWN_CONSTRUCT) &&
         resultValueId == ZR_VALUE_ID_INVALID) ||
        ((spec.opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
          spec.opcode == ZR_SEMANTIC_IR_BORROW_MUT) &&
         spec.loanId == ZR_SEMANTIC_LOAN_ID_INVALID)) {
        return ZR_FALSE;
    }
    return compiler_semantic_ir_emit(cs, &spec);
}

TZrBool compiler_semantic_ir_lower_load(SZrCompilerState *cs,
                                        TZrUInt32 stackSlot,
                                        TZrUInt32 resultSlot,
                                        SZrFileRange sourceRange) {
    const SZrSemanticIrInstruction *instruction;
    EZrInstructionCode opcode;

    if (!compiler_semantic_ir_emit_load(cs, stackSlot, sourceRange)) {
        return ZR_FALSE;
    }
    instruction = compiler_semantic_ir_last_instruction(cs);
    opcode = compiler_semantic_ir_exec_opcode(instruction);
    if (opcode != ZR_INSTRUCTION_ENUM(GET_STACK)) {
        return ZR_FALSE;
    }
    emit_instruction(
            cs,
            create_instruction_1(
                    opcode,
                    (TZrUInt16)resultSlot,
                    (TZrInt32)stackSlot));
    return ZR_TRUE;
}

TZrBool compiler_semantic_ir_lower_store(SZrCompilerState *cs,
                                         TZrUInt32 stackSlot,
                                         TZrUInt32 valueSlot,
                                         SZrFileRange sourceRange) {
    const SZrSemanticIrInstruction *instruction;
    EZrInstructionCode opcode;

    if (!compiler_semantic_ir_emit_store(cs, stackSlot, sourceRange)) {
        return ZR_FALSE;
    }
    instruction = compiler_semantic_ir_last_instruction(cs);
    opcode = compiler_semantic_ir_exec_opcode(instruction);
    if (opcode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
        return ZR_FALSE;
    }
    emit_instruction(
            cs,
            create_instruction_1(
                    opcode,
                    (TZrUInt16)stackSlot,
                    (TZrInt32)valueSlot));
    return ZR_TRUE;
}

TZrBool compiler_semantic_ir_lower_ownership(
        SZrCompilerState *cs,
        EZrOwnershipBuiltinKind builtinKind,
        TZrUInt32 sourceSlot,
        TZrUInt32 resultSlot,
        SZrFileRange sourceRange) {
    const SZrSemanticIrInstruction *instruction;
    EZrInstructionCode opcode;

    if (!compiler_semantic_ir_emit_ownership(
                cs, builtinKind, sourceSlot, sourceRange)) {
        return ZR_FALSE;
    }
    instruction = compiler_semantic_ir_last_instruction(cs);
    opcode = compiler_semantic_ir_exec_opcode(instruction);
    if (opcode == ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
        return ZR_FALSE;
    }
    emit_instruction(
            cs,
            create_instruction_2(
                    opcode,
                    (TZrUInt16)resultSlot,
                    (TZrUInt16)sourceSlot,
                    0));
    return ZR_TRUE;
}
