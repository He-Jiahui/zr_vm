#include "semantic/lsp_local_semantic_query.h"

#include "interface/lsp_interface_internal.h"
#include "semantic/lsp_local_semantic_hover_text.h"
#include "semantic/lsp_semantic_query.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_language_server/incremental_parser.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/type_inference.h"

#include <stdarg.h>
#include <stdio.h>

/* 行列宽度估算使用的跨行权重；零偏移是否进入该分支见下方 TODO。 */
#define ZR_LSP_LOCAL_QUERY_VISIBLE_RANGE_LINE_WIDTH ((TZrSize)100000)

/* 独立局部 hover 的首段与事实附录共享容量边界；失败由请求入口放弃该 hover。 */
static TZrBool local_query_append_format(TZrChar *buffer,
                                         TZrSize bufferSize,
                                         TZrSize *used,
                                         const TZrChar *format,
                                         ...) {
    va_list args;
    int written;

    if (buffer == ZR_NULL || used == ZR_NULL || format == ZR_NULL || *used >= bufferSize) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *used, bufferSize - *used, format, args);
    va_end(args);

    if (written < 0 || (TZrSize)written >= bufferSize - *used) {
        buffer[bufferSize - 1u] = '\0';
        return ZR_FALSE;
    }

    *used += (TZrSize)written;
    return ZR_TRUE;
}

/* 多类事实同时命中时，优先选择 intrinsic 名称等最适合用户定位的范围。 */
static SZrFileRange local_query_hover_range(const SZrLspLocalSemanticQueryResult *query) {
    if (query == ZR_NULL) {
        return ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 1, 1),
                                         ZrParser_FilePosition_Create(0, 1, 1),
                                         ZR_NULL);
    }

    if (query->ownershipIntrinsicFact != ZR_NULL) {
        return query->ownershipIntrinsicFact->nameRange;
    }
    if (query->expressionFact != ZR_NULL) {
        return query->expressionFact->range;
    }
    if (query->numericFact != ZR_NULL) {
        return query->numericFact->range;
    }
    if (query->logicalFact != ZR_NULL) {
        return query->logicalFact->range;
    }
    if (query->ownershipFact != ZR_NULL) {
        return query->ownershipFact->range;
    }
    if (query->reachabilityFact != ZR_NULL) {
        return query->reachabilityFact->range;
    }
    if (query->referenceFact != ZR_NULL) {
        return query->referenceFact->range;
    }

    return query->queryRange;
}

/* 扫描载荷与候选事实时，双侧 URI 已知则先核对来源，再按偏移判定光标归属。 */
static TZrBool local_query_range_contains_position(SZrFileRange range, SZrFileRange position) {
    TZrSize startOffset;
    TZrSize endOffset;
    TZrSize queryOffset;

    if (range.source != ZR_NULL &&
        position.source != ZR_NULL &&
        range.source != position.source &&
        !ZrLanguageServer_Lsp_StringsEqual(range.source, position.source)) {
        return ZR_FALSE;
    }

    startOffset = range.start.offset;
    endOffset = range.end.offset;
    queryOffset = position.start.offset;
    if (endOffset < startOffset) {
        endOffset = startOffset;
    }

    return queryOffset >= startOffset && queryOffset <= endOffset;
}

/* 同一光标命中多个事实时以较窄范围优先，供数值/所有权事实回退选择。 */
static TZrSize local_query_range_width(SZrFileRange range) {
    /* TODO: 当事实仅有行列、start/end.offset 同为零时首个分支直接返回零，
     * 后面的行列估算不可达。需核查 parser 是否仍会产出这类范围及悬停候选选择。 */
    if (range.end.offset >= range.start.offset) {
        return range.end.offset - range.start.offset;
    }

    if (range.end.line == range.start.line &&
        range.end.column >= range.start.column) {
        return (TZrSize)(range.end.column - range.start.column);
    }

    if (range.end.line > range.start.line) {
        return (TZrSize)(range.end.line - range.start.line) * ZR_LSP_LOCAL_QUERY_VISIBLE_RANGE_LINE_WIDTH +
               (range.end.column >= 0 ? (TZrSize)range.end.column : 0);
    }

    return 0;
}

