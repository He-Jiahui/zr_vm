#include "semantic/lsp_semantic_definition_query.h"
#include "semantic/semantic_analyzer_query_source.h"

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
    ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(query->analyzer->semanticContext);
    if (query->analyzer->ast != ZR_NULL) {
        /* TODO: CFG 解析返回失败时这里仍继续查询；需构造解析失败的快照，
         * 核对线性事实是否可能让跳转落到不可达赋值，或是否应回退到声明。 */
        (void)ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(
                query->analyzer->semanticContext,
                query->analyzer->ast);
    }
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
