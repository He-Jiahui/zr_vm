#include "zr_vm_parser/semantic_calls.h"

#include <string.h>

#include "semantic/semantic_scope_facts.h"
#include "zr_vm_parser/semantic_query.h"

/* 范围来源只按对象或非空字符串内容判等，不做 URI 归一化或 basename 匹配。 */
static TZrBool semantic_calls_same_source(SZrString *left, SZrString *right) {
    return (TZrBool)(left == right ||
                     (left != ZR_NULL && right != ZR_NULL &&
                      ZrCore_String_Equal(left, right)));
}

/* 边范围相等先要求来源相同；有 offset 时比较 offset，否则比较行列。 */
static TZrBool semantic_calls_same_range(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !semantic_calls_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }
    if (left->start.offset > 0U || left->end.offset > 0U ||
        right->start.offset > 0U || right->end.offset > 0U) {
        return (TZrBool)(left->start.offset == right->start.offset &&
                         left->end.offset == right->end.offset);
    }
    return (TZrBool)(left->start.line == right->start.line &&
                     left->start.column == right->start.column &&
                     left->end.line == right->end.line &&
                     left->end.column == right->end.column);
}

/* 范围包含复用来源身份及 offset/行列回退，供 caller、调用表达式和查询位置共用。 */
static TZrBool semantic_calls_range_contains(
        const SZrFileRange *outer,
        const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !semantic_calls_same_source(outer->source, inner->source)) {
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

/* 只要来源或任一坐标存在就保留事实；零值范围且无来源才视为未知。 */
static TZrBool semantic_calls_range_is_known(const SZrFileRange *range) {
    return (TZrBool)(range != ZR_NULL &&
                      (range->source != ZR_NULL || range->start.offset > 0U ||
                       range->end.offset > 0U || range->start.line > 0 ||
                       range->end.line > 0 || range->start.column > 0 ||
                       range->end.column > 0));
}

/* 最窄候选按 offset 跨度比较；倒置或空指针范围返回最大值。 */
static TZrSize semantic_calls_range_width(const SZrFileRange *range) {
    if (range == ZR_NULL || range->end.offset < range->start.offset) {
        return ZR_MAX_SIZE;
    }
    return range->end.offset - range->start.offset;
}

/* caller 取包含调用点的最窄函数 scope；同宽异主或无有效函数 owner 时不猜测。 */
static TZrSymbolId semantic_calls_find_caller(
        const SZrSemanticContext *context,
        const SZrFileRange *callSiteRange) {
    const SZrSemanticScopeFact *bestScope = ZR_NULL;
    TZrSize bestWidth = ZR_MAX_SIZE;
    TZrBool isAmbiguous = ZR_FALSE;
    TZrSize index;

    if (context == ZR_NULL || callSiteRange == ZR_NULL ||
        !context->scopeFacts.isValid) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    for (index = 0U; index < context->scopeFacts.length; index++) {
        const SZrSemanticScopeFact *scope =
                (const SZrSemanticScopeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->scopeFacts, index);
        TZrSize width;

        if (scope == ZR_NULL || scope->kind != ZR_SEMANTIC_SCOPE_KIND_FUNCTION ||
            !semantic_calls_range_contains(&scope->range, callSiteRange)) {
            continue;
        }
        width = semantic_calls_range_width(&scope->range);
        if (bestScope == ZR_NULL || width < bestWidth) {
            bestScope = scope;
            bestWidth = width;
            isAmbiguous = ZR_FALSE;
        } else if (width == bestWidth &&
                   bestScope->ownerSymbolId != scope->ownerSymbolId) {
            isAmbiguous = ZR_TRUE;
        }
    }
    if (!isAmbiguous && bestScope != ZR_NULL &&
        bestScope->ownerSymbolId != ZR_SEMANTIC_ID_INVALID) {
        const SZrSemanticSymbolRecord *owner = ZrParser_Semantic_FindSymbolById(
                context, bestScope->ownerSymbolId);
        if (owner != ZR_NULL && owner->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
            return owner->id;
        }
    }
    return ZR_SEMANTIC_ID_INVALID;
}

/* 以 CALL 引用落入 callTargetRange 关联表达式，再取最窄调用范围；exactness 由发布处检查。 */
static const SZrSemanticExpressionFact *semantic_calls_find_expression(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference) {
    const SZrSemanticExpressionFact *best = ZR_NULL;
    TZrSize bestWidth = ZR_MAX_SIZE;
    TZrSize index;

    if (context == ZR_NULL || reference == ZR_NULL ||
        !context->expressionFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->expressionFacts.length; index++) {
        const SZrSemanticExpressionFact *expression =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->expressionFacts, index);
        TZrSize width;

        if (expression == ZR_NULL || !expression->hasCallInfo ||
            !semantic_calls_range_contains(&expression->callTargetRange, &reference->range)) {
            continue;
        }
        width = semantic_calls_range_width(&expression->range);
        if (best == ZR_NULL || width < bestWidth) {
            best = expression;
            bestWidth = width;
        }
    }
    return best;
}