/* 当前光标被 parser 诊断覆盖时，局部事实查询先报告遮挡，不展示旧 AST 的推断。 */
static SZrDiagnostic *local_query_find_parser_diagnostic_at(SZrFileVersion *fileVersion,
                                                            SZrFileRange position) {
    if (fileVersion == ZR_NULL || !fileVersion->parserDiagnostics.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < fileVersion->parserDiagnostics.length; index++) {
        SZrDiagnostic **diagnosticPtr =
            (SZrDiagnostic **)ZrCore_Array_Get(&fileVersion->parserDiagnostics, index);
        if (diagnosticPtr != ZR_NULL &&
            *diagnosticPtr != ZR_NULL &&
            local_query_range_contains_position((*diagnosticPtr)->location, position)) {
            return *diagnosticPtr;
        }
    }

    return ZR_NULL;
}

/* 依当前文档内容把 LSP UTF-16 位置投影为 parser 字节位置，再查询语义快照。 */
static SZrFileRange local_query_position_range(SZrLspContext *context,
                                               SZrString *uri,
                                               SZrLspPosition position) {
    SZrFileVersion *fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    SZrFileVersionContentSnapshot snapshot;
    SZrFilePosition filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);

    if (fileVersion != ZR_NULL &&
        ZrLanguageServer_FileVersionContentSnapshot_Acquire(context->state, fileVersion, &snapshot)) {
        filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                              snapshot.content,
                                                                              snapshot.contentLength);
        /* TODO: 旧 AST 回退时仅将偏移清零，行列仍来自新内容；parser 的
         * semantic_facts_range_contains_position 可退回行列比较。需用编辑后移位且
         * 仍无当前位置诊断的文档验证是否会显示旧事实。 */
        if (snapshot.usesFallbackAst &&
            (!fileVersion->hasIncrementalInfo ||
             filePosition.offset >= fileVersion->lastChangeInfo.newRange.start.offset)) {
            /* Only source coordinates before the current edit are identical to the last-good AST. */
            filePosition.offset = 0;
        }
        ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
    }
    return ZrParser_FileRange_Create(filePosition, filePosition, uri);
}

/* 同一栈结果可复用；每次查询先丢弃旧借用指针并绑定新的文档位置。 */
static void local_query_seed(SZrLspContext *context,
                             SZrString *uri,
                             SZrLspPosition position,
                             SZrLspLocalSemanticQueryResult *result) {
    ZrLanguageServer_LspLocalSemanticQuery_Clear(result);
    result->status = ZR_LSP_LOCAL_SEMANTIC_QUERY_UNKNOWN;
    result->queryRange = local_query_position_range(context, uri, position);
}

/* 查询执行成功不等于有事实；parser 错误遮挡通过独立状态交给 hover 决策。 */
static TZrBool local_query_set_diagnostic_if_blocked(SZrFileVersion *fileVersion,
                                                     SZrLspLocalSemanticQueryResult *result) {
    SZrDiagnostic *diagnostic;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }

    diagnostic = local_query_find_parser_diagnostic_at(fileVersion, result->queryRange);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }

    result->status = ZR_LSP_LOCAL_SEMANTIC_QUERY_DIAGNOSTIC_FAILURE;
    result->diagnostic = diagnostic;
    return ZR_TRUE;
}

/* 复合表达式节点应保留结构事实身份，避免用子表达式回退伪装整体结果。 */
static TZrBool local_query_node_prefers_structural_fact(const SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    return node->type == ZR_AST_BINARY_EXPRESSION ||
           node->type == ZR_AST_LOGICAL_EXPRESSION ||
           node->type == ZR_AST_UNARY_EXPRESSION ||
           node->type == ZR_AST_ASSIGNMENT_EXPRESSION ||
           node->type == ZR_AST_CONDITIONAL_EXPRESSION;
}

/* 计算成员访问的 reference 可以找回其外层表达式载荷。 */
static TZrBool local_query_reference_is_member_payload(const SZrSemanticReferenceFact *referenceFact) {
    return referenceFact != ZR_NULL &&
           (referenceFact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
            referenceFact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE);
}

/* 用户悬停在调用目标或成员名时，优先展示覆盖该 token 的最窄表达式载荷。 */
static const SZrSemanticExpressionFact *local_query_find_payload_expression_fact(
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange queryRange) {
    const SZrSemanticExpressionFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        !analyzer->semanticContext->expressionFacts.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->semanticContext->expressionFacts.length; index++) {
        const SZrSemanticExpressionFact *fact =
            (const SZrSemanticExpressionFact *)ZrCore_Array_Get(&analyzer->semanticContext->expressionFacts, index);
        TZrBool containsPayload = ZR_FALSE;
        TZrSize width;

        if (fact == ZR_NULL) {
            continue;
        }

        if (fact->hasCallInfo &&
            local_query_range_contains_position(fact->callTargetRange, queryRange)) {
            containsPayload = ZR_TRUE;
        }
        if (fact->hasMemberInfo &&
            local_query_range_contains_position(fact->memberRange, queryRange)) {
            containsPayload = ZR_TRUE;
        }
        if (!containsPayload) {
            continue;
        }

        width = fact->range.end.offset >= fact->range.start.offset
                    ? fact->range.end.offset - fact->range.start.offset
                    : 0;
        if (best == ZR_NULL || width <= bestWidth) {
            best = fact;
            bestWidth = width;
        }
    }

    return best;
}

