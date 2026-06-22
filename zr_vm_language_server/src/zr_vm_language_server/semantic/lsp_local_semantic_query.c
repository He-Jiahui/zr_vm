#include "semantic/lsp_local_semantic_query.h"

#include "interface/lsp_interface_internal.h"
#include "semantic/lsp_local_semantic_hover_text.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_language_server/incremental_parser.h"
#include "zr_vm_parser/type_inference.h"

#include <stdarg.h>
#include <stdio.h>

#define ZR_LSP_LOCAL_QUERY_VISIBLE_RANGE_LINE_WIDTH ((TZrSize)100000)

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

static SZrFileRange local_query_hover_range(const SZrLspLocalSemanticQueryResult *query) {
    if (query == ZR_NULL) {
        return ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 1, 1),
                                         ZrParser_FilePosition_Create(0, 1, 1),
                                         ZR_NULL);
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

static TZrSize local_query_range_width(SZrFileRange range) {
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

static SZrFileRange local_query_position_range(SZrLspContext *context,
                                               SZrString *uri,
                                               SZrLspPosition position) {
    SZrFilePosition filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    return ZrParser_FileRange_Create(filePosition, filePosition, uri);
}

static void local_query_seed(SZrLspContext *context,
                             SZrString *uri,
                             SZrLspPosition position,
                             SZrLspLocalSemanticQueryResult *result) {
    ZrLanguageServer_LspLocalSemanticQuery_Clear(result);
    result->status = ZR_LSP_LOCAL_SEMANTIC_QUERY_UNKNOWN;
    result->queryRange = local_query_position_range(context, uri, position);
}

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

static TZrBool local_query_reference_is_member_payload(const SZrSemanticReferenceFact *referenceFact) {
    return referenceFact != ZR_NULL &&
           (referenceFact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
            referenceFact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE);
}

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
    result->ownershipFact =
        ZrParser_SemanticFacts_FindOwnershipAtPosition(analyzer->semanticContext, result->queryRange);
}

static void local_query_materialize_expression_fact(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrFileRange queryRange,
                                                    const SZrSemanticReferenceFact *referenceFact) {
    SZrInferredType inferredType;
    SZrAstNode *expressionNode;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        analyzer->compilerState == ZR_NULL) {
        return;
    }

    if (local_query_reference_is_member_payload(referenceFact) && referenceFact->node != ZR_NULL) {
        ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        (void)ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state,
                                                                         analyzer,
                                                                         referenceFact->node,
                                                                         &inferredType);
        ZrParser_InferredType_Free(state, &inferredType);
        return;
    }

    expressionNode =
        ZrLanguageServer_SemanticAnalyzer_FindExpressionNodeAtPosition(analyzer->ast, queryRange);
    if (expressionNode != ZR_NULL) {
        ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        (void)ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state,
                                                                         analyzer,
                                                                         expressionNode,
                                                                         &inferredType);
        ZrParser_InferredType_Free(state, &inferredType);
        return;
    }

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    (void)ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(state,
                                                                  analyzer,
                                                                  queryRange,
                                                                  &inferredType);
    ZrParser_InferredType_Free(state, &inferredType);
}

static void local_query_set_fact_status_if_any(SZrLspLocalSemanticQueryResult *result) {
    if (result == ZR_NULL) {
        return;
    }

    if (result->expressionFact != ZR_NULL ||
        result->numericFact != ZR_NULL ||
        result->referenceFact != ZR_NULL ||
        result->reachabilityFact != ZR_NULL ||
        result->logicalFact != ZR_NULL ||
        result->ownershipFact != ZR_NULL) {
        result->status = ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT;
    }
}

static TZrBool local_query_should_materialize_expression_fact(const SZrLspLocalSemanticQueryResult *result) {
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (local_query_reference_is_member_payload(result->referenceFact) &&
        (result->expressionFact == ZR_NULL || !result->expressionFact->hasMemberInfo)) {
        return ZR_TRUE;
    }

    return result->expressionFact == ZR_NULL ||
           (result->expressionFact->inferredType.baseType == ZR_VALUE_TYPE_BOOL &&
            result->logicalFact == ZR_NULL);
}

void ZrLanguageServer_LspLocalSemanticQuery_Init(SZrLspLocalSemanticQueryResult *result) {
    if (result == ZR_NULL) {
        return;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Clear(result);
}

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
}

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

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    local_query_collect_facts(analyzer, result);
    if (local_query_should_materialize_expression_fact(result)) {
        local_query_materialize_expression_fact(state, analyzer, result->queryRange, result->referenceFact);
        local_query_collect_facts(analyzer, result);
    }
    local_query_set_fact_status_if_any(result);

    return ZR_TRUE;
}

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

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    result->referenceFact =
        ZrParser_SemanticFacts_FindReferenceAtPosition(analyzer->semanticContext, result->queryRange);
    if (result->referenceFact != ZR_NULL) {
        result->status = ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT;
    }

    return ZR_TRUE;
}

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
        typeText = ZrParser_TypeNameString_Get(state,
                                               &query->expressionFact->inferredType,
                                               typeBuffer,
                                               sizeof(typeBuffer));
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

TZrBool ZrLanguageServer_LspLocalSemanticQuery_BuildHover(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover **result) {
    return local_query_build_hover_with_document(state, ZR_NULL, ZR_NULL, query, result);
}

TZrBool ZrLanguageServer_LspLocalSemanticQuery_BuildHoverForDocument(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover **result) {
    return local_query_build_hover_with_document(state, context, uri, query, result);
}

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
