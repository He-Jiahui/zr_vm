#include <stdint.h>
#include "semantic/query/lsp_semantic_definition_query.h"
#include "semantic/analyzer/semantic_analyzer_query_source.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

/* 将解析器事实的源位置转为 LSP Location，结果所有权交给上层导航结果数组。 */
static TZrBool semantic_definition_query_append_location(SZrState *state,
                                                         SZrLspContext *context,
                                                         SZrArray *result,
                                                         SZrString *uri,
                                                         SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = uri;
    location->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

/* FNV-1a over the identity fields that reaching-definition resolution never writes. */
static TZrUInt64 semantic_definition_query_fact_signature(const SZrSemanticContext *context) {
    TZrUInt64 hash = 1469598103934665603ULL;

    for (TZrSize index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get((SZrArray *)&context->referenceFacts, index);
        TZrUInt64 words[8];

        if (fact == ZR_NULL) {
            continue;
        }
        words[0] = (TZrUInt64)(uintptr_t)fact->node;
        words[1] = (TZrUInt64)fact->kind;
        words[2] = (TZrUInt64)fact->symbolId;
        words[3] = (TZrUInt64)fact->isResolved;
        words[4] = (TZrUInt64)fact->range.start.offset;
        words[5] = (TZrUInt64)fact->range.end.offset;
        words[6] = ((TZrUInt64)(TZrUInt32)fact->range.start.line << 32) | (TZrUInt32)fact->range.start.column;
        words[7] = (TZrUInt64)(uintptr_t)fact->range.source;
        for (TZrSize w = 0; w < 8; w++) {
            hash ^= words[w];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

/* Linear and CFG reaching definitions rewrite only definition ranges and are idempotent, so on an
 * unchanged analysis (same context, AST, execution, facts) a repeated definition request (e.g. VS
 * Code Ctrl+hover) can reuse the previous resolution instead of re-solving the whole file. */
static void semantic_definition_query_resolve_reaching_definitions(SZrSemanticAnalyzer *analyzer) {
    SZrSemanticReachingDefinitionCache *cache = &analyzer->reachingDefinitionCache;
    SZrSemanticContext *context = analyzer->semanticContext;
    TZrUInt64 signature = semantic_definition_query_fact_signature(context);
    TZrBool resolved = ZR_TRUE;

    if (cache->isValid && cache->context == context && cache->ast == analyzer->ast &&
        cache->executionCount == analyzer->metrics.executionCount &&
        cache->factCount == context->referenceFacts.length && cache->factSignature == signature) {
        return;
    }
    cache->isValid = ZR_FALSE;
    ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(context);
    if (analyzer->ast != ZR_NULL) {
        /* TODO: CFG 解析返回失败时这里仍继续查询；需构造解析失败的快照，
         * 核对线性事实是否可能让跳转落到不可达赋值，或是否应回退到声明。 */
        resolved = ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(context, analyzer->ast);
    }
    if (resolved) {
        cache->context = context;
        cache->ast = analyzer->ast;
        cache->executionCount = analyzer->metrics.executionCount;
        cache->factCount = context->referenceFacts.length;
        cache->factSignature = signature;
        cache->isValid = ZR_TRUE;
    }
}

/* 跳转定义优先使用光标处控制流可达的赋值，解析失败时保留声明导航。 */
TZrBool ZrLanguageServer_LspSemanticDefinitionQuery_AppendReachingDefinition(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result) {
    SZrParserSemanticQueryScope scope;
    SZrArray definitions;
    const SZrSemanticReferenceFact *declaration;
    TZrSize index;
    TZrBool appended = ZR_FALSE;
    SZrFileRange definitionRange;
    TZrSymbolId symbolId;

    if (state == ZR_NULL ||
        context == ZR_NULL ||
        query == ZR_NULL ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query->analyzer == ZR_NULL ||
        query->analyzer->semanticContext == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    symbolId = query->hasCanonicalSymbol &&
                       (query->symbol == ZR_NULL ||
                        query->symbol->semanticId == query->canonicalSymbol.symbolId)
            ? query->canonicalSymbol.symbolId
            : query->symbol != ZR_NULL
                    ? query->symbol->semanticId
                    : ZR_SEMANTIC_ID_INVALID;
    if (symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    /* 先尝试补齐线性及 CFG 路径；DefinitionsOf 据此选择使用点可达的定义集合。 */
    semantic_definition_query_resolve_reaching_definitions(query->analyzer);
    ZrParser_SemanticQueryScope_Module(&scope);

    ZrCore_Array_Construct(&definitions);
    if (!ZrParser_SemanticQuery_DefinitionsOf(query->analyzer->semanticContext,
                                              query->queryRange,
                                              &scope,
                                              &definitions)) {
        if (definitions.isValid) {
            ZrCore_Array_Free(query->analyzer->semanticContext->state, &definitions);
        }
        declaration = ZrParser_SemanticQuery_DeclarationOf(
                query->analyzer->semanticContext,
                symbolId,
                &scope);
        if (declaration == ZR_NULL) {
            return ZR_FALSE;
        }
        definitionRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
                query->analyzer,
                declaration->hasDefinitionRange
                        ? declaration->definitionRange
                        : declaration->range);
        return definitionRange.source != ZR_NULL &&
               semantic_definition_query_append_location(
                       state,
                       context,
                       result,
                       definitionRange.source,
                       definitionRange);
    }

    for (index = 0; index < definitions.length; index++) {
        const SZrSemanticReferenceFact **definitionSlot =
                (const SZrSemanticReferenceFact **)ZrCore_Array_Get(&definitions, index);
        const SZrSemanticReferenceFact *definition =
                definitionSlot != ZR_NULL ? *definitionSlot : ZR_NULL;
        if (definition == ZR_NULL) {
            continue;
        }

        definitionRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
                query->analyzer,
                definition->hasDefinitionRange
                        ? definition->definitionRange
                        : definition->range);
        if (definitionRange.source == ZR_NULL) {
            continue;
        }

        appended = semantic_definition_query_append_location(state,
                                                             context,
                                                             result,
                                                             definitionRange.source,
                                                             definitionRange) || appended;
    }

    ZrCore_Array_Free(query->analyzer->semanticContext->state, &definitions);
    return appended;
}
