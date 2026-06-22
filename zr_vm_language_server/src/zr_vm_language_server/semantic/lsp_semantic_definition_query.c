#include "semantic/lsp_semantic_definition_query.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

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

TZrBool ZrLanguageServer_LspSemanticDefinitionQuery_AppendReachingDefinition(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result) {
    SZrParserSemanticQueryScope scope;
    SZrArray definitions;
    TZrSize index;
    TZrBool appended = ZR_FALSE;
    SZrFileRange definitionRange;
    SZrString *definitionUri;

    if (state == ZR_NULL ||
        context == ZR_NULL ||
        query == ZR_NULL ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query->analyzer == ZR_NULL ||
        query->analyzer->semanticContext == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(query->analyzer->semanticContext);
    if (query->analyzer->ast != ZR_NULL) {
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
        return ZR_FALSE;
    }

    for (index = 0; index < definitions.length; index++) {
        const SZrSemanticReferenceFact **definitionSlot =
                (const SZrSemanticReferenceFact **)ZrCore_Array_Get(&definitions, index);
        const SZrSemanticReferenceFact *definition =
                definitionSlot != ZR_NULL ? *definitionSlot : ZR_NULL;
        if (definition == ZR_NULL) {
            continue;
        }

        definitionRange = definition->hasDefinitionRange ? definition->definitionRange : definition->range;
        definitionUri = definitionRange.source != ZR_NULL ? definitionRange.source : query->uri;
        if (definitionUri == ZR_NULL && query->symbol != ZR_NULL) {
            definitionUri = query->symbol->location.source;
        }

        appended = semantic_definition_query_append_location(state,
                                                             context,
                                                             result,
                                                             definitionUri,
                                                             definitionRange) || appended;
    }

    ZrCore_Array_Free(query->analyzer->semanticContext->state, &definitions);
    return appended;
}