/* 计算成员 token 常先命中引用事实；按节点或最窄范围回连到成员表达式。 */
static const SZrSemanticExpressionFact *local_query_find_reference_payload_expression_fact(
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange queryRange,
    const SZrSemanticReferenceFact *referenceFact) {
    const SZrSemanticExpressionFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        !local_query_reference_is_member_payload(referenceFact) ||
        !analyzer->semanticContext->expressionFacts.isValid) {
        return ZR_NULL;
    }

    if (referenceFact->node != ZR_NULL) {
        const SZrSemanticExpressionFact *nodeFact =
            ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, referenceFact->node);
        if (nodeFact != ZR_NULL && nodeFact->hasMemberInfo) {
            return nodeFact;
        }
    }

    for (TZrSize index = 0; index < analyzer->semanticContext->expressionFacts.length; index++) {
        const SZrSemanticExpressionFact *fact =
            (const SZrSemanticExpressionFact *)ZrCore_Array_Get(&analyzer->semanticContext->expressionFacts, index);
        TZrSize width;

        if (fact == ZR_NULL ||
            !fact->hasMemberInfo ||
            (!local_query_range_contains_position(fact->range, queryRange) &&
             !local_query_range_contains_position(fact->range, referenceFact->range))) {
            continue;
        }

        width = fact->range.end.offset >= fact->range.start.offset
                    ? fact->range.end.offset - fact->range.start.offset
                    : 0;
        if (best == ZR_NULL || width <= bestWidth) {
            best = fact;
            bestWidth = width;
        }
    }

    return best;
}

/* 先尊重成员/调用载荷，再按 AST 结构和位置回退，供 hover 与 inline value 同用。 */
static const SZrSemanticExpressionFact *local_query_find_expression_fact(SZrSemanticAnalyzer *analyzer,
                                                                         SZrFileRange queryRange,
                                                                         const SZrSemanticReferenceFact *referenceFact) {
    SZrAstNode *expressionNode;
    const SZrSemanticExpressionFact *fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_NULL;
    }

    fact = local_query_find_reference_payload_expression_fact(analyzer, queryRange, referenceFact);
    if (fact != ZR_NULL) {
        return fact;
    }

    fact = local_query_find_payload_expression_fact(analyzer, queryRange);
    if (fact != ZR_NULL) {
        return fact;
    }

    expressionNode =
        ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(analyzer->ast, queryRange);
    if (expressionNode != ZR_NULL) {
        fact = ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, expressionNode);
        if (fact != ZR_NULL || local_query_node_prefers_structural_fact(expressionNode)) {
            return fact;
        }
    }

    return ZrLanguageServer_SemanticAnalyzer_FindExpressionFactAtPosition(analyzer, queryRange);
}

/* 优先取同一 AST 节点的数值事实；没有节点配对时取覆盖光标的最窄事实。 */
static const SZrSemanticNumericFact *local_query_find_numeric_fact(
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange queryRange,
    const SZrSemanticExpressionFact *expressionFact) {
    const SZrSemanticNumericFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL) {
        return ZR_NULL;
    }

    if (expressionFact != ZR_NULL && expressionFact->node != ZR_NULL) {
        best = ZrParser_SemanticFacts_FindNumericByNode(analyzer->semanticContext, expressionFact->node);
        if (best != ZR_NULL) {
            return best;
        }
    }

    if (!analyzer->semanticContext->numericFacts.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->semanticContext->numericFacts.length; index++) {
        const SZrSemanticNumericFact *fact =
            (const SZrSemanticNumericFact *)ZrCore_Array_Get(&analyzer->semanticContext->numericFacts, index);
        TZrSize width;

        if (fact == ZR_NULL ||
            !local_query_range_contains_position(fact->range, queryRange)) {
            continue;
        }

        width = local_query_range_width(fact->range);
        if (best == ZR_NULL || width <= bestWidth) {
            best = fact;
            bestWidth = width;
        }
    }

    return best;
}

