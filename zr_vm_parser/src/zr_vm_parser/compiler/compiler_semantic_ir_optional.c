#include "compiler_internal.h"

TZrBool compiler_semantic_ir_lower_optional_field_read(
        SZrCompilerState *cs,
        SZrAstNode *primaryNode,
        SZrAstNode *memberNode,
        const SZrTypeMemberInfo *memberInfo,
        TZrPlaceId receiverPlaceId,
        TZrUInt32 receiverSlot,
        TZrUInt32 resultSlot) {
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticSymbolRecord *symbol;
    const SZrSemanticExpressionFact *result;
    const SZrParserPlace *receiverPlace;
    const SZrCanonicalTypeNode *valueType;
    SZrInferredType declaredType;
    SZrInferredType presentType;
    SZrParserPlaceProjection projection;
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId presentTypeId;
    TZrPlaceId fieldPlaceId;
    TZrValueId fieldValueId;
    TZrBool declaredTypeMatches;

    if (cs == ZR_NULL || !cs->preSemanticIrCfgActive) {
        return ZR_FALSE;
    }
    if (primaryNode == ZR_NULL || memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberInfo == ZR_NULL || memberInfo->isStatic ||
        memberInfo->memberType != ZR_AST_CLASS_FIELD ||
        memberInfo->fieldType == ZR_NULL ||
        receiverPlaceId == ZR_PLACE_ID_INVALID ||
        receiverSlot == ZR_PARSER_SLOT_NONE ||
        resultSlot == ZR_PARSER_SLOT_NONE ||
        compiler_semantic_ir_slot_value(cs, receiverSlot) ==
                ZR_VALUE_ID_INVALID) {
        return compiler_semantic_cfg_abandon(cs);
    }
    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            cs->semanticContext,
            memberNode->data.memberExpression.property,
            ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS);
    result = ZrParser_SemanticFacts_FindExpressionByNode(
            cs->semanticContext, primaryNode);
    receiverPlace = ZrParser_PlaceGraph_Get(
            &cs->preSemanticIr.places, receiverPlaceId);
    symbol = reference != ZR_NULL && reference->isResolved
            ? ZrParser_Semantic_FindSymbolById(
                      cs->semanticContext, reference->symbolId)
            : ZR_NULL;
    if (reference == ZR_NULL || !reference->isResolved ||
        reference->symbolId == ZR_SEMANTIC_ID_INVALID ||
        reference->typeId == ZR_SEMANTIC_ID_INVALID ||
        symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FIELD ||
        symbol->astNode != memberInfo->declarationNode ||
        reference->symbolId != memberInfo->symbolId ||
        result == ZR_NULL || !result->inferredType.isNullable ||
        receiverPlace == ZR_NULL ||
        (receiverPlace->base.kind != ZR_PARSER_PLACE_BASE_LOCAL &&
         receiverPlace->base.kind != ZR_PARSER_PLACE_BASE_PARAMETER)) {
        return compiler_semantic_cfg_abandon(cs);
    }
    presentType = result->inferredType;
    presentType.isNullable = ZR_FALSE;
    if (!ZrParser_AstTypeToInferredType_Convert(
                cs, memberInfo->fieldType, &declaredType)) {
        return cs->hasError ? ZR_FALSE : compiler_semantic_cfg_abandon(cs);
    }
    declaredTypeMatches = (TZrBool)(
            !declaredType.isNullable &&
            ZrParser_InferredType_Equal(&declaredType, &presentType));
    ZrParser_InferredType_Free(cs->state, &declaredType);
    if (!declaredTypeMatches) {
        return compiler_semantic_cfg_abandon(cs);
    }
    presentTypeId = ZrParser_Semantic_RegisterInferredType(
            cs->semanticContext, &presentType,
            ZR_SEMANTIC_TYPE_KIND_UNKNOWN, ZR_NULL, ZR_NULL);
    valueType = ZrParser_CanonicalType_Find(
            cs->semanticContext, presentTypeId);
    if (presentTypeId == ZR_SEMANTIC_ID_INVALID || valueType == ZR_NULL ||
        valueType->kind != ZR_CANONICAL_TYPE_PRIMITIVE) {
        return compiler_semantic_cfg_abandon(cs);
    }

    memset(&projection, 0, sizeof(projection));
    projection.kind = ZR_PARSER_PLACE_PROJECTION_FIELD;
    projection.data.symbolId = reference->symbolId;
    fieldPlaceId = ZrParser_PlaceGraph_Project(
            &cs->preSemanticIr.places, receiverPlaceId, &projection,
            presentTypeId, memberNode->location);
    if (fieldPlaceId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_PROJECT;
    spec.typeId = presentTypeId;
    spec.placeId = fieldPlaceId;
    spec.symbolId = reference->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = memberNode->location;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }

    fieldValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, presentTypeId, memberNode->location);
    if (fieldValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_LOAD;
    spec.typeId = presentTypeId;
    spec.placeId = fieldPlaceId;
    spec.resultValueId = fieldValueId;
    spec.symbolId = reference->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = memberNode->location;
    return (TZrBool)(compiler_semantic_ir_emit(cs, &spec) &&
            compiler_semantic_ir_bind_result_value(
                    cs, resultSlot, presentTypeId, fieldValueId,
                    memberNode->location));
}

TZrBool compiler_semantic_ir_wake_optional_receiver(
        SZrCompilerState *cs,
        TZrUInt32 sourceSlot,
        TZrUInt32 wakeSlot,
        const SZrInferredType *guardedType,
        SZrFileRange sourceRange) {
    const SZrCompilerSemanticIrSlot *source;
    SZrInferredType wakeType;
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId wakeTypeId;
    TZrValueId wakeValueId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        guardedType == ZR_NULL || wakeSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    source = compiler_semantic_ir_find_slot(cs, sourceSlot);
    if (source == ZR_NULL || source->valueId == ZR_VALUE_ID_INVALID ||
        source->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    wakeType = *guardedType;
    wakeType.isNullable = ZR_TRUE;
    wakeTypeId = ZrParser_Semantic_RegisterInferredType(
            cs->semanticContext, &wakeType, ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
            ZR_NULL, ZR_NULL);
    if (wakeTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    wakeValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, wakeTypeId, sourceRange);
    if (wakeValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
    spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_WAKE;
    spec.typeId = wakeTypeId;
    spec.placeId = source->placeId;
    spec.valueId = source->valueId;
    spec.resultValueId = wakeValueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return (TZrBool)(compiler_semantic_ir_emit(cs, &spec) &&
            compiler_semantic_ir_bind_result_value(
                    cs, wakeSlot, wakeTypeId, wakeValueId, sourceRange));
}

TZrBool compiler_semantic_ir_drop_optional_receiver(
        SZrCompilerState *cs,
        TZrUInt32 wakeSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot;
    SZrSemanticIrInstructionSpec spec;

    slot = compiler_semantic_ir_find_slot(cs, wakeSlot);
    if (slot == ZR_NULL || slot->placeId == ZR_PLACE_ID_INVALID ||
        slot->valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_DROP;
    spec.typeId = slot->typeId;
    spec.placeId = slot->placeId;
    spec.valueId = slot->valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    slot->valueId = ZR_VALUE_ID_INVALID;
    return ZR_TRUE;
}

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
