#include "semantic/lsp_semantic_implementation_query.h"
#include "semantic/semantic_analyzer_query_source.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool semantic_implementation_ranges_equal(
        SZrLspRange left,
        SZrLspRange right) {
    return left.start.line == right.start.line &&
           left.start.character == right.start.character &&
           left.end.line == right.end.line &&
           left.end.character == right.end.character;
}

static TZrBool semantic_implementation_has_location(
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
            semantic_implementation_ranges_equal((*slot)->range, range)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_implementation_append_location(
        SZrState *state,
        SZrLspContext *context,
        SZrArray *result,
        SZrString *uri,
        SZrFileRange range) {
    SZrLspLocation *location;
    SZrLspRange lspRange;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL ||
        uri == ZR_NULL) {
        return ZR_FALSE;
    }
    lspRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, uri, range);
    if (semantic_implementation_has_location(result, uri, lspRange)) {
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
    location->uri = uri;
    location->range = lspRange;
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspSemanticImplementationQuery_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    SZrLspSemanticQuery query;
    SZrParserSemanticQueryScope scope;
    SZrArray relations = {0};
    TZrSize index;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        result == ZR_NULL ||
        ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, position, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query.analyzer == ZR_NULL ||
        query.analyzer->semanticContext == ZR_NULL ||
        query.symbol == ZR_NULL ||
        query.symbol->semanticId == ZR_SEMANTIC_ID_INVALID) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        return ZR_FALSE;
    }

    ZrParser_SemanticQueryScope_Module(&scope);
    if (ZrParser_SemanticQuery_ImplementationsOf(
                query.analyzer->semanticContext,
                query.symbol->semanticId,
                &scope,
                &relations)) {
        for (index = 0U; index < relations.length; index++) {
            const SZrParserSemanticRelationQuery *relation =
                    (const SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
                            &relations, index);
            SZrFileRange sourceRange;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(
                        context)) {
                appended = ZR_FALSE;
                break;
            }
            if (relation == ZR_NULL || !relation->hasSourceRange) {
                continue;
            }
            sourceRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
                    query.analyzer, relation->sourceRange);
            if (sourceRange.source == ZR_NULL) {
                continue;
            }
            appended = semantic_implementation_append_location(
                    state,
                    context,
                    result,
                    sourceRange.source,
                    sourceRange) || appended;
        }
    }
    if (relations.isValid) {
        ZrCore_Array_Free(state, &relations);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    return appended;
}
