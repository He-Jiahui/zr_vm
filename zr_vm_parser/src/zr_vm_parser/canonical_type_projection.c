#include "canonical_type_definition_internal.h"

static TZrTypeId canonical_type_substitute_projection(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrCanonicalTypeDefinitionRecord *definition,
        const SZrCanonicalTypeNode *instantiation,
        TZrSize depth) {
    const SZrCanonicalTypeNode *node;

    if (context == ZR_NULL || definition == ZR_NULL || depth > 256U) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    node = ZrParser_CanonicalType_Find(context, typeId);
    if (node == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    if (node->kind == ZR_CANONICAL_TYPE_GENERIC_PARAMETER &&
        instantiation != ZR_NULL &&
        node->data.genericParameter.ownerSymbolId == definition->genericOwnerSymbolId &&
        node->data.genericParameter.ordinal < instantiation->data.genericInstance.arguments.length) {
        const SZrCanonicalGenericArgument *argument =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                        (SZrArray *)&instantiation->data.genericInstance.arguments,
                        node->data.genericParameter.ordinal);
        return argument != ZR_NULL && argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE
                       ? argument->data.typeId
                       : ZR_SEMANTIC_ID_INVALID;
    }

    switch (node->kind) {
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE: {
            SZrArray arguments;
            TZrTypeId result;
            TZrSize index;

            ZrCore_Array_Init(
                    context->state,
                    &arguments,
                    sizeof(SZrCanonicalGenericArgument),
                    node->data.genericInstance.arguments.length);
            for (index = 0; index < node->data.genericInstance.arguments.length; index++) {
                const SZrCanonicalGenericArgument *source =
                        (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                                (SZrArray *)&node->data.genericInstance.arguments,
                                index);
                SZrCanonicalGenericArgument argument;

                if (source == ZR_NULL) {
                    ZrCore_Array_Free(context->state, &arguments);
                    return ZR_SEMANTIC_ID_INVALID;
                }
                argument = *source;
                if (argument.kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE) {
                    argument.data.typeId = canonical_type_substitute_projection(
                            context,
                            argument.data.typeId,
                            definition,
                            instantiation,
                            depth + 1U);
                    if (argument.data.typeId == ZR_SEMANTIC_ID_INVALID) {
                        ZrCore_Array_Free(context->state, &arguments);
                        return ZR_SEMANTIC_ID_INVALID;
                    }
                } else if (argument.kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER &&
                           instantiation != ZR_NULL &&
                           argument.data.constParameter.ownerSymbolId ==
                                   definition->genericOwnerSymbolId &&
                           argument.data.constParameter.ordinal <
                                   instantiation->data.genericInstance.arguments.length) {
                    const SZrCanonicalGenericArgument *boundArgument =
                            (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                                    (SZrArray *)&instantiation->data.genericInstance.arguments,
                                    argument.data.constParameter.ordinal);
                    if (boundArgument == ZR_NULL ||
                        (boundArgument->kind != ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT &&
                         boundArgument->kind != ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER)) {
                        ZrCore_Array_Free(context->state, &arguments);
                        return ZR_SEMANTIC_ID_INVALID;
                    }
                    argument = *boundArgument;
                }
                ZrCore_Array_Push(context->state, &arguments, &argument);
            }
            result = ZrParser_CanonicalType_InternGenericInstanceEx(
                    context,
                    node->data.genericInstance.definitionTypeId,
                    (const SZrCanonicalGenericArgument *)arguments.head,
                    arguments.length);
            ZrCore_Array_Free(context->state, &arguments);
            return result;
        }
        case ZR_CANONICAL_TYPE_ARRAY: {
            TZrTypeId elementTypeId = canonical_type_substitute_projection(
                    context,
                    node->data.array.elementTypeId,
                    definition,
                    instantiation,
                    depth + 1U);
            return ZrParser_CanonicalType_InternArray(
                    context,
                    elementTypeId,
                    node->data.array.rank,
                    node->data.array.storageKind);
        }
        case ZR_CANONICAL_TYPE_TUPLE:
        case ZR_CANONICAL_TYPE_UNION: {
            const SZrArray *sourceIds = node->kind == ZR_CANONICAL_TYPE_TUPLE
                                                ? &node->data.typeList.elementTypeIds
                                                : &node->data.unionType.variantTypeIds;
            SZrArray typeIds;
            TZrTypeId result;
            TZrSize index;

            ZrCore_Array_Init(
                    context->state,
                    &typeIds,
                    sizeof(TZrTypeId),
                    sourceIds->length > 0U ? sourceIds->length : ZR_PARSER_INITIAL_CAPACITY_TINY);
            for (index = 0; index < sourceIds->length; index++) {
                const TZrTypeId *sourceTypeId =
                        (const TZrTypeId *)ZrCore_Array_Get((SZrArray *)sourceIds, index);
                TZrTypeId substitutedTypeId = sourceTypeId != ZR_NULL
                                                      ? canonical_type_substitute_projection(
                                                                context,
                                                                *sourceTypeId,
                                                                definition,
                                                                instantiation,
                                                                depth + 1U)
                                                      : ZR_SEMANTIC_ID_INVALID;
                if (substitutedTypeId == ZR_SEMANTIC_ID_INVALID) {
                    ZrCore_Array_Free(context->state, &typeIds);
                    return ZR_SEMANTIC_ID_INVALID;
                }
                ZrCore_Array_Push(context->state, &typeIds, &substitutedTypeId);
            }
            if (node->kind == ZR_CANONICAL_TYPE_TUPLE) {
                result = ZrParser_CanonicalType_InternTuple(
                        context,
                        (const TZrTypeId *)typeIds.head,
                        typeIds.length);
            } else {
                TZrTypeId projectedDefinitionTypeId =
                        typeId == definition->projectionTypeId && instantiation != ZR_NULL
                                ? instantiation->id
                                : canonical_type_substitute_projection(
                                          context,
                                          node->data.unionType.definitionTypeId,
                                          definition,
                                          instantiation,
                                          depth + 1U);
                if (projectedDefinitionTypeId == ZR_SEMANTIC_ID_INVALID) {
                    ZrCore_Array_Free(context->state, &typeIds);
                    return ZR_SEMANTIC_ID_INVALID;
                }
                result = ZrParser_CanonicalType_InternUnion(
                        context,
                        projectedDefinitionTypeId,
                        (const TZrTypeId *)typeIds.head,
                        typeIds.length);
            }
            ZrCore_Array_Free(context->state, &typeIds);
            return result;
        }
        case ZR_CANONICAL_TYPE_REF: {
            TZrTypeId pointeeTypeId = canonical_type_substitute_projection(
                    context,
                    node->data.refType.pointeeTypeId,
                    definition,
                    instantiation,
                    depth + 1U);
            return ZrParser_CanonicalType_InternRef(
                    context,
                    pointeeTypeId,
                    node->data.refType.access);
        }
        case ZR_CANONICAL_TYPE_OWNER: {
            TZrTypeId targetTypeId = canonical_type_substitute_projection(
                    context,
                    node->data.owner.targetTypeId,
                    definition,
                    instantiation,
                    depth + 1U);
            return ZrParser_CanonicalType_InternOwner(
                    context,
                    targetTypeId,
                    node->data.owner.ownerKind);
        }
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
        case ZR_CANONICAL_TYPE_NULLABLE: {
            TZrTypeId targetTypeId = canonical_type_substitute_projection(
                    context,
                    node->data.target.targetTypeId,
                    definition,
                    instantiation,
                    depth + 1U);
            return node->kind == ZR_CANONICAL_TYPE_READONLY_VIEW
                           ? ZrParser_CanonicalType_InternReadonlyView(context, targetTypeId)
                           : ZrParser_CanonicalType_InternNullable(context, targetTypeId);
        }
        case ZR_CANONICAL_TYPE_FUNCTION: {
            SZrArray contracts;
            TZrTypeId returnTypeId;
            TZrTypeId result;
            TZrSize index;

            returnTypeId = canonical_type_substitute_projection(
                    context,
                    node->data.function.returnTypeId,
                    definition,
                    instantiation,
                    depth + 1U);
            if (returnTypeId == ZR_SEMANTIC_ID_INVALID) {
                return returnTypeId;
            }
            ZrCore_Array_Init(
                    context->state,
                    &contracts,
                    sizeof(SZrCanonicalParameterContract),
                    node->data.function.parameterContracts.length > 0U
                            ? node->data.function.parameterContracts.length
                            : ZR_PARSER_INITIAL_CAPACITY_TINY);
            for (index = 0; index < node->data.function.parameterContracts.length; index++) {
                const SZrCanonicalParameterContract *source =
                        (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                                (SZrArray *)&node->data.function.parameterContracts,
                                index);
                SZrCanonicalParameterContract contract;

                if (source == ZR_NULL) {
                    ZrCore_Array_Free(context->state, &contracts);
                    return ZR_SEMANTIC_ID_INVALID;
                }
                contract = *source;
                contract.typeId = canonical_type_substitute_projection(
                        context,
                        contract.typeId,
                        definition,
                        instantiation,
                        depth + 1U);
                if (contract.typeId == ZR_SEMANTIC_ID_INVALID) {
                    ZrCore_Array_Free(context->state, &contracts);
                    return ZR_SEMANTIC_ID_INVALID;
                }
                ZrCore_Array_Push(context->state, &contracts, &contract);
            }
            result = ZrParser_CanonicalType_InternFunction(
                    context,
                    (const SZrCanonicalParameterContract *)contracts.head,
                    contracts.length,
                    returnTypeId,
                    node->data.function.receiverEffect,
                    node->data.function.effectFlags);
            ZrCore_Array_Free(context->state, &contracts);
            return result;
        }
        default:
            return typeId;
    }
}

