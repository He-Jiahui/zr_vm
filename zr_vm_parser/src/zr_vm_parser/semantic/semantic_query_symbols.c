#include "zr_vm_parser/semantic_query.h"

#include <string.h>

/* VisibleSymbols 的临时排序项；record 借用当前 context 的符号数组，
 * 只在本次查询内用于遮蔽判断，最终仅把 symbol 的值副本写入输出。
 */
typedef struct SZrSemanticVisibleSymbolCandidate {
    SZrParserSemanticSymbolQuery symbol;
    const SZrSemanticSymbolRecord *record;
    TZrUInt32 scopeDistance;
    TZrUInt32 declarationOrder;
    TZrOverloadSetId overloadSetId;
} SZrSemanticVisibleSymbolCandidate;

static TZrBool semantic_query_symbols_same_source(
        SZrString *left,
        SZrString *right) {
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
}

static TZrBool semantic_query_symbols_range_contains(
        const SZrFileRange *range,
        const SZrFileRange *position) {
    if (range == ZR_NULL || position == ZR_NULL ||
        !semantic_query_symbols_same_source(range->source, position->source)) {
        return ZR_FALSE;
    }
    return range->start.offset <= position->start.offset &&
           range->end.offset >= position->start.offset;
}

static TZrSize semantic_query_symbols_range_width(const SZrFileRange *range) {
    if (range == ZR_NULL || range->end.offset < range->start.offset) {
        return (TZrSize)-1;
    }
    return range->end.offset - range->start.offset;
}

/**
 * @brief NULL/MODULE 接受任意位置，NODE 仅接受其 root 源码范围内的位置。
 * @note NODE 缺少 root 或未知 kind 时拒绝查询；root 借用调用方 AST，不延长其生命周期。
 */
static TZrBool semantic_query_symbols_scope_allows_position(
        const SZrParserSemanticQueryScope *scope,
        SZrFileRange position) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    return scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE && scope->root != ZR_NULL &&
           semantic_query_symbols_range_contains(&scope->root->location, &position);
}

/* 沿 scope ID 的 parent 链判断祖先关系；深度上限取已发布 scope 数，避免坏链循环。 */
static TZrBool semantic_query_symbols_scope_descends_from(
        const SZrSemanticContext *context,
        const SZrSemanticScopeFact *candidate,
        const SZrSemanticScopeFact *ancestor) {
    const SZrSemanticScopeFact *current = candidate;
    TZrSize depth = 0U;

    if (context == ZR_NULL || candidate == ZR_NULL || ancestor == ZR_NULL) {
        return ZR_FALSE;
    }
    while (current != ZR_NULL && depth < context->scopeFacts.length) {
        if (current->parentScopeId == ancestor->id) {
            return ZR_TRUE;
        }
        current = ZrParser_Semantic_FindScopeFactById(context, current->parentScopeId);
        depth++;
    }
    return ZR_FALSE;
}

/* 从覆盖位置的事实中优先选择树上更深的作用域；非祖先候选按源码范围宽度取窄者。 */
static const SZrSemanticScopeFact *semantic_query_symbols_find_innermost_scope(
        const SZrSemanticContext *context,
        SZrFileRange position) {
    const SZrSemanticScopeFact *best = ZR_NULL;
    TZrSize bestWidth = 0U;
    TZrSize index;

    if (context == ZR_NULL || !context->scopeFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->scopeFacts.length; index++) {
        const SZrSemanticScopeFact *candidate =
                (const SZrSemanticScopeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->scopeFacts, index);
        TZrSize width;

        if (candidate == ZR_NULL ||
            !semantic_query_symbols_range_contains(&candidate->range, &position)) {
            continue;
        }
        width = semantic_query_symbols_range_width(&candidate->range);
        if (best == ZR_NULL ||
            semantic_query_symbols_scope_descends_from(context, candidate, best) ||
            (!semantic_query_symbols_scope_descends_from(context, best, candidate) &&
             width < bestWidth)) {
            best = candidate;
            bestWidth = width;
        }
    }
    return best;
}

