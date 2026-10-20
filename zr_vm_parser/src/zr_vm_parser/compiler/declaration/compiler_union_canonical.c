#include "compiler/declaration/compiler_union_canonical.h"

#include "compiler/compiler_internal.h"
#include "semantic/semantic_type_publication.h"
#include "semantic/type_declaration_context.h"

static TZrBool compiler_union_can_publish_type(
        const SZrSemanticContext *context,
        SZrString *name,
        SZrAstNode *declarationNode,
        SZrSyntaxContext normalizedContext) {
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                ZrCore_Array_Get((SZrArray *)&context->symbols, index);
        SZrSyntaxContext symbolContext;

        if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
            symbol->name == ZR_NULL || !ZrCore_String_Equal(symbol->name, name)) {
            continue;
        }
        if (!type_declaration_normalize_context(symbol->syntaxContext, &symbolContext)) {
            return ZR_FALSE;
        }
        if (ZrParser_SyntaxContext_Equal(symbolContext, normalizedContext) &&
            symbol->astNode != declarationNode) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* 将变体字段类型归一为 canonical payload ID，供 union 节点复用同一类型图。 */
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

    /*
     * BUG: registrar 对合法带字段变体调用本 helper 时，fieldTypeIds 的 Init OOM 仍留下正 capacity、有效状态和空 head；
     * 后续 void Push 在断言开启时终止，关闭时 RawCopy 写向空地址，故本 helper 不能将该分配失败转为 false。可达输入见
     * tests/parser/test_canonical_type_graph_union_cases.h:48-54；状态/写入见 zr_vm_core/include/zr_vm_core/array.h:36-42、74-88，RawCopy 实现见 zr_vm_core/include/zr_vm_core/memory.h:101；
     * 断言配置见 zr_vm_common/include/zr_vm_common/zr_common_conf.h:105-107。
     */
    /* 使用本声明的泛型绑定解析每个字段，并在离开本轮前释放临时 inferred type。 */
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

    /* 单字段直接作为 payload；零字段与多字段统一为 tuple，保留变体的解构形状。 */
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

