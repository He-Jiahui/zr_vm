#include "zr_vm_core/reflection.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/value.h"

#define ZR_REFLECTION_GENERIC_ARGUMENT_MAX_RECURSION_DEPTH 64u

static void reflection_clear_dynamic_generic_type_instance(
        SZrReflectionDynamicGenericTypeInstance *instance) {
    if (instance == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(instance, 0, sizeof(*instance));
    instance->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

TZrBool ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        SZrReflectionDynamicGenericTypeInstance *outInstance) {
    SZrMetadataRuntimeTypeSpecGenericBindingView bindingView;

    reflection_clear_dynamic_generic_type_instance(outInstance);
    if (runtime == ZR_NULL || outInstance == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(typeSpecToken) != ZR_METADATA_TABLE_TYPE_SPEC ||
        !ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, typeSpecToken, &bindingView)) {
        return ZR_FALSE;
    }

    outInstance->typeSpecToken = typeSpecToken;
    outInstance->genericSignatureToken = bindingView.signatureView.signatureToken;
    outInstance->genericSignatureHash = bindingView.signatureView.signatureHash;
    outInstance->genericBaseToken = bindingView.baseToken;
    outInstance->genericBaseRecord = bindingView.baseRecord;
    outInstance->genericArgumentCount = bindingView.signatureView.argumentCount;
    outInstance->genericArgumentListBlobOffset = bindingView.signatureView.argumentListBlobOffset;
    outInstance->typeLayout = ZrCore_MetadataRuntime_ResolveTypeTokenLayout(
            runtime, typeSpecToken, &outInstance->typeLayoutId);
    if (outInstance->typeLayout == ZR_NULL) {
        outInstance->route = ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT;
        return ZR_TRUE;
    }

    outInstance->route = ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT;
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider(
        SZrMetadataRuntime *requesterRuntime,
        TZrMetadataToken requesterTypeSpecToken,
        SZrMetadataRuntime *providerRuntime,
        SZrReflectionDynamicGenericTypeInstance *outInstance) {
    const SZrMetadataTokenRecord *requesterRecord;
    const SZrMetadataTokenRecord *requesterSignatureRecord;
    const SZrMetadataTokenRecord *providerRecord;
    const SZrMetadataTokenRecord *providerSignatureRecord;
    const SZrMetadataTokenBinding *binding;
    SZrMetadataRuntimeTypeSpecSignatureView requesterSignatureView;
    SZrMetadataRuntimeTypeSpecSignatureView providerSignatureView;
    SZrFunction *requesterFunction;
    SZrFunction *providerFunction;

    reflection_clear_dynamic_generic_type_instance(outInstance);
    if (requesterRuntime == ZR_NULL || providerRuntime == ZR_NULL || outInstance == ZR_NULL ||
        requesterRuntime == providerRuntime || requesterRuntime->module == providerRuntime->module ||
        ZR_METADATA_TOKEN_TABLE(requesterTypeSpecToken) != ZR_METADATA_TABLE_TYPE_SPEC) {
        return ZR_FALSE;
    }
    requesterFunction = requesterRuntime->metadataFunction;
    providerFunction = providerRuntime->metadataFunction;
    if (requesterFunction == ZR_NULL || providerFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    requesterRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(
            requesterRuntime, requesterTypeSpecToken);
    requesterSignatureRecord = ZrCore_MetadataRuntime_ResolveSignatureRecord(
            requesterRuntime, requesterTypeSpecToken);
    binding = ZrCore_Function_FindModuleMetadataBinding(
            requesterFunction, requesterTypeSpecToken);
    if (requesterRecord == ZR_NULL || requesterSignatureRecord == ZR_NULL || binding == ZR_NULL ||
        binding->refToken != requesterTypeSpecToken ||
        binding->refSignatureToken != requesterSignatureRecord->token ||
        binding->refSignatureHash != requesterRecord->signatureHash ||
        binding->expectedMetadataToken != requesterTypeSpecToken ||
        binding->expectedSignatureToken != requesterSignatureRecord->token ||
        binding->expectedSignatureHash != requesterRecord->signatureHash ||
        ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(
                binding,
                requesterRecord,
                providerFunction->moduleVersion,
                ZR_NULL) != ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE ||
        binding->resolvedModuleSignatureHash != providerFunction->moduleSignatureHash ||
        ZR_METADATA_TOKEN_TABLE(binding->resolvedMetadataToken) != ZR_METADATA_TABLE_TYPE_SPEC ||
        ZR_METADATA_TOKEN_TABLE(binding->resolvedSignatureToken) != ZR_METADATA_TABLE_SIGNATURE) {
        return ZR_FALSE;
    }

    providerRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(
            providerRuntime, binding->resolvedMetadataToken);
    providerSignatureRecord = ZrCore_MetadataRuntime_ResolveSignatureRecord(
            providerRuntime, binding->resolvedMetadataToken);
    if (providerRecord == ZR_NULL || providerSignatureRecord == ZR_NULL ||
        providerRecord->token != binding->resolvedMetadataToken ||
        providerRecord->relatedToken != binding->resolvedSignatureToken ||
        providerRecord->signatureHash != binding->resolvedSignatureHash ||
        providerSignatureRecord->token != binding->resolvedSignatureToken ||
        providerSignatureRecord->relatedToken != binding->resolvedMetadataToken ||
        providerSignatureRecord->signatureHash != binding->resolvedSignatureHash) {
        return ZR_FALSE;
    }
    if (!ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(
                requesterRuntime, requesterTypeSpecToken, &requesterSignatureView) ||
        !ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(
                providerRuntime, binding->resolvedMetadataToken, &providerSignatureView) ||
        requesterSignatureView.signatureToken != binding->refSignatureToken ||
        requesterSignatureView.signatureHash != binding->refSignatureHash ||
        providerSignatureView.signatureToken != binding->resolvedSignatureToken ||
        providerSignatureView.signatureHash != binding->resolvedSignatureHash ||
        requesterSignatureView.blob.byteLength == 0u ||
        requesterSignatureView.blob.byteLength != providerSignatureView.blob.byteLength ||
        ZrCore_Memory_RawCompare((TZrPtr)requesterSignatureView.blob.data,
                                 (TZrPtr)providerSignatureView.blob.data,
                                 requesterSignatureView.blob.byteLength) != 0) {
        return ZR_FALSE;
    }

    return ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            providerRuntime, binding->resolvedMetadataToken, outInstance);
}

static TZrBool reflection_validate_generic_type_argument(
        SZrMetadataRuntime *runtime,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth) {
    TZrUInt32 table;
    TZrUInt32 index;

    if (argument == ZR_NULL || depth >= ZR_REFLECTION_GENERIC_ARGUMENT_MAX_RECURSION_DEPTH) {
        return ZR_FALSE;
    }

    switch (argument->kind) {
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE:
            return (TZrBool)(argument->typeToken == 0u &&
                             argument->primitiveValueType > (TZrUInt32)ZR_VALUE_TYPE_NULL &&
                             argument->primitiveValueType < (TZrUInt32)ZR_VALUE_TYPE_UNKNOWN);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN:
            table = ZR_METADATA_TOKEN_TABLE(argument->typeToken);
            if (argument->primitiveValueType != 0u ||
                ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, argument->typeToken) == ZR_NULL) {
                return ZR_FALSE;
            }
            if (table == ZR_METADATA_TABLE_TYPE_SPEC) {
                SZrMetadataRuntimeTypeSpecSignatureView typeSpecView;
                return ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(
                        runtime, argument->typeToken, &typeSpecView);
            }
            return (TZrBool)(table == ZR_METADATA_TABLE_TYPE_DEF || table == ZR_METADATA_TABLE_TYPE_REF);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY:
            return (TZrBool)(argument->primitiveValueType == 0u &&
                             argument->typeToken == 0u &&
                             argument->arrayRank > 0u &&
                             reflection_validate_generic_type_argument(runtime,
                                                                       argument->elementType,
                                                                       depth + 1u));

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE:
            if (argument->primitiveValueType != 0u || argument->typeToken != 0u ||
                argument->childCount == 0u || argument->childTypes == ZR_NULL) {
                return ZR_FALSE;
            }
            for (index = 0u; index < argument->childCount; ++index) {
                if (!reflection_validate_generic_type_argument(
                            runtime, &argument->childTypes[index], depth + 1u)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP:
            return (TZrBool)(argument->primitiveValueType == 0u &&
                             argument->typeToken == 0u &&
                             argument->ownershipQualifier > ZR_REFLECTION_OWNERSHIP_QUALIFIER_NONE &&
                             argument->ownershipQualifier <= ZR_REFLECTION_OWNERSHIP_QUALIFIER_LOANED &&
                             reflection_validate_generic_type_argument(runtime,
                                                                       argument->elementType,
                                                                       depth + 1u));

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE:
            return (TZrBool)(argument->primitiveValueType == 0u &&
                             argument->typeToken == 0u &&
                             reflection_validate_generic_type_argument(runtime,
                                                                       argument->elementType,
                                                                       depth + 1u));

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION:
            if (argument->primitiveValueType != 0u || argument->typeToken != 0u ||
                argument->unionValueType <= (TZrUInt32)ZR_VALUE_TYPE_NULL ||
                argument->unionValueType >= (TZrUInt32)ZR_VALUE_TYPE_UNKNOWN ||
                argument->unionNameStringOffset == 0u ||
                (argument->childCount == 0u && argument->childTypes != ZR_NULL) ||
                (argument->childCount > 0u && argument->childTypes == ZR_NULL)) {
                return ZR_FALSE;
            }
            for (index = 0u; index < argument->childCount; ++index) {
                if (!reflection_validate_generic_type_argument(
                            runtime, &argument->childTypes[index], depth + 1u)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;

        default:
            return ZR_FALSE;
    }
}

static TZrBool reflection_signature_node_spans_match(
        const SZrZrpMetadataPoolSliceView *leftBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *leftNode,
        const SZrZrpMetadataPoolSliceView *rightBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *rightNode) {
    TZrSize leftLength;
    TZrSize rightLength;

    if (leftBlob == ZR_NULL || leftNode == ZR_NULL || rightBlob == ZR_NULL || rightNode == ZR_NULL ||
        leftNode->nextBlobOffset < leftNode->blobOffset || leftNode->nextBlobOffset > leftBlob->byteLength ||
        rightNode->nextBlobOffset < rightNode->blobOffset || rightNode->nextBlobOffset > rightBlob->byteLength) {
        return ZR_FALSE;
    }

    leftLength = (TZrSize)(leftNode->nextBlobOffset - leftNode->blobOffset);
    rightLength = (TZrSize)(rightNode->nextBlobOffset - rightNode->blobOffset);
    return (TZrBool)(leftLength == rightLength &&
                     ZrCore_Memory_RawCompare((TZrPtr)(leftBlob->data + leftNode->blobOffset),
                                              (TZrPtr)(rightBlob->data + rightNode->blobOffset),
                                              leftLength) == 0);
}

static TZrBool reflection_signature_node_matches_type_token(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *candidateBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *candidateNode,
        TZrMetadataToken requestedTypeToken) {
    SZrZrpMetadataPoolSliceView requestedBlob;
    SZrMetadataRuntimeSignatureTypeNodeView requestedNode;

    if (ZR_METADATA_TOKEN_TABLE(requestedTypeToken) == ZR_METADATA_TABLE_TYPE_SPEC) {
        SZrMetadataRuntimeTypeSpecSignatureView requestedTypeSpecView;
        if (!ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(
                    runtime, requestedTypeToken, &requestedTypeSpecView)) {
            return ZR_FALSE;
        }
        return reflection_signature_node_spans_match(candidateBlob,
                                                     candidateNode,
                                                     &requestedTypeSpecView.blob,
                                                     &requestedTypeSpecView.genericInstanceNode);
    }

    if (!ZrCore_MetadataRuntime_GetSignatureBlob(runtime, requestedTypeToken, &requestedBlob) ||
        !ZrCore_MetadataRuntime_ReadSignatureTypeNode(&requestedBlob, 0u, &requestedNode)) {
        return ZR_FALSE;
    }
    return reflection_signature_node_spans_match(candidateBlob,
                                                 candidateNode,
                                                 &requestedBlob,
                                                  &requestedNode);
}

static TZrBool reflection_generic_type_node_matches(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *candidateBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *candidateNode,
        TZrMetadataToken resolvedCandidateToken,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth);

static TZrBool reflection_generic_type_child_list_matches(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *candidateBlob,
        TZrUInt32 childListBlobOffset,
        const SZrReflectionGenericTypeArgument *childTypes,
        TZrUInt32 childCount,
        TZrUInt32 depth) {
    SZrMetadataRuntimeSignatureTypeNodeView childNode;
    TZrUInt32 childOffset = childListBlobOffset;
    TZrUInt32 index;

    for (index = 0u; index < childCount; ++index) {
        if (!ZrCore_MetadataRuntime_ReadSignatureTypeNode(candidateBlob, childOffset, &childNode) ||
            !reflection_generic_type_node_matches(runtime,
                                                  candidateBlob,
                                                  &childNode,
                                                  0u,
                                                  &childTypes[index],
                                                  depth + 1u)) {
            return ZR_FALSE;
        }
        childOffset = childNode.nextBlobOffset;
    }
    return ZR_TRUE;
}

static TZrBool reflection_generic_type_node_matches(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *candidateBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *candidateNode,
        TZrMetadataToken resolvedCandidateToken,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth) {
    SZrMetadataRuntimeSignatureTypeNodeView elementNode;

    if (depth >= ZR_REFLECTION_GENERIC_ARGUMENT_MAX_RECURSION_DEPTH) {
        return ZR_FALSE;
    }

    switch (argument->kind) {
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE:
            return (TZrBool)(candidateNode->node == ZR_METADATA_SIGNATURE_NODE_PRIMITIVE &&
                             candidateNode->payload0 == argument->primitiveValueType);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN:
            if (resolvedCandidateToken != 0u) {
                return (TZrBool)(resolvedCandidateToken == argument->typeToken);
            }
            return reflection_signature_node_matches_type_token(
                    runtime, candidateBlob, candidateNode, argument->typeToken);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY:
            if (candidateNode->node != ZR_METADATA_SIGNATURE_NODE_ARRAY ||
                candidateNode->payload0 != argument->arrayRank ||
                !ZrCore_MetadataRuntime_ReadSignatureTypeNode(candidateBlob,
                                                             candidateNode->baseTypeBlobOffset,
                                                             &elementNode)) {
                return ZR_FALSE;
            }
            return reflection_generic_type_node_matches(runtime,
                                                        candidateBlob,
                                                        &elementNode,
                                                        0u,
                                                        argument->elementType,
                                                        depth + 1u);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE:
            if (candidateNode->node != ZR_METADATA_SIGNATURE_NODE_TUPLE ||
                candidateNode->childCount != argument->childCount) {
                return ZR_FALSE;
            }
            return reflection_generic_type_child_list_matches(runtime,
                                                              candidateBlob,
                                                              candidateNode->childListBlobOffset,
                                                              argument->childTypes,
                                                              argument->childCount,
                                                              depth);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP:
            if (candidateNode->node != ZR_METADATA_SIGNATURE_NODE_OWNERSHIP ||
                candidateNode->payload0 != (TZrUInt32)argument->ownershipQualifier ||
                !ZrCore_MetadataRuntime_ReadSignatureTypeNode(candidateBlob,
                                                             candidateNode->baseTypeBlobOffset,
                                                             &elementNode)) {
                return ZR_FALSE;
            }
            return reflection_generic_type_node_matches(runtime,
                                                        candidateBlob,
                                                        &elementNode,
                                                        0u,
                                                        argument->elementType,
                                                        depth + 1u);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE:
            if (candidateNode->node != ZR_METADATA_SIGNATURE_NODE_NULLABLE ||
                !ZrCore_MetadataRuntime_ReadSignatureTypeNode(candidateBlob,
                                                             candidateNode->baseTypeBlobOffset,
                                                             &elementNode)) {
                return ZR_FALSE;
            }
            return reflection_generic_type_node_matches(runtime,
                                                        candidateBlob,
                                                        &elementNode,
                                                        0u,
                                                        argument->elementType,
                                                        depth + 1u);

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION:
            if (candidateNode->node != ZR_METADATA_SIGNATURE_NODE_UNION ||
                candidateNode->payload0 != argument->unionValueType ||
                candidateNode->payload1 != argument->unionNameStringOffset ||
                candidateNode->childCount != argument->childCount) {
                return ZR_FALSE;
            }
            return reflection_generic_type_child_list_matches(runtime,
                                                              candidateBlob,
                                                              candidateNode->childListBlobOffset,
                                                              argument->childTypes,
                                                              argument->childCount,
                                                              depth);

        default:
            return ZR_FALSE;
    }
}

static TZrBool reflection_generic_type_argument_matches(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        TZrUInt32 argumentIndex,
        const SZrReflectionGenericTypeArgument *argument) {
    SZrMetadataRuntimeTypeSpecGenericArgumentView view;

    if (!ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
                runtime, typeSpecToken, argumentIndex, &view)) {
        return ZR_FALSE;
    }

    return reflection_generic_type_node_matches(runtime,
                                                &view.bindingView.signatureView.blob,
                                                &view.argumentNode,
                                                view.argumentToken,
                                                argument,
                                                0u);
}

static TZrBool reflection_type_spec_matches_request(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        TZrMetadataToken genericBaseToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        TZrBool *outBaseMatches) {
    SZrMetadataRuntimeTypeSpecGenericBindingView bindingView;
    TZrUInt32 index;

    *outBaseMatches = ZR_FALSE;
    if (!ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, typeSpecToken, &bindingView) ||
        bindingView.baseToken != genericBaseToken) {
        return ZR_FALSE;
    }
    *outBaseMatches = ZR_TRUE;
    if (bindingView.signatureView.argumentCount != argumentCount) {
        return ZR_FALSE;
    }

    for (index = 0u; index < argumentCount; ++index) {
        if (!reflection_generic_type_argument_matches(runtime, typeSpecToken, index, &arguments[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_ResolveConstructedGenericType(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken genericBaseToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount,
        SZrReflectionDynamicGenericTypeInstance *outInstance) {
    const SZrMetadataTokenRecord *baseRecord;
    SZrFunction *metadataFunction;
    TZrBool hasGenericBaseBinding = ZR_FALSE;
    TZrUInt32 baseTable;
    TZrUInt32 index;

    reflection_clear_dynamic_generic_type_instance(outInstance);
    baseTable = ZR_METADATA_TOKEN_TABLE(genericBaseToken);
    if (runtime == ZR_NULL || outInstance == ZR_NULL || arguments == ZR_NULL || argumentCount == 0u ||
        (baseTable != ZR_METADATA_TABLE_TYPE_DEF && baseTable != ZR_METADATA_TABLE_TYPE_REF)) {
        return ZR_FALSE;
    }

    baseRecord = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, genericBaseToken);
    if (baseRecord == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0u; index < argumentCount; ++index) {
        if (!reflection_validate_generic_type_argument(runtime, &arguments[index], 0u)) {
            return ZR_FALSE;
        }
    }

    metadataFunction = runtime->metadataFunction;
    if (metadataFunction != ZR_NULL && metadataFunction->metadataTokenRecords != ZR_NULL) {
        for (index = 0u; index < metadataFunction->metadataTokenRecordLength; ++index) {
            TZrMetadataToken candidateToken = metadataFunction->metadataTokenRecords[index].token;
            TZrBool baseMatches = ZR_FALSE;

            if (ZR_METADATA_TOKEN_TABLE(candidateToken) != ZR_METADATA_TABLE_TYPE_SPEC ||
                !reflection_type_spec_matches_request(runtime,
                                                      candidateToken,
                                                      genericBaseToken,
                                                      arguments,
                                                      argumentCount,
                                                      &baseMatches)) {
                if (baseMatches) {
                    hasGenericBaseBinding = ZR_TRUE;
                }
                continue;
            }
            hasGenericBaseBinding = ZR_TRUE;
            if (!ZrCore_Reflection_ResolveDynamicGenericTypeInstance(runtime, candidateToken, outInstance)) {
                return ZR_FALSE;
            }
            outInstance->requestedArguments = arguments;
            return ZR_TRUE;
        }
    }

    if (!hasGenericBaseBinding) {
        return ZR_FALSE;
    }

    outInstance->route = ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT;
    outInstance->genericBaseToken = genericBaseToken;
    outInstance->genericBaseRecord = baseRecord;
    outInstance->genericArgumentCount = argumentCount;
    outInstance->requestedArguments = arguments;
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_RevalidateDynamicGenericTypeInstance(
        SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance,
        SZrReflectionDynamicGenericTypeInstance *outResolved) {
    SZrReflectionDynamicGenericTypeInstance input;
    SZrReflectionDynamicGenericTypeInstance resolved;
    TZrBool resolutionSucceeded;

    if (instance == ZR_NULL || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }
    input = *instance;
    reflection_clear_dynamic_generic_type_instance(outResolved);
    if (runtime == ZR_NULL) {
        return ZR_FALSE;
    }

    if (input.requestedArguments != ZR_NULL) {
        resolutionSucceeded = ZrCore_Reflection_ResolveConstructedGenericType(
                runtime,
                input.genericBaseToken,
                input.requestedArguments,
                input.genericArgumentCount,
                &resolved);
    } else {
        resolutionSucceeded = ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
                runtime, input.typeSpecToken, &resolved);
    }
    if (!resolutionSucceeded || resolved.route != input.route ||
        resolved.typeSpecToken != input.typeSpecToken ||
        resolved.genericSignatureToken != input.genericSignatureToken ||
        resolved.genericSignatureHash != input.genericSignatureHash ||
        resolved.genericBaseToken != input.genericBaseToken ||
        resolved.genericBaseRecord != input.genericBaseRecord ||
        resolved.genericArgumentCount != input.genericArgumentCount ||
        resolved.genericArgumentListBlobOffset != input.genericArgumentListBlobOffset ||
        resolved.typeLayoutId != input.typeLayoutId || resolved.typeLayout != input.typeLayout) {
        return ZR_FALSE;
    }

    *outResolved = resolved;
    return ZR_TRUE;
}
