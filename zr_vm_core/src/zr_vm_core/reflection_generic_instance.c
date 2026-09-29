/* 从元数据 TypeSpec 和借用的构造请求恢复泛型类型身份，供对象构造与解释器回退复用。 */
#include "zr_vm_core/reflection.h"

#include "reflection_generic_argument_internal.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/value.h"

/* 与请求树校验和候选签名匹配共用的失效关闭深度上限。 */
#define ZR_REFLECTION_GENERIC_ARGUMENT_MAX_RECURSION_DEPTH 64u

/** @brief 为所有解析失败路径提供相同的空结果，避免调用方沿用旧的 route 或 layout。 */
static void reflection_clear_dynamic_generic_type_instance(
        SZrReflectionDynamicGenericTypeInstance *instance) {
    if (instance == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(instance, 0, sizeof(*instance));
    instance->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

/**
 * @brief 从现有 TypeSpec 恢复泛型实例身份，并按注册的静态布局选择 AOT 或解释器回退。
 * @pre runtime 和 outInstance 有效；TypeSpec 属于该 runtime 的元数据。
 * @return 元数据绑定有效时返回 true；未注册布局仍是成功的解释器回退，不伪造 layout。
 * @note 失败时清空输出；返回的 record 和 layout 指针借用自 runtime。
 */
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

/**
 * @brief 把请求模块的 TypeSpec 绑定到不同提供者模块的同一泛型实例身份。
 * @pre 两个 runtime 所属模块不同，绑定记录及元数据在调用期间保持有效。
 * @return 仅在 token、版本、签名身份和完整签名字节均兼容时，从 provider 解析 route/layout。
 * @note 失败时清空输出；仓内直接调用位于跨模块绑定测试。
 */
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
    /* 先核对 requester 预期身份与 provider 版本，再接受绑定后的 token；避免仅凭 RID 猜测跨模块类型。 */
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
    /* token/hash 相等仍不足以证明泛型实参相同；两侧完整规范签名字节也须一致。 */
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

/**
 * @brief 在 TypeSpec/MethodSpec 查询前验证借用的递归实参树与当前 runtime 元数据。
 * @pre 各子节点在递归期间可读；含 token 的节点必须提供有效 runtime；外层从 depth=0 调用。
 * @return 无效类别、缺失子节点、无效 token 或达到深度上限时返回 false。
 */
TZrBool ZrCore_Reflection_ValidateGenericTypeArgument(
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
                             ZrCore_Reflection_ValidateGenericTypeArgument(runtime,
                                                                          argument->elementType,
                                                                          depth + 1u));

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE:
            if (argument->primitiveValueType != 0u || argument->typeToken != 0u ||
                argument->childCount == 0u || argument->childTypes == ZR_NULL) {
                return ZR_FALSE;
            }
            for (index = 0u; index < argument->childCount; ++index) {
                if (!ZrCore_Reflection_ValidateGenericTypeArgument(
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
                             ZrCore_Reflection_ValidateGenericTypeArgument(runtime,
                                                                          argument->elementType,
                                                                          depth + 1u));

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE:
            return (TZrBool)(argument->primitiveValueType == 0u &&
                             argument->typeToken == 0u &&
                             ZrCore_Reflection_ValidateGenericTypeArgument(runtime,
                                                                          argument->elementType,
                                                                          depth + 1u));

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION:
            /*
             * TODO: 名称偏移目前只要求非零，匹配器也只比较数值；需确认公开请求是否必须使用
             *       当前 runtime 的有效 string-heap 偏移，并确定在何处验证这一归属契约。
             */
            if (argument->primitiveValueType != 0u || argument->typeToken != 0u ||
                argument->unionValueType <= (TZrUInt32)ZR_VALUE_TYPE_NULL ||
                argument->unionValueType >= (TZrUInt32)ZR_VALUE_TYPE_UNKNOWN ||
                argument->unionNameStringOffset == 0u ||
                (argument->childCount == 0u && argument->childTypes != ZR_NULL) ||
                (argument->childCount > 0u && argument->childTypes == ZR_NULL)) {
                return ZR_FALSE;
            }
            for (index = 0u; index < argument->childCount; ++index) {
                if (!ZrCore_Reflection_ValidateGenericTypeArgument(
                            runtime, &argument->childTypes[index], depth + 1u)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;

        default:
            return ZR_FALSE;
    }
}

/** @brief 用完整、有界的编码跨度比较嵌套 TypeSpec，避免仅凭 token/hash 把不同结构合并。 */
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

/** @brief 将请求的类型 token 展开为签名节点，并与没有直接 token 的复合子节点比较。 */
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

/* 子列表比较需要递归回到主匹配器，先声明共享入口。 */
TZrBool ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *candidateBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *candidateNode,
        TZrMetadataToken resolvedCandidateToken,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth);

/** @brief 按元数据顺序逐项比较 tuple/union 子节点；任一读取或结构匹配失败即拒绝。 */
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
            !ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(runtime,
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

/**
 * @brief 将已验证的请求实参树与候选元数据签名节点按类别递归比较。
 * @pre candidateBlob、candidateNode、argument 有效；调用方先验证请求树；depth 受共同上限约束。
 * @note 已解析的顶层类型 token 优先按 token 身份比较；复合子节点无独立 token 时才比较其签名。
 */
TZrBool ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(
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
            return ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(runtime,
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
            return ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(runtime,
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
            return ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(runtime,
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

/** @brief 将 TypeSpec 的位置索引适配到共享签名匹配器，使请求解析不直接依赖 blob 编码。 */
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

    return ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(
            runtime,
            &view.bindingView.signatureView.blob,
            &view.argumentNode,
            view.argumentToken,
            argument,
            0u);
}

/**
 * @brief 核对候选 TypeSpec 的开放基类型、元数及全部有序实参。
 * @note outBaseMatches 即使在实参失配时仍标记已见过该基类型，供解释器回退判定使用。
 */
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

/**
 * @brief 为开放泛型类型和请求实参寻找现有 TypeSpec，或返回可解释执行的未物化组合。
 * @pre arguments 中的树在本次调用及返回 carrier 的消费期间可读；runtime 属于目标元数据模块。
 * @return 现有精确组合复用其 AOT/deopt route；仅当开放基类型已有绑定而组合缺失时返回无 token 的 deopt carrier。
 * @note 精确命中和回退结果都借用 requestedArguments；对象构造方须在调用方释放该树前复制。
 */
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
        if (!ZrCore_Reflection_ValidateGenericTypeArgument(runtime, &arguments[index], 0u)) {
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

    /* 仅对已证明存在泛型基类型绑定的请求开放解释器回退，避免凭任意 TypeDef 伪造组合。 */
    /*
     * BUG: TypeDef 的声明元数未与 argumentCount 比较；同 base 的 TypeSpec 即使元数失配，
     *      仍会使 hasGenericBaseBinding 为真，错误地接受少实参请求为解释器回退实例。
     */
    if (!hasGenericBaseBinding) {
        return ZR_FALSE;
    }

    /* 未命中的请求没有元数据 TypeSpec；保留借用的请求树，交给后续构造器深拷贝。 */
    outInstance->route = ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT;
    outInstance->genericBaseToken = genericBaseToken;
    outInstance->genericBaseRecord = baseRecord;
    outInstance->genericArgumentCount = argumentCount;
    outInstance->requestedArguments = arguments;
    return ZR_TRUE;
}

/**
 * @brief 在创建反射对象或解释器实例前，按当前 runtime 重新解析并核对先前的 carrier。
 * @pre 输入 carrier 与其借用的 requestedArguments 在本次调用内仍有效。
 * @return 有效输入与输出下，route、token、签名、基类型和布局全部一致时输出新结果；重解析或比较失败时清空输出。
 */
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