/* 多条记录指向同一 AST 声明时取首个完整函数 symbol；无法证明时保留输入目标。 */
static const SZrSemanticSymbolRecord *semantic_calls_canonical_function(
        const SZrSemanticContext *context,
        const SZrSemanticSymbolRecord *symbol) {
    TZrSize index;

    if (context == ZR_NULL || symbol == ZR_NULL || symbol->astNode == ZR_NULL ||
        !context->symbols.isValid) {
        return symbol;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *candidate =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (candidate != ZR_NULL &&
            candidate->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
            candidate->astNode == symbol->astNode &&
            candidate->id != ZR_SEMANTIC_ID_INVALID &&
            candidate->typeId != ZR_SEMANTIC_ID_INVALID) {
            return candidate;
        }
    }
    return symbol;
}

/* 此分数只决定同一调用边的事实更新优先级，不代表解析置信度。 */
static TZrUInt32 semantic_calls_edge_completeness(
        const SZrSemanticCallEdgeFact *edge) {
    TZrUInt32 score = 0U;

    if (edge == ZR_NULL) {
        return 0U;
    }
    if (edge->callerSymbolId != ZR_SEMANTIC_ID_INVALID) {
        score += 8U;
    }
    if (edge->targetSymbolId != ZR_SEMANTIC_ID_INVALID) {
        score += 4U;
    }
    if (edge->hasTargetDeclarationRange) {
        score += 2U;
    }
    if (edge->callableTypeId != ZR_SEMANTIC_ID_INVALID) {
        score += 1U;
    }
    if (edge->resolution == ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED) {
        score += 16U;
    }
    return score;
}