/* 不可达位置可借触发它的条件节点补回逻辑事实，让 hover 同时解释原因与结果。 */
static const SZrSemanticLogicalFact *local_query_find_logical_fact(
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange queryRange,
    const SZrSemanticExpressionFact *expressionFact,
    const SZrSemanticReachabilityFact *reachabilityFact) {
    const SZrSemanticLogicalFact *fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_NULL;
    }

    if (expressionFact != ZR_NULL && expressionFact->node != ZR_NULL) {
        fact = ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext, expressionFact->node);
        if (fact != ZR_NULL) {
            return fact;
        }
    }

    fact = ZrParser_SemanticFacts_FindLogicalAtPosition(analyzer->semanticContext, queryRange);
    if (fact != ZR_NULL) {
        return fact;
    }

    if (reachabilityFact != ZR_NULL && reachabilityFact->causeNode != ZR_NULL) {
        if (reachabilityFact->causeNode->type == ZR_AST_IF_EXPRESSION) {
            fact = ZrParser_SemanticFacts_FindLogicalByNode(
                    analyzer->semanticContext,
                    reachabilityFact->causeNode->data.ifExpression.condition);
            if (fact != ZR_NULL) {
                return fact;
            }
        }
        if (reachabilityFact->causeNode->type == ZR_AST_WHILE_LOOP) {
            fact = ZrParser_SemanticFacts_FindLogicalByNode(
                    analyzer->semanticContext,
                    reachabilityFact->causeNode->data.whileLoop.cond);
            if (fact != ZR_NULL) {
                return fact;
            }
        }
        if (reachabilityFact->causeNode->type == ZR_AST_FOR_LOOP) {
            fact = ZrParser_SemanticFacts_FindLogicalByNode(
                    analyzer->semanticContext,
                    reachabilityFact->causeNode->data.forLoop.cond);
            if (fact != ZR_NULL) {
                return fact;
            }
        }
        return ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext,
                                                        reachabilityFact->causeNode);
    }

    return ZR_NULL;
}

/* 光标未直接落在可达性事实内时，尝试从关联逻辑节点恢复控制流说明。 */
static const SZrSemanticReachabilityFact *local_query_find_reachability_fact(
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange queryRange,
    const SZrSemanticLogicalFact *logicalFact) {
    const SZrSemanticReachabilityFact *fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_NULL;
    }

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(
            analyzer->semanticContext,
            queryRange);
    if (fact != ZR_NULL) {
        return fact;
    }

    if (logicalFact != ZR_NULL && logicalFact->relatedNode != ZR_NULL) {
        return ZrParser_SemanticFacts_FindReachabilityAtPosition(
                analyzer->semanticContext,
                logicalFact->relatedNode->location);
    }

    return ZR_NULL;
}

/* 所有权违规可能在表达式内部子范围，回退时优先展示最窄的违规事实。 */
static const SZrSemanticOwnershipFact *local_query_find_ownership_fact(
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange queryRange,
    const SZrSemanticExpressionFact *expressionFact) {
    const SZrSemanticOwnershipFact *fact;
    const SZrSemanticOwnershipFact *best = ZR_NULL;
    TZrSize bestWidth = 0;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_NULL;
    }

    if (expressionFact != ZR_NULL && expressionFact->node != ZR_NULL) {
        fact = ZrParser_SemanticFacts_FindOwnershipByNode(
                analyzer->semanticContext,
                expressionFact->node);
        if (fact != ZR_NULL) {
            return fact;
        }
    }

    fact = ZrParser_SemanticFacts_FindOwnershipAtPosition(
            analyzer->semanticContext,
            queryRange);
    if (fact != ZR_NULL) {
        return fact;
    }

    if (expressionFact != ZR_NULL &&
        analyzer->semanticContext->ownershipFacts.isValid) {
        for (TZrSize index = 0;
             index < analyzer->semanticContext->ownershipFacts.length;
             index++) {
            const SZrSemanticOwnershipFact *candidate =
                    (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                            &analyzer->semanticContext->ownershipFacts,
                            index);
            TZrSize width;

            if (candidate == ZR_NULL ||
                (candidate->range.source != expressionFact->range.source &&
                 !ZrLanguageServer_Lsp_StringsEqual(candidate->range.source,
                                                    expressionFact->range.source)) ||
                candidate->range.start.offset < expressionFact->range.start.offset ||
                candidate->range.end.offset > expressionFact->range.end.offset) {
                continue;
            }

            width = candidate->range.end.offset >= candidate->range.start.offset
                            ? candidate->range.end.offset - candidate->range.start.offset
                            : 0;
            if (best == ZR_NULL ||
                width < bestWidth ||
                (width == bestWidth && candidate->isViolation && !best->isViolation)) {
                best = candidate;
                bestWidth = width;
            }
        }
    }

    return best;
}

