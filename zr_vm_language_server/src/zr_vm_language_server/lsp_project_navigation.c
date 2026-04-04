#include "lsp_module_metadata.h"
#include "lsp_project_internal.h"

#include "zr_vm_core/memory.h"

static TZrBool append_lsp_location(SZrState *state, SZrArray *result, SZrString *uri, SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
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

static SZrFileRange project_navigation_metadata_file_entry_range(SZrString *uri) {
    SZrFilePosition start = ZrParser_FilePosition_Create(0, 1, 1);
    return ZrParser_FileRange_Create(start, start, uri);
}

static SZrFileRange project_navigation_intermediate_symbol_range(SZrString *uri,
                                                                 const SZrLspIntermediateExportSymbol *symbol) {
    SZrFilePosition start;
    SZrFilePosition end;

    if (uri == ZR_NULL || symbol == ZR_NULL) {
        return project_navigation_metadata_file_entry_range(uri);
    }

    start = ZrParser_FilePosition_Create(0,
                                         symbol->declarationLine > 0 ? symbol->declarationLine : 1,
                                         symbol->declarationStartColumn > 0 ? symbol->declarationStartColumn : 1);
    end = ZrParser_FilePosition_Create(0,
                                       symbol->declarationLine > 0 ? symbol->declarationLine : 1,
                                       symbol->declarationEndColumn > symbol->declarationStartColumn
                                           ? symbol->declarationEndColumn
                                           : symbol->declarationStartColumn);
    return ZrParser_FileRange_Create(start, end, uri);
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

typedef struct SZrLspProjectResolvedExternalImportedMember {
    SZrLspProjectIndex *projectIndex;
    SZrString *moduleName;
    SZrString *memberName;
    EZrLspImportedModuleSourceKind sourceKind;
    SZrString *declarationUri;
    SZrFileRange declarationRange;
    TZrBool hasDeclaration;
} SZrLspProjectResolvedExternalImportedMember;

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

    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
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

static TZrBool append_imported_member_locations_from_analyzer(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrString *moduleName,
                                                              SZrString *memberName,
                                                              SZrArray *result) {
    SZrArray bindings;
    TZrBool appended;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL || moduleName == ZR_NULL ||
        memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    appended = ZrLanguageServer_LspProject_AppendMatchingImportedMemberLocations(state,
                                                                                 analyzer->ast,
                                                                                 &bindings,
                                                                                 moduleName,
                                                                                 memberName,
                                                                                 result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

static TZrBool append_project_imported_references(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrLspProjectIndex *projectIndex,
                                                  SZrString *fallbackUri,
                                                  SZrString *moduleName,
                                                  SZrString *memberName,
                                                  SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || moduleName == ZR_NULL || memberName == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (projectIndex == ZR_NULL) {
        SZrSemanticAnalyzer *fallbackAnalyzer = fallbackUri != ZR_NULL
                                                    ? ZrLanguageServer_Lsp_FindAnalyzer(state, context, fallbackUri)
                                                    : ZR_NULL;
        return fallbackAnalyzer != ZR_NULL
                   ? append_imported_member_locations_from_analyzer(state,
                                                                   fallbackAnalyzer,
                                                                   moduleName,
                                                                   memberName,
                                                                   result)
                   : ZR_FALSE;
    }

    for (TZrSize fileIndex = 0; fileIndex < projectIndex->files.length; fileIndex++) {
        SZrLspProjectFileRecord **candidatePtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, fileIndex);
        SZrSemanticAnalyzer *candidateAnalyzer;

        if (candidatePtr == ZR_NULL || *candidatePtr == ZR_NULL) {
            continue;
        }

        candidateAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, (*candidatePtr)->uri);
        if (candidateAnalyzer == ZR_NULL || candidateAnalyzer->ast == ZR_NULL) {
            continue;
        }

        if (!append_imported_member_locations_from_analyzer(state,
                                                            candidateAnalyzer,
                                                            moduleName,
                                                            memberName,
                                                            result)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool append_locations_as_document_highlights(SZrState *state,
                                                       SZrArray *locations,
                                                       SZrString *uri,
                                                       TZrInt32 kind,
                                                       SZrArray *result) {
    if (state == ZR_NULL || locations == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        SZrLspDocumentHighlight *highlight;

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL || (*locationPtr)->uri == ZR_NULL ||
            !ZrLanguageServer_Lsp_StringsEqual((*locationPtr)->uri, uri)) {
            continue;
        }

        highlight =
            (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
        if (highlight == ZR_NULL) {
            return ZR_FALSE;
        }

        highlight->range = (*locationPtr)->range;
        highlight->kind = kind;
        ZrCore_Array_Push(state, result, &highlight);
    }

    return result->length > 0;
}

static TZrBool project_try_find_imported_member_resolution(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *uri,
                                                           SZrLspPosition position,
                                                           SZrLspProjectIndex **outProjectIndex,
                                                           SZrSemanticAnalyzer **outAnalyzer,
                                                           SZrLspImportedMemberHit *outHit,
                                                           SZrLspResolvedImportedModule *outResolved) {
    SZrLspProjectIndex *projectIndex;
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrArray bindings;
    SZrLspImportedMemberHit hit;
    SZrLspResolvedImportedModule resolved;
    TZrBool found = ZR_FALSE;

    if (outProjectIndex != ZR_NULL) {
        *outProjectIndex = ZR_NULL;
    }
    if (outAnalyzer != ZR_NULL) {
        *outAnalyzer = ZR_NULL;
    }
    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    if (ZrLanguageServer_LspProject_FindImportedMemberHit(analyzer->ast, &bindings, fileRange, &hit)) {
        memset(&resolved, 0, sizeof(resolved));
        if (ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                     analyzer,
                                                                     projectIndex,
                                                                     hit.moduleName,
                                                                     &resolved)) {
            *outHit = hit;
            if (outProjectIndex != ZR_NULL) {
                *outProjectIndex = projectIndex;
            }
            if (outAnalyzer != ZR_NULL) {
                *outAnalyzer = analyzer;
            }
            if (outResolved != ZR_NULL) {
                *outResolved = resolved;
            }
            found = ZR_TRUE;
        }
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return found;
}

static TZrBool project_try_resolve_external_imported_member(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrString *uri,
                                                            SZrLspPosition position,
                                                            SZrLspProjectResolvedExternalImportedMember *outResolved) {
    SZrLspProjectIndex *projectIndex = ZR_NULL;
    SZrLspImportedMemberHit hit;
    SZrLspResolvedImportedModule resolved;

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outResolved == ZR_NULL ||
        !project_try_find_imported_member_resolution(state,
                                                     context,
                                                     uri,
                                                     position,
                                                     &projectIndex,
                                                     ZR_NULL,
                                                     &hit,
                                                     &resolved) ||
        resolved.sourceRecord != ZR_NULL) {
        return ZR_FALSE;
    }

    outResolved->projectIndex = projectIndex;
    outResolved->moduleName = hit.moduleName;
    outResolved->memberName = hit.memberName;
    outResolved->sourceKind = resolved.sourceKind;

    if (resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
        projectIndex != ZR_NULL &&
        ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(state,
                                                                  projectIndex,
                                                                  hit.moduleName,
                                                                  &outResolved->declarationUri) &&
        outResolved->declarationUri != ZR_NULL) {
        SZrLspIntermediateModuleMetadata metadata;
        const SZrLspIntermediateExportSymbol *symbol = ZR_NULL;

        outResolved->declarationRange =
            project_navigation_metadata_file_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
        ZrCore_Array_Construct(&metadata.exportedSymbols);
        if (ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleMetadata(state,
                                                                              projectIndex,
                                                                              hit.moduleName,
                                                                              &metadata)) {
            symbol = ZrLanguageServer_LspModuleMetadata_FindIntermediateExportSymbol(&metadata, hit.memberName);
            if (symbol != ZR_NULL) {
                outResolved->declarationRange =
                    project_navigation_intermediate_symbol_range(outResolved->declarationUri, symbol);
            }
        }
        ZrLanguageServer_LspModuleMetadata_FreeIntermediateModuleMetadata(state, &metadata);
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

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
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

static TZrBool project_try_append_external_imported_member_definition(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result) {
    SZrLspProjectResolvedExternalImportedMember resolved;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL ||
        !project_try_resolve_external_imported_member(state, context, uri, position, &resolved) ||
        !resolved.hasDeclaration || resolved.declarationUri == ZR_NULL) {
        return ZR_FALSE;
    }

    return append_lsp_location(state,
                               result,
                               resolved.declarationUri,
                               resolved.declarationRange);
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

    if (project_try_append_external_imported_member_definition(state, context, uri, position, result)) {
        return ZR_TRUE;
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

    {
        SZrLspProjectResolvedExternalImportedMember externalResolved;

        if (project_try_resolve_external_imported_member(state, context, uri, position, &externalResolved)) {
            if (includeDeclaration && externalResolved.hasDeclaration &&
                !append_lsp_location(state,
                                     result,
                                     externalResolved.declarationUri,
                                     externalResolved.declarationRange)) {
                return ZR_FALSE;
            }

            return append_project_imported_references(state,
                                                      context,
                                                      externalResolved.projectIndex,
                                                      uri,
                                                      externalResolved.moduleName,
                                                      externalResolved.memberName,
                                                      result);
        }
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
                                              uri,
                                              resolved.record->moduleName,
                                              resolved.symbol->name,
                                              result);
}

TZrBool ZrLanguageServer_Lsp_ProjectTryGetDocumentHighlights(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrString *uri,
                                                             SZrLspPosition position,
                                                             SZrArray *result) {
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrLspImportedMemberHit hit;
    SZrArray locations;
    TZrBool appended;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL ||
        !project_try_find_imported_member_resolution(state,
                                                     context,
                                                     uri,
                                                     position,
                                                     ZR_NULL,
                                                     &analyzer,
                                                     &hit,
                                                     ZR_NULL)) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &locations, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    appended = append_imported_member_locations_from_analyzer(state,
                                                              analyzer,
                                                              hit.moduleName,
                                                              hit.memberName,
                                                              &locations);
    if (!appended) {
        ZrCore_Array_Free(state, &locations);
        return ZR_FALSE;
    }

    appended = append_locations_as_document_highlights(state, &locations, uri, 2, result);
    ZrCore_Array_Free(state, &locations);
    return appended;
}
