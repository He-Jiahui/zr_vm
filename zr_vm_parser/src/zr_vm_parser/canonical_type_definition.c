#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include "canonical_type_definition_internal.h"

#include "zr_vm_core/array.h"

typedef struct SZrCanonicalConstructorRecord {
    TZrSymbolId symbolId;
    SZrArray parameterTypeIds; // TZrTypeId
    TZrBool isPublic;
} SZrCanonicalConstructorRecord;

static const TZrUInt32 ZR_CANONICAL_TYPE_CAPABILITY_ALL =
        ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
        ZR_CANONICAL_TYPE_CAPABILITY_GC_CLASS |
        ZR_CANONICAL_TYPE_CAPABILITY_RESOURCE_CLASS |
        ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE |
        ZR_CANONICAL_TYPE_CAPABILITY_READONLY_TYPE |
        ZR_CANONICAL_TYPE_CAPABILITY_REF_LIKE |
        ZR_CANONICAL_TYPE_CAPABILITY_HAS_DROP |
        ZR_CANONICAL_TYPE_CAPABILITY_HAS_GC_REFERENCES |
        ZR_CANONICAL_TYPE_CAPABILITY_HAS_OWNERSHIP_FIELDS |
        ZR_CANONICAL_TYPE_CAPABILITY_BLITTABLE |
        ZR_CANONICAL_TYPE_CAPABILITY_SEND |
        ZR_CANONICAL_TYPE_CAPABILITY_SYNC;

SZrCanonicalTypeDefinitionRecord *ZrParser_CanonicalTypeDefinition_FindRecord(
        const SZrSemanticContext *context,
        TZrTypeId typeId) {
    TZrSize index;

    if (context == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < context->canonicalTypeDefinitions.length; index++) {
        SZrCanonicalTypeDefinitionRecord *definition =
                (SZrCanonicalTypeDefinitionRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->canonicalTypeDefinitions,
                        index);
        if (definition != ZR_NULL && definition->typeId == typeId) {
            return definition;
        }
    }
    return ZR_NULL;
}