TZrTypeId ZrParser_CanonicalType_ResolveProjection(
        SZrSemanticContext *context,
        TZrTypeId typeId) {
    const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(context, typeId);
    const SZrCanonicalTypeDefinitionRecord *definition;
    const SZrCanonicalTypeNode *instantiation = ZR_NULL;

    if (node == ZR_NULL || node->kind == ZR_CANONICAL_TYPE_UNION) {
        return node != ZR_NULL ? typeId : ZR_SEMANTIC_ID_INVALID;
    }
    if (node->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE) {
        const SZrCanonicalTypeDefinitionRecord *rawDefinition =
                ZrParser_CanonicalTypeDefinition_FindRecord(
                        context,
                        node->data.genericInstance.definitionTypeId);
        definition = ZrParser_CanonicalTypeDefinition_ResolveRecord(
                context,
                typeId,
                &instantiation);
        if (definition == ZR_NULL && rawDefinition != ZR_NULL) {
            return ZR_SEMANTIC_ID_INVALID;
        }
    } else {
        definition = ZrParser_CanonicalTypeDefinition_FindRecord(context, typeId);
    }
    if (definition == ZR_NULL || definition->projectionTypeId == ZR_SEMANTIC_ID_INVALID) {
        return typeId;
    }
    return instantiation == ZR_NULL
                   ? definition->projectionTypeId
                   : canonical_type_substitute_projection(
                             context,
                             definition->projectionTypeId,
                             definition,
                             instantiation,
                             0U);
}

