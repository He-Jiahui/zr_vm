#include "semantic/lsp_cross_snapshot_references.h"

#include "metadata/lsp_external_metadata_identity.h"
#include "semantic/lsp_external_target_identity.h"
#include "project/lsp_project_internal.h"
#include "semantic/lsp_semantic_reference_query.h"

#include "zr_vm_parser/semantic_query.h"

static TZrBool cross_snapshot_references_same_declaration(
        const SZrFileRange *left,
        const SZrLspExternalMetadataIdentityDeclaration *right) {
    return left != ZR_NULL && right != ZR_NULL && left->source != ZR_NULL &&
           right->uri != ZR_NULL &&
           ZrLanguageServer_Lsp_StringsEqual(left->source, right->uri) &&
           left->start.line == right->range.start.line &&
           left->start.column == right->range.start.column &&
           left->end.line == right->range.end.line &&
           left->end.column == right->range.end.column;
}

static TZrBool cross_snapshot_references_append_analyzer(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrSemanticAnalyzer *analyzer,
        const SZrFileRange *targetDeclaration,
        SZrArray *result,
        TZrBool *outAppended) {
    SZrLspMetadataProvider provider;
    SZrArray externalReferences = {0};
    TZrSize index;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL ||
        analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        targetDeclaration == ZR_NULL || result == ZR_NULL ||
        outAppended == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    if (!ZrParser_SemanticQuery_ExternalReferences(
                analyzer->semanticContext, ZR_NULL, &externalReferences)) {
        if (externalReferences.isValid) {
            ZrCore_Array_Free(state, &externalReferences);
        }
        return ZR_TRUE;
    }

    for (index = 0U; index < externalReferences.length; index++) {
        const SZrParserSemanticExternalReferenceQuery *identity =
                (const SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                        &externalReferences, index);
        SZrLspExternalMetadataIdentityDeclaration declaration;

        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            ZrCore_Array_Free(state, &externalReferences);
            return ZR_FALSE;
        }
        if (identity == ZR_NULL ||
            !ZrLanguageServer_LspExternalMetadataIdentity_ResolveDeclaration(
                    &provider,
                    analyzer,
                    projectIndex,
                    identity,
                    &declaration) ||
            !cross_snapshot_references_same_declaration(
                    targetDeclaration, &declaration)) {
            continue;
        }
        *outAppended =
                ZrLanguageServer_LspSemanticReferenceQuery_AppendRange(
                        state,
                        context,
                        analyzer,
                        result,
                        identity->referenceRange) ||
                *outAppended;
    }

    ZrCore_Array_Free(state, &externalReferences);
    return ZR_TRUE;
}

static TZrBool cross_snapshot_references_append_external_analyzer(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticSymbolQuery *target,
        SZrArray *result,
        TZrBool *outAppended) {
    SZrArray externalReferences = {0};

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || target == ZR_NULL ||
        result == ZR_NULL || outAppended == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrParser_SemanticQuery_ExternalReferences(
                analyzer->semanticContext, ZR_NULL, &externalReferences)) {
        if (externalReferences.isValid) {
            ZrCore_Array_Free(state, &externalReferences);
        }
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < externalReferences.length; index++) {
        const SZrParserSemanticExternalReferenceQuery *reference =
                (const SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                        &externalReferences, index);
        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            ZrCore_Array_Free(state, &externalReferences);
            return ZR_FALSE;
        }
        if (ZrLanguageServer_LspExternalTargetIdentity_MatchesReference(
                    target, reference)) {
            *outAppended =
                    ZrLanguageServer_LspSemanticReferenceQuery_AppendRange(
                            state,
                            context,
                            analyzer,
                            result,
                            reference->referenceRange) ||
                    *outAppended;
        }
    }
    ZrCore_Array_Free(state, &externalReferences);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspCrossSnapshotReferences_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result) {
    SZrFileRange targetDeclaration;
    TZrSize index;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        !query->hasCanonicalSymbol || query->projectIndex == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    targetDeclaration = query->canonicalSymbol.declarationRange;
    if (targetDeclaration.source == ZR_NULL ||
        !ZrLanguageServer_LspProject_EnsureScannedSourceGraph(
                state, context, query->projectIndex)) {
        return ZR_FALSE;
    }

    for (index = 0U; index < query->projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **record =
                (SZrLspProjectFileRecord **)ZrCore_Array_Get(
                        &query->projectIndex->files, index);
        SZrSemanticAnalyzer *analyzer = ZR_NULL;

        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            return ZR_FALSE;
        }
        if (record == ZR_NULL || *record == ZR_NULL || (*record)->uri == ZR_NULL ||
            !ZrLanguageServer_LspSemanticQuery_TryGetAnalyzerForUri(
                    state, context, (*record)->uri, &analyzer) ||
            analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
            continue;
        }
        if (!cross_snapshot_references_append_analyzer(
                    state,
                    context,
                    query->projectIndex,
                    analyzer,
                    &targetDeclaration,
                    result,
                    &appended)) {
            return ZR_FALSE;
        }
    }
    return appended;
}

TZrBool ZrLanguageServer_LspCrossSnapshotReferences_AppendExternal(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER ||
        !query->hasCanonicalSymbol ||
        !ZrLanguageServer_LspExternalTargetIdentity_IsAvailable(
                &query->canonicalSymbol) ||
        (query->canonicalSymbol.externalProviderGeneration != 0U &&
         query->canonicalSymbol.externalProviderGeneration !=
                 context->semanticSnapshotProviderGeneration) ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!cross_snapshot_references_append_external_analyzer(
                state, context, query->analyzer, &query->canonicalSymbol,
                result, &appended)) {
        return ZR_FALSE;
    }
    if (query->projectIndex == ZR_NULL) {
        return appended;
    }
    if (!ZrLanguageServer_LspProject_EnsureScannedSourceGraph(
                state, context, query->projectIndex)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0U; index < query->projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **record =
                (SZrLspProjectFileRecord **)ZrCore_Array_Get(
                        &query->projectIndex->files, index);
        SZrSemanticAnalyzer *analyzer = ZR_NULL;

        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            return ZR_FALSE;
        }
        if (record == ZR_NULL || *record == ZR_NULL || (*record)->uri == ZR_NULL ||
            !ZrLanguageServer_LspSemanticQuery_TryGetAnalyzerForUri(
                    state, context, (*record)->uri, &analyzer) ||
            analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
            continue;
        }
        if (analyzer == query->analyzer) {
            continue;
        }
        if (!cross_snapshot_references_append_external_analyzer(
                    state,
                    context,
                    analyzer,
                    &query->canonicalSymbol,
                    result,
                    &appended)) {
            return ZR_FALSE;
        }
    }
    return appended;
}
