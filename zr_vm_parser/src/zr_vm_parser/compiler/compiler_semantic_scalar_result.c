#include "compiler_semantic_scalar_result.h"

static TZrBool canonical_int64(const SZrCompilerState *cs, TZrTypeId typeId) {
    const SZrCanonicalTypeNode *type =
            ZrParser_CanonicalType_Find(cs->semanticContext, typeId);
    return (TZrBool)(type != ZR_NULL && type->id == typeId &&
            type->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            type->data.primitive.valueType == ZR_VALUE_TYPE_INT64);
}

TZrValueId compiler_semantic_scalar_result_initializer_value(
        const SZrCompilerState *cs,
        const SZrCompilerSemanticIrSlot *source,
        TZrTypeId localTypeId) {
    const SZrParserPlace *place;
    const SZrSemanticIrValue *value;
    const SZrSemanticIrInstruction *definition;
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        !cs->preSemanticIrInitialized || source == ZR_NULL ||
        source->loanId != ZR_SEMANTIC_LOAN_ID_INVALID ||
        source->typeId != localTypeId || !canonical_int64(cs, localTypeId))
        return ZR_VALUE_ID_INVALID;
    place = ZrParser_PlaceGraph_Get(&cs->preSemanticIr.places, source->placeId);
    value = ZrParser_SemanticIr_Value(&cs->preSemanticIr, source->valueId);
    if (place == ZR_NULL || place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY ||
        place->typeId != localTypeId || value == ZR_NULL ||
        value->id != source->valueId || value->typeId != localTypeId ||
        value->definitionInstructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        value->definitionInstructionId > cs->preSemanticIr.instructions.length)
        return ZR_VALUE_ID_INVALID;
    definition = ZrParser_SemanticIr_InstructionAt(
            &cs->preSemanticIr, value->definitionInstructionId - 1U);
    if (definition == ZR_NULL || definition->resultValueId != value->id ||
        definition->typeId != localTypeId)
        return ZR_VALUE_ID_INVALID;
    return value->id;
}

/* Callers prove the type and definition and consume all source pointers first.
 * The destination must be absent or TEMPORARY; no local identity is replaced. */
static EZrCompilerSemanticScalarResult bind_fresh_temporary(
        SZrCompilerState *cs, TZrUInt32 resultStackSlot, TZrTypeId typeId,
        TZrValueId resultValueId, SZrFileRange sourceRange) {
    const SZrCompilerSemanticIrSlot *existing =
            compiler_semantic_ir_find_slot(cs, resultStackSlot);
    SZrCompilerSemanticIrSlot slot = {0};
    SZrParserPlaceBase base = {0};
    SZrSemanticIrInstructionSpec spec = {0};
    TZrSize index;
    if (existing != ZR_NULL) {
        const SZrParserPlace *place = ZrParser_PlaceGraph_Get(
                &cs->preSemanticIr.places, existing->placeId);
        if (place == ZR_NULL || place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY)
            return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    } else if (cs->localVars.isValid) {
        /* Neither entry point may hide an unregistered named local. A reserved
         * initializer destination is still unnamed when its value is bound. */
        for (index = cs->localVars.length; index > 0U; --index) {
            const SZrFunctionLocalVariable *local =
                    (const SZrFunctionLocalVariable *)ZrCore_Array_Get(
                            &cs->localVars, index - 1U);
            if (local != ZR_NULL && local->stackSlot == resultStackSlot &&
                local->name != ZR_NULL)
                return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
        }
    }
    base.kind = ZR_PARSER_PLACE_BASE_TEMPORARY;
    base.identity = resultStackSlot;
    slot.stackSlot = resultStackSlot;
    slot.typeId = typeId;
    slot.valueId = resultValueId;
    slot.placeId = ZrParser_PlaceGraph_AddBase(
            &cs->preSemanticIr.places, &base, typeId, sourceRange);
    if (slot.placeId == ZR_PLACE_ID_INVALID)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);
    spec.opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    spec.typeId = typeId;
    spec.placeId = slot.placeId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec)
            ? ZR_COMPILER_SEMANTIC_SCALAR_RESULT_BOUND
            : ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
}

EZrCompilerSemanticScalarResult compiler_semantic_scalar_result_bind_literal(
        SZrCompilerState *cs, TZrUInt32 resultStackSlot, TZrTypeId typeId,
        TZrValueId resultValueId, TZrUInt32 constantPoolIndex,
        EZrValueType literalType, SZrFileRange sourceRange) {
    const SZrSemanticIrValue *value;
    const SZrSemanticIrInstruction *constant;
    const SZrTypeValue *pooledConstant;
    if (literalType != ZR_VALUE_TYPE_INT64)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_NOT_APPLICABLE;
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        !cs->preSemanticIrInitialized || cs->preSemanticIrCfgTerminated ||
        resultStackSlot == ZR_PARSER_SLOT_NONE || !canonical_int64(cs, typeId) ||
        constantPoolIndex >= cs->constants.length)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    value = ZrParser_SemanticIr_Value(&cs->preSemanticIr, resultValueId);
    if (value == ZR_NULL || value->id != resultValueId || value->typeId != typeId ||
        value->definitionInstructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        value->definitionInstructionId > cs->preSemanticIr.instructions.length)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    constant = ZrParser_SemanticIr_InstructionAt(
            &cs->preSemanticIr, value->definitionInstructionId - 1U);
    if (constant == ZR_NULL || constant->opcode != ZR_SEMANTIC_IR_CONSTANT ||
        constant->typeId != typeId || constant->resultValueId != resultValueId ||
        !constant->hasConstantPoolIndex || constant->constantPoolIndex != constantPoolIndex)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    pooledConstant = (const SZrTypeValue *)ZrCore_Array_Get(
            &cs->constants, constantPoolIndex);
    if (pooledConstant == ZR_NULL || pooledConstant->type != literalType)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    /* Validate at the producer, before later constant-storage compression.
     * Only stable IDs survive fresh binding's place/slot/instruction growth. */
    return bind_fresh_temporary(cs, resultStackSlot, typeId, resultValueId, sourceRange);
}

