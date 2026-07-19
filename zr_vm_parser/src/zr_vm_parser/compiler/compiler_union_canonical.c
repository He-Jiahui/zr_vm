#include "compiler_union_canonical.h"

#include "compiler_internal.h"

static TZrBool compiler_union_build_canonical_payload_type(
        SZrCompilerState *cs,
        const SZrAstNodeArray *fields,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount,
        TZrTypeId *outTypeId) {
    SZrArray fieldTypeIds;
    TZrTypeId payloadTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrSize fieldCount = fields != ZR_NULL ? fields->count : 0U;
    TZrSize index;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || outTypeId == ZR_NULL) {
        return ZR_FALSE;
    }
    *outTypeId = ZR_SEMANTIC_ID_INVALID;
    ZrCore_Array_Init(
            cs->state,
            &fieldTypeIds,
            sizeof(TZrTypeId),
            fieldCount > 0U ? fieldCount : ZR_PARSER_INITIAL_CAPACITY_TINY);

    for (index = 0; index < fieldCount; index++) {
        SZrAstNode *fieldNode = fields->nodes[index];
        SZrInferredType inferredType;
        TZrTypeId fieldTypeId;

        if (fieldNode == ZR_NULL ||
            fieldNode->type != ZR_AST_PARAMETER ||
            fieldNode->data.parameter.typeInfo == ZR_NULL ||
            !ZrParser_AstTypeToInferredType_Convert(
                    cs,
                    fieldNode->data.parameter.typeInfo,
                    &inferredType)) {
            ZrCore_Array_Free(cs->state, &fieldTypeIds);
            return ZR_FALSE;
        }
        fieldTypeId = ZrParser_CanonicalType_FromInferredWithGenericBindings(
                cs->semanticContext,
                &inferredType,
                genericBindings,
                genericBindingCount);
        ZrParser_InferredType_Free(cs->state, &inferredType);
        if (fieldTypeId == ZR_SEMANTIC_ID_INVALID) {
            ZrCore_Array_Free(cs->state, &fieldTypeIds);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, &fieldTypeIds, &fieldTypeId);
    }

    if (fieldTypeIds.length == 1U) {
        const TZrTypeId *onlyTypeId = (const TZrTypeId *)ZrCore_Array_Get(&fieldTypeIds, 0U);
        if (onlyTypeId != ZR_NULL) {
            payloadTypeId = *onlyTypeId;
        }
    } else {
        payloadTypeId = ZrParser_CanonicalType_InternTuple(
                cs->semanticContext,
                (const TZrTypeId *)fieldTypeIds.head,
                fieldTypeIds.length);
    }
    ZrCore_Array_Free(cs->state, &fieldTypeIds);
    if (payloadTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    *outTypeId = payloadTypeId;
    return ZR_TRUE;
}