/* caller 与完整调用范围构成边身份；已解析到不同目标的同址事实分别保留。 */
static TZrBool semantic_calls_merge_edge(
        SZrSemanticContext *context,
        const SZrSemanticCallEdgeFact *candidate) {
    TZrSize index;

    if (context == ZR_NULL || candidate == ZR_NULL || !context->callEdgeFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->callEdgeFacts.length; index++) {
        SZrSemanticCallEdgeFact *edge =
                (SZrSemanticCallEdgeFact *)ZrCore_Array_Get(
                        &context->callEdgeFacts, index);
        if (edge == ZR_NULL) {
            continue;
        }
        if (edge->callerSymbolId == candidate->callerSymbolId &&
            semantic_calls_same_range(
                    &edge->callSiteRange, &candidate->callSiteRange)) {
            if (edge->resolution == ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED &&
                candidate->resolution == ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED &&
                edge->targetSymbolId != candidate->targetSymbolId) {
                continue;
            }
            if (semantic_calls_edge_completeness(candidate) >=
                semantic_calls_edge_completeness(edge)) {
                *edge = *candidate;
            }
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* BUG: 新 context 的 callEdgeFacts 初次分配失败会被 Array_Init 伪装成有效；遇到可投影 CALL 引用时，Publish 会在首条 Array_Push 对空 head 断言或写入，不能通过返回值报告失败。 */
/**
 * @brief 初始化语义快照持有的调用边事实数组。
 * @pre context/state 有效，且该数组尚未持有旧存储；由语义上下文初始化阶段调用。
 */
void ZrParser_SemanticCalls_Init(SZrSemanticContext *context) {
    if (context != ZR_NULL && context->state != ZR_NULL) {
        ZrCore_Array_Init(context->state,
                          &context->callEdgeFacts,
                          sizeof(SZrSemanticCallEdgeFact),
                          ZR_PARSER_INITIAL_CAPACITY_SMALL);
    }
}

/** 清空调用边事实并保留数组容量，供同一上下文开始新快照时复用。 */
void ZrParser_SemanticCalls_Reset(SZrSemanticContext *context) {
    if (context != ZR_NULL && context->callEdgeFacts.isValid) {
        context->callEdgeFacts.length = 0U;
    }
}

/** 释放调用边数组；语义上下文随后才释放其他事实数组。 */
void ZrParser_SemanticCalls_Free(SZrSemanticContext *context) {
    if (context != ZR_NULL && context->state != ZR_NULL) {
        ZrParser_SemanticCalls_Reset(context);
        ZrCore_Array_Free(context->state, &context->callEdgeFacts);
    }
}

/**
 * @brief 从已有 CALL 引用和词法 scope owner 投影稳定调用边，不按名称重新解析端点。
 * @pre reference、scope、symbol 与 callEdgeFacts 属于同一语义快照，且调用边数组已初始化。
 * @note 不清空已有边；同快照可继续合并补全事实，换快照前由上下文生命周期执行 Reset。
 * @return 输入上下文/事实数组不可用时返回 false；未解析端点会以 unresolved edge 保留。
 */
TZrBool ZrParser_SemanticCalls_Publish(SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid ||
        !context->callEdgeFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts, index);
        const SZrSemanticExpressionFact *expression;
        const SZrSemanticSymbolRecord *target = ZR_NULL;
        SZrSemanticCallEdgeFact edge;

        if (reference == ZR_NULL || reference->kind != ZR_SEMANTIC_REFERENCE_CALL ||
            !semantic_calls_range_is_known(&reference->range)) {
            continue;
        }
        memset(&edge, 0, sizeof(edge));
        expression = semantic_calls_find_expression(context, reference);
        if (expression != ZR_NULL &&
            !ZrParser_SemanticQuery_ExactnessAllowsProjection(
                    expression->exactness)) {
            continue;
        }
        edge.callSiteRange = expression != ZR_NULL ? expression->range : reference->range;
        edge.callableTypeId = reference->typeId;
        edge.callerSymbolId = semantic_calls_find_caller(context, &edge.callSiteRange);
        edge.targetSymbolId = reference->symbolId;
        edge.resolution = ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED;

        if (!reference->isResolved || edge.targetSymbolId == ZR_SEMANTIC_ID_INVALID) {
            edge.targetSymbolId = ZR_SEMANTIC_ID_INVALID;
            edge.resolution = ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_UNRESOLVED;
        } else {
            target = ZrParser_Semantic_FindSymbolById(context, edge.targetSymbolId);
            if (target == ZR_NULL || target->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
                edge.targetSymbolId = ZR_SEMANTIC_ID_INVALID;
                edge.resolution = ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_UNRESOLVED;
            } else {
                target = semantic_calls_canonical_function(context, target);
                edge.targetSymbolId = target->id;
                edge.targetDeclarationRange = semantic_calls_range_is_known(
                        &reference->declarationRange)
                        ? reference->declarationRange
                        : target->location;
                edge.hasTargetDeclarationRange = semantic_calls_range_is_known(
                        &edge.targetDeclarationRange);
                if (!edge.hasTargetDeclarationRange) {
                    edge.resolution =
                            ZR_SEMANTIC_CALL_EDGE_RESOLUTION_TARGET_DECLARATION_UNAVAILABLE;
                }
            }
        }
        if (edge.callerSymbolId == ZR_SEMANTIC_ID_INVALID) {
            edge.resolution = ZR_SEMANTIC_CALL_EDGE_RESOLUTION_CALLER_UNAVAILABLE;
        }
        if (!semantic_calls_merge_edge(context, &edge)) {
            ZrCore_Array_Push(context->state, &context->callEdgeFacts, &edge);
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 先从脚本 AST 发布词法 scope owner，再投影该上下文已有的 CALL 引用。
 * @pre root 与语义事实属于同一快照；重复换源前调用方须先重建/重置上下文。
 * @return scope 构建或边发布失败时返回 false；本函数不清空旧 scope/edge 数组。
 */
TZrBool ZrParser_SemanticCalls_PublishSource(
        SZrSemanticContext *context,
        SZrAstNode *root) {
    if (context == ZR_NULL || root == ZR_NULL ||
        !ZrParser_Semantic_BuildSourceScopeFacts(context, root)) {
        return ZR_FALSE;
    }
    return ZrParser_SemanticCalls_Publish(context);
}

/* 空 scope 与模块 scope 覆盖整份快照；节点 scope 只允许调用范围完全落在 root 内。 */
static TZrBool semantic_calls_scope_allows(
        const SZrParserSemanticQueryScope *scope,
        const SZrSemanticCallEdgeFact *edge) {
    if (scope == ZR_NULL || scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_MODULE) {
        return ZR_TRUE;
    }
    return (TZrBool)(scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
                     scope->root != ZR_NULL && edge != ZR_NULL &&
                     semantic_calls_range_contains(
                             &scope->root->location, &edge->callSiteRange));
}

/* BUG: 首次查询 outEdges 分配失败仍被 Array_Init 标记有效；只有查询命中已发布边时，append_query 才对空 head Push 并断言或写入，接口无法正常返回失败。
 * 证据链：CallEdgesAt/OutgoingCalls/IncomingCalls -> 此处 Array_Init -> semantic_calls_append_query -> ZrCore_Array_Push。 */
static TZrBool semantic_calls_prepare_output(
        const SZrSemanticContext *context,
        SZrArray *outEdges) {
    if (context == ZR_NULL || outEdges == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outEdges->isValid) {
        if (outEdges->elementSize != sizeof(SZrParserSemanticCallEdgeQuery)) {
            return ZR_FALSE;
        }
        outEdges->length = 0U;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      outEdges,
                      sizeof(SZrParserSemanticCallEdgeQuery),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

/* 输出排序按 source 的 NUL 原生文本字节序；空来源排在非空来源之前。 */
static TZrInt32 semantic_calls_compare_sources(
        const SZrString *left,
        const SZrString *right) {
    if (left == right) {
        return 0;
    }
    if (left == ZR_NULL) {
        return -1;
    }
    if (right == ZR_NULL) {
        return 1;
    }
    return strcmp(ZrCore_String_GetNativeString(left),
                  ZrCore_String_GetNativeString(right));
}

/* 范围排序先比较来源，再按存在的 offset 或行列坐标排序。 */
static TZrInt32 semantic_calls_compare_ranges(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    TZrInt32 sourceOrder = semantic_calls_compare_sources(
            left->source, right->source);

    if (sourceOrder != 0) {
        return sourceOrder;
    }
    if (left->start.offset > 0U || left->end.offset > 0U ||
        right->start.offset > 0U || right->end.offset > 0U) {
        if (left->start.offset != right->start.offset) {
            return left->start.offset < right->start.offset ? -1 : 1;
        }
        if (left->end.offset != right->end.offset) {
            return left->end.offset < right->end.offset ? -1 : 1;
        }
        return 0;
    }
    if (left->start.line != right->start.line) {
        return left->start.line < right->start.line ? -1 : 1;
    }
    if (left->start.column != right->start.column) {
        return left->start.column < right->start.column ? -1 : 1;
    }
    if (left->end.line != right->end.line) {
        return left->end.line < right->end.line ? -1 : 1;
    }
    if (left->end.column != right->end.column) {
        return left->end.column < right->end.column ? -1 : 1;
    }
    return 0;
}

/* 排序键为调用范围、caller、target、resolution；完全同键时保持稳定输入顺序。 */
static TZrBool semantic_calls_query_precedes(
        const SZrParserSemanticCallEdgeQuery *left,
        const SZrParserSemanticCallEdgeQuery *right) {
    TZrInt32 rangeOrder = semantic_calls_compare_ranges(
            &left->callSiteRange, &right->callSiteRange);

    if (rangeOrder != 0) {
        return rangeOrder < 0;
    }
    if (left->callerSymbolId != right->callerSymbolId) {
        return left->callerSymbolId < right->callerSymbolId;
    }
    if (left->targetSymbolId != right->targetSymbolId) {
        return left->targetSymbolId < right->targetSymbolId;
    }
    return left->resolution <= right->resolution;
}

/* 对查询结果原地插入排序，避免为短调用列表另分配排序缓冲。 */
static void semantic_calls_sort(SZrArray *edges) {
    TZrSize index;

    if (edges == ZR_NULL) {
        return;
    }
    for (index = 1U; index < edges->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticCallEdgeQuery *before =
                    (SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(edges, current - 1U);
            SZrParserSemanticCallEdgeQuery *after =
                    (SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(edges, current);
            SZrParserSemanticCallEdgeQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                semantic_calls_query_precedes(before, after)) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

/* 查询结果是值副本，但其中 FileRange.source 仍借用语义快照；不能脱离该快照保留。 */
static void semantic_calls_append_query(
        const SZrSemanticContext *context,
        SZrArray *outEdges,
        const SZrSemanticCallEdgeFact *edge) {
    SZrParserSemanticCallEdgeQuery query;

    if (context == ZR_NULL || outEdges == ZR_NULL || edge == ZR_NULL) {
        return;
    }
    memset(&query, 0, sizeof(query));
    query.callerSymbolId = edge->callerSymbolId;
    query.targetSymbolId = edge->targetSymbolId;
    query.callableTypeId = edge->callableTypeId;
    query.callSiteRange = edge->callSiteRange;
    query.targetDeclarationRange = edge->targetDeclarationRange;
    query.resolution = edge->resolution;
    query.hasTargetDeclarationRange = edge->hasTargetDeclarationRange;
    ZrCore_Array_Push(context->state, outEdges, &query);
}

/**
 * @brief 返回包含 position 的全部调用边，并按可选 module/node scope 过滤。
 * @pre position、context 与 scope 属于同一快照；outEdges 已构造，或可复用且元素类型匹配、allocator 与 context->state 兼容。
 * @note 返回的范围是值副本，source 指针仍借用快照；有效查询无命中时返回 true 和空数组。
 */
TZrBool ZrParser_SemanticQuery_CallEdgesAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges) {
    TZrSize index;

    if (!semantic_calls_prepare_output(context, outEdges) ||
        (scope != ZR_NULL && scope->kind == ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE &&
         (scope->root == ZR_NULL ||
          !semantic_calls_range_contains(&scope->root->location, &position)))) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->callEdgeFacts.length; index++) {
        const SZrSemanticCallEdgeFact *edge =
                (const SZrSemanticCallEdgeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->callEdgeFacts, index);
        if (edge != ZR_NULL && semantic_calls_range_contains(&edge->callSiteRange, &position) &&
            semantic_calls_scope_allows(scope, edge)) {
            semantic_calls_append_query(context, outEdges, edge);
        }
    }
    semantic_calls_sort(outEdges);
    return ZR_TRUE;
}

/**
 * @brief 按有效 caller symbol id 投影其直接调用边，保留未解析 target 记录。
 * @pre callerSymbolId、context 与 scope 属于同一快照；outEdges 已构造或可复用且元素类型匹配、allocator 与 context->state 兼容。
 * @return caller id 无效或输出数组不可用时返回 false；合法但无命中时返回 true 和空数组。
 */
TZrBool ZrParser_SemanticQuery_OutgoingCalls(
        const SZrSemanticContext *context,
        TZrSymbolId callerSymbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges) {
    TZrSize index;

    if (!semantic_calls_prepare_output(context, outEdges) ||
        callerSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->callEdgeFacts.length; index++) {
        const SZrSemanticCallEdgeFact *edge =
                (const SZrSemanticCallEdgeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->callEdgeFacts, index);
        if (edge != ZR_NULL && edge->callerSymbolId == callerSymbolId &&
            semantic_calls_scope_allows(scope, edge)) {
            semantic_calls_append_query(context, outEdges, edge);
        }
    }
    semantic_calls_sort(outEdges);
    return ZR_TRUE;
}

/**
 * @brief 按有效 target symbol id 投影直接调用边；target 未解析的边不匹配该查询。
 * @pre targetSymbolId、context 与 scope 属于同一快照；outEdges 已构造或可复用且元素类型匹配、allocator 与 context->state 兼容。
 * @return target id 无效或输出数组不可用时返回 false；合法但无命中时返回 true 和空数组。
 */
TZrBool ZrParser_SemanticQuery_IncomingCalls(
        const SZrSemanticContext *context,
        TZrSymbolId targetSymbolId,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outEdges) {
    TZrSize index;

    if (!semantic_calls_prepare_output(context, outEdges) ||
        targetSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->callEdgeFacts.length; index++) {
        const SZrSemanticCallEdgeFact *edge =
                (const SZrSemanticCallEdgeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->callEdgeFacts, index);
        if (edge != ZR_NULL && edge->targetSymbolId == targetSymbolId &&
            semantic_calls_scope_allows(scope, edge)) {
            semantic_calls_append_query(context, outEdges, edge);
        }
    }
    semantic_calls_sort(outEdges);
    return ZR_TRUE;
}

/* BUG: 首次 outCandidates 分配失败仍被 Array_Init 标记有效；精确且已解析的调用进入候选追加时，Push 会对空 head 断言或写入。
 * 证据链：CallCandidatesAt -> 此处 Array_Init -> semantic_calls_append_candidate -> ZrCore_Array_Push。 */
static TZrBool semantic_calls_prepare_candidates(
        const SZrSemanticContext *context,
        SZrArray *outCandidates) {
    if (context == ZR_NULL || outCandidates == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outCandidates->isValid) {
        if (outCandidates->elementSize != sizeof(SZrParserSemanticCallCandidateQuery)) {
            return ZR_FALSE;
        }
        outCandidates->length = 0U;
        return ZR_TRUE;
    }
    ZrCore_Array_Init(context->state,
                      outCandidates,
                      sizeof(SZrParserSemanticCallCandidateQuery),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    return ZR_TRUE;
}

/* 候选按 symbol id 去重，重复项保留首次副本。 */
static TZrBool semantic_calls_has_candidate(
        const SZrArray *candidates,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (candidates == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < candidates->length; index++) {
        const SZrParserSemanticCallCandidateQuery *candidate =
                (const SZrParserSemanticCallCandidateQuery *)ZrCore_Array_Get(
                        (SZrArray *)candidates, index);
        if (candidate != ZR_NULL && candidate->symbolId == symbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* TODO: 原样复制 symbol->typeId；方法 symbol 可能使用 owner TypeId。先补方法 overload 候选用例并确认真实 consumer，再定义 callableTypeId 的签名语义。 */
static TZrBool semantic_calls_append_candidate(
        const SZrSemanticContext *context,
        SZrArray *outCandidates,
        const SZrSemanticSymbolRecord *symbol,
        TZrSymbolId selectedSymbolId) {
    SZrParserSemanticCallCandidateQuery candidate;

    if (context == ZR_NULL || outCandidates == ZR_NULL || symbol == ZR_NULL ||
        symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->id == ZR_SEMANTIC_ID_INVALID ||
        symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (semantic_calls_has_candidate(outCandidates, symbol->id)) {
        return ZR_TRUE;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.symbolId = symbol->id;
    candidate.callableTypeId = symbol->typeId;
    candidate.declarationRange = symbol->location;
    candidate.isSelected = symbol->id == selectedSymbolId;
    ZrCore_Array_Push(context->state, outCandidates, &candidate);
    return ZR_TRUE;
}

/* 候选输出按 symbol id 升序稳定排列；顺序不表示重载优先级。 */
static void semantic_calls_sort_candidates(SZrArray *candidates) {
    TZrSize index;

    if (candidates == ZR_NULL) {
        return;
    }
    for (index = 1U; index < candidates->length; index++) {
        TZrSize current = index;
        while (current > 0U) {
            SZrParserSemanticCallCandidateQuery *before =
                    (SZrParserSemanticCallCandidateQuery *)ZrCore_Array_Get(
                            candidates, current - 1U);
            SZrParserSemanticCallCandidateQuery *after =
                    (SZrParserSemanticCallCandidateQuery *)ZrCore_Array_Get(
                            candidates, current);
            SZrParserSemanticCallCandidateQuery swap;

            if (before == ZR_NULL || after == ZR_NULL ||
                before->symbolId <= after->symbolId) {
                break;
            }
            swap = *before;
            *before = *after;
            *after = swap;
            current--;
        }
    }
}

/**
 * @brief 从精确调用查询取得已选函数及其 overload set 候选，并标记实际选中项。
 * @pre position、context 与 scope 属于同一快照；outCandidates 已构造或可复用且元素类型匹配、allocator 与 context->state 兼容。
 * @note 近似调用、未解析目标或不一致 overload set 失败关闭；此接口不负责名称可见性筛选。declarationRange.source 借用 context 快照。
 * @note candidate callableTypeId 原样来自 symbol->typeId；方法成员的字段语义须由专门候选用例和真实 consumer 确认。
 * @return 选中项必须出现在结果中；失败返回 false，成功结果按 symbol id 排序。
 */
TZrBool ZrParser_SemanticQuery_CallCandidatesAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrArray *outCandidates) {
    SZrParserSemanticCallQuery call;
    const SZrSemanticSymbolRecord *selected;
    const SZrSemanticOverloadSetRecord *overloads = ZR_NULL;
    TZrSize index;

    if (!semantic_calls_prepare_candidates(context, outCandidates)) {
        return ZR_FALSE;
    }
    memset(&call, 0, sizeof(call));
    if (!ZrParser_SemanticQuery_CallAt(context, position, scope, &call) ||
        call.expression == ZR_NULL ||
        !ZrParser_SemanticQuery_ExactnessAllowsProjection(
                call.expression->exactness) ||
        !call.hasResolvedTarget ||
        call.targetSymbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    selected = ZrParser_Semantic_FindSymbolById(context, call.targetSymbolId);
    if (selected == ZR_NULL || selected->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        selected->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (selected->overloadSetId != ZR_SEMANTIC_ID_INVALID) {
        for (index = 0U; index < context->overloadSets.length; index++) {
            const SZrSemanticOverloadSetRecord *candidateSet =
                    (const SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(
                            (SZrArray *)&context->overloadSets, index);
            if (candidateSet != ZR_NULL && candidateSet->id == selected->overloadSetId) {
                overloads = candidateSet;
                break;
            }
        }
        if (overloads == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    if (overloads == ZR_NULL) {
        if (!semantic_calls_append_candidate(
                    context, outCandidates, selected, call.targetSymbolId)) {
            return ZR_FALSE;
        }
    } else {
        for (index = 0U; index < overloads->members.length; index++) {
            const TZrSymbolId *symbolId = (const TZrSymbolId *)ZrCore_Array_Get(
                    (SZrArray *)&overloads->members, index);
            if (symbolId == ZR_NULL ||
                !semantic_calls_append_candidate(
                        context,
                        outCandidates,
                        ZrParser_Semantic_FindSymbolById(context, *symbolId),
                        call.targetSymbolId)) {
                outCandidates->length = 0U;
                return ZR_FALSE;
            }
        }
    }
    if (!semantic_calls_has_candidate(outCandidates, call.targetSymbolId)) {
        outCandidates->length = 0U;
        return ZR_FALSE;
    }
    semantic_calls_sort_candidates(outCandidates);
    return outCandidates->length > 0U;
}
