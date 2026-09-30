#include "zr_vm_parser/semantic_query.h"

#include <string.h>

#include "zr_vm_parser/canonical_type.h"

#include "semantic_relations_identity.h"
#include "semantic_relations_order.h"

/* source 标签先按对象身份匹配；独立分配但内容相同的标签仍指向同一源码。 */
static TZrBool semantic_relations_same_source(
        SZrString *left,
        SZrString *right) {
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
}

/* 关系快照保留 URI 到语义 state 的副本，避免依赖发布者临时字符串的寿命。 */
static SZrString *semantic_relations_clone_string(
        SZrSemanticContext *context,
        SZrString *value) {
    TZrNativeString text;

    if (context == ZR_NULL || context->state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }
    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(
            context->state, text, ZrCore_String_GetByteLength(value));
}

/* 同源范围优先按字节偏移包含；任一端缺偏移时回退到行列坐标。 */
static TZrBool semantic_relations_range_contains(
        const SZrFileRange *outer,
    const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !semantic_relations_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }
    if ((outer->start.offset > 0U || outer->end.offset > 0U) &&
        (inner->start.offset > 0U || inner->end.offset > 0U)) {
        return (TZrBool)(outer->start.offset <= inner->start.offset &&
                         inner->end.offset <= outer->end.offset);
    }
    return (TZrBool)(
            (outer->start.line < inner->start.line ||
             (outer->start.line == inner->start.line &&
              outer->start.column <= inner->start.column)) &&
            (inner->end.line < outer->end.line ||
             (inner->end.line == outer->end.line &&
              inner->end.column <= outer->end.column)));
}

/* 全零且无 source 的范围是缺省 sentinel，不能充当关系端点位置。 */
static TZrBool semantic_relations_range_is_known(const SZrFileRange *range) {
    return (TZrBool)(range != ZR_NULL &&
                      (range->source != ZR_NULL ||
                       range->start.offset > 0U || range->end.offset > 0U ||
                       range->start.line > 0 || range->end.line > 0 ||
                       range->start.column > 0 || range->end.column > 0));
}

static TZrBool semantic_relations_endpoint_has_identity(
        TZrSymbolId symbolId,
        TZrTypeId typeId) {
    return (TZrBool)(symbolId != ZR_SEMANTIC_ID_INVALID ||
                     typeId != ZR_SEMANTIC_ID_INVALID);
}

/* module scope 不过滤；node scope 只保留任一已知端点落在根节点同源范围内的边。 */
static TZrBool semantic_relations_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrSemanticRelationFact *fact) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    if (scope->kind != ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE ||
        scope->root == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)((fact->hasSourceRange &&
                       semantic_relations_range_contains(
                               &scope->root->location, &fact->sourceRange)) ||
                      (fact->hasTargetRange &&
                       semantic_relations_range_contains(
                               &scope->root->location, &fact->targetRange)));
}

