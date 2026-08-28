#include "semantic/lsp_semantic_call_hierarchy.h"

#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_query_source.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool semantic_call_hierarchy_symbol_is_callable(
        const SZrSymbol *symbol) {
    return symbol != ZR_NULL &&
           (symbol->type == ZR_SYMBOL_FUNCTION ||
            symbol->type == ZR_SYMBOL_METHOD);
}

static TZrInt32 semantic_call_hierarchy_symbol_kind(const SZrSymbol *symbol) {
    return symbol != ZR_NULL && symbol->type == ZR_SYMBOL_METHOD
                   ? ZR_LSP_SYMBOL_KIND_METHOD
                   : ZR_LSP_SYMBOL_KIND_FUNCTION;
}

static const SZrSemanticSymbolRecord *
semantic_call_hierarchy_find_semantic_symbol(
        SZrSemanticAnalyzer *analyzer,
        const SZrAstNode *astNode) {
    TZrSize index;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        astNode == ZR_NULL || !analyzer->semanticContext->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U;
         index < analyzer->semanticContext->symbols.length;
         index++) {
        const SZrSemanticSymbolRecord *candidate =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        &analyzer->semanticContext->symbols, index);

        if (candidate != ZR_NULL &&
            candidate->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
            candidate->astNode == astNode &&
            candidate->id != ZR_SEMANTIC_ID_INVALID &&
            candidate->typeId != ZR_SEMANTIC_ID_INVALID) {
            return candidate;
        }
    }
    return ZR_NULL;
}

static SZrSymbol *semantic_call_hierarchy_find_symbol(
        SZrSemanticAnalyzer *analyzer,
        TZrSymbolId semanticId) {
    const SZrSemanticSymbolRecord *semanticSymbol;
    TZrSize scopeIndex;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        semanticId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    semanticSymbol = ZrParser_Semantic_FindSymbolById(
            analyzer->semanticContext, semanticId);
    if (semanticSymbol == ZR_NULL ||
        semanticSymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        semanticSymbol->astNode == ZR_NULL) {
        return ZR_NULL;
    }
    for (scopeIndex = 0U;
         scopeIndex < analyzer->symbolTable->allScopes.length;
         scopeIndex++) {
        SZrSymbolScope **scopeSlot =
                (SZrSymbolScope **)ZrCore_Array_Get(
                        &analyzer->symbolTable->allScopes, scopeIndex);
        SZrSymbolScope *scope =
                scopeSlot != ZR_NULL ? *scopeSlot : ZR_NULL;
        TZrSize symbolIndex;

        for (symbolIndex = 0U;
             scope != ZR_NULL && symbolIndex < scope->symbols.length;
             symbolIndex++) {
            SZrSymbol **symbolSlot =
                    (SZrSymbol **)ZrCore_Array_Get(
                            &scope->symbols, symbolIndex);
            if (symbolSlot != ZR_NULL &&
                semantic_call_hierarchy_symbol_is_callable(*symbolSlot) &&
                (*symbolSlot)->astNode == semanticSymbol->astNode) {
                return *symbolSlot;
            }
        }
    }
    return ZR_NULL;
}

static TZrBool semantic_call_hierarchy_create_item(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *symbol,
        SZrFileRange declarationRange,
        SZrLspHierarchyItem **outItem) {
    const SZrSemanticSymbolRecord *semanticSymbol;
    SZrFileRange boundDeclaration;
    SZrFileRange boundSelection;
    SZrFileVersion *fileVersion;
    SZrLspHierarchyItem *item;

    if (outItem != ZR_NULL) {
        *outItem = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        !semantic_call_hierarchy_symbol_is_callable(symbol) ||
        symbol->astNode == ZR_NULL ||
        outItem == ZR_NULL) {
        return ZR_FALSE;
    }
    semanticSymbol = semantic_call_hierarchy_find_semantic_symbol(
            analyzer, symbol->astNode);
    if (semanticSymbol == ZR_NULL ||
        semanticSymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        semanticSymbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    boundDeclaration = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, declarationRange);
    boundSelection = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, symbol->selectionRange);
    if (boundDeclaration.source == ZR_NULL || boundSelection.source == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(
                boundDeclaration.source, boundSelection.source)) {
        return ZR_FALSE;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(
            context, boundDeclaration.source);
    if (fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }
    item = (SZrLspHierarchyItem *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspHierarchyItem));
    if (item == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(item, 0, sizeof(SZrLspHierarchyItem));
    item->name = symbol->name;
    item->kind = semantic_call_hierarchy_symbol_kind(symbol);
    item->uri = boundDeclaration.source;
    item->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, item->uri, boundDeclaration);
    item->selectionRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, item->uri, boundSelection);
    item->hasSemanticIdentity = ZR_TRUE;
    item->semanticId = semanticSymbol->id;
    item->semanticTypeId = semanticSymbol->typeId;
    item->semanticVersion = fileVersion->version;
    *outItem = item;
    return ZR_TRUE;
}

