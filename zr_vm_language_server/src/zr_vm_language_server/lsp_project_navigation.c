#include "lsp_project_internal.h"

#include "zr_vm_core/memory.h"

static TZrBool append_lsp_location(SZrState *state, SZrArray *result, SZrString *uri, SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 4);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = uri;
    location->range = ZrLanguageServer_LspRange_FromFileRange(range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static SZrSymbol *find_global_symbol_by_name(SZrSemanticAnalyzer *analyzer, SZrString *name, SZrString *uri) {
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || analyzer->symbolTable->globalScope == ZR_NULL ||
        name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->symbolTable->globalScope->symbols.length; index++) {
        SZrSymbol **symbolPtr =
            (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*symbolPtr)->name, name) &&
            (uri == ZR_NULL || ZrLanguageServer_Lsp_StringsEqual((*symbolPtr)->location.source, uri))) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool symbol_is_project_global(SZrSemanticAnalyzer *analyzer, SZrSymbol *symbol) {
    return analyzer != ZR_NULL && analyzer->symbolTable != ZR_NULL &&
           analyzer->symbolTable->globalScope != ZR_NULL && symbol != ZR_NULL &&
           symbol->scope == analyzer->symbolTable->globalScope;
}

static TZrBool append_symbol_references_from_tracker(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrSymbol *symbol,
                                                     TZrBool includeDeclaration,
                                                     SZrArray *result) {
    SZrArray references;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->referenceTracker == ZR_NULL || symbol == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), 8);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < references.length; index++) {
        SZrReference **referencePtr = (SZrReference **)ZrCore_Array_Get(&references, index);
        if (referencePtr == ZR_NULL || *referencePtr == ZR_NULL ||
            ((*referencePtr)->type == ZR_REFERENCE_DEFINITION && !includeDeclaration)) {
            continue;
        }

        if (!append_lsp_location(state, result, (*referencePtr)->location.source, (*referencePtr)->location)) {
            ZrCore_Array_Free(state, &references);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

static TZrBool append_project_imported_references(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrLspProjectIndex *projectIndex,
                                                  SZrLspProjectFileRecord *record,
                                                  SZrSymbol *symbol,
                                                  SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || record == ZR_NULL ||
        symbol == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize fileIndex = 0; fileIndex < projectIndex->files.length; fileIndex++) {
        SZrLspProjectFileRecord **candidatePtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, fileIndex);
        SZrSemanticAnalyzer *candidateAnalyzer;
        SZrArray bindings;
        TZrBool appended;

        if (candidatePtr == ZR_NULL || *candidatePtr == ZR_NULL) {
            continue;
        }

        candidateAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, (*candidatePtr)->uri);
        if (candidateAnalyzer == ZR_NULL || candidateAnalyzer->ast == ZR_NULL) {
            continue;
        }

        ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), 4);
        ZrLanguageServer_LspProject_CollectImportBindings(state, candidateAnalyzer->ast, &bindings);
        appended = ZrLanguageServer_LspProject_AppendMatchingImportedMemberLocations(state,
                                                                                     candidateAnalyzer->ast,
                                                                                     &bindings,
                                                                                     record->moduleName,
                                                                                     symbol->name,
                                                                                     result);
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        if (!appended) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool project_resolve_symbol_at_position(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  TZrBool allowLocalProjectSymbol,
                                                  SZrLspProjectResolvedSymbol *outResolved) {
    SZrLspProjectIndex *projectIndex;
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrArray bindings;
    SZrLspImportedMemberHit hit;
    SZrLspProjectFileRecord *record;
    SZrSemanticAnalyzer *targetAnalyzer;
    SZrSymbol *targetSymbol;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (projectIndex == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), 4);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    if (ZrLanguageServer_LspProject_FindImportedMemberHit(analyzer->ast, &bindings, fileRange, &hit)) {
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        record = ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, hit.moduleName);
        if (record == ZR_NULL) {
            return ZR_FALSE;
        }

        targetAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, record->uri);
        if (targetAnalyzer == ZR_NULL) {
            targetAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, record->uri);
        }
        targetSymbol = find_global_symbol_by_name(targetAnalyzer, hit.memberName, record->uri);
        if (targetAnalyzer == ZR_NULL || targetSymbol == ZR_NULL) {
            return ZR_FALSE;
        }

        outResolved->projectIndex = projectIndex;
        outResolved->record = record;
        outResolved->analyzer = targetAnalyzer;
        outResolved->symbol = targetSymbol;
        return ZR_TRUE;
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);

    if (!allowLocalProjectSymbol) {
        return ZR_FALSE;
    }

    targetSymbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, fileRange);
    if (!symbol_is_project_global(analyzer, targetSymbol) || targetSymbol->location.source == ZR_NULL) {
        return ZR_FALSE;
    }

    record = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, targetSymbol->location.source);
    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    targetAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, record->uri);
    if (targetAnalyzer == ZR_NULL) {
        targetAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, record->uri);
    }
    if (targetAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    outResolved->projectIndex = projectIndex;
    outResolved->record = record;
    outResolved->analyzer = targetAnalyzer;
    outResolved->symbol = targetSymbol;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_ProjectTryGetDefinition(SZrState *state,
                                                     SZrLspContext *context,
                                                     SZrString *uri,
                                                     SZrLspPosition position,
                                                     SZrArray *result) {
    SZrLspProjectResolvedSymbol resolved;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!project_resolve_symbol_at_position(state, context, uri, position, ZR_FALSE, &resolved)) {
        return ZR_FALSE;
    }

    return append_lsp_location(state,
                               result,
                               resolved.record->uri,
                               ZrLanguageServer_Lsp_GetSymbolLookupRange(resolved.symbol));
}

TZrBool ZrLanguageServer_Lsp_ProjectTryFindReferences(SZrState *state,
                                                      SZrLspContext *context,
                                                      SZrString *uri,
                                                      SZrLspPosition position,
                                                      TZrBool includeDeclaration,
                                                      SZrArray *result) {
    SZrLspProjectResolvedSymbol resolved;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!project_resolve_symbol_at_position(state, context, uri, position, ZR_TRUE, &resolved)) {
        return ZR_FALSE;
    }

    if (!append_symbol_references_from_tracker(state,
                                               resolved.analyzer,
                                               resolved.symbol,
                                               includeDeclaration,
                                               result)) {
        return ZR_FALSE;
    }

    return append_project_imported_references(state,
                                              context,
                                              resolved.projectIndex,
                                              resolved.record,
                                              resolved.symbol,
                                              result);
}
