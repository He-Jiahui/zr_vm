#include "semantic/lsp_semantic_reference_query.h"
#include "semantic/semantic_analyzer_query_source.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool semantic_reference_query_ranges_equal(
        SZrLspRange left,
        SZrLspRange right) {
    return left.start.line == right.start.line &&
           left.start.character == right.start.character &&
           left.end.line == right.end.line &&
           left.end.character == right.end.character;
}

static TZrBool semantic_reference_query_has_location(
        const SZrArray *result,
        SZrString *uri,
        SZrLspRange range) {
    TZrSize index;

    if (result == ZR_NULL || !result->isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < result->length; index++) {
        SZrLspLocation *const *slot =
                (SZrLspLocation *const *)ZrCore_Array_Get(
                        (SZrArray *)result, index);
        if (slot != ZR_NULL && *slot != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*slot)->uri, uri) &&
            semantic_reference_query_ranges_equal((*slot)->range, range)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendRange(
        SZrState *state,
        SZrLspContext *context,
        const SZrSemanticAnalyzer *analyzer,
        SZrArray *result,
        SZrFileRange range) {
    SZrLspLocation *location;
    SZrFileRange factRange;
    SZrLspRange lspRange;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL ||
        analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    factRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, range);
    if (factRange.source == ZR_NULL) {
        return ZR_FALSE;
    }
    lspRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, factRange.source, factRange);
    if (semantic_reference_query_has_location(
                result, factRange.source, lspRange)) {
        return ZR_TRUE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspLocation *),
                ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }
    location->uri = factRange.source;
    location->range = lspRange;
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static TZrBool semantic_reference_query_append_location(
        SZrState *state,
        SZrLspContext *context,
        const SZrSemanticAnalyzer *analyzer,
        SZrArray *result,
        const SZrSemanticReferenceFact *fact) {
    return fact != ZR_NULL &&
           ZrLanguageServer_LspSemanticReferenceQuery_AppendRange(
                   state, context, analyzer, result, fact->range);
}

static TZrInt32 semantic_reference_query_highlight_kind(
        EZrSemanticReferenceKind kind) {
    return kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
                   kind == ZR_SEMANTIC_REFERENCE_WRITE ||
                   kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE
            ? 3
            : 2;
}

static TZrSymbolId semantic_reference_query_symbol_id(
        const SZrLspSemanticQuery *query) {
    if (query == ZR_NULL || !query->hasCanonicalSymbol ||
        query->canonicalSymbol.symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    if (query->symbol != ZR_NULL &&
        query->symbol->semanticId != query->canonicalSymbol.symbolId) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return query->canonicalSymbol.symbolId;
}

static SZrLspDocumentHighlight *semantic_reference_query_find_highlight(
        const SZrArray *result,
        SZrLspRange range) {
    TZrSize index;

    if (result == ZR_NULL || !result->isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < result->length; index++) {
        SZrLspDocumentHighlight *const *slot =
                (SZrLspDocumentHighlight *const *)ZrCore_Array_Get(
                        (SZrArray *)result, index);
        if (slot != ZR_NULL && *slot != ZR_NULL &&
            semantic_reference_query_ranges_equal((*slot)->range, range)) {
            return *slot;
        }
    }
    return ZR_NULL;
}

static TZrBool semantic_reference_query_append_highlight(
        SZrState *state,
        SZrLspContext *context,
        const SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrArray *result,
        const SZrSemanticReferenceFact *fact) {
    SZrLspDocumentHighlight *highlight;
    SZrFileRange factRange;
    SZrLspRange range;
    TZrInt32 kind;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        uri == ZR_NULL || result == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    factRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, fact->range);
    if (factRange.source == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(factRange.source, uri)) {
        return ZR_FALSE;
    }
    range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, uri, factRange);
    kind = semantic_reference_query_highlight_kind(fact->kind);
    highlight = semantic_reference_query_find_highlight(result, range);
    if (highlight != ZR_NULL) {
        if (kind == 3) {
            highlight->kind = kind;
        }
        return ZR_TRUE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspDocumentHighlight *),
                ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight == ZR_NULL) {
        return ZR_FALSE;
    }
    highlight->range = range;
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