/* 查询复用调用者数组时只清结果；新数组由快照 state 管理其缓冲区。 */
/* BUG: 新数组的 Array_Init 分配失败仍可能标为 valid 且 head 为空；命中关系后 Push 会断言或向空地址写入。尚无 OOM 注入复现。 */
static TZrBool semantic_relations_prepare_output(
        const SZrSemanticContext *context,
        SZrArray *outRelations) {
    if (context == ZR_NULL || outRelations == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outRelations->isValid) {
        if (outRelations->elementSize != sizeof(SZrParserSemanticRelationQuery)) {
            return ZR_FALSE;
        }
        outRelations->length = 0U;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      outRelations,
                      sizeof(SZrParserSemanticRelationQuery),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

/* module identity 只从 nominal 定义取得；generic instance 有界追到定义，异常链不猜测回退。 */
static SZrString *semantic_relations_module_identity_for_type(
        const SZrSemanticContext *context,
        TZrTypeId typeId) {
    TZrSize remaining;

    if (context == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    remaining = context->canonicalTypes.length + 1U;
    while (remaining-- > 0U) {
        const SZrCanonicalTypeNode *node =
                ZrParser_CanonicalType_Find(context, typeId);

        if (node == ZR_NULL) {
            return ZR_NULL;
        }
        if (node->kind == ZR_CANONICAL_TYPE_NOMINAL) {
            return node->data.nominal.moduleIdentity;
        }
        if (node->kind != ZR_CANONICAL_TYPE_GENERIC_INSTANCE ||
            node->data.genericInstance.definitionTypeId == ZR_SEMANTIC_ID_INVALID) {
            return ZR_NULL;
        }
        typeId = node->data.genericInstance.definitionTypeId;
    }
    return ZR_NULL;
}

/* 投影只复制查询值；range.source、URI 与 canonical module identity 继续借用当前语义快照。 */
/* BUG: DerivedTypesOf 等合法查询可返回超过初始容量的匹配边；Push 扩容分配失败会留下空 head，随后 RawCopy 向空地址写入，尚无 OOM 注入复现。 */
static void semantic_relations_append_query(
        const SZrSemanticContext *context,
        SZrArray *outRelations,
        const SZrSemanticRelationFact *fact) {
    SZrParserSemanticRelationQuery query;

    if (context == ZR_NULL || outRelations == ZR_NULL || fact == ZR_NULL) {
        return;
    }
    memset(&query, 0, sizeof(query));
    query.kind = fact->kind;
    query.sourceSymbolId = fact->sourceSymbolId;
    query.targetSymbolId = fact->targetSymbolId;
    query.sourceTypeId = fact->sourceTypeId;
    query.targetTypeId = fact->targetTypeId;
    query.sourceModuleIdentity = semantic_relations_module_identity_for_type(
            context, fact->sourceTypeId);
    query.targetModuleIdentity = semantic_relations_module_identity_for_type(
            context, fact->targetTypeId);
    query.sourceProviderGeneration = fact->sourceProviderGeneration;
    query.targetProviderGeneration = fact->targetProviderGeneration;
    query.sourceRange = fact->sourceRange;
    query.targetRange = fact->targetRange;
    query.externalOriginUri = fact->externalOriginUri;
    query.virtualDeclarationUri = fact->virtualDeclarationUri;
    query.hasSourceRange = fact->hasSourceRange;
    query.hasTargetRange = fact->hasTargetRange;
    query.isExternal = fact->isExternal;
    ZrCore_Array_Push(context->state, outRelations, &query);
}

/* relationFacts 与 semantic context 共用 state；调用者随后通过 Reset/Free 管理数组长度和存储。 */
/* BUG: Array_Init 分配失败仍可能保留 isValid=true、head=NULL；合法类型关系首次 Append 会断言或空地址写入，尚无 OOM 注入复现。 */
void ZrParser_SemanticRelations_Init(SZrSemanticContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }
    ZrCore_Array_Init(context->state,
                      &context->relationFacts,
                      sizeof(SZrSemanticRelationFact),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

/* 新快照只丢弃已有边，不释放数组容量或 state 所有的 URI 副本。 */
void ZrParser_SemanticRelations_Reset(SZrSemanticContext *context) {
    if (context != ZR_NULL && context->relationFacts.isValid) {
        context->relationFacts.length = 0U;
    }
}

/* 释放关系数组本身；URI 字符串由 context state 管理，不在此逐条释放。 */
void ZrParser_SemanticRelations_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }
    ZrParser_SemanticRelations_Reset(context);
    ZrCore_Array_Free(context->state, &context->relationFacts);
}

/* BUG: isValid 不保证关系缓冲区非空；Init 或扩容分配失败后，合法的 BASE_TYPE 发布仍会进入 Array_Push 并断言/空写。尚无 OOM 注入复现。 */
TZrBool ZrParser_SemanticRelations_Append(
        SZrSemanticContext *context,
        const SZrSemanticRelationFact *fact) {
    SZrSemanticRelationFact copy;
    TZrSize index;

    if (context == ZR_NULL || fact == ZR_NULL || !context->relationFacts.isValid ||
        fact->kind == ZR_SEMANTIC_RELATION_UNKNOWN ||
        !semantic_relations_endpoint_has_identity(
                fact->sourceSymbolId, fact->sourceTypeId) ||
        !semantic_relations_endpoint_has_identity(
                fact->targetSymbolId, fact->targetTypeId) ||
        (fact->isExternal && !fact->hasSourceRange &&
         (fact->externalOriginUri == ZR_NULL ||
          fact->virtualDeclarationUri == ZR_NULL))) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *existing =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        &context->relationFacts, index);
        if (ZrParser_SemanticRelations_FactsEqual(existing, fact)) {
            return ZR_TRUE;
        }
    }
    copy = *fact;
    copy.externalOriginUri = semantic_relations_clone_string(context, fact->externalOriginUri);
    if (fact->externalOriginUri != ZR_NULL && copy.externalOriginUri == ZR_NULL) {
        return ZR_FALSE;
    }
    copy.virtualDeclarationUri = semantic_relations_clone_string(
            context, fact->virtualDeclarationUri);
    if (fact->virtualDeclarationUri != ZR_NULL &&
        copy.virtualDeclarationUri == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->relationFacts, &copy);
    return ZR_TRUE;
}

