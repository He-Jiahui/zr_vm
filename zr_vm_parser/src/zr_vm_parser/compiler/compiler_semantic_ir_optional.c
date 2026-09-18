#include "compiler_internal.h"

TZrBool compiler_semantic_ir_prepare_optional_merge(
        SZrCompilerState *cs,
        TZrUInt32 mergeSlot,
        const SZrInferredType *resultType,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot slot;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId typeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        !cs->preSemanticIrInitialized || mergeSlot == ZR_PARSER_SLOT_NONE ||
        resultType == ZR_NULL ||
        compiler_semantic_ir_find_slot(cs, mergeSlot) != ZR_NULL) {
        return ZR_FALSE;
    }
    typeId = ZrParser_Semantic_RegisterInferredType(
            cs->semanticContext,
            resultType,
            ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
            ZR_NULL,
            ZR_NULL);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&slot, 0, sizeof(slot));
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_TEMPORARY;
    base.identity = mergeSlot;
    slot.stackSlot = mergeSlot;
    slot.typeId = typeId;
    slot.valueId = ZR_VALUE_ID_INVALID;
    slot.placeId = ZrParser_PlaceGraph_AddBase(
            &cs->preSemanticIr.places, &base, typeId, sourceRange);
    if (slot.placeId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    spec.typeId = typeId;
    spec.placeId = slot.placeId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

TZrBool compiler_semantic_ir_store_optional_present(
        SZrCompilerState *cs,
        TZrUInt32 mergeSlot,
        TZrUInt32 sourceSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *destination;
    const SZrCompilerSemanticIrSlot *source;
    SZrSemanticIrInstructionSpec spec;
    TZrValueId convertedValueId;

    destination = compiler_semantic_ir_find_slot(cs, mergeSlot);
    source = compiler_semantic_ir_find_slot(cs, sourceSlot);
    if (destination == ZR_NULL || source == ZR_NULL ||
        destination->placeId == ZR_PLACE_ID_INVALID ||
        destination->typeId == ZR_SEMANTIC_ID_INVALID ||
        source->valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    convertedValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, destination->typeId, sourceRange);
    if (convertedValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_CONVERT;
    spec.typeId = destination->typeId;
    spec.valueId = source->valueId;
    spec.resultValueId = convertedValueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_STORE;
    spec.typeId = destination->typeId;
    spec.placeId = destination->placeId;
    spec.valueId = convertedValueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    destination->valueId = convertedValueId;
    return ZR_TRUE;
}

TZrBool compiler_semantic_ir_store_optional_absent(
        SZrCompilerState *cs,
        TZrUInt32 mergeSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *destination;
    SZrSemanticIrInstructionSpec spec;
    TZrUInt32 constantPoolIndex;
    TZrValueId nullValueId;

    destination = compiler_semantic_ir_find_slot(cs, mergeSlot);
    if (destination == ZR_NULL ||
        destination->placeId == ZR_PLACE_ID_INVALID ||
        destination->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    constantPoolIndex = compiler_get_cached_null_constant_index(cs);
    nullValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, destination->typeId, sourceRange);
    if (constantPoolIndex == ZR_PARSER_INDEX_NONE ||
        nullValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_CONSTANT;
    spec.typeId = destination->typeId;
    spec.resultValueId = nullValueId;
    spec.constantPoolIndex = constantPoolIndex;
    spec.hasConstantPoolIndex = ZR_TRUE;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_STORE;
    spec.typeId = destination->typeId;
    spec.placeId = destination->placeId;
    spec.valueId = nullValueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    destination->valueId = nullValueId;
    return ZR_TRUE;
}

TZrBool compiler_semantic_ir_load_optional_merge(
        SZrCompilerState *cs,
        TZrUInt32 mergeSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *merge;
    SZrSemanticIrInstructionSpec spec;
    TZrValueId resultValueId;

    merge = compiler_semantic_ir_find_slot(cs, mergeSlot);
    if (merge == ZR_NULL || merge->placeId == ZR_PLACE_ID_INVALID ||
        merge->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    resultValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, merge->typeId, sourceRange);
    if (resultValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_LOAD;
    spec.typeId = merge->typeId;
    spec.placeId = merge->placeId;
    spec.resultValueId = resultValueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    merge->valueId = resultValueId;
    return ZR_TRUE;
}