static TZrBool semantic_reference_query_append_references_for_symbol_id(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        TZrSymbolId symbolId,
        TZrBool includeDeclaration,
        SZrArray *result,
        TZrBool *outAppended) {
    SZrArray references = {0};
    const SZrSemanticReferenceFact *declaration;
    TZrSize index;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        result == ZR_NULL || outAppended == ZR_NULL) {
        return ZR_FALSE;
    }
    *outAppended = ZR_FALSE;
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            analyzer->semanticContext,
            symbolId,
            ZR_NULL);
    if (includeDeclaration && declaration != ZR_NULL) {
        appended = semantic_reference_query_append_location(
                state, context, analyzer, result, declaration);
    }
    if (ZrParser_SemanticQuery_ReferencesOf(
                analyzer->semanticContext,
                symbolId,
                ZR_NULL,
                &references)) {
        for (index = 0U; index < references.length; index++) {
            const SZrSemanticReferenceFact *const *slot =
                    (const SZrSemanticReferenceFact *const *)ZrCore_Array_Get(
                            &references, index);
            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(
                        context)) {
                ZrCore_Array_Free(state, &references);
                return ZR_FALSE;
            }
            if (slot == ZR_NULL || *slot == ZR_NULL ||
                (*slot)->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
                !(*slot)->isResolved) {
                continue;
            }
            appended = semantic_reference_query_append_location(
                    state, context, analyzer, result, *slot) || appended;
        }
    }
    if (references.isValid) {
        ZrCore_Array_Free(state, &references);
    }
    *outAppended = appended;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        TZrBool includeDeclaration,
        SZrArray *result) {
    TZrBool appended = ZR_FALSE;
    TZrSymbolId symbolId;

    if (query == ZR_NULL ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL) {
        return ZR_FALSE;
    }
    symbolId = semantic_reference_query_symbol_id(query);
    if (!semantic_reference_query_append_references_for_symbol_id(
                state,
                context,
                query->analyzer,
                symbolId,
                includeDeclaration,
                result,
                &appended)) {
        return ZR_FALSE;
    }
    return appended;
}

TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendHighlights(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result) {
    SZrArray references = {0};
    const SZrSemanticReferenceFact *declaration;
    TZrSize index;
    TZrBool appended = ZR_FALSE;
    TZrSymbolId symbolId;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query->analyzer == ZR_NULL || query->analyzer->semanticContext == ZR_NULL ||
        query->uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    symbolId = semantic_reference_query_symbol_id(query);
    if (symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            query->analyzer->semanticContext,
            symbolId,
            ZR_NULL);
    if (declaration != ZR_NULL) {
        appended = semantic_reference_query_append_highlight(
                state,
                context,
                query->analyzer,
                query->uri,
                result,
                declaration);
    }
    if (ZrParser_SemanticQuery_ReferencesOf(
                query->analyzer->semanticContext,
                symbolId,
                ZR_NULL,
                &references)) {
        for (index = 0U; index < references.length; index++) {
            const SZrSemanticReferenceFact *const *slot =
                    (const SZrSemanticReferenceFact *const *)ZrCore_Array_Get(
                            &references, index);
            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(
                        context)) {
                ZrCore_Array_Free(state, &references);
                return ZR_FALSE;
            }
            if (slot == ZR_NULL || *slot == ZR_NULL ||
                (*slot)->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
                !(*slot)->isResolved) {
                continue;
            }
            appended = semantic_reference_query_append_highlight(
                    state,
                    context,
                    query->analyzer,
                    query->uri,
                    result,
                    *slot) || appended;
        }
    }
    if (references.isValid) {
        ZrCore_Array_Free(state, &references);
    }
    return appended;
}