/* 提升声明不受位置限制；其余绑定需与查询位置同源，且声明起点不晚于查询偏移。 */
static TZrBool semantic_query_symbols_is_available(
        const SZrSemanticVisibleSymbolFact *fact,
        SZrFileRange position) {
    if (fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fact->isHoisted) {
        return ZR_TRUE;
    }
    return semantic_query_symbols_same_source(
                   fact->declarationRange.source, position.source) &&
           fact->declarationRange.start.offset <= position.start.offset;
}

/* 在可用性之外应用可选类别、可访问性与静态接收者上下文筛选。 */
static TZrBool semantic_query_symbols_is_eligible(
        const SZrSemanticVisibleSymbolFact *fact,
        const SZrSemanticScopeFact *activeScope,
        SZrFileRange position,
        const SZrParserSemanticVisibleSymbolOptions *options) {
    if (fact == ZR_NULL || activeScope == ZR_NULL ||
        !semantic_query_symbols_is_available(fact, position)) {
        return ZR_FALSE;
    }
    if (fact->isReceiverMember &&
        (options == ZR_NULL || !options->includeReceiverMembers)) {
        return ZR_FALSE;
    }
    if ((fact->isImport || fact->isAlias) &&
        (options == ZR_NULL || !options->includeImports)) {
        return ZR_FALSE;
    }
    if (!fact->isAccessible &&
        (options == ZR_NULL || !options->includeInaccessible)) {
        return ZR_FALSE;
    }
    return !(activeScope->isStaticContext && fact->isReceiverMember && !fact->isStatic);
}

/* 类型名占独立命名空间；其他符号种类在此查询中共享值命名空间。 */
static TZrBool semantic_query_symbols_same_namespace(
        const SZrSemanticSymbolRecord *left,
        const SZrSemanticSymbolRecord *right) {
    TZrBool leftIsType;
    TZrBool rightIsType;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    leftIsType = left->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE;
    rightIsType = right->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE;
    return leftIsType == rightIsType;
}