static TZrBool canonical_type_ids_are_valid(
        const SZrSemanticContext *context,
        const TZrTypeId *typeIds,
        TZrSize typeCount) {
    TZrSize index;

    if (typeCount > 0 && typeIds == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < typeCount; index++) {
        if (ZrParser_CanonicalType_Find(context, typeIds[index]) == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

const SZrCanonicalTypeDefinitionRecord *ZrParser_CanonicalTypeDefinition_ResolveRecord(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrCanonicalTypeNode **outInstantiation) {
    const SZrCanonicalTypeNode *typeNode = ZrParser_CanonicalType_Find(context, typeId);
    const SZrCanonicalTypeDefinitionRecord *definition;

    if (outInstantiation != ZR_NULL) {
        *outInstantiation = ZR_NULL;
    }
    if (typeNode == ZR_NULL) {
        return ZR_NULL;
    }
    if (typeNode->kind == ZR_CANONICAL_TYPE_UNION) {
        typeId = typeNode->data.unionType.definitionTypeId;
        typeNode = ZrParser_CanonicalType_Find(context, typeId);
        if (typeNode == ZR_NULL) {
            return ZR_NULL;
        }
    }
    if (typeNode->kind != ZR_CANONICAL_TYPE_GENERIC_INSTANCE) {
        return ZrParser_CanonicalTypeDefinition_FindRecord(context, typeId);
    }

    definition = ZrParser_CanonicalTypeDefinition_FindRecord(
            context,
            typeNode->data.genericInstance.definitionTypeId);
    if (definition == ZR_NULL ||
        definition->genericOwnerSymbolId == ZR_SEMANTIC_ID_INVALID ||
        definition->genericParameterCount != typeNode->data.genericInstance.arguments.length) {
        return ZR_NULL;
    }
    {
        TZrSize index;
        for (index = 0; index < definition->genericParameterCount; index++) {
            const EZrCanonicalGenericArgumentKind *parameterKind =
                    (const EZrCanonicalGenericArgumentKind *)ZrCore_Array_Get(
                            (SZrArray *)&definition->genericParameterKinds,
                            index);
            const SZrCanonicalGenericArgument *argument =
                    (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                            (SZrArray *)&typeNode->data.genericInstance.arguments,
                            index);
            if (parameterKind == ZR_NULL || argument == ZR_NULL ||
                (*parameterKind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE
                         ? argument->kind != ZR_CANONICAL_GENERIC_ARGUMENT_TYPE
                         : *parameterKind != ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT ||
                                   (argument->kind != ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT &&
                                    argument->kind != ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER))) {
                return ZR_NULL;
            }
        }
    }
    if (outInstantiation != ZR_NULL) {
        *outInstantiation = typeNode;
    }
    return definition;
}

void ZrParser_CanonicalTypeDefinition_Init(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    ZrCore_Array_Init(
            context->state,
            &context->canonicalTypeDefinitions,
            sizeof(SZrCanonicalTypeDefinitionRecord),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
}

static TZrBool canonical_type_register_definition(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId genericOwnerSymbolId,
        const EZrCanonicalGenericArgumentKind *genericParameterKinds,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind,
        TZrTypeId projectionTypeId) {
    const SZrCanonicalTypeNode *typeNode;
    SZrCanonicalTypeDefinitionRecord *existing;
    SZrCanonicalTypeDefinitionRecord definition;

    if (context == ZR_NULL ||
        (genericParameterCount == 0U) !=
                (genericOwnerSymbolId == ZR_SEMANTIC_ID_INVALID) ||
        (genericParameterCount > 0U && genericParameterKinds == ZR_NULL) ||
        (TZrInt32)gcScanKind < (TZrInt32)ZR_CANONICAL_GC_SCAN_FREE ||
        gcScanKind > ZR_CANONICAL_GC_SCAN_BARRIERED ||
        (capabilityFlags & ~ZR_CANONICAL_TYPE_CAPABILITY_ALL) != 0U) {
        return ZR_FALSE;
    }
    typeNode = ZrParser_CanonicalType_Find(context, typeId);
    if (typeNode == ZR_NULL || typeNode->kind != ZR_CANONICAL_TYPE_NOMINAL) {
        return ZR_FALSE;
    }
    if (projectionTypeId != ZR_SEMANTIC_ID_INVALID) {
        const SZrCanonicalTypeNode *projection =
                ZrParser_CanonicalType_Find(context, projectionTypeId);
        if (projection == ZR_NULL ||
            projection->kind != ZR_CANONICAL_TYPE_UNION ||
            projection->data.unionType.definitionTypeId != typeId) {
            return ZR_FALSE;
        }
    }
    {
        TZrSize index;
        for (index = 0; index < genericParameterCount; index++) {
            if ((TZrInt32)genericParameterKinds[index] <
                        (TZrInt32)ZR_CANONICAL_GENERIC_ARGUMENT_TYPE ||
                genericParameterKinds[index] > ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT) {
                return ZR_FALSE;
            }
        }
    }

    existing = ZrParser_CanonicalTypeDefinition_FindRecord(context, typeId);
    if (existing != ZR_NULL) {
        if (existing->genericOwnerSymbolId != genericOwnerSymbolId ||
            existing->genericParameterCount != genericParameterCount) {
            return ZR_FALSE;
        }
        {
            TZrSize index;
            for (index = 0; index < genericParameterCount; index++) {
                const EZrCanonicalGenericArgumentKind *existingKind =
                        (const EZrCanonicalGenericArgumentKind *)ZrCore_Array_Get(
                                &existing->genericParameterKinds,
                                index);
                if (existingKind == ZR_NULL || *existingKind != genericParameterKinds[index]) {
                    return ZR_FALSE;
                }
            }
        }
        if (projectionTypeId != ZR_SEMANTIC_ID_INVALID &&
            existing->projectionTypeId != ZR_SEMANTIC_ID_INVALID &&
            existing->projectionTypeId != projectionTypeId) {
            return ZR_FALSE;
        }
        existing->capabilityFlags = capabilityFlags;
        existing->gcScanKind = gcScanKind;
        if (projectionTypeId != ZR_SEMANTIC_ID_INVALID) {
            existing->projectionTypeId = projectionTypeId;
        }
        return ZR_TRUE;
    }

    definition.typeId = typeId;
    definition.genericOwnerSymbolId = genericOwnerSymbolId;
    definition.genericParameterCount = genericParameterCount;
    ZrCore_Array_Init(
            context->state,
            &definition.genericParameterKinds,
            sizeof(EZrCanonicalGenericArgumentKind),
            genericParameterCount > 0U ? genericParameterCount : ZR_PARSER_INITIAL_CAPACITY_TINY);
    {
        TZrSize index;
        for (index = 0; index < genericParameterCount; index++) {
            EZrCanonicalGenericArgumentKind kind = genericParameterKinds[index];
            ZrCore_Array_Push(context->state, &definition.genericParameterKinds, &kind);
        }
    }
    definition.capabilityFlags = capabilityFlags;
    definition.gcScanKind = gcScanKind;
    definition.projectionTypeId = projectionTypeId;
    ZrCore_Array_Init(
            context->state,
            &definition.constructors,
            sizeof(SZrCanonicalConstructorRecord),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Push(context->state, &context->canonicalTypeDefinitions, &definition);
    return ZR_TRUE;
}

TZrBool ZrParser_CanonicalType_RegisterDefinition(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind) {
    return canonical_type_register_definition(
            context,
            typeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            0U,
            capabilityFlags,
            gcScanKind,
            ZR_SEMANTIC_ID_INVALID);
}

TZrBool ZrParser_CanonicalType_RegisterGenericDefinition(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind) {
    SZrArray kinds;
    TZrBool result;
    TZrSize index;

    if (context == ZR_NULL ||
        ownerSymbolId == ZR_SEMANTIC_ID_INVALID ||
        genericParameterCount == 0U) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(
            context->state,
            &kinds,
            sizeof(EZrCanonicalGenericArgumentKind),
            genericParameterCount);
    for (index = 0; index < genericParameterCount; index++) {
        EZrCanonicalGenericArgumentKind kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
        ZrCore_Array_Push(context->state, &kinds, &kind);
    }
    result = canonical_type_register_definition(
            context,
            typeId,
            ownerSymbolId,
            (const EZrCanonicalGenericArgumentKind *)kinds.head,
            genericParameterCount,
            capabilityFlags,
            gcScanKind,
            ZR_SEMANTIC_ID_INVALID);
    ZrCore_Array_Free(context->state, &kinds);
    return result;
}

TZrBool ZrParser_CanonicalType_RegisterGenericDefinitionEx(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        const EZrCanonicalGenericArgumentKind *parameterKinds,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind) {
    return canonical_type_register_definition(
            context,
            typeId,
            ownerSymbolId,
            parameterKinds,
            genericParameterCount,
            capabilityFlags,
            gcScanKind,
            ZR_SEMANTIC_ID_INVALID);
}

TZrBool ZrParser_CanonicalType_RegisterDefinitionProjection(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind,
        TZrTypeId projectionTypeId) {
    return canonical_type_register_definition(
            context,
            typeId,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            0U,
            capabilityFlags,
            gcScanKind,
            projectionTypeId);
}

TZrBool ZrParser_CanonicalType_RegisterGenericDefinitionProjection(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        const EZrCanonicalGenericArgumentKind *parameterKinds,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind,
        TZrTypeId projectionTypeId) {
    return canonical_type_register_definition(
            context,
            typeId,
            ownerSymbolId,
            parameterKinds,
            genericParameterCount,
            capabilityFlags,
            gcScanKind,
            projectionTypeId);
}

TZrBool ZrParser_CanonicalType_RegisterConstructor(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId constructorSymbolId,
        const TZrTypeId *parameterTypeIds,
        TZrSize parameterCount,
        TZrBool isPublic) {
    SZrCanonicalTypeDefinitionRecord *definition =
            ZrParser_CanonicalTypeDefinition_FindRecord(context, typeId);
    SZrCanonicalConstructorRecord constructor;
    TZrSize index;

    if (definition == ZR_NULL ||
        constructorSymbolId == ZR_SEMANTIC_ID_INVALID ||
        (definition->capabilityFlags & ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE) == 0U ||
        !canonical_type_ids_are_valid(context, parameterTypeIds, parameterCount)) {
        return ZR_FALSE;
    }

    constructor.symbolId = constructorSymbolId;
    constructor.isPublic = isPublic;
    ZrCore_Array_Init(
            context->state,
            &constructor.parameterTypeIds,
            sizeof(TZrTypeId),
            parameterCount > 0 ? parameterCount : ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (index = 0; index < parameterCount; index++) {
        TZrTypeId parameterTypeId = parameterTypeIds[index];
        ZrCore_Array_Push(context->state, &constructor.parameterTypeIds, &parameterTypeId);
    }
    ZrCore_Array_Push(context->state, &definition->constructors, &constructor);
    return ZR_TRUE;
}

static TZrBool canonical_type_pattern_matches(
        const SZrSemanticContext *context,
        TZrTypeId patternTypeId,
        TZrTypeId actualTypeId,
        const SZrCanonicalTypeDefinitionRecord *definition,
        const SZrCanonicalTypeNode *instantiation,
        TZrSize depth);

static TZrBool canonical_type_pattern_list_matches(
        const SZrSemanticContext *context,
        const SZrArray *patterns,
        const SZrArray *actuals,
        const SZrCanonicalTypeDefinitionRecord *definition,
        const SZrCanonicalTypeNode *instantiation,
        TZrSize depth) {
    TZrSize index;

    if (patterns == ZR_NULL || actuals == ZR_NULL || patterns->length != actuals->length) {
        return ZR_FALSE;
    }
    for (index = 0; index < patterns->length; index++) {
        const TZrTypeId *pattern = (const TZrTypeId *)ZrCore_Array_Get((SZrArray *)patterns, index);
        const TZrTypeId *actual = (const TZrTypeId *)ZrCore_Array_Get((SZrArray *)actuals, index);
        if (pattern == ZR_NULL || actual == ZR_NULL ||
            !canonical_type_pattern_matches(
                    context,
                    *pattern,
                    *actual,
                    definition,
                    instantiation,
                    depth + 1U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_generic_argument_pattern_list_matches(
        const SZrSemanticContext *context,
        const SZrArray *patterns,
        const SZrArray *actuals,
        const SZrCanonicalTypeDefinitionRecord *definition,
        const SZrCanonicalTypeNode *instantiation,
        TZrSize depth) {
    TZrSize index;

    if (patterns == ZR_NULL || actuals == ZR_NULL || patterns->length != actuals->length) {
        return ZR_FALSE;
    }
    for (index = 0; index < patterns->length; index++) {
        const SZrCanonicalGenericArgument *pattern =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get((SZrArray *)patterns, index);
        const SZrCanonicalGenericArgument *actual =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get((SZrArray *)actuals, index);
        const SZrCanonicalGenericArgument *resolvedPattern = pattern;

        if (pattern != ZR_NULL &&
            pattern->kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER &&
            definition != ZR_NULL && instantiation != ZR_NULL &&
            pattern->data.constParameter.ownerSymbolId == definition->genericOwnerSymbolId &&
            pattern->data.constParameter.ordinal <
                    instantiation->data.genericInstance.arguments.length) {
            resolvedPattern = (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                    (SZrArray *)&instantiation->data.genericInstance.arguments,
                    pattern->data.constParameter.ordinal);
        }
        if (resolvedPattern == ZR_NULL || actual == ZR_NULL ||
            resolvedPattern->kind != actual->kind) {
            return ZR_FALSE;
        }
        if (resolvedPattern->kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT) {
            if (resolvedPattern->data.constIntValue != actual->data.constIntValue) {
                return ZR_FALSE;
            }
        } else if (resolvedPattern->kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER) {
            if (resolvedPattern->data.constParameter.ownerSymbolId !=
                        actual->data.constParameter.ownerSymbolId ||
                resolvedPattern->data.constParameter.ordinal !=
                        actual->data.constParameter.ordinal) {
                return ZR_FALSE;
            }
        } else if (resolvedPattern->kind != ZR_CANONICAL_GENERIC_ARGUMENT_TYPE ||
                   !canonical_type_pattern_matches(
                           context,
                           resolvedPattern->data.typeId,
                           actual->data.typeId,
                           definition,
                           instantiation,
                           depth + 1U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_function_pattern_matches(
        const SZrSemanticContext *context,
        const SZrCanonicalFunctionType *pattern,
        const SZrCanonicalFunctionType *actual,
        const SZrCanonicalTypeDefinitionRecord *definition,
        const SZrCanonicalTypeNode *instantiation,
        TZrSize depth) {
    TZrSize index;

    if (pattern->parameterContracts.length != actual->parameterContracts.length ||
        pattern->receiverEffect != actual->receiverEffect ||
        pattern->effectFlags != actual->effectFlags ||
        !canonical_type_pattern_matches(
                context,
                pattern->returnTypeId,
                actual->returnTypeId,
                definition,
                instantiation,
                depth + 1U)) {
        return ZR_FALSE;
    }
    for (index = 0; index < pattern->parameterContracts.length; index++) {
        const SZrCanonicalParameterContract *patternContract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&pattern->parameterContracts,
                        index);
        const SZrCanonicalParameterContract *actualContract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&actual->parameterContracts,
                        index);
        if (patternContract == ZR_NULL || actualContract == ZR_NULL ||
            patternContract->passingForm != actualContract->passingForm ||
            patternContract->escapeUpperBound != actualContract->escapeUpperBound ||
            patternContract->entryInitialization != actualContract->entryInitialization ||
            patternContract->exitInitialization != actualContract->exitInitialization ||
            patternContract->acceptsTemporary != actualContract->acceptsTemporary ||
            patternContract->callSiteMarker != actualContract->callSiteMarker ||
            !canonical_type_pattern_matches(
                    context,
                    patternContract->typeId,
                    actualContract->typeId,
                    definition,
                    instantiation,
                    depth + 1U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_type_pattern_matches(
        const SZrSemanticContext *context,
        TZrTypeId patternTypeId,
        TZrTypeId actualTypeId,
        const SZrCanonicalTypeDefinitionRecord *definition,
        const SZrCanonicalTypeNode *instantiation,
        TZrSize depth) {
    const SZrCanonicalTypeNode *pattern;
    const SZrCanonicalTypeNode *actual;

    if (depth > 256U) {
        return ZR_FALSE;
    }
    pattern = ZrParser_CanonicalType_Find(context, patternTypeId);
    actual = ZrParser_CanonicalType_Find(context, actualTypeId);
    if (pattern == ZR_NULL || actual == ZR_NULL) {
        return ZR_FALSE;
    }
    if (pattern->kind == ZR_CANONICAL_TYPE_GENERIC_PARAMETER &&
        instantiation != ZR_NULL &&
        pattern->data.genericParameter.ownerSymbolId == definition->genericOwnerSymbolId &&
        pattern->data.genericParameter.ordinal < instantiation->data.genericInstance.arguments.length) {
        const SZrCanonicalGenericArgument *argument =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                (SZrArray *)&instantiation->data.genericInstance.arguments,
                pattern->data.genericParameter.ordinal);
        return argument != ZR_NULL &&
               argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE &&
               argument->data.typeId == actualTypeId;
    }
    if (patternTypeId == actualTypeId) {
        return ZR_TRUE;
    }
    if (pattern->kind != actual->kind) {
        return ZR_FALSE;
    }

    switch (pattern->kind) {
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE:
            return pattern->data.genericInstance.definitionTypeId ==
                           actual->data.genericInstance.definitionTypeId &&
                   canonical_generic_argument_pattern_list_matches(
                           context,
                           &pattern->data.genericInstance.arguments,
                           &actual->data.genericInstance.arguments,
                           definition,
                           instantiation,
                           depth);
        case ZR_CANONICAL_TYPE_ARRAY:
            return pattern->data.array.rank == actual->data.array.rank &&
                   pattern->data.array.storageKind == actual->data.array.storageKind &&
                   canonical_type_pattern_matches(
                           context,
                           pattern->data.array.elementTypeId,
                           actual->data.array.elementTypeId,
                           definition,
                           instantiation,
                           depth + 1U);
        case ZR_CANONICAL_TYPE_TUPLE:
            return canonical_type_pattern_list_matches(
                    context,
                    &pattern->data.typeList.elementTypeIds,
                    &actual->data.typeList.elementTypeIds,
                    definition,
                    instantiation,
                    depth);
        case ZR_CANONICAL_TYPE_UNION:
            return (pattern->data.unionType.definitionTypeId ==
                            actual->data.unionType.definitionTypeId ||
                    (definition != ZR_NULL && instantiation != ZR_NULL &&
                     pattern->data.unionType.definitionTypeId == definition->typeId &&
                     actual->data.unionType.definitionTypeId == instantiation->id) ||
                    canonical_type_pattern_matches(
                            context,
                            pattern->data.unionType.definitionTypeId,
                            actual->data.unionType.definitionTypeId,
                            definition,
                            instantiation,
                            depth + 1U)) &&
                   canonical_type_pattern_list_matches(
                           context,
                           &pattern->data.unionType.variantTypeIds,
                           &actual->data.unionType.variantTypeIds,
                           definition,
                           instantiation,
                           depth);
        case ZR_CANONICAL_TYPE_REF:
            return pattern->data.refType.access == actual->data.refType.access &&
                   canonical_type_pattern_matches(
                           context,
                           pattern->data.refType.pointeeTypeId,
                           actual->data.refType.pointeeTypeId,
                           definition,
                           instantiation,
                           depth + 1U);
        case ZR_CANONICAL_TYPE_OWNER:
            return pattern->data.owner.ownerKind == actual->data.owner.ownerKind &&
                   canonical_type_pattern_matches(
                           context,
                           pattern->data.owner.targetTypeId,
                           actual->data.owner.targetTypeId,
                           definition,
                           instantiation,
                           depth + 1U);
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
        case ZR_CANONICAL_TYPE_NULLABLE:
            return canonical_type_pattern_matches(
                    context,
                    pattern->data.target.targetTypeId,
                    actual->data.target.targetTypeId,
                    definition,
                    instantiation,
                    depth + 1U);
        case ZR_CANONICAL_TYPE_FUNCTION:
            return canonical_function_pattern_matches(
                    context,
                    &pattern->data.function,
                    &actual->data.function,
                    definition,
                    instantiation,
                    depth);
        default:
            return ZR_FALSE;
    }
}

static TZrBool canonical_constructor_parameters_match(
        const SZrSemanticContext *context,
        const SZrArray *patterns,
        const TZrTypeId *actualTypeIds,
        TZrSize actualCount,
        const SZrCanonicalTypeDefinitionRecord *definition,
        const SZrCanonicalTypeNode *instantiation) {
    TZrSize index;

    if (patterns == ZR_NULL || patterns->length != actualCount) {
        return ZR_FALSE;
    }
    for (index = 0; index < actualCount; index++) {
        const TZrTypeId *pattern = (const TZrTypeId *)ZrCore_Array_Get((SZrArray *)patterns, index);
        if (pattern == ZR_NULL ||
            !canonical_type_pattern_matches(
                    context,
                    *pattern,
                    actualTypeIds[index],
                    definition,
                    instantiation,
                    0U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_CanonicalType_HasCapabilities(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 requiredCapabilityFlags) {
    const SZrCanonicalTypeDefinitionRecord *definition =
            ZrParser_CanonicalTypeDefinition_ResolveRecord(context, typeId, ZR_NULL);

    return definition != ZR_NULL &&
           (requiredCapabilityFlags & ~ZR_CANONICAL_TYPE_CAPABILITY_ALL) == 0U &&
           (definition->capabilityFlags & requiredCapabilityFlags) == requiredCapabilityFlags;
}

TZrBool ZrParser_CanonicalType_ResolveValueConstructor(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        const TZrTypeId *argumentTypeIds,
        TZrSize argumentCount,
        TZrSymbolId *outConstructorSymbolId) {
    const SZrCanonicalTypeNode *instantiation = ZR_NULL;
    const SZrCanonicalTypeDefinitionRecord *definition =
            ZrParser_CanonicalTypeDefinition_ResolveRecord(context, typeId, &instantiation);
    TZrSymbolId match = ZR_SEMANTIC_ID_INVALID;
    TZrSize index;

    if (outConstructorSymbolId != ZR_NULL) {
        *outConstructorSymbolId = ZR_SEMANTIC_ID_INVALID;
    }
    if (definition == ZR_NULL ||
        outConstructorSymbolId == ZR_NULL ||
        (definition->capabilityFlags & ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE) == 0U ||
        !canonical_type_ids_are_valid(context, argumentTypeIds, argumentCount)) {
        return ZR_FALSE;
    }

    for (index = 0; index < definition->constructors.length; index++) {
        const SZrCanonicalConstructorRecord *constructor =
                (const SZrCanonicalConstructorRecord *)ZrCore_Array_Get(
                        (SZrArray *)&definition->constructors,
                        index);
        if (constructor == ZR_NULL ||
            !constructor->isPublic ||
            !canonical_constructor_parameters_match(
                    context,
                    &constructor->parameterTypeIds,
                    argumentTypeIds,
                    argumentCount,
                    definition,
                    instantiation)) {
            continue;
        }
        if (match != ZR_SEMANTIC_ID_INVALID && match != constructor->symbolId) {
            return ZR_FALSE;
        }
        match = constructor->symbolId;
    }

    if (match == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    *outConstructorSymbolId = match;
    return ZR_TRUE;
}

void ZrParser_CanonicalTypeDefinition_Reset(SZrSemanticContext *context) {
    TZrSize definitionIndex;

    if (context == ZR_NULL) {
        return;
    }
    for (definitionIndex = 0;
         definitionIndex < context->canonicalTypeDefinitions.length;
         definitionIndex++) {
        SZrCanonicalTypeDefinitionRecord *definition =
                (SZrCanonicalTypeDefinitionRecord *)ZrCore_Array_Get(
                        &context->canonicalTypeDefinitions,
                        definitionIndex);
        TZrSize constructorIndex;

        if (definition == ZR_NULL) {
            continue;
        }
        for (constructorIndex = 0; constructorIndex < definition->constructors.length; constructorIndex++) {
            SZrCanonicalConstructorRecord *constructor =
                    (SZrCanonicalConstructorRecord *)ZrCore_Array_Get(
                            &definition->constructors,
                            constructorIndex);
            if (constructor != ZR_NULL) {
                ZrCore_Array_Free(context->state, &constructor->parameterTypeIds);
            }
        }
        ZrCore_Array_Free(context->state, &definition->constructors);
        ZrCore_Array_Free(context->state, &definition->genericParameterKinds);
    }
    context->canonicalTypeDefinitions.length = 0;
}

void ZrParser_CanonicalTypeDefinition_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    ZrParser_CanonicalTypeDefinition_Reset(context);
    ZrCore_Array_Free(context->state, &context->canonicalTypeDefinitions);
}