/* 先校验声明身份与泛型元数据，再构造规范图并发布语义名称。 */
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
    const SZrNamedTypeBinding *existingBinding;
    const SZrSemanticSymbolRecord *existingSymbol;
    SZrSyntaxContext syntaxContext;
    SZrSyntaxContext normalizedContext;
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
    /* 重复名和不支持的 generic kind 在预留 symbol ID 前拒绝，不让无效声明进入 canonical 登记。 */
    if (cs->typeEnv == ZR_NULL ||
        !type_declaration_raw_context(node, &syntaxContext) ||
        !type_declaration_normalize_context(syntaxContext, &normalizedContext)) {
        return ZR_FALSE;
    }
    existingBinding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            cs->typeEnv, prototype->name, syntaxContext);
    if (existingBinding != ZR_NULL && existingBinding->declarationNode != node) {
        return ZR_FALSE;
    }
    if (!compiler_union_can_publish_type(
                cs->semanticContext,
                prototype->name,
                node,
                normalizedContext)) {
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

    /* A forward annotation may already own a provisional nominal identity; publish the union through it. */
    if (existingBinding != ZR_NULL) {
        existingSymbol = ZrParser_Semantic_FindSymbolById(
                cs->semanticContext, existingBinding->symbolId);
        if (existingSymbol == ZR_NULL ||
            existingSymbol->astNode != node ||
            existingSymbol->name == ZR_NULL ||
            !ZrCore_String_Equal(existingSymbol->name, prototype->name) ||
            existingBinding->typeId == ZR_SEMANTIC_ID_INVALID) {
            return ZR_FALSE;
        }
        symbolId = existingBinding->symbolId;
        definitionTypeId = existingBinding->typeId;
    } else {
        /* Reserve the symbol ID as generic owner; each type parameter is keyed by owner and ordinal. */
        symbolId = ZrParser_Semantic_ReserveSymbolId(cs->semanticContext);
        if (symbolId == ZR_SEMANTIC_ID_INVALID) {
            return ZR_FALSE;
        }
        definitionTypeId = ZrParser_CanonicalType_InternNominal(
                cs->semanticContext,
                ZR_NULL,
                prototype->name,
                ZrParser_SyntaxContext_Equal(normalizedContext, ZrParser_SyntaxContext_Root())
                        ? 0U : symbolId);
        if (definitionTypeId == ZR_SEMANTIC_ID_INVALID) {
            return ZR_FALSE;
        }
    }

    /*
     * BUG: 合法泛型输入会让 genericBindings 与 genericParameterKinds 经 Init 后逐参数 Push；任一 Init OOM 都留下空 head、正
     * capacity 和有效状态，后续 void Push 在断言开启时终止，关闭时 RawCopy 写向空地址，不能以 false 结束。输入见
     * tests/parser/test_canonical_type_graph_union_cases.h:51-52；底层见 zr_vm_core/include/zr_vm_core/array.h:36-42、74-88，RawCopy 实现见 zr_vm_core/include/zr_vm_core/memory.h:101；
     * 断言配置见 zr_vm_common/include/zr_vm_common/zr_common_conf.h:105-107。
     */
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
    /* 类型形参拥有 canonical 参数节点；const 形参只保留 kind 与序号，不伪造类型 ID。 */
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

    /*
     * BUG: 合法非空 union 的 variantTypeIds Init OOM 后仍由逐变体 Push 使用空 head、正 capacity 和有效状态；Push 在断言
     * 开启时终止，关闭时 RawCopy 写向空地址，不能以 false 返回。可达输入见
     * tests/parser/test_canonical_type_graph_union_cases.h:48-54；底层见 zr_vm_core/include/zr_vm_core/array.h:36-42、74-88，RawCopy 实现见 zr_vm_core/include/zr_vm_core/memory.h:101；
     * 断言配置见 zr_vm_common/include/zr_vm_common/zr_common_conf.h:105-107。
     */
    /* 按 AST 声明顺序生成变体 payload，使 canonical 列表与 tag/成员序号对齐。 */
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

    /* 扫描种类由所有分支 payload 合并得出；非 FREE 类型才声明含 GC 引用能力。 */
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

    /* 先登记 generic/non-generic 投影，再发布语义符号。
     * 后者失败时不回滚已登记的 canonical 记录；typeEnv 名称只在发布成功后追加。 */
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

    if (existingBinding != ZR_NULL) {
        if (!ZrParser_Semantic_RegisterCanonicalType(
                    cs->semanticContext,
                    unionTypeId,
                    ZR_SEMANTIC_TYPE_KIND_UNION,
                    prototype->name,
                    node) ||
            !ZrParser_Semantic_RebindSymbolType(
                    cs->semanticContext, symbolId, unionTypeId)) {
            ZrCore_Array_Free(cs->state, &genericParameterKinds);
            ZrCore_Array_Free(cs->state, &genericBindings);
            return ZR_FALSE;
        }
        /* RegisterCanonicalType does not project the nominal token for unions. */
        for (index = 0U; index < cs->semanticContext->types.length; index++) {
            SZrSemanticTypeRecord *record = (SZrSemanticTypeRecord *)ZrCore_Array_Get(
                    &cs->semanticContext->types, index);
            const SZrCanonicalTypeNode *definition = semantic_type_nominal_definition(
                    cs->semanticContext, unionTypeId);
            if (record != ZR_NULL && record->id == unionTypeId && definition != ZR_NULL) {
                record->inferredType.nominalDefinitionToken =
                        definition->data.nominal.definitionToken;
                break;
            }
        }
    } else if (!ZrParser_Semantic_PublishCanonicalTypeSymbol(
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
    if (!ZrParser_TypeEnvironment_RegisterTypeDeclaration(
                cs->state, cs->typeEnv, prototype->name, node) ||
        (cs->compileTimeTypeEnv != ZR_NULL &&
         !ZrParser_TypeEnvironment_RegisterTypeDeclaration(
                 cs->state, cs->compileTimeTypeEnv, prototype->name, node))) {
        ZrCore_Array_Free(cs->state, &genericParameterKinds);
        ZrCore_Array_Free(cs->state, &genericBindings);
        return ZR_FALSE;
    }

    ZrCore_Array_Free(cs->state, &genericParameterKinds);
    ZrCore_Array_Free(cs->state, &genericBindings);
    return ZR_TRUE;
}