/* property/accessor 边以 kind 和两个稳定 SymbolId 判重，支持 compiler 与 LSP 重复发布。 */
static TZrBool semantic_relations_has_property_accessor(
        const SZrSemanticContext *context,
        TZrSymbolId propertySymbolId,
        TZrSymbolId accessorSymbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_RELATION_PROPERTY_ACCESSOR &&
            fact->sourceSymbolId == propertySymbolId &&
            fact->targetSymbolId == accessorSymbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 未配置的 optional accessor 合法跳过；已配置项必须解析为同类型的 callable function。 */
static TZrBool semantic_relations_property_accessor_is_valid(
        const SZrSemanticContext *context,
        TZrSymbolId accessorSymbolId,
        TZrTypeId callableTypeId) {
    const SZrSemanticSymbolRecord *accessor;

    if (accessorSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    accessor = ZrParser_Semantic_FindSymbolById(context, accessorSymbolId);
    return (TZrBool)(accessor != ZR_NULL &&
                      accessor->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
                      callableTypeId != ZR_SEMANTIC_ID_INVALID &&
                      accessor->typeId == callableTypeId);
}

/* 将 canonical property contract 与当前 symbol 表交叉核对后才允许生成关系。 */
static TZrBool semantic_relations_property_contract_is_valid(
        const SZrSemanticContext *context,
        const SZrSemanticPropertyContract *contract) {
    const SZrSemanticSymbolRecord *property;

    if (context == ZR_NULL || contract == ZR_NULL ||
        contract->propertySymbolId == ZR_SEMANTIC_ID_INVALID ||
        contract->propertyTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    property = ZrParser_Semantic_FindSymbolById(
            context, contract->propertySymbolId);
    return (TZrBool)(property != ZR_NULL &&
                      property->kind == ZR_SEMANTIC_SYMBOL_KIND_PROPERTY &&
                      property->typeId == contract->propertyTypeId &&
                      semantic_relations_property_accessor_is_valid(
                              context,
                              contract->getterSymbolId,
                              contract->getterCallableTypeId) &&
                      semantic_relations_property_accessor_is_valid(
                              context,
                              contract->setterSymbolId,
                              contract->setterCallableTypeId) &&
                      semantic_relations_property_accessor_is_valid(
                              context,
                              contract->initializerSymbolId,
                              contract->initializerCallableTypeId));
}

/* 同一声明的不同写入位置各自保留；仅精确相同的目标 range 视为重复。 */
static TZrBool semantic_relations_has_reference_definition(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrFileRange *definitionRange) {
    TZrSize index;

    if (context == ZR_NULL || definitionRange == ZR_NULL ||
        !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION &&
            fact->sourceSymbolId == symbolId &&
            fact->targetSymbolId == symbolId &&
            fact->hasTargetRange &&
            ZrParser_SemanticRelations_RangesEqual(
                    &fact->targetRange, definitionRange)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 单个 accessor 从 property declaration 指向 accessor location，再由 Append 保存快照。 */
static TZrBool semantic_relations_publish_property_accessor(
        SZrSemanticContext *context,
        const SZrSemanticPropertyContract *contract,
        TZrSymbolId accessorSymbolId,
        TZrTypeId callableTypeId) {
    const SZrSemanticSymbolRecord *accessor;
    SZrSemanticRelationFact fact;

    if (accessorSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    accessor = ZrParser_Semantic_FindSymbolById(context, accessorSymbolId);
    if (!semantic_relations_property_accessor_is_valid(
                context, accessorSymbolId, callableTypeId) ||
        accessor == ZR_NULL) {
        return ZR_FALSE;
    }
    if (semantic_relations_has_property_accessor(
                context, contract->propertySymbolId, accessorSymbolId)) {
        return ZR_TRUE;
    }

    memset(&fact, 0, sizeof(fact));
    fact.kind = ZR_SEMANTIC_RELATION_PROPERTY_ACCESSOR;
    fact.sourceSymbolId = contract->propertySymbolId;
    fact.targetSymbolId = accessorSymbolId;
    fact.sourceTypeId = contract->propertyTypeId;
    fact.targetTypeId = callableTypeId;
    fact.sourceRange = contract->declarationRange;
    fact.targetRange = accessor->location;
    fact.hasSourceRange = semantic_relations_range_is_known(&fact.sourceRange);
    fact.hasTargetRange = semantic_relations_range_is_known(&fact.targetRange);
    return ZrParser_SemanticRelations_Append(context, &fact);
}

/* 完整合同先验证、再逐项投影；仅验证错误不会留下前半批边，发布阶段失败不回滚已追加项。 */
TZrBool ZrParser_SemanticRelations_PublishPropertyContracts(
        SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->propertyContracts.isValid ||
        !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    /* 先验证整批合同，避免后续无效条目令前面条目已发布。 */
    for (index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *contract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        &context->propertyContracts, index);

        if (!semantic_relations_property_contract_is_valid(context, contract)) {
            return ZR_FALSE;
        }
    }
    for (index = 0U; index < context->propertyContracts.length; index++) {
        const SZrSemanticPropertyContract *contract =
                (const SZrSemanticPropertyContract *)ZrCore_Array_Get(
                        &context->propertyContracts, index);

        if (!semantic_relations_publish_property_accessor(
                    context,
                    contract,
                    contract->getterSymbolId,
                    contract->getterCallableTypeId) ||
            !semantic_relations_publish_property_accessor(
                    context,
                    contract,
                    contract->setterSymbolId,
                    contract->setterCallableTypeId) ||
            !semantic_relations_publish_property_accessor(
                    context,
                    contract,
                    contract->initializerSymbolId,
                    contract->initializerCallableTypeId)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* 只投影已解析 WRITE fact；缺少声明身份或任一可信位置的项保持未发布。 */
TZrBool ZrParser_SemanticRelations_PublishReferenceDefinitions(
        SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid ||
        !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts, index);
        const SZrSemanticSymbolRecord *symbol;
        SZrSemanticRelationFact fact;

        if (reference == ZR_NULL || !reference->isResolved ||
            reference->kind != ZR_SEMANTIC_REFERENCE_WRITE ||
            reference->symbolId == ZR_SEMANTIC_ID_INVALID ||
            !reference->hasDefinitionRange ||
            !semantic_relations_range_is_known(&reference->definitionRange)) {
            continue;
        }
        symbol = ZrParser_Semantic_FindSymbolById(context, reference->symbolId);
        if (symbol == ZR_NULL || symbol->typeId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_relations_range_is_known(&symbol->location) ||
            semantic_relations_has_reference_definition(
                    context, reference->symbolId, &reference->definitionRange)) {
            continue;
        }

        memset(&fact, 0, sizeof(fact));
        fact.kind = ZR_SEMANTIC_RELATION_DECLARATION_DEFINITION;
        fact.sourceSymbolId = reference->symbolId;
        fact.targetSymbolId = reference->symbolId;
        fact.sourceTypeId = symbol->typeId;
        fact.targetTypeId = symbol->typeId;
        fact.sourceRange = symbol->location;
        fact.targetRange = reference->definitionRange;
        fact.hasSourceRange = ZR_TRUE;
        fact.hasTargetRange = ZR_TRUE;
        if (!ZrParser_SemanticRelations_Append(context, &fact)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* import origin 按来源符号、目标类型和 URI 内容查重，允许两条发布路径收敛。 */
static TZrBool semantic_relations_has_import_origin(
        const SZrSemanticContext *context,
        TZrSymbolId sourceSymbolId,
        TZrTypeId targetTypeId,
        SZrString *originUri) {
    TZrSize index;

    if (context == ZR_NULL || originUri == ZR_NULL || !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN &&
            fact->sourceSymbolId == sourceSymbolId &&
            fact->targetTypeId == targetTypeId && fact->isExternal &&
            fact->externalOriginUri != ZR_NULL &&
            ZrCore_String_Equal(fact->externalOriginUri, originUri)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 仅把可见 import 的 external origin 投影为事实；resolver 返回值由 Append 及时克隆。 */
TZrBool ZrParser_SemanticRelations_PublishImportOrigins(
        SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->visibleSymbolFacts.isValid ||
        !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->visibleSymbolFacts.length; index++) {
        const SZrSemanticVisibleSymbolFact *visible =
                (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                        &context->visibleSymbolFacts, index);
        const SZrSemanticSymbolRecord *symbol;
        SZrSemanticRelationFact fact;

        if (visible == ZR_NULL || !visible->isImport ||
            visible->symbolId == ZR_SEMANTIC_ID_INVALID ||
            visible->externalOriginUri == ZR_NULL) {
            continue;
        }
        symbol = ZrParser_Semantic_FindSymbolById(context, visible->symbolId);
        if (symbol == ZR_NULL || symbol->typeId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_relations_range_is_known(&symbol->location) ||
            semantic_relations_has_import_origin(
                    context, symbol->id, symbol->typeId, visible->externalOriginUri)) {
            continue;
        }

        memset(&fact, 0, sizeof(fact));
        fact.kind = ZR_SEMANTIC_RELATION_IMPORT_EXPORT_ORIGIN;
        fact.sourceSymbolId = symbol->id;
        fact.sourceTypeId = symbol->typeId;
        fact.targetTypeId = symbol->typeId;
        fact.sourceRange = symbol->location;
        fact.externalOriginUri = visible->externalOriginUri;
        if (context->virtualDeclarationUriResolver != ZR_NULL) {
            fact.virtualDeclarationUri = context->virtualDeclarationUriResolver(
                    context,
                    visible->externalOriginUri,
                    context->virtualDeclarationUriResolverUserData);
        }
        fact.hasSourceRange = ZR_TRUE;
        fact.isExternal = ZR_TRUE;
        if (!ZrParser_SemanticRelations_Append(context, &fact)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* resolver 与 userData 仅借用保存；同步发布期间均须有效，context reset 后需重新注册。 */
void ZrParser_SemanticRelations_SetVirtualDeclarationUriResolver(
        SZrSemanticContext *context,
        FZrSemanticVirtualDeclarationUriResolver resolver,
        TZrPtr userData) {
    if (context == ZR_NULL) {
        return;
    }

    context->virtualDeclarationUriResolver = resolver;
    context->virtualDeclarationUriResolverUserData = userData;
}

/* alias-to-type 以来源符号和目标 TypeId 去重。 */
static TZrBool semantic_relations_has_alias_target(
        const SZrSemanticContext *context,
        TZrSymbolId sourceSymbolId,
        TZrTypeId targetTypeId) {
    TZrSize index;

    if (context == ZR_NULL || !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);

        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_RELATION_ALIAS_TARGET &&
            fact->sourceSymbolId == sourceSymbolId &&
            fact->targetTypeId == targetTypeId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 从可见 alias 符号发布其已解析 TypeId；缺少位置/身份的项不形成导航边。 */
TZrBool ZrParser_SemanticRelations_PublishAliasTargets(
        SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->visibleSymbolFacts.isValid ||
        !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->visibleSymbolFacts.length; index++) {
        const SZrSemanticVisibleSymbolFact *visible =
                (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                        &context->visibleSymbolFacts, index);
        const SZrSemanticSymbolRecord *symbol;
        SZrSemanticRelationFact fact;

        if (visible == ZR_NULL || !visible->isAlias ||
            visible->symbolId == ZR_SEMANTIC_ID_INVALID) {
            continue;
        }
        symbol = ZrParser_Semantic_FindSymbolById(context, visible->symbolId);
        if (symbol == ZR_NULL || symbol->typeId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_relations_range_is_known(&symbol->location) ||
            semantic_relations_has_alias_target(
                    context, symbol->id, symbol->typeId)) {
            continue;
        }

        memset(&fact, 0, sizeof(fact));
        fact.kind = ZR_SEMANTIC_RELATION_ALIAS_TARGET;
        fact.sourceSymbolId = symbol->id;
        fact.sourceTypeId = symbol->typeId;
        fact.targetTypeId = symbol->typeId;
        fact.sourceRange = symbol->location;
        fact.hasSourceRange = ZR_TRUE;
        if (!ZrParser_SemanticRelations_Append(context, &fact)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* 只接受 symbol 表中 AST 指针完全相同且具有有效类型身份与位置的类型声明。 */
static const SZrSemanticSymbolRecord *semantic_relations_find_type_declaration(
        const SZrSemanticContext *context,
        const SZrAstNode *declaration) {
    TZrSize index;

    if (context == ZR_NULL || declaration == ZR_NULL || !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (symbol != ZR_NULL && symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE &&
            symbol->astNode == declaration &&
            symbol->typeId != ZR_SEMANTIC_ID_INVALID &&
            semantic_relations_range_is_known(&symbol->location)) {
            return symbol;
        }
    }
    return ZR_NULL;
}

/* AST 指针必须唯一映射到一个 SymbolId；多身份映射按歧义拒绝。 */
static const SZrSemanticSymbolRecord *semantic_relations_find_symbol_declaration(
        const SZrSemanticContext *context,
        const SZrAstNode *declaration) {
    const SZrSemanticSymbolRecord *matched = ZR_NULL;
    TZrSize index;

    if (context == ZR_NULL || declaration == ZR_NULL ||
        !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (symbol == ZR_NULL || symbol->astNode != declaration ||
            symbol->id == ZR_SEMANTIC_ID_INVALID ||
            symbol->typeId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_relations_range_is_known(&symbol->location)) {
            continue;
        }
        if (matched != ZR_NULL && matched->id != symbol->id) {
            return ZR_NULL;
        }
        matched = symbol;
    }
    return matched;
}

/* compiler relation wrapper 按 kind 与端点 SymbolId 保证重复发布幂等。 */
static TZrBool semantic_relations_has_symbol_relation(
        const SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        TZrSymbolId sourceSymbolId,
        TZrSymbolId targetSymbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->relationFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL && fact->kind == kind &&
            fact->sourceSymbolId == sourceSymbolId &&
            fact->targetSymbolId == targetSymbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 仅接受由 compiler 建立的四类成员/层级边，并要求两端均有可导航身份和位置。 */
TZrBool ZrParser_SemanticRelations_PublishSymbolRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        TZrSymbolId sourceSymbolId,
        TZrSymbolId targetSymbolId) {
    const SZrSemanticSymbolRecord *source;
    const SZrSemanticSymbolRecord *target;
    SZrSemanticRelationFact fact;

    if (context == ZR_NULL || !context->relationFacts.isValid ||
        (kind != ZR_SEMANTIC_RELATION_BASE_TYPE &&
         kind != ZR_SEMANTIC_RELATION_IMPLEMENTATION &&
         kind != ZR_SEMANTIC_RELATION_OVERRIDE &&
         kind != ZR_SEMANTIC_RELATION_CONSTRUCTOR) ||
        sourceSymbolId == ZR_SEMANTIC_ID_INVALID ||
        targetSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    source = ZrParser_Semantic_FindSymbolById(context, sourceSymbolId);
    target = ZrParser_Semantic_FindSymbolById(context, targetSymbolId);
    if (source == ZR_NULL || target == ZR_NULL ||
        source->typeId == ZR_SEMANTIC_ID_INVALID ||
        target->typeId == ZR_SEMANTIC_ID_INVALID ||
        !semantic_relations_range_is_known(&source->location) ||
        !semantic_relations_range_is_known(&target->location)) {
        return ZR_FALSE;
    }
    if (semantic_relations_has_symbol_relation(
                context, kind, sourceSymbolId, targetSymbolId)) {
        return ZR_TRUE;
    }

    memset(&fact, 0, sizeof(fact));
    fact.kind = kind;
    fact.sourceSymbolId = sourceSymbolId;
    fact.targetSymbolId = targetSymbolId;
    fact.sourceTypeId = source->typeId;
    fact.targetTypeId = target->typeId;
    fact.sourceRange = source->location;
    fact.targetRange = target->location;
    fact.hasSourceRange = ZR_TRUE;
    fact.hasTargetRange = ZR_TRUE;
    return ZrParser_SemanticRelations_Append(context, &fact);
}

/* 把精确 AST 声明映射到稳定符号后复用统一关系校验与去重路径。 */
TZrBool ZrParser_SemanticRelations_PublishSymbolDeclarationRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        const SZrAstNode *sourceDeclaration,
        const SZrAstNode *targetDeclaration) {
    const SZrSemanticSymbolRecord *source;
    const SZrSemanticSymbolRecord *target;

    if (context == ZR_NULL || sourceDeclaration == ZR_NULL ||
        targetDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }
    source = semantic_relations_find_symbol_declaration(
            context, sourceDeclaration);
    target = semantic_relations_find_symbol_declaration(
            context, targetDeclaration);
    if (source == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrParser_SemanticRelations_PublishSymbolRelation(
            context, kind, source->id, target->id);
}

/* 层级边只接受 symbol 表中精确匹配的类型声明，不从名称相似度推断目标。 */
TZrBool ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
        SZrSemanticContext *context,
        EZrSemanticRelationKind kind,
        const SZrAstNode *sourceDeclaration,
        const SZrAstNode *targetDeclaration) {
    const SZrSemanticSymbolRecord *source;
    const SZrSemanticSymbolRecord *target;

    if (context == ZR_NULL || !context->relationFacts.isValid ||
        (kind != ZR_SEMANTIC_RELATION_BASE_TYPE &&
         kind != ZR_SEMANTIC_RELATION_IMPLEMENTATION)) {
        return ZR_FALSE;
    }
    source = semantic_relations_find_type_declaration(context, sourceDeclaration);
    target = semantic_relations_find_type_declaration(context, targetDeclaration);
    if (source == ZR_NULL || target == ZR_NULL ||
        source->id == ZR_SEMANTIC_ID_INVALID || target->id == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    return ZrParser_SemanticRelations_PublishSymbolRelation(
            context, kind, source->id, target->id);
}

/* 构造关系由类型声明和已解析构造函数符号组成，方向为类型到构造函数。 */
TZrBool ZrParser_SemanticRelations_PublishConstructorRelation(
        SZrSemanticContext *context,
        const SZrAstNode *sourceTypeDeclaration,
        TZrSymbolId constructorSymbolId) {
    const SZrSemanticSymbolRecord *source;

    if (context == ZR_NULL || constructorSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    source = semantic_relations_find_type_declaration(context, sourceTypeDeclaration);
    if (source == ZR_NULL || source->id == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    return ZrParser_SemanticRelations_PublishSymbolRelation(
            context,
            ZR_SEMANTIC_RELATION_CONSTRUCTOR,
            source->id,
            constructorSymbolId);
}

/**
 * @brief 返回命中符号作为任一端点的直接关系，并按共享关系排序键排序。
 *
 * outRelations 可传零初始化数组或同元素类型的已初始化数组；context 与输出存储
 * 兼容时函数先清空结果，已初始化数组若元素类型不符则在清空前返回 false。
 * 查询范围为 NULL/module 时不过滤，node 范围要求关系任一端的 range 位于根节点内。
 * 数组缓冲区使用 context state；range.source、URI 和模块身份仍借用 snapshot，
 * 调用方须在 reset/free snapshot 前消费这些字段，并使用同一 state 释放数组。
 *
 * @return 查询完成且至少有一条匹配关系时为 true；context、输出存储或符号无效，或无匹配项时为 false。
 */
TZrBool ZrParser_SemanticQuery_RelationsOfSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outRelations) {
    TZrSize index;

    if (context == ZR_NULL || !context->relationFacts.isValid ||
        !semantic_relations_prepare_output(context, outRelations)) {
        return ZR_FALSE;
    }
    if (symbolId == ZR_SEMANTIC_ID_INVALID) return ZR_FALSE;
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL &&
            (fact->sourceSymbolId == symbolId || fact->targetSymbolId == symbolId) &&
            semantic_relations_scope_allows(scope, fact)) {
            semantic_relations_append_query(context, outRelations, fact);
        }
    }
    ZrParser_SemanticRelations_SortQueries(outRelations);
    return outRelations->length > 0U;
}

/**
 * @brief 返回指向目标符号的直接 implementation/override 关系，不展开传递闭包。
 *
 * outRelations 可传零初始化数组或同元素类型的已初始化数组；context 与输出存储
 * 兼容时函数先清空结果，已初始化数组若元素类型不符则在清空前返回 false。
 * 查询范围为 NULL/module 时不过滤，node 范围要求关系任一端的 range 位于根节点内。
 * 结果数组由 context state 分配；其 range.source、URI 和模块身份借用当前快照，
 * 必须先消费再 reset/free snapshot，并由调用方用同一 state 释放数组。
 *
 * @return 查询完成且至少有一条匹配关系时为 true；context、输出存储或符号无效，或无匹配项时为 false。
 */
TZrBool ZrParser_SemanticQuery_ImplementationsOf(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outRelations) {
    TZrSize index;

    if (context == ZR_NULL || !context->relationFacts.isValid ||
        !semantic_relations_prepare_output(context, outRelations)) {
        return ZR_FALSE;
    }
    if (symbolId == ZR_SEMANTIC_ID_INVALID) return ZR_FALSE;
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL &&
            (fact->kind == ZR_SEMANTIC_RELATION_IMPLEMENTATION ||
             fact->kind == ZR_SEMANTIC_RELATION_OVERRIDE) &&
            fact->targetSymbolId == symbolId &&
            semantic_relations_scope_allows(scope, fact)) {
            semantic_relations_append_query(context, outRelations, fact);
        }
    }
    ZrParser_SemanticRelations_SortQueries(outRelations);
    return outRelations->length > 0U;
}

/**
 * @brief 返回指定类型作为 derived endpoint 的直接 BASE_TYPE 关系。
 *
 * outRelations 可传零初始化数组或同元素类型的已初始化数组；context 与输出存储
 * 兼容时函数先清空结果，已初始化数组若元素类型不符则在清空前返回 false。
 * 本查询不做传递展开；range.source、URI 和模块身份借用当前 snapshot，结果数组
 * 使用 context state，调用方须在 snapshot reset/free 前消费借用字段并释放数组。
 *
 * @return 查询完成且至少有一条匹配关系时为 true；context、输出存储或类型无效，或无匹配项时为 false。
 */
TZrBool ZrParser_SemanticQuery_BaseTypesOf(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        SZrArray *outRelations) {
    TZrSize index;

    if (context == ZR_NULL || !context->relationFacts.isValid ||
        !semantic_relations_prepare_output(context, outRelations)) {
        return ZR_FALSE;
    }
    if (typeId == ZR_SEMANTIC_ID_INVALID) return ZR_FALSE;
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_RELATION_BASE_TYPE &&
            fact->sourceTypeId == typeId) {
            semantic_relations_append_query(context, outRelations, fact);
        }
    }
    ZrParser_SemanticRelations_SortQueries(outRelations);
    return outRelations->length > 0U;
}

/**
 * @brief 返回指定类型作为 base endpoint 的直接派生类型关系。
 *
 * outRelations 可传零初始化数组或同元素类型的已初始化数组；context 与输出存储
 * 兼容时函数先清空结果，已初始化数组若元素类型不符则在清空前返回 false。
 * 本查询不做传递展开；range.source、URI 和模块身份借用当前 snapshot，结果数组
 * 使用 context state，调用方须在 snapshot reset/free 前消费借用字段并释放数组。
 *
 * @return 查询完成且至少有一条匹配关系时为 true；context、输出存储或类型无效，或无匹配项时为 false。
 */
TZrBool ZrParser_SemanticQuery_DerivedTypesOf(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        SZrArray *outRelations) {
    TZrSize index;

    if (context == ZR_NULL || !context->relationFacts.isValid ||
        !semantic_relations_prepare_output(context, outRelations)) {
        return ZR_FALSE;
    }
    if (typeId == ZR_SEMANTIC_ID_INVALID) return ZR_FALSE;
    for (index = 0U; index < context->relationFacts.length; index++) {
        const SZrSemanticRelationFact *fact =
                (const SZrSemanticRelationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->relationFacts, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_RELATION_BASE_TYPE &&
            fact->targetTypeId == typeId) {
            semantic_relations_append_query(context, outRelations, fact);
        }
    }
    ZrParser_SemanticRelations_SortQueries(outRelations);
    return outRelations->length > 0U;
}