/* 一次查询从同一 analyzer 快照收集各类事实，保证附录各段对应同一文档版本。 */
static void local_query_collect_facts(SZrSemanticAnalyzer *analyzer,
                                      SZrLspLocalSemanticQueryResult *result) {
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || result == ZR_NULL) {
        return;
    }

    result->referenceFact =
        ZrParser_SemanticFacts_FindReferenceAtPosition(analyzer->semanticContext, result->queryRange);
    result->expressionFact =
        local_query_find_expression_fact(analyzer, result->queryRange, result->referenceFact);
    result->numericFact = local_query_find_numeric_fact(analyzer, result->queryRange, result->expressionFact);
    result->reachabilityFact =
        ZrParser_SemanticFacts_FindReachabilityAtPosition(analyzer->semanticContext, result->queryRange);
    result->logicalFact =
        local_query_find_logical_fact(analyzer,
                                      result->queryRange,
                                      result->expressionFact,
                                      result->reachabilityFact);
    if (result->reachabilityFact == ZR_NULL) {
        result->reachabilityFact = local_query_find_reachability_fact(
                analyzer,
                result->queryRange,
                result->logicalFact);
    }
    result->ownershipFact = local_query_find_ownership_fact(
            analyzer,
            result->queryRange,
            result->expressionFact);
    result->ownershipIntrinsicFact =
        ZrParser_SemanticFacts_FindOwnershipIntrinsicAtPosition(
                analyzer->semanticContext, result->queryRange);
}

/* 任何独立事实都可支撑局部 hover；UNKNOWN 表示查询成功但当前位置尚无可展示事实。 */
static void local_query_set_fact_status_if_any(SZrLspLocalSemanticQueryResult *result) {
    if (result == ZR_NULL) {
        return;
    }

    if (result->expressionFact != ZR_NULL ||
        result->numericFact != ZR_NULL ||
        result->referenceFact != ZR_NULL ||
        result->reachabilityFact != ZR_NULL ||
        result->logicalFact != ZR_NULL ||
        result->ownershipFact != ZR_NULL ||
        result->ownershipIntrinsicFact != ZR_NULL) {
        result->status = ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT;
    }
}

/* 栈结果首次使用前建立空借用视图，不分配事实内存。 */
void ZrLanguageServer_LspLocalSemanticQuery_Init(SZrLspLocalSemanticQueryResult *result) {
    if (result == ZR_NULL) {
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Clear(result);
}

/* 清除是解除指针关系，不释放 analyzer 的事实或文件版本中的诊断。 */
void ZrLanguageServer_LspLocalSemanticQuery_Clear(SZrLspLocalSemanticQueryResult *result) {
    if (result == ZR_NULL) {
        return;
    }

    result->status = ZR_LSP_LOCAL_SEMANTIC_QUERY_UNKNOWN;
    result->queryRange = ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 1, 1),
                                                   ZrParser_FilePosition_Create(0, 1, 1),
                                                   ZR_NULL);
    result->diagnostic = ZR_NULL;
    result->expressionFact = ZR_NULL;
    result->numericFact = ZR_NULL;
    result->referenceFact = ZR_NULL;
    result->reachabilityFact = ZR_NULL;
    result->logicalFact = ZR_NULL;
    result->ownershipFact = ZR_NULL;
    result->ownershipIntrinsicFact = ZR_NULL;
}