static TZrBool semantic_call_hierarchy_resolve_item(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrLspSemanticQuery *query) {
    const SZrSemanticSymbolRecord *semanticSymbol;
    SZrFileVersion *fileVersion;

    if (state == ZR_NULL || context == ZR_NULL || item == ZR_NULL ||
        query == ZR_NULL || item->uri == ZR_NULL ||
        !item->hasSemanticIdentity ||
        item->semanticId == ZR_SEMANTIC_ID_INVALID ||
        item->semanticTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(
            context, item->uri);
    if (fileVersion == ZR_NULL || fileVersion->version != item->semanticVersion) {
        return ZR_FALSE;
    }
    ZrLanguageServer_LspSemanticQuery_Init(query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state,
                context,
                item->uri,
                item->selectionRange.start,
                query) ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query->analyzer == ZR_NULL ||
        query->analyzer->semanticContext == ZR_NULL ||
        query->symbol == ZR_NULL ||
        !semantic_call_hierarchy_symbol_is_callable(query->symbol) ||
        query->symbol->astNode == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, query);
        return ZR_FALSE;
    }
    semanticSymbol = semantic_call_hierarchy_find_semantic_symbol(
            query->analyzer, query->symbol->astNode);
    if (semanticSymbol == ZR_NULL ||
        semanticSymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        semanticSymbol->id != item->semanticId ||
        semanticSymbol->typeId != item->semanticTypeId) {
        ZrLanguageServer_LspSemanticQuery_Free(state, query);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static SZrLspHierarchyCall *semantic_call_hierarchy_find_call(
        SZrArray *result,
        TZrSymbolId semanticId) {
    TZrSize index;

    if (result == ZR_NULL || !result->isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < result->length; index++) {
        SZrLspHierarchyCall **slot =
                (SZrLspHierarchyCall **)ZrCore_Array_Get(result, index);
        if (slot != ZR_NULL && *slot != ZR_NULL &&
            (*slot)->item != ZR_NULL &&
            (*slot)->item->hasSemanticIdentity &&
            (*slot)->item->semanticId == semanticId) {
            return *slot;
        }
    }
    return ZR_NULL;
}

static TZrBool semantic_call_hierarchy_has_range(
        const SZrArray *ranges,
        SZrLspRange range) {
    TZrSize index;

    if (ranges == ZR_NULL || !ranges->isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < ranges->length; index++) {
        const SZrLspRange *candidate =
                (const SZrLspRange *)ZrCore_Array_Get(
                        (SZrArray *)ranges, index);
        if (candidate != ZR_NULL &&
            candidate->start.line == range.start.line &&
            candidate->start.character == range.start.character &&
            candidate->end.line == range.end.line &&
            candidate->end.character == range.end.character) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_call_hierarchy_append_call(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *symbol,
        SZrFileRange declarationRange,
        SZrFileRange callSiteRange,
        SZrArray *result) {
    const SZrSemanticSymbolRecord *semanticSymbol;
    SZrFileRange boundCallSite;
    SZrLspHierarchyCall *call;
    SZrLspRange fromRange;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        symbol == ZR_NULL || symbol->astNode == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    semanticSymbol = semantic_call_hierarchy_find_semantic_symbol(
            analyzer, symbol->astNode);
    if (semanticSymbol == ZR_NULL) {
        return ZR_FALSE;
    }
    boundCallSite = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, callSiteRange);
    if (boundCallSite.source == ZR_NULL) {
        return ZR_FALSE;
    }
    call = semantic_call_hierarchy_find_call(result, semanticSymbol->id);
    if (call == ZR_NULL) {
        SZrLspHierarchyItem *item = ZR_NULL;

        if (!semantic_call_hierarchy_create_item(
                    state,
                    context,
                    analyzer,
                    symbol,
                    declarationRange,
                    &item) ||
            !ZrLanguageServer_Lsp_StringsEqual(
                    item->uri, boundCallSite.source)) {
            if (item != ZR_NULL) {
                ZrCore_Memory_RawFree(
                        state->global,
                        item,
                        sizeof(SZrLspHierarchyItem));
            }
            return ZR_FALSE;
        }
        call = (SZrLspHierarchyCall *)ZrCore_Memory_RawMalloc(
                state->global, sizeof(SZrLspHierarchyCall));
        if (call == ZR_NULL) {
            ZrCore_Memory_RawFree(
                    state->global, item, sizeof(SZrLspHierarchyItem));
            return ZR_FALSE;
        }
        call->item = item;
        ZrCore_Array_Init(
                state,
                &call->fromRanges,
                sizeof(SZrLspRange),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        ZrCore_Array_Push(state, result, &call);
    } else if (!ZrLanguageServer_Lsp_StringsEqual(
                       call->item->uri, boundCallSite.source)) {
        return ZR_FALSE;
    }
    fromRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, boundCallSite.source, boundCallSite);
    if (!semantic_call_hierarchy_has_range(&call->fromRanges, fromRange)) {
        ZrCore_Array_Push(state, &call->fromRanges, &fromRange);
    }
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspSemanticCallHierarchy_Prepare(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    SZrLspSemanticQuery query;
    TZrBool ok = ZR_TRUE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        result == ZR_NULL ||
        ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspHierarchyItem *),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, position, &query) &&
        query.kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL &&
        query.analyzer != ZR_NULL && query.symbol != ZR_NULL &&
        semantic_call_hierarchy_symbol_is_callable(query.symbol)) {
        SZrLspHierarchyItem *item;

        ok = semantic_call_hierarchy_create_item(
                state,
                context,
                query.analyzer,
                query.symbol,
                query.symbol->location,
                &item);
        if (ok) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    return ok;
}

static TZrBool semantic_call_hierarchy_append_edges(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        TZrBool incoming,
        SZrArray *result) {
    SZrLspSemanticQuery query;
    SZrArray edges = {0};
    TZrSize index;
    TZrBool ok = ZR_TRUE;
    TZrBool queried;

    if (state == ZR_NULL || context == ZR_NULL || item == ZR_NULL ||
        result == ZR_NULL ||
        ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspHierarchyCall *),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    if (!semantic_call_hierarchy_resolve_item(
                state, context, item, &query)) {
        return ZR_FALSE;
    }
    queried = incoming
                      ? ZrParser_SemanticQuery_IncomingCalls(
                                query.analyzer->semanticContext,
                                item->semanticId,
                                ZR_NULL,
                                &edges)
                      : ZrParser_SemanticQuery_OutgoingCalls(
                                query.analyzer->semanticContext,
                                item->semanticId,
                                ZR_NULL,
                                &edges);
    if (queried) {
        for (index = 0U; index < edges.length; index++) {
            const SZrParserSemanticCallEdgeQuery *edge =
                    (const SZrParserSemanticCallEdgeQuery *)ZrCore_Array_Get(
                            &edges, index);
            TZrSymbolId relatedId;
            SZrFileRange declarationRange;
            SZrSymbol *relatedSymbol;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(
                        context)) {
                ok = ZR_FALSE;
                break;
            }
            if (edge == ZR_NULL ||
                edge->resolution != ZR_SEMANTIC_CALL_EDGE_RESOLUTION_RESOLVED) {
                continue;
            }
            relatedId = incoming
                                ? edge->callerSymbolId
                                : edge->targetSymbolId;
            relatedSymbol = semantic_call_hierarchy_find_symbol(
                    query.analyzer, relatedId);
            if (!semantic_call_hierarchy_symbol_is_callable(relatedSymbol)) {
                continue;
            }
            if (incoming) {
                const SZrSemanticReferenceFact *declaration =
                        ZrParser_SemanticQuery_DeclarationOf(
                                query.analyzer->semanticContext,
                                relatedId,
                                ZR_NULL);
                if (declaration == ZR_NULL || !declaration->isResolved) {
                    continue;
                }
                declarationRange = declaration->range;
            } else {
                if (!edge->hasTargetDeclarationRange) {
                    continue;
                }
                declarationRange = edge->targetDeclarationRange;
            }
            if (!semantic_call_hierarchy_append_call(
                        state,
                        context,
                        query.analyzer,
                        relatedSymbol,
                        declarationRange,
                        edge->callSiteRange,
                        result)) {
                ok = ZR_FALSE;
                break;
            }
        }
    }
    if (edges.isValid) {
        ZrCore_Array_Free(state, &edges);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    return queried && ok;
}

TZrBool ZrLanguageServer_LspSemanticCallHierarchy_AppendIncoming(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return semantic_call_hierarchy_append_edges(
            state, context, item, ZR_TRUE, result);
}

TZrBool ZrLanguageServer_LspSemanticCallHierarchy_AppendOutgoing(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return semantic_call_hierarchy_append_edges(
            state, context, item, ZR_FALSE, result);
}