TZrBool compiler_union_register_canonical_type(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const SZrTypePrototypeInfo *prototype) {
    SZrArray genericBindings;
    SZrArray genericParameterKinds;
    SZrArray variantTypeIds;
    TZrTypeId definitionTypeId;
    TZrTypeId unionTypeId;
    TZrSymbolId symbolId;
    EZrCanonicalGcScanKind gcScanKind;
    TZrUInt32 capabilityFlags = ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE;
    TZrSize index;

    if (cs == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_UNION_DECLARATION ||
        prototype == ZR_NULL ||
        prototype->name == ZR_NULL ||
        cs->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cs->typeEnv == ZR_NULL ||
        ZrParser_TypeEnvironment_LookupType(cs->typeEnv, prototype->name) ||
        ZrParser_Semantic_FindSymbolByNameAndKind(
                cs->semanticContext,
                prototype->name,
                ZR_SEMANTIC_SYMBOL_KIND_TYPE) != ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < prototype->genericParameters.length; index++) {
        const SZrTypeGenericParameterInfo *parameter =
                (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->genericParameters,
                        index);
        if (parameter == ZR_NULL ||
            parameter->name == ZR_NULL ||
            (parameter->genericKind != ZR_GENERIC_PARAMETER_TYPE &&
             parameter->genericKind != ZR_GENERIC_PARAMETER_CONST_INT)) {
            return ZR_FALSE;
        }
    }

    symbolId = ZrParser_Semantic_ReserveSymbolId(cs->semanticContext);
    definitionTypeId = ZrParser_CanonicalType_InternNominal(
            cs->semanticContext,
            ZR_NULL,
            prototype->name,
            0U);
    if (symbolId == ZR_SEMANTIC_ID_INVALID || definitionTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(
            cs->state,
            &genericBindings,
            sizeof(SZrCanonicalGenericBinding),
            prototype->genericParameters.length > 0U
                    ? prototype->genericParameters.length
                    : ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(
            cs->state,
            &genericParameterKinds,
            sizeof(EZrCanonicalGenericArgumentKind),
            prototype->genericParameters.length > 0U
                    ? prototype->genericParameters.length
                    : ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (index = 0; index < prototype->genericParameters.length; index++) {
        const SZrTypeGenericParameterInfo *parameter =
                (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->genericParameters,
                        index);
        SZrCanonicalGenericBinding binding;
        EZrCanonicalGenericArgumentKind parameterKind =
                parameter->genericKind == ZR_GENERIC_PARAMETER_TYPE
                        ? ZR_CANONICAL_GENERIC_ARGUMENT_TYPE
                        : ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT;

        ZrCore_Array_Push(cs->state, &genericParameterKinds, &parameterKind);
        binding.name = parameter->name;
        binding.kind = parameter->genericKind == ZR_GENERIC_PARAMETER_TYPE
                               ? ZR_CANONICAL_GENERIC_ARGUMENT_TYPE
                               : ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
        binding.ownerSymbolId = symbolId;
        binding.ordinal = (TZrUInt32)index;
        binding.typeId = ZR_SEMANTIC_ID_INVALID;
        if (parameter->genericKind == ZR_GENERIC_PARAMETER_TYPE) {
            binding.typeId = ZrParser_CanonicalType_InternGenericParameter(
                    cs->semanticContext,
                    symbolId,
                    (TZrUInt32)index);
            if (binding.typeId == ZR_SEMANTIC_ID_INVALID) {
                ZrCore_Array_Free(cs->state, &genericParameterKinds);
                ZrCore_Array_Free(cs->state, &genericBindings);
                return ZR_FALSE;
            }
        }
        ZrCore_Array_Push(cs->state, &genericBindings, &binding);
    }

    ZrCore_Array_Init(
            cs->state,
            &variantTypeIds,
            sizeof(TZrTypeId),
            node->data.unionDeclaration.variants != ZR_NULL &&
                            node->data.unionDeclaration.variants->count > 0U
                    ? node->data.unionDeclaration.variants->count
                    : ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (node->data.unionDeclaration.variants != ZR_NULL) {
        for (index = 0; index < node->data.unionDeclaration.variants->count; index++) {
            SZrAstNode *variantNode = node->data.unionDeclaration.variants->nodes[index];
            TZrTypeId payloadTypeId;

            if (variantNode == ZR_NULL || variantNode->type != ZR_AST_UNION_VARIANT ||
                !compiler_union_build_canonical_payload_type(
                        cs,
                        variantNode->data.unionVariant.fields,
                        (const SZrCanonicalGenericBinding *)genericBindings.head,
                        genericBindings.length,
                        &payloadTypeId)) {
                ZrCore_Array_Free(cs->state, &variantTypeIds);
                ZrCore_Array_Free(cs->state, &genericParameterKinds);
                ZrCore_Array_Free(cs->state, &genericBindings);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(cs->state, &variantTypeIds, &payloadTypeId);
        }
    }

    unionTypeId = ZrParser_CanonicalType_InternUnion(
            cs->semanticContext,
            definitionTypeId,
            (const TZrTypeId *)variantTypeIds.head,
            variantTypeIds.length);
    ZrCore_Array_Free(cs->state, &variantTypeIds);
    if (unionTypeId == ZR_SEMANTIC_ID_INVALID ||
        !ZrParser_CanonicalType_GetGcScanKind(
                cs->semanticContext,
                unionTypeId,
                &gcScanKind)) {
        ZrCore_Array_Free(cs->state, &genericParameterKinds);
        ZrCore_Array_Free(cs->state, &genericBindings);
        return ZR_FALSE;
    }
    if (gcScanKind != ZR_CANONICAL_GC_SCAN_FREE) {
        capabilityFlags |= ZR_CANONICAL_TYPE_CAPABILITY_HAS_GC_REFERENCES;
    }

    if (genericParameterKinds.length > 0U) {
        if (!ZrParser_CanonicalType_RegisterGenericDefinitionProjection(
                    cs->semanticContext,
                    definitionTypeId,
                    symbolId,
                    (const EZrCanonicalGenericArgumentKind *)genericParameterKinds.head,
                    genericParameterKinds.length,
                    capabilityFlags,
                    gcScanKind,
                    unionTypeId)) {
            ZrCore_Array_Free(cs->state, &genericParameterKinds);
            ZrCore_Array_Free(cs->state, &genericBindings);
            return ZR_FALSE;
        }
    } else if (!ZrParser_CanonicalType_RegisterDefinitionProjection(
                       cs->semanticContext,
                       definitionTypeId,
                       capabilityFlags,
                       gcScanKind,
                       unionTypeId)) {
        ZrCore_Array_Free(cs->state, &genericParameterKinds);
        ZrCore_Array_Free(cs->state, &genericBindings);
        return ZR_FALSE;
    }

    if (!ZrParser_Semantic_PublishCanonicalTypeSymbol(
                cs->semanticContext,
                unionTypeId,
                ZR_SEMANTIC_TYPE_KIND_UNION,
                prototype->name,
                node,
                symbolId,
                node->location)) {
        ZrCore_Array_Free(cs->state, &genericParameterKinds);
        ZrCore_Array_Free(cs->state, &genericBindings);
        return ZR_FALSE;
    }
    {
        SZrString *publishedName = prototype->name;
        ZrCore_Array_Push(cs->state, &cs->typeEnv->typeNames, &publishedName);
    }

    ZrCore_Array_Free(cs->state, &genericParameterKinds);
    ZrCore_Array_Free(cs->state, &genericBindings);
    return ZR_TRUE;
}