/* hover 与 stdio inline value 共用入口；parser 诊断遮挡优先于旧 AST/事实回退。 */
TZrBool ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspLocalSemanticQueryResult *result) {
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    local_query_seed(context, uri, position, result);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (local_query_set_diagnostic_if_blocked(fileVersion, result)) {
        return ZR_TRUE;
    }

    if (!ZrLanguageServer_LspSemanticQuery_TryGetAnalyzerForUri(
                state, context, uri, &analyzer) || analyzer->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    local_query_collect_facts(analyzer, result);
    local_query_set_fact_status_if_any(result);

    return ZR_TRUE;
}

/* 导航侧只需要引用事实，跳过表达式事实的多轮候选选择。 */
TZrBool ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspLocalSemanticQueryResult *result) {
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    local_query_seed(context, uri, position, result);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (local_query_set_diagnostic_if_blocked(fileVersion, result)) {
        return ZR_TRUE;
    }

    if (!ZrLanguageServer_LspSemanticQuery_TryGetAnalyzerForUri(
                state, context, uri, &analyzer) || analyzer->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    result->referenceFact =
        ZrParser_SemanticFacts_FindReferenceAtPosition(analyzer->semanticContext, result->queryRange);
    if (result->referenceFact != ZR_NULL) {
        result->status = ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT;
    }

    return ZR_TRUE;
}

/* 在请求生命周期内把借用事实物化成独立 hover；有文档时再附 canonical 类型。 */
static TZrBool local_query_build_hover_with_document(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover **result) {
    TZrChar markdown[ZR_LSP_MARKDOWN_BUFFER_SIZE];
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *typeText = ZR_NULL;
    TZrSize used = 0;
    SZrString *content;
    SZrLspHover *hover;

    if (result != ZR_NULL) {
        *result = ZR_NULL;
    }
    if (state == ZR_NULL ||
        query == ZR_NULL ||
        result == ZR_NULL ||
        query->status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT) {
        return ZR_FALSE;
    }

    markdown[0] = '\0';
    if (query->expressionFact != ZR_NULL) {
        if (context != ZR_NULL && uri != ZR_NULL) {
            SZrSemanticAnalyzer *analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
            SZrParserSemanticTypeQuery typeQuery;

            if (analyzer != ZR_NULL &&
                ZrParser_SemanticQuery_CanonicalTypeAt(analyzer->semanticContext,
                                                       query->queryRange,
                                                       ZR_NULL,
                                                       &typeQuery) &&
                ZrParser_CanonicalType_Format(analyzer->semanticContext,
                                              typeQuery.typeId,
                                              typeBuffer,
                                              sizeof(typeBuffer))) {
                typeText = typeBuffer;
            }
        }
        if (typeText == ZR_NULL || typeText[0] == '\0') {
            typeText = "unknown";
        }
        if (!local_query_append_format(markdown,
                                       sizeof(markdown),
                                       &used,
                                       "**expression**\n\nType: %s",
                                       typeText)) {
            return ZR_FALSE;
        }
    } else if (!local_query_append_format(markdown,
                                          sizeof(markdown),
                                          &used,
                                          "**semantic fact**")) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_LspLocalSemanticHoverText_AppendFacts(markdown,
                                                                sizeof(markdown),
                                                                &used,
                                                                query)) {
        return ZR_FALSE;
    }

    content = ZrCore_String_Create(state, markdown, used);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (hover == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
        context,
        uri,
        local_query_hover_range(query));
    *result = hover;
    return ZR_TRUE;
}

/* 测试和无文档调用者保留纯事实悬停，类型不可查时显式显示 unknown。 */
TZrBool ZrLanguageServer_LspLocalSemanticQuery_BuildHover(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover **result) {
    return local_query_build_hover_with_document(state, ZR_NULL, ZR_NULL, query, result);
}

/* 交给 LSP 请求入口的版本使用当前文档将 canonical 类型和范围投影进 hover。 */
TZrBool ZrLanguageServer_LspLocalSemanticQuery_BuildHoverForDocument(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover **result) {
    return local_query_build_hover_with_document(state, context, uri, query, result);
}

/* 将局部事实附到已有的符号/签名 hover；请求入口仍负责释放最终 hover。 */
void ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover *hover) {
    SZrString *appendix;
    SZrString **firstContent;
    SZrString *merged;

    if (state == ZR_NULL || query == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    appendix = ZrLanguageServer_LspLocalSemanticHoverText_BuildFactMarkdown(state, query);
    if (appendix == ZR_NULL) {
        return;
    }

    if (!hover->contents.isValid || hover->contents.length == 0) {
        if (!hover->contents.isValid) {
            ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
        }
        ZrCore_Array_Push(state, &hover->contents, &appendix);
        return;
    }

    firstContent = (SZrString **)ZrCore_Array_Get(&hover->contents, 0);
    if (firstContent == ZR_NULL || *firstContent == ZR_NULL) {
        return;
    }

    merged = ZrLanguageServer_LspLocalSemanticHoverText_AppendMarkdownSection(state, *firstContent, appendix);
    if (merged != ZR_NULL) {
        *firstContent = merged;
    }
}