static EZrCanonicalGcScanKind canonical_gc_scan_join(
        EZrCanonicalGcScanKind left,
        EZrCanonicalGcScanKind right) {
    return left > right ? left : right;
}

static TZrBool canonical_type_compute_gc_scan_kind(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrCanonicalGcScanKind *outGcScanKind,
        TZrSize depth) {
    const SZrCanonicalTypeNode *node;
    EZrCanonicalGcScanKind result = ZR_CANONICAL_GC_SCAN_FREE;
    TZrSize index;

    if (context == ZR_NULL || outGcScanKind == ZR_NULL || depth > 256U) {
        return ZR_FALSE;
    }
    node = ZrParser_CanonicalType_Find(context, typeId);
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
            switch (node->data.primitive.valueType) {
                case ZR_VALUE_TYPE_STRING:
                case ZR_VALUE_TYPE_BUFFER:
                case ZR_VALUE_TYPE_FUNCTION:
                case ZR_VALUE_TYPE_CLOSURE_VALUE:
                case ZR_VALUE_TYPE_CLOSURE:
                case ZR_VALUE_TYPE_OBJECT:
                case ZR_VALUE_TYPE_THREAD:
                    result = ZR_CANONICAL_GC_SCAN_MAPPED;
                    break;
                case ZR_VALUE_TYPE_ARRAY:
                    result = ZR_CANONICAL_GC_SCAN_BARRIERED;
                    break;
                default:
                    result = ZR_CANONICAL_GC_SCAN_FREE;
                    break;
            }
            break;
        case ZR_CANONICAL_TYPE_NOMINAL: {
            const SZrCanonicalTypeDefinitionRecord *definition =
                    ZrParser_CanonicalTypeDefinition_FindRecord(context, typeId);
            result = definition != ZR_NULL
                             ? definition->gcScanKind
                             : ZR_CANONICAL_GC_SCAN_BARRIERED;
            break;
        }
        case ZR_CANONICAL_TYPE_GENERIC_PARAMETER:
            result = ZR_CANONICAL_GC_SCAN_BARRIERED;
            break;
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE: {
            const SZrCanonicalTypeDefinitionRecord *definition =
                    ZrParser_CanonicalTypeDefinition_ResolveRecord(
                            context,
                            typeId,
                            ZR_NULL);
            result = definition != ZR_NULL
                             ? definition->gcScanKind
                             : ZR_CANONICAL_GC_SCAN_BARRIERED;
            break;
        }
        case ZR_CANONICAL_TYPE_ARRAY:
        case ZR_CANONICAL_TYPE_REF:
        case ZR_CANONICAL_TYPE_OWNER:
            result = ZR_CANONICAL_GC_SCAN_BARRIERED;
            break;
        case ZR_CANONICAL_TYPE_TUPLE:
        case ZR_CANONICAL_TYPE_UNION: {
            const SZrArray *typeIds = node->kind == ZR_CANONICAL_TYPE_TUPLE
                                              ? &node->data.typeList.elementTypeIds
                                              : &node->data.unionType.variantTypeIds;
            for (index = 0; index < typeIds->length; index++) {
                const TZrTypeId *elementTypeId =
                        (const TZrTypeId *)ZrCore_Array_Get((SZrArray *)typeIds, index);
                EZrCanonicalGcScanKind elementScanKind;
                if (elementTypeId == ZR_NULL ||
                    !canonical_type_compute_gc_scan_kind(
                            context,
                            *elementTypeId,
                            &elementScanKind,
                            depth + 1U)) {
                    return ZR_FALSE;
                }
                result = canonical_gc_scan_join(result, elementScanKind);
            }
            break;
        }
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
        case ZR_CANONICAL_TYPE_NULLABLE:
            return canonical_type_compute_gc_scan_kind(
                    context,
                    node->data.target.targetTypeId,
                    outGcScanKind,
                    depth + 1U);
        case ZR_CANONICAL_TYPE_FUNCTION:
            result = ZR_CANONICAL_GC_SCAN_MAPPED;
            break;
        case ZR_CANONICAL_TYPE_ERROR:
        case ZR_CANONICAL_TYPE_NEVER:
            result = ZR_CANONICAL_GC_SCAN_FREE;
            break;
        default:
            return ZR_FALSE;
    }
    *outGcScanKind = result;
    return ZR_TRUE;
}

TZrBool ZrParser_CanonicalType_GetGcScanKind(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrCanonicalGcScanKind *outGcScanKind) {
    return canonical_type_compute_gc_scan_kind(context, typeId, outGcScanKind, 0U);
}