/* 同名且同命名空间时，内层候选遮蔽外层候选；同一有效 overload set 保留全部成员。 */
static TZrBool semantic_query_symbols_is_shadowed(
        const SZrArray *candidates,
        const SZrSemanticVisibleSymbolCandidate *candidate) {
    TZrSize index;

    if (candidates == ZR_NULL || candidate == ZR_NULL || candidate->record == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < candidates->length; index++) {
        const SZrSemanticVisibleSymbolCandidate *existing =
                (const SZrSemanticVisibleSymbolCandidate *)ZrCore_Array_Get(
                        (SZrArray *)candidates, index);
        if (existing == ZR_NULL || existing->record == ZR_NULL ||
            !semantic_query_symbols_same_namespace(existing->record, candidate->record) ||
            existing->record->name == ZR_NULL || candidate->record->name == ZR_NULL ||
            !ZrCore_String_Equal(existing->record->name, candidate->record->name)) {
            continue;
        }
        if (existing->overloadSetId != ZR_SEMANTIC_ID_INVALID &&
            existing->overloadSetId == candidate->overloadSetId) {
            continue;
        }
        if (existing->scopeDistance <= candidate->scopeDistance) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 可见结果先按词法距离、再按声明顺序、最后按 SymbolId 排序。 */
static TZrBool semantic_query_symbols_candidate_precedes(
        const SZrSemanticVisibleSymbolCandidate *left,
        const SZrSemanticVisibleSymbolCandidate *right) {
    if (left->scopeDistance != right->scopeDistance) {
        return left->scopeDistance < right->scopeDistance;
    }
    if (left->declarationOrder != right->declarationOrder) {
        return left->declarationOrder < right->declarationOrder;
    }
    return left->symbol.symbolId < right->symbol.symbolId;
}

static void semantic_query_symbols_sort(SZrArray *candidates) {
    TZrSize index;

    if (candidates == ZR_NULL) {
        return;
    }
    for (index = 1U; index < candidates->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrSemanticVisibleSymbolCandidate *before =
                    (SZrSemanticVisibleSymbolCandidate *)ZrCore_Array_Get(
                            candidates, current - 1U);
            SZrSemanticVisibleSymbolCandidate *after =
                    (SZrSemanticVisibleSymbolCandidate *)ZrCore_Array_Get(candidates, current);
            SZrSemanticVisibleSymbolCandidate swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_query_symbols_candidate_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

/* 新建或清空同元素类型的值数组；元素类型不匹配时失败且不改动原数组。 */
static TZrBool semantic_query_symbols_prepare_output(
        const SZrSemanticContext *context,
        SZrArray *outSymbols) {
    if (context == ZR_NULL || outSymbols == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outSymbols->isValid) {
        if (outSymbols->elementSize != sizeof(SZrParserSemanticSymbolQuery)) {
            return ZR_FALSE;
        }
        outSymbols->length = 0U;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      outSymbols,
                      sizeof(SZrParserSemanticSymbolQuery),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

/**
 * @brief 对声明投影应用相同 MODULE/NODE 边界，并拒绝空声明或无 root 的 NODE scope。
 * @note NULL scope 与 MODULE 不限制范围；NODE 的 source/range 必须通过同源范围判定。
 */
static TZrBool semantic_query_symbols_scope_allows_declaration(
        const SZrParserSemanticQueryScope *scope,
        const SZrSemanticReferenceFact *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_FALSE;
    }
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    return scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
           scope->root != ZR_NULL &&
           semantic_query_symbols_range_contains(
                   &scope->root->location, &declaration->range);
}

static TZrBool semantic_query_symbols_contains_id(
        const SZrArray *symbols,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (symbols == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    for (index = 0U; index < symbols->length; index++) {
        const SZrParserSemanticSymbolQuery *symbol =
                (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                        (SZrArray *)symbols, index);
        if (symbol != ZR_NULL && symbol->symbolId == symbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_query_symbols_declaration_precedes(
        const SZrParserSemanticSymbolQuery *left,
        const SZrParserSemanticSymbolQuery *right) {
    if (left->declarationRange.start.offset != right->declarationRange.start.offset) {
        return left->declarationRange.start.offset < right->declarationRange.start.offset;
    }
    if (left->declarationRange.end.offset != right->declarationRange.end.offset) {
        return left->declarationRange.end.offset < right->declarationRange.end.offset;
    }
    return left->symbolId < right->symbolId;
}

static void semantic_query_symbols_sort_declarations(SZrArray *symbols) {
    TZrSize index;

    if (symbols == ZR_NULL) {
        return;
    }
    for (index = 1U; index < symbols->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticSymbolQuery *before =
                    (SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                            symbols, current - 1U);
            SZrParserSemanticSymbolQuery *after =
                    (SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                            symbols, current);
            SZrParserSemanticSymbolQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_query_symbols_declaration_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

/* 外部引用必须带完整 provider 身份；canonical typeId 与 provider generation 不参与此门槛。 */
static TZrBool semantic_query_external_reference_is_complete(
        const SZrSemanticReferenceFact *reference) {
    return reference != ZR_NULL && reference->isResolved &&
           reference->symbolId != ZR_SEMANTIC_ID_INVALID &&
           reference->hasExternalTarget &&
           reference->externalTargetKind != ZR_SEMANTIC_EXTERNAL_TARGET_UNKNOWN &&
           reference->externalOwnerIdentity != ZR_NULL &&
           ZrCore_String_GetByteLength(reference->externalOwnerIdentity) > 0U &&
           reference->externalMetadataToken != 0U &&
           reference->externalSignatureToken != 0U &&
           reference->externalSignatureHash != 0U;
}

/* 外部引用先按 source 指针非 NULL 排序（包括指向空字符串的非 NULL 指针），再按范围、token 和 SymbolId。 */
static TZrBool semantic_query_external_reference_precedes(
        const SZrParserSemanticExternalReferenceQuery *left,
        const SZrParserSemanticExternalReferenceQuery *right) {
    const TZrChar *leftSource;
    const TZrChar *rightSource;
    TZrInt32 sourceOrder;

    if (left == ZR_NULL || right == ZR_NULL) {
        return left != ZR_NULL;
    }
    leftSource = left->referenceRange.source != ZR_NULL
            ? ZrCore_String_GetNativeString(left->referenceRange.source)
            : ZR_NULL;
    rightSource = right->referenceRange.source != ZR_NULL
            ? ZrCore_String_GetNativeString(right->referenceRange.source)
            : ZR_NULL;
    if (leftSource != ZR_NULL || rightSource != ZR_NULL) {
        if (leftSource == ZR_NULL || rightSource == ZR_NULL) {
            return leftSource != ZR_NULL;
        }
        sourceOrder = strcmp(leftSource, rightSource);
        if (sourceOrder != 0) {
            return sourceOrder < 0;
        }
    }
    if (left->referenceRange.start.offset != right->referenceRange.start.offset) {
        return left->referenceRange.start.offset < right->referenceRange.start.offset;
    }
    if (left->referenceRange.end.offset != right->referenceRange.end.offset) {
        return left->referenceRange.end.offset < right->referenceRange.end.offset;
    }
    if (left->externalMetadataToken != right->externalMetadataToken) {
        return left->externalMetadataToken < right->externalMetadataToken;
    }
    return left->symbolId <= right->symbolId;
}

static void semantic_query_external_references_sort(SZrArray *references) {
    TZrSize index;

    if (references == ZR_NULL) {
        return;
    }
    for (index = 1U; index < references->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticExternalReferenceQuery *before =
                    (SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                            references, current - 1U);
            SZrParserSemanticExternalReferenceQuery *after =
                    (SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                            references, current);
            SZrParserSemanticExternalReferenceQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_query_external_reference_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

/* 只按 SymbolId 合并当前快照中的 import 来源；同一符号出现冲突 URI 时拒绝该投影。 */
static TZrBool semantic_query_symbols_apply_import_identity(
        const SZrSemanticContext *context,
        SZrParserSemanticSymbolQuery *symbol) {
    TZrSize index;

    if (context == ZR_NULL || symbol == ZR_NULL ||
        symbol->symbolId == ZR_SEMANTIC_ID_INVALID ||
        !context->visibleSymbolFacts.isValid) {
        return ZR_TRUE;
    }
    for (index = 0U; index < context->visibleSymbolFacts.length; index++) {
        const SZrSemanticVisibleSymbolFact *fact =
                (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->visibleSymbolFacts, index);

        if (fact == ZR_NULL || fact->symbolId != symbol->symbolId ||
            !fact->isImport) {
            continue;
        }
        if (symbol->isImport) {
            if ((symbol->externalOriginUri == ZR_NULL) !=
                    (fact->externalOriginUri == ZR_NULL) ||
                (symbol->externalOriginUri != ZR_NULL &&
                 !ZrCore_String_Equal(symbol->externalOriginUri,
                                      fact->externalOriginUri))) {
                return ZR_FALSE;
            }
        }
        symbol->isImport = ZR_TRUE;
        symbol->externalOriginUri = fact->externalOriginUri;
    }
    return ZR_TRUE;
}

/**
 * @brief 将位置上的 resolved reference 投影为符号值；不要求 canonical typeId 有效。
 * @param context 提供当前 reference facts 与 symbol records；上下文存活不延长事实指针目标的生命周期。
 * @param position 需要查询的源码位置。
 * @param scope 可选的 MODULE/NODE 范围；NODE root 必须非空，且借用 AST 在查询期间保持存活。
 * @param outSymbol 输出值；进入查询前清零，失败时保持清零。
 * @return reference 已解析、SymbolId 有效且 import identity 无冲突时返回 true；无符号记录时 kind/node 仍为零/NULL。
 * @note isResolved 与 canonical typeId 独立。输出浅拷贝 name、signature、identity/import URI、三个 range.source 与 declarationNode 指针；range 值拷贝不会复制 source string。
 *       name 是 fact producer 提供的 SZrString*（identifier 路径取自 binding->name）；range.source 也是 SZrString*，签名/identity 来自各自 producer，declarationNode 属于来源 AST owner。
 *       消费输出期间须保持这些 SZrString 的实际 GC roots（含 range.source 的源字符串 root）、provider owners 与来源 AST owner 存活；context->state 只为数组查询提供 allocator，不保证 referents 存活。
 */
TZrBool ZrParser_SemanticQuery_SymbolAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticSymbolQuery *outSymbol) {
    SZrParserSemanticQueryFacts facts;
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticReferenceFact *definition;

    if (outSymbol != ZR_NULL) {
        memset(outSymbol, 0, sizeof(*outSymbol));
    }
    if (context == ZR_NULL || outSymbol == ZR_NULL ||
        !ZrParser_SemanticQuery_FactsAt(context, position, scope, &facts)) {
        return ZR_FALSE;
    }

    reference = facts.reference;
    /* reference 状态与 canonical 类型可用性独立；这里不验证 typeId，只验证引用和符号身份。 */
    if (reference == ZR_NULL || !reference->isResolved ||
        reference->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    outSymbol->symbolId = reference->symbolId;
    outSymbol->typeId = reference->typeId;
    outSymbol->ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
    outSymbol->role = reference->kind;
    outSymbol->referenceRange = reference->range;
    outSymbol->declarationRange = reference->declarationRange;
    outSymbol->displayName = reference->name;
    outSymbol->signatureDisplay = reference->signatureDisplay;
    outSymbol->externalOwnerIdentity = reference->externalOwnerIdentity;
    outSymbol->externalProviderGeneration = reference->externalProviderGeneration;
    outSymbol->externalMetadataToken = reference->externalMetadataToken;
    outSymbol->externalSignatureToken = reference->externalSignatureToken;
    outSymbol->externalSignatureHash = reference->externalSignatureHash;
    outSymbol->externalTargetKind = reference->externalTargetKind;
    outSymbol->hasExternalTarget = reference->hasExternalTarget;
    if (!semantic_query_symbols_apply_import_identity(context, outSymbol)) {
        memset(outSymbol, 0, sizeof(*outSymbol));
        return ZR_FALSE;
    }
    {
        const SZrSemanticSymbolRecord *record =
                ZrParser_Semantic_FindSymbolById(context, reference->symbolId);
        if (record != ZR_NULL && record->id == reference->symbolId) {
            outSymbol->kind = record->kind;
            outSymbol->declarationNode = record->astNode;
        }
    }
    if (reference->hasDefinitionRange) {
        outSymbol->definitionRange = reference->definitionRange;
        return ZR_TRUE;
    }

    definition = ZrParser_SemanticQuery_DefinitionOf(context, position, scope);
    if (definition != ZR_NULL) {
        outSymbol->definitionRange = definition->range;
    }
    return ZR_TRUE;
}

/**
 * @brief 投影有匹配符号记录的 resolved 声明，按源码位置排序并去重。
 * @param context 含有有效 reference facts 的语义快照。
 * @param scope 可选的 MODULE/NODE 范围。
 * @param outSymbols 调用方拥有的输出数组；已有数组须使用与 context->state 兼容的 allocator。
 * @pre 首次调用前输出数组须经 Construct（其前不得持有缓冲区；或等价全零初始化），也可先按精确元素宽度合法 Init；复用时宽度必须匹配。
 *      已 Init 的数组须与 context->state 使用兼容 allocator，以供增长及后续 Free。
 * @return 参数/快照有效时返回 true，即使结果为空；无效输入或元素宽度不符时返回 false。
 * @note 缓冲区归调用方；首次自动 Init、Push 扩容与最终 Free 均须使用与 context->state 兼容的 allocator。
 * @note 数组仅保存元素字节副本，不接管或延长其中 AST/string/URI 指针目标的生命周期；保持来源 AST 与字符串/provider owner/root 存活。
 *       context->state 只提供数组 allocator，不是元素内部指针目标的 owner/root。
 */
TZrBool ZrParser_SemanticQuery_DeclaredSymbols(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outSymbols) {
    TZrSize index;

    if (!semantic_query_symbols_prepare_output(context, outSymbols) ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *declaration =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        const SZrSemanticSymbolRecord *record;
        SZrParserSemanticSymbolQuery symbol;

        if (declaration == ZR_NULL ||
            declaration->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
            !declaration->isResolved ||
            declaration->symbolId == ZR_SEMANTIC_ID_INVALID ||
            declaration->node == ZR_NULL ||
            !semantic_query_symbols_scope_allows_declaration(scope, declaration) ||
            semantic_query_symbols_contains_id(outSymbols, declaration->symbolId)) {
            continue;
        }

        record = ZrParser_Semantic_FindSymbolById(context, declaration->symbolId);
        if (record == ZR_NULL || record->id != declaration->symbolId ||
            record->typeId != declaration->typeId ||
            record->astNode != declaration->node) {
            continue;
        }

        memset(&symbol, 0, sizeof(symbol));
        symbol.symbolId = declaration->symbolId;
        symbol.typeId = declaration->typeId;
        symbol.ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
        symbol.kind = record->kind;
        symbol.role = ZR_SEMANTIC_REFERENCE_DECLARATION;
        symbol.referenceRange = declaration->range;
        symbol.declarationRange = declaration->declarationRange;
        symbol.definitionRange = declaration->hasDefinitionRange
                ? declaration->definitionRange
                : declaration->declarationRange;
        symbol.declarationNode = declaration->node;
        symbol.displayName = record->name;
        symbol.signatureDisplay = declaration->signatureDisplay;
        if (!semantic_query_symbols_apply_import_identity(context, &symbol)) {
            continue;
        }
        ZrCore_Array_Push(context->state, outSymbols, &symbol);
    }

    semantic_query_symbols_sort_declarations(outSymbols);
    return ZR_TRUE;
}

/**
 * @brief 投影 provider 身份完整的外部引用，并按稳定 identity 键排序。
 * @param context 含有有效 reference facts 的语义快照。
 * @param scope 可选的 MODULE/NODE 范围。
 * @param outReferences 调用方拥有的输出数组；已有数组须使用与 context->state 兼容的 allocator。
 * @pre 首次调用前输出数组须经 Construct（其前不得持有缓冲区；或等价全零初始化），也可先按精确元素宽度合法 Init；复用时宽度必须匹配。
 *      已 Init 的数组须与 context->state 使用兼容 allocator，以供增长及后续 Free。
 * @return 有合格引用时返回 true；无合格项、无效输入或元素宽度不符时返回 false。
 * @note 缓冲区归调用方；首次自动 Init、Push 扩容与最终 Free 均须使用与 context->state 兼容的 allocator。
 * @note 数组仅保存元素字节副本，不接管或延长 externalOwnerIdentity 指针目标的生命周期；保持对应字符串/provider owner/root 存活。
 *       context->state 只提供数组 allocator，不是元素内部指针目标的 owner/root。
 */
TZrBool ZrParser_SemanticQuery_ExternalReferences(
        const SZrSemanticContext *context,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outReferences) {
    TZrSize index;

    if (context == ZR_NULL || outReferences == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    if (outReferences->isValid) {
        if (outReferences->elementSize !=
            sizeof(SZrParserSemanticExternalReferenceQuery)) {
            return ZR_FALSE;
        }
        outReferences->length = 0U;
    } else {
        ZrCore_Array_Init(
                context->state,
                outReferences,
                sizeof(SZrParserSemanticExternalReferenceQuery),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }

    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        SZrParserSemanticExternalReferenceQuery projected;

        if (!semantic_query_external_reference_is_complete(reference) ||
            !semantic_query_symbols_scope_allows_declaration(scope, reference)) {
            continue;
        }
        memset(&projected, 0, sizeof(projected));
        projected.referenceRange = reference->range;
        projected.symbolId = reference->symbolId;
        projected.typeId = reference->typeId;
        projected.role = reference->kind;
        projected.externalOwnerIdentity = reference->externalOwnerIdentity;
        projected.externalProviderGeneration =
                reference->externalProviderGeneration;
        projected.externalMetadataToken = reference->externalMetadataToken;
        projected.externalSignatureToken = reference->externalSignatureToken;
        projected.externalSignatureHash = reference->externalSignatureHash;
        projected.externalTargetKind = reference->externalTargetKind;
        ZrCore_Array_Push(context->state, outReferences, &projected);
    }

    semantic_query_external_references_sort(outReferences);
    return outReferences->length > 0U;
}

/**
 * @brief 从已发布 scope/visible facts 投影当前位置可见符号，不从全局符号表推测绑定。
 * @param context 当前语义快照及其已发布可见性事实。
 * @param position 查询位置。
 * @param scope 可选的 MODULE/NODE 范围；NODE root 借用 AST 并须在查询期间保持存活。
 * @param options 可选过滤项；NULL 等价于全部采用默认关闭值。
 * @param outSymbols 调用方拥有的输出数组；已有数组须使用与 context->state 兼容的 allocator。
 * @pre 首次调用前输出数组须经 Construct（其前不得持有缓冲区；或等价全零初始化），也可先按精确元素宽度合法 Init；复用时宽度必须匹配。
 *      已 Init 的数组须与 context->state 使用兼容 allocator，以供增长及后续 Free。
 * @return 至少投影一个符号时返回 true；空集合或无效输入、范围、元素宽度时返回 false。
 * @note 缓冲区归调用方；首次自动 Init、Push 扩容与最终 Free 均须使用与 context->state 兼容的 allocator。
 * @note 数组仅保存元素字节副本，不接管或延长其中 AST/string/URI 指针目标的生命周期；保持来源 AST 与字符串/provider owner/root 存活。
 *       context->state 只提供数组 allocator，不是元素内部指针目标的 owner/root。
 */
TZrBool ZrParser_SemanticQuery_VisibleSymbols(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        const SZrParserSemanticVisibleSymbolOptions *options,
        SZrArray *outSymbols) {
    const SZrSemanticScopeFact *activeScope;
    const SZrSemanticScopeFact *currentScope;
    SZrArray candidates;
    TZrUInt32 distance;
    TZrSize index;

    if (!semantic_query_symbols_prepare_output(context, outSymbols) ||
        !semantic_query_symbols_scope_allows_position(scope, position)) {
        return ZR_FALSE;
    }
    activeScope = semantic_query_symbols_find_innermost_scope(context, position);
    if (activeScope == ZR_NULL || !context->visibleSymbolFacts.isValid) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&candidates);
    ZrCore_Array_Init(context->state,
                      &candidates,
                      sizeof(SZrSemanticVisibleSymbolCandidate),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    currentScope = activeScope;
    distance = 0U;
    while (currentScope != ZR_NULL && distance < context->scopeFacts.length) {
        for (index = 0U; index < context->visibleSymbolFacts.length; index++) {
            const SZrSemanticVisibleSymbolFact *fact =
                    (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                            (SZrArray *)&context->visibleSymbolFacts, index);
            const SZrSemanticSymbolRecord *record;
            SZrSemanticVisibleSymbolCandidate candidate;

            if (fact == ZR_NULL || fact->scopeId != currentScope->id ||
                !semantic_query_symbols_is_eligible(fact, activeScope, position, options)) {
                continue;
            }
            record = ZrParser_Semantic_FindSymbolById(context, fact->symbolId);
            if (record == ZR_NULL || record->name == ZR_NULL) {
                continue;
            }

            memset(&candidate, 0, sizeof(candidate));
            candidate.symbol.symbolId = record->id;
            candidate.symbol.typeId = record->typeId;
            candidate.symbol.ownerSymbolId = fact->ownerSymbolId;
            candidate.symbol.kind = record->kind;
            candidate.symbol.role = ZR_SEMANTIC_REFERENCE_DECLARATION;
            candidate.symbol.declarationRange = fact->declarationRange;
            candidate.symbol.definitionRange = fact->hasDefinitionRange
                    ? fact->definitionRange
                    : fact->declarationRange;
            candidate.symbol.displayName = record->name;
            candidate.symbol.signatureDisplay = fact->signatureDisplay;
            if (!semantic_query_symbols_apply_import_identity(
                        context, &candidate.symbol)) {
                continue;
            }
            candidate.symbol.declarationNode = record->astNode;
            candidate.record = record;
            /* record 仅供本次遮蔽判定借用；candidate.symbol 是唯一会逃逸到结果数组的部分。 */
            candidate.scopeDistance = distance;
            candidate.declarationOrder = fact->declarationOrder;
            candidate.overloadSetId = record->overloadSetId;
            if (!semantic_query_symbols_is_shadowed(&candidates, &candidate)) {
                ZrCore_Array_Push(context->state, &candidates, &candidate);
            }
        }
        currentScope = ZrParser_Semantic_FindScopeFactById(
                context, currentScope->parentScopeId);
        distance++;
    }

    semantic_query_symbols_sort(&candidates);
    for (index = 0U; index < candidates.length; index++) {
        const SZrSemanticVisibleSymbolCandidate *candidate =
                (const SZrSemanticVisibleSymbolCandidate *)ZrCore_Array_Get(&candidates, index);
        if (candidate != ZR_NULL) {
            SZrParserSemanticSymbolQuery symbol = candidate->symbol;
            ZrCore_Array_Push(context->state, outSymbols, &symbol);
        }
    }
    ZrCore_Array_Free(context->state, &candidates);
    return outSymbols->length > 0U;
}