EZrCompilerSemanticScalarResult compiler_semantic_scalar_result_bind_load(
        SZrCompilerState *cs,
        TZrUInt32 sourceStackSlot,
        TZrUInt32 resultStackSlot,
        TZrTypeId typeId,
        TZrPlaceId sourcePlaceId,
        TZrValueId resultValueId,
        SZrFileRange sourceRange) {
    const SZrSemanticIrValue *value;
    const SZrSemanticIrInstruction *load;
    const SZrParserPlace *place;
    const SZrCompilerSemanticIrSlot *source;
    SZrCompilerSemanticIrSlot *existing;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        !cs->preSemanticIrInitialized || cs->preSemanticIrCfgTerminated ||
        sourceStackSlot == ZR_PARSER_SLOT_NONE ||
        resultStackSlot == ZR_PARSER_SLOT_NONE)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    if (!canonical_int64(cs, typeId))
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_NOT_APPLICABLE;

    source = compiler_semantic_ir_find_slot(cs, sourceStackSlot);
    value = ZrParser_SemanticIr_Value(&cs->preSemanticIr, resultValueId);
    place = ZrParser_PlaceGraph_Get(&cs->preSemanticIr.places, sourcePlaceId);
    if (source == ZR_NULL || source->typeId != typeId ||
        source->placeId != sourcePlaceId || place == ZR_NULL ||
        place->typeId != typeId || value == ZR_NULL ||
        value->id != resultValueId || value->typeId != typeId ||
        value->definitionInstructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        value->definitionInstructionId > cs->preSemanticIr.instructions.length)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    load = ZrParser_SemanticIr_InstructionAt(
            &cs->preSemanticIr, value->definitionInstructionId - 1U);
    if (load == ZR_NULL || load->opcode != ZR_SEMANTIC_IR_LOAD ||
        load->typeId != typeId || load->placeId != sourcePlaceId ||
        load->resultValueId != resultValueId || load->symbolId != source->symbolId)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    if (source->loanId != ZR_SEMANTIC_LOAN_ID_INVALID)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_NOT_APPLICABLE;

    existing = compiler_semantic_ir_find_slot(cs, resultStackSlot);
    if (existing != ZR_NULL) {
        place = ZrParser_PlaceGraph_Get(&cs->preSemanticIr.places, existing->placeId);
        if (place == ZR_NULL) return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
        if (place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY) {
            if (place->base.kind != ZR_PARSER_PLACE_BASE_LOCAL ||
                sourceStackSlot != resultStackSlot ||
                existing->placeId != sourcePlaceId || existing->typeId != typeId)
                return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
            existing->valueId = resultValueId;
            return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_BOUND;
        }
    }

    return bind_fresh_temporary(cs, resultStackSlot, typeId, resultValueId, sourceRange);
}

EZrCompilerSemanticScalarResult compiler_semantic_scalar_result_transfer(
        SZrCompilerState *cs, TZrUInt32 sourceStackSlot,
        TZrUInt32 resultStackSlot, SZrFileRange sourceRange) {
    const SZrCompilerSemanticIrSlot *source;
    const SZrSemanticIrValue *value;
    const SZrSemanticIrInstruction *definition;
    TZrTypeId typeId;
    TZrValueId valueId;
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        !cs->preSemanticIrInitialized || cs->preSemanticIrCfgTerminated ||
        sourceStackSlot == ZR_PARSER_SLOT_NONE || resultStackSlot == ZR_PARSER_SLOT_NONE)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    source = compiler_semantic_ir_find_slot(cs, sourceStackSlot);
    if (source == ZR_NULL || source->loanId != ZR_SEMANTIC_LOAN_ID_INVALID ||
        !canonical_int64(cs, source->typeId))
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_NOT_APPLICABLE;
    typeId = source->typeId;
    valueId = source->valueId;
    value = ZrParser_SemanticIr_Value(&cs->preSemanticIr, valueId);
    if (value == ZR_NULL || value->typeId != typeId || value->id != valueId)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    if (value->definitionInstructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_NOT_APPLICABLE;
    if (value->definitionInstructionId > cs->preSemanticIr.instructions.length)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    definition = ZrParser_SemanticIr_InstructionAt(
            &cs->preSemanticIr, value->definitionInstructionId - 1U);
    if (definition == ZR_NULL || definition->resultValueId != valueId ||
        definition->typeId != typeId)
        return ZR_COMPILER_SEMANTIC_SCALAR_RESULT_FAILED;
    return bind_fresh_temporary(cs, resultStackSlot, typeId, valueId, sourceRange);
}
