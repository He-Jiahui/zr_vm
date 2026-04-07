#include "lsp_project_internal.h"
#include "lsp_metadata_provider.h"
#include "../../../zr_vm_parser/src/zr_vm_parser/module_init_analysis.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *project_diagnostic_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static TZrBool project_diagnostic_location_is_empty(SZrFileRange location) {
    return location.start.line == 0 && location.start.column == 0 &&
           location.end.line == 0 && location.end.column == 0 &&
           location.start.offset == 0 && location.end.offset == 0;
}

static void project_diagnostic_ensure_location_source(SZrFileRange *location, SZrString *uri) {
    if (location == ZR_NULL) {
        return;
    }

    if (location->source == ZR_NULL) {
        location->source = uri;
    }

    if (project_diagnostic_location_is_empty(*location)) {
        location->start = ZrParser_FilePosition_Create(0, 1, 1);
        location->end = ZrParser_FilePosition_Create(0, 1, 1);
    }
}

static SZrFileRange project_diagnostic_module_entry_location(SZrString *uri) {
    SZrFilePosition start = ZrParser_FilePosition_Create(0, 1, 1);
    return ZrParser_FileRange_Create(start, start, uri);
}

static TZrBool project_diagnostic_append_ex(SZrState *state,
                                            SZrArray *result,
                                            SZrFileRange location,
                                            const TZrChar *message,
                                            const TZrChar *code,
                                            SZrDiagnostic **outDiagnostic) {
    SZrDiagnostic *diagnostic;

    if (outDiagnostic != ZR_NULL) {
        *outDiagnostic = ZR_NULL;
    }
    if (state == ZR_NULL || result == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrDiagnostic *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    diagnostic = ZrLanguageServer_Diagnostic_New(state, ZR_DIAGNOSTIC_ERROR, location, message, code);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, result, &diagnostic);
    if (outDiagnostic != ZR_NULL) {
        *outDiagnostic = diagnostic;
    }
    return ZR_TRUE;
}

static TZrBool project_diagnostic_append(SZrState *state,
                                         SZrArray *result,
                                         SZrFileRange location,
                                         const TZrChar *message,
                                         const TZrChar *code) {
    return project_diagnostic_append_ex(state, result, location, message, code, ZR_NULL);
}

static TZrBool project_diagnostic_add_related_information(SZrState *state,
                                                          SZrDiagnostic *diagnostic,
                                                          SZrFileRange location,
                                                          SZrString *fallbackUri,
                                                          const TZrChar *message) {
    if (state == ZR_NULL || diagnostic == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }

    project_diagnostic_ensure_location_source(&location, fallbackUri);
    return ZrLanguageServer_Diagnostic_AddRelatedInformation(state, diagnostic, location, message);
}

static TZrBool project_diagnostic_find_record_index_by_module_name(SZrLspProjectIndex *projectIndex,
                                                                   SZrString *moduleName,
                                                                   TZrSize *outIndex) {
    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (projectIndex == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        if (recordPtr != ZR_NULL &&
            *recordPtr != ZR_NULL &&
            (*recordPtr)->moduleName != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*recordPtr)->moduleName, moduleName)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrString *project_diagnostic_find_module_name_for_uri(SZrLspProjectIndex *projectIndex, SZrString *uri) {
    SZrLspProjectFileRecord *record =
        projectIndex != ZR_NULL && uri != ZR_NULL ? ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, uri) : ZR_NULL;
    return record != ZR_NULL ? record->moduleName : ZR_NULL;
}

static TZrBool project_diagnostic_append_module_entry_related_information(SZrState *state,
                                                                          SZrDiagnostic *diagnostic,
                                                                          SZrLspProjectIndex *projectIndex,
                                                                          SZrString *moduleName,
                                                                          const TZrChar *message) {
    TZrSize targetIndex;
    SZrLspProjectFileRecord **recordPtr;

    if (state == ZR_NULL || diagnostic == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!project_diagnostic_find_record_index_by_module_name(projectIndex, moduleName, &targetIndex)) {
        return ZR_TRUE;
    }

    recordPtr = (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, targetIndex);
    if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL || (*recordPtr)->uri == ZR_NULL) {
        return ZR_TRUE;
    }

    return project_diagnostic_add_related_information(state,
                                                      diagnostic,
                                                      project_diagnostic_module_entry_location((*recordPtr)->uri),
                                                      (*recordPtr)->uri,
                                                      message);
}

static TZrBool project_diagnostic_append_import_trace_between_modules(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrLspProjectIndex *projectIndex,
                                                                      SZrString *fromModuleName,
                                                                      SZrString *toModuleName,
                                                                      SZrDiagnostic *diagnostic,
                                                                      const TZrChar *stepPrefix,
                                                                      const TZrChar *targetMessage) {
    TZrSize moduleCount;
    TZrSize fromIndex;
    TZrSize toIndex;
    TZrBool found = ZR_FALSE;
    TZrBool *visited = ZR_NULL;
    TZrSize *predecessor = ZR_NULL;
    SZrFileRange *edgeLocations = ZR_NULL;
    TZrSize *queue = ZR_NULL;
    TZrSize queueHead = 0;
    TZrSize queueTail = 0;
    TZrSize *path = ZR_NULL;
    TZrSize pathLength = 0;
    TZrBool success = ZR_TRUE;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || fromModuleName == ZR_NULL ||
        toModuleName == ZR_NULL || diagnostic == ZR_NULL || stepPrefix == ZR_NULL || targetMessage == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleCount = projectIndex->files.length;
    if (moduleCount == 0 ||
        !project_diagnostic_find_record_index_by_module_name(projectIndex, fromModuleName, &fromIndex) ||
        !project_diagnostic_find_record_index_by_module_name(projectIndex, toModuleName, &toIndex)) {
        return ZR_TRUE;
    }

    visited = (TZrBool *)ZrCore_Memory_RawMalloc(state->global, sizeof(TZrBool) * moduleCount);
    predecessor = (TZrSize *)ZrCore_Memory_RawMalloc(state->global, sizeof(TZrSize) * moduleCount);
    edgeLocations = (SZrFileRange *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrFileRange) * moduleCount);
    queue = (TZrSize *)ZrCore_Memory_RawMalloc(state->global, sizeof(TZrSize) * moduleCount);
    path = (TZrSize *)ZrCore_Memory_RawMalloc(state->global, sizeof(TZrSize) * moduleCount);
    if (visited == ZR_NULL || predecessor == ZR_NULL || edgeLocations == ZR_NULL || queue == ZR_NULL || path == ZR_NULL) {
        success = ZR_FALSE;
        goto cleanup;
    }

    for (TZrSize index = 0; index < moduleCount; index++) {
        visited[index] = ZR_FALSE;
        predecessor[index] = moduleCount;
        ZrCore_Memory_RawSet(&edgeLocations[index], 0, sizeof(edgeLocations[index]));
    }

    visited[fromIndex] = ZR_TRUE;
    queue[queueTail++] = fromIndex;

    while (queueHead < queueTail && !found) {
        TZrSize currentIndex = queue[queueHead++];
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, currentIndex);
        SZrSemanticAnalyzer *analyzer;
        SZrArray bindings;

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL || (*recordPtr)->uri == ZR_NULL) {
            continue;
        }

        analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, (*recordPtr)->uri);
        if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
            continue;
        }

        ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
        for (TZrSize bindingIndex = 0; bindingIndex < bindings.length; bindingIndex++) {
            SZrLspImportBinding **bindingPtr =
                (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, bindingIndex);
            TZrSize nextIndex;

            if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL ||
                !project_diagnostic_find_record_index_by_module_name(projectIndex, (*bindingPtr)->moduleName, &nextIndex) ||
                visited[nextIndex]) {
                continue;
            }

            visited[nextIndex] = ZR_TRUE;
            predecessor[nextIndex] = currentIndex;
            edgeLocations[nextIndex] = (*bindingPtr)->modulePathLocation;
            if (edgeLocations[nextIndex].source == ZR_NULL) {
                edgeLocations[nextIndex].source = (*recordPtr)->uri;
            }
            if (nextIndex == toIndex) {
                found = ZR_TRUE;
                break;
            }

            queue[queueTail++] = nextIndex;
        }
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    }

    if (!found) {
        goto cleanup;
    }

    for (TZrSize node = toIndex; node != fromIndex && pathLength < moduleCount; node = predecessor[node]) {
        path[pathLength++] = node;
        if (predecessor[node] == moduleCount) {
            pathLength = 0;
            goto cleanup;
        }
    }
    path[pathLength++] = fromIndex;

    for (TZrSize index = pathLength; index > 1; index--) {
        TZrSize fromStepIndex = path[index - 1];
        TZrSize toStepIndex = path[index - 2];
        SZrLspProjectFileRecord **fromRecordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, fromStepIndex);
        SZrLspProjectFileRecord **toRecordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, toStepIndex);
        SZrFileRange stepLocation = edgeLocations[toStepIndex];
        TZrChar stepMessage[ZR_LSP_TEXT_BUFFER_LENGTH];

        if (fromRecordPtr == ZR_NULL || *fromRecordPtr == ZR_NULL || toRecordPtr == ZR_NULL || *toRecordPtr == ZR_NULL) {
            continue;
        }

        project_diagnostic_ensure_location_source(&stepLocation, (*fromRecordPtr)->uri);
        snprintf(stepMessage,
                 sizeof(stepMessage),
                 "%s: module '%s' imports '%s' here",
                 stepPrefix,
                 project_diagnostic_string_text((*fromRecordPtr)->moduleName),
                 project_diagnostic_string_text((*toRecordPtr)->moduleName));
        if (!project_diagnostic_add_related_information(state,
                                                        diagnostic,
                                                        stepLocation,
                                                        (*fromRecordPtr)->uri,
                                                        stepMessage)) {
            success = ZR_FALSE;
            goto cleanup;
        }
    }

    if (!project_diagnostic_append_module_entry_related_information(state,
                                                                    diagnostic,
                                                                    projectIndex,
                                                                    toModuleName,
                                                                    targetMessage)) {
        success = ZR_FALSE;
    }

cleanup:
    if (path != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, path, sizeof(TZrSize) * moduleCount);
    }
    if (queue != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, queue, sizeof(TZrSize) * moduleCount);
    }
    if (edgeLocations != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, edgeLocations, sizeof(SZrFileRange) * moduleCount);
    }
    if (predecessor != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, predecessor, sizeof(TZrSize) * moduleCount);
    }
    if (visited != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, visited, sizeof(TZrBool) * moduleCount);
    }
    return success;
}

static TZrBool project_diagnostic_locations_overlap(SZrFileRange left, SZrFileRange right) {
    if (left.start.line == 0 || right.start.line == 0) {
        return ZR_FALSE;
    }

    return left.start.line == right.start.line &&
           left.start.column == right.start.column &&
           left.end.line == right.end.line &&
           left.end.column == right.end.column;
}

static const SZrFunctionModuleEffect *project_diagnostic_find_effect_for_location(
    const SZrParserModuleInitSummary *summary,
    SZrFileRange location) {
    if (summary == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < summary->entryEffects.length; index++) {
        const SZrFunctionModuleEffect *effect =
            (const SZrFunctionModuleEffect *)ZrCore_Array_Get((SZrArray *)&summary->entryEffects, index);
        SZrFileRange effectLocation;

        if (effect == ZR_NULL) {
            continue;
        }

        effectLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, (TZrInt32)effect->lineInSourceStart, (TZrInt32)effect->columnInSourceStart),
            ZrParser_FilePosition_Create(0, (TZrInt32)effect->lineInSourceEnd, (TZrInt32)effect->columnInSourceEnd),
            ZR_NULL);
        if (project_diagnostic_locations_overlap(effectLocation, location)) {
            return effect;
        }
    }

    return ZR_NULL;
}

static TZrBool project_diagnostic_resolved_member_exists(const SZrLspResolvedMetadataMember *resolvedMember) {
    return resolvedMember != ZR_NULL &&
           (resolvedMember->memberKind != ZR_LSP_METADATA_MEMBER_NONE ||
            resolvedMember->moduleLinkDescriptor != ZR_NULL ||
            resolvedMember->constantDescriptor != ZR_NULL ||
            resolvedMember->functionDescriptor != ZR_NULL ||
            resolvedMember->typeDescriptor != ZR_NULL ||
            resolvedMember->fieldDescriptor != ZR_NULL ||
            resolvedMember->methodDescriptor != ZR_NULL ||
            resolvedMember->declarationSymbol != ZR_NULL ||
            resolvedMember->resolvedTypeText != ZR_NULL);
}

static TZrBool project_diagnostic_primary_expression_get_imported_member(SZrAstNode *node,
                                                                         SZrArray *bindings,
                                                                         SZrLspImportedMemberHit *outHit) {
    SZrAstNode *receiverNode;
    SZrAstNode *memberNode;
    SZrLspImportBinding *binding;

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION || bindings == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverNode = node->data.primaryExpression.property;
    if (receiverNode == ZR_NULL || receiverNode->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.primaryExpression.members == ZR_NULL || node->data.primaryExpression.members->count == 0) {
        return ZR_FALSE;
    }

    memberNode = node->data.primaryExpression.members->nodes[0];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    binding = ZrLanguageServer_LspProject_FindImportBindingByAlias(bindings, receiverNode->data.identifier.name);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    outHit->moduleName = binding->moduleName;
    outHit->memberName = memberNode->data.memberExpression.property->data.identifier.name;
    outHit->receiverLocation = receiverNode->location;
    outHit->location = memberNode->data.memberExpression.property->location;
    return ZR_TRUE;
}

static TZrBool project_diagnostic_append_unresolved_imports(SZrState *state,
                                                            SZrLspMetadataProvider *provider,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrLspProjectIndex *projectIndex,
                                                            SZrString *uri,
                                                            SZrArray *bindings,
                                                            SZrArray *result) {
    for (TZrSize index = 0; bindings != ZR_NULL && index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr = (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);
        SZrLspResolvedImportedModule resolvedModule;
        SZrFileRange location;
        TZrChar message[ZR_LSP_TEXT_BUFFER_LENGTH];
        const TZrChar *moduleText;

        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL) {
            continue;
        }

        if (ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(provider,
                                                                       analyzer,
                                                                       projectIndex,
                                                                       (*bindingPtr)->moduleName,
                                                                       &resolvedModule)) {
            continue;
        }

        moduleText = project_diagnostic_string_text((*bindingPtr)->moduleName);
        snprintf(message,
                 sizeof(message),
                 "Import target '%s' could not be resolved",
                 moduleText != ZR_NULL ? moduleText : "<module>");
        location = (*bindingPtr)->modulePathLocation;
        project_diagnostic_ensure_location_source(&location, uri);
        if (!project_diagnostic_append(state, result, location, message, "import_unresolved")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static const SZrParserModuleInitSummary *project_diagnostic_find_cycle_summary_by_ast(SZrState *state,
                                                                                      const SZrAstNode *ast) {
    const SZrParserModuleInitSummary *summary;

    if (state == ZR_NULL || state->global == ZR_NULL || ast == ZR_NULL) {
        return ZR_NULL;
    }

    summary = ZrParser_ModuleInitAnalysis_FindSummaryByAst(state->global, ast);
    if (summary == ZR_NULL || summary->errorMessage[0] == '\0' ||
        strstr(summary->errorMessage, "circular import initialization") == ZR_NULL) {
        return ZR_NULL;
    }

    return summary;
}

static const SZrParserModuleInitSummary *project_diagnostic_find_cycle_summary_by_module(SZrState *state,
                                                                                         SZrString *moduleName) {
    const SZrParserModuleInitSummary *summary;

    if (state == ZR_NULL || state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    summary = ZrParser_ModuleInitAnalysis_FindSummary(state->global, moduleName);
    if (summary == ZR_NULL || summary->errorMessage[0] == '\0' ||
        strstr(summary->errorMessage, "circular import initialization") == ZR_NULL) {
        return ZR_NULL;
    }

    return summary;
}

static TZrBool project_diagnostic_append_member_diagnostics_recursive(SZrState *state,
                                                                      SZrLspMetadataProvider *provider,
                                                                      SZrSemanticAnalyzer *analyzer,
                                                                      SZrLspProjectIndex *projectIndex,
                                                                      SZrString *uri,
                                                                      SZrArray *bindings,
                                                                      SZrAstNode *node,
                                                                      SZrArray *result);

static TZrBool project_diagnostic_append_member_diagnostics_in_array(SZrState *state,
                                                                     SZrLspMetadataProvider *provider,
                                                                     SZrSemanticAnalyzer *analyzer,
                                                                     SZrLspProjectIndex *projectIndex,
                                                                     SZrString *uri,
                                                                     SZrArray *bindings,
                                                                     SZrAstNodeArray *nodes,
                                                                     SZrArray *result) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (!project_diagnostic_append_member_diagnostics_recursive(state,
                                                                    provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    uri,
                                                                    bindings,
                                                                    nodes->nodes[index],
                                                                    result)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool project_diagnostic_append_missing_imported_member(SZrState *state,
                                                                 SZrLspMetadataProvider *provider,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrLspProjectIndex *projectIndex,
                                                                 SZrString *uri,
                                                                 const SZrLspImportedMemberHit *hit,
                                                                 SZrArray *result) {
    SZrLspResolvedImportedModule resolvedModule;
    SZrLspResolvedMetadataMember resolvedMember;
    const SZrParserModuleInitSummary *cycleSummary = ZR_NULL;
    SZrFileRange location;
    TZrChar message[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *moduleText;
    const TZrChar *memberText;
    SZrDiagnostic *diagnostic = ZR_NULL;
    SZrString *currentModuleName;

    if (hit == ZR_NULL || hit->moduleName == ZR_NULL || hit->memberName == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    hit->moduleName,
                                                                    &resolvedModule)) {
        return ZR_TRUE;
    }

    cycleSummary = project_diagnostic_find_cycle_summary_by_module(state, hit->moduleName);
    if (cycleSummary != ZR_NULL) {
        location = hit->location;
        project_diagnostic_ensure_location_source(&location, uri);
        if (!project_diagnostic_append_ex(state,
                                          result,
                                          location,
                                          cycleSummary->errorMessage,
                                          "import_cycle_initialization",
                                          &diagnostic)) {
            return ZR_FALSE;
        }

        currentModuleName = project_diagnostic_find_module_name_for_uri(projectIndex, uri);
        if (diagnostic != ZR_NULL && currentModuleName != ZR_NULL) {
            if (!project_diagnostic_append_import_trace_between_modules(state,
                                                                        provider->context,
                                                                        projectIndex,
                                                                        currentModuleName,
                                                                        hit->moduleName,
                                                                        diagnostic,
                                                                        "Import trace",
                                                                        "Import trace target: referenced module resolves here") ||
                !project_diagnostic_append_import_trace_between_modules(state,
                                                                        provider->context,
                                                                        projectIndex,
                                                                        hit->moduleName,
                                                                        currentModuleName,
                                                                        diagnostic,
                                                                        "Cycle return",
                                                                        "Cycle return target: current module resolves here")) {
                return ZR_FALSE;
            }
        }

        return ZR_TRUE;
    }

    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    hit->moduleName,
                                                                    hit->memberName,
                                                                    &resolvedMember) ||
        project_diagnostic_resolved_member_exists(&resolvedMember)) {
        return ZR_TRUE;
    }

    moduleText = project_diagnostic_string_text(hit->moduleName);
    memberText = project_diagnostic_string_text(hit->memberName);
    snprintf(message,
             sizeof(message),
             "Import member '%s.%s' could not be resolved",
             moduleText != ZR_NULL ? moduleText : "<module>",
             memberText != ZR_NULL ? memberText : "<member>");
    location = hit->location;
    project_diagnostic_ensure_location_source(&location, uri);
    if (!project_diagnostic_append_ex(state,
                                      result,
                                      location,
                                      message,
                                      "import_member_unresolved",
                                      &diagnostic)) {
        return ZR_FALSE;
    }

    currentModuleName = project_diagnostic_find_module_name_for_uri(projectIndex, uri);
    if (diagnostic != ZR_NULL && currentModuleName != ZR_NULL &&
        !project_diagnostic_append_import_trace_between_modules(state,
                                                                provider->context,
                                                                projectIndex,
                                                                currentModuleName,
                                                                hit->moduleName,
                                                                diagnostic,
                                                                "Import trace",
                                                                "Import trace target: imported module resolves here")) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool project_diagnostic_append_member_diagnostics_recursive(SZrState *state,
                                                                      SZrLspMetadataProvider *provider,
                                                                      SZrSemanticAnalyzer *analyzer,
                                                                      SZrLspProjectIndex *projectIndex,
                                                                      SZrString *uri,
                                                                      SZrArray *bindings,
                                                                      SZrAstNode *node,
                                                                      SZrArray *result) {
    SZrLspImportedMemberHit hit;

    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&hit, 0, sizeof(hit));
    if (project_diagnostic_primary_expression_get_imported_member(node, bindings, &hit) &&
        !project_diagnostic_append_missing_imported_member(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           uri,
                                                           &hit,
                                                           result)) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return project_diagnostic_append_member_diagnostics_in_array(state,
                                                                         provider,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         uri,
                                                                         bindings,
                                                                         node->data.script.statements,
                                                                         result);

        case ZR_AST_BLOCK:
            return project_diagnostic_append_member_diagnostics_in_array(state,
                                                                         provider,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         uri,
                                                                         bindings,
                                                                         node->data.block.body,
                                                                         result);

        case ZR_AST_FUNCTION_DECLARATION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.functionDeclaration.body,
                                                                          result);

        case ZR_AST_TEST_DECLARATION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.testDeclaration.body,
                                                                          result);

        case ZR_AST_STRUCT_METHOD:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.structMethod.body,
                                                                          result);

        case ZR_AST_STRUCT_META_FUNCTION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.structMetaFunction.body,
                                                                          result);

        case ZR_AST_CLASS_METHOD:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.classMethod.body,
                                                                          result);

        case ZR_AST_CLASS_META_FUNCTION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.classMetaFunction.body,
                                                                          result);

        case ZR_AST_PROPERTY_GET:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.propertyGet.body,
                                                                          result);

        case ZR_AST_PROPERTY_SET:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.propertySet.body,
                                                                          result);

        case ZR_AST_CLASS_PROPERTY:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.classProperty.modifier,
                                                                          result);

        case ZR_AST_VARIABLE_DECLARATION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.variableDeclaration.value,
                                                                          result);

        case ZR_AST_STRUCT_FIELD:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.structField.init,
                                                                          result);

        case ZR_AST_CLASS_FIELD:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.classField.init,
                                                                          result);

        case ZR_AST_ENUM_MEMBER:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.enumMember.value,
                                                                          result);

        case ZR_AST_RETURN_STATEMENT:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.returnStatement.expr,
                                                                          result);

        case ZR_AST_EXPRESSION_STATEMENT:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.expressionStatement.expr,
                                                                          result);

        case ZR_AST_USING_STATEMENT:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.usingStatement.resource,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.usingStatement.body,
                                                                          result);

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.breakContinueStatement.expr,
                                                                          result);

        case ZR_AST_THROW_STATEMENT:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.throwStatement.expr,
                                                                          result);

        case ZR_AST_OUT_STATEMENT:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.outStatement.expr,
                                                                          result);

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.tryCatchFinallyStatement.block,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_in_array(state,
                                                                         provider,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         uri,
                                                                         bindings,
                                                                         node->data.tryCatchFinallyStatement.catchClauses,
                                                                         result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.tryCatchFinallyStatement.finallyBlock,
                                                                          result);

        case ZR_AST_CATCH_CLAUSE:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.catchClause.block,
                                                                          result);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.assignmentExpression.left,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.assignmentExpression.right,
                                                                          result);

        case ZR_AST_BINARY_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.binaryExpression.left,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.binaryExpression.right,
                                                                          result);

        case ZR_AST_LOGICAL_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.logicalExpression.left,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.logicalExpression.right,
                                                                          result);

        case ZR_AST_CONDITIONAL_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.conditionalExpression.test,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.conditionalExpression.consequent,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.conditionalExpression.alternate,
                                                                          result);

        case ZR_AST_UNARY_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.unaryExpression.argument,
                                                                          result);

        case ZR_AST_TYPE_CAST_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.typeCastExpression.expression,
                                                                          result);

        case ZR_AST_LAMBDA_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.lambdaExpression.block,
                                                                          result);

        case ZR_AST_IF_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.ifExpression.condition,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.ifExpression.thenExpr,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.ifExpression.elseExpr,
                                                                          result);

        case ZR_AST_SWITCH_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.switchExpression.expr,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_in_array(state,
                                                                         provider,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         uri,
                                                                         bindings,
                                                                         node->data.switchExpression.cases,
                                                                         result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.switchExpression.defaultCase,
                                                                          result);

        case ZR_AST_SWITCH_CASE:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.switchCase.value,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.switchCase.block,
                                                                          result);

        case ZR_AST_SWITCH_DEFAULT:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.switchDefault.block,
                                                                          result);

        case ZR_AST_FUNCTION_CALL:
            return project_diagnostic_append_member_diagnostics_in_array(state,
                                                                         provider,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         uri,
                                                                         bindings,
                                                                         node->data.functionCall.args,
                                                                         result);

        case ZR_AST_PRIMARY_EXPRESSION:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.primaryExpression.property,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_in_array(state,
                                                                         provider,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         uri,
                                                                         bindings,
                                                                         node->data.primaryExpression.members,
                                                                         result);

        case ZR_AST_MEMBER_EXPRESSION:
            return !node->data.memberExpression.computed ||
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.memberExpression.property,
                                                                          result);

        case ZR_AST_ARRAY_LITERAL:
            return project_diagnostic_append_member_diagnostics_in_array(state,
                                                                         provider,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         uri,
                                                                         bindings,
                                                                         node->data.arrayLiteral.elements,
                                                                         result);

        case ZR_AST_OBJECT_LITERAL:
            return project_diagnostic_append_member_diagnostics_in_array(state,
                                                                         provider,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         uri,
                                                                         bindings,
                                                                         node->data.objectLiteral.properties,
                                                                         result);

        case ZR_AST_KEY_VALUE_PAIR:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.keyValuePair.key,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.keyValuePair.value,
                                                                          result);

        case ZR_AST_WHILE_LOOP:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.whileLoop.cond,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.whileLoop.block,
                                                                          result);

        case ZR_AST_FOR_LOOP:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.forLoop.init,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.forLoop.cond,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.forLoop.step,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.forLoop.block,
                                                                          result);

        case ZR_AST_FOREACH_LOOP:
            return project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.foreachLoop.expr,
                                                                          result) &&
                   project_diagnostic_append_member_diagnostics_recursive(state,
                                                                          provider,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          uri,
                                                                          bindings,
                                                                          node->data.foreachLoop.block,
                                                                          result);

        default:
            break;
    }

    return ZR_TRUE;
}

static TZrBool project_diagnostic_append_cycle_error(SZrState *state,
                                                     SZrLspMetadataProvider *provider,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrLspProjectIndex *projectIndex,
                                                     SZrString *uri,
                                                     SZrArray *result) {
    const SZrParserModuleInitSummary *summary;
    const SZrFunctionModuleEffect *effect;
    SZrFileRange location;
    const TZrChar *messageText;
    SZrDiagnostic *diagnostic = ZR_NULL;
    SZrString *currentModuleName;

    if (state == ZR_NULL || provider == ZR_NULL || analyzer == ZR_NULL) {
        return ZR_TRUE;
    }

    summary = project_diagnostic_find_cycle_summary_by_ast(state, analyzer->ast);
    if (summary == ZR_NULL) {
        return ZR_TRUE;
    }

    messageText = summary->errorMessage;
    location = summary->errorLocation;
    if (project_diagnostic_location_is_empty(location) && analyzer->ast != ZR_NULL) {
        location = analyzer->ast->location;
    }
    project_diagnostic_ensure_location_source(&location, uri);
    if (!project_diagnostic_append_ex(state,
                                      result,
                                      location,
                                      messageText,
                                      "import_cycle_initialization",
                                      &diagnostic)) {
        return ZR_FALSE;
    }

    effect = project_diagnostic_find_effect_for_location(summary, summary->errorLocation);
    currentModuleName = project_diagnostic_find_module_name_for_uri(projectIndex, uri);
    if (diagnostic != ZR_NULL && effect != ZR_NULL && effect->moduleName != ZR_NULL && currentModuleName != ZR_NULL) {
        if (!project_diagnostic_append_import_trace_between_modules(state,
                                                                    provider->context,
                                                                    projectIndex,
                                                                    currentModuleName,
                                                                    effect->moduleName,
                                                                    diagnostic,
                                                                    "Import trace",
                                                                    "Import trace target: referenced module resolves here") ||
            !project_diagnostic_append_import_trace_between_modules(state,
                                                                    provider->context,
                                                                    projectIndex,
                                                                    effect->moduleName,
                                                                    currentModuleName,
                                                                    diagnostic,
                                                                    "Cycle return",
                                                                    "Cycle return target: current module resolves here")) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspProject_CollectImportDiagnostics(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrLspProjectIndex *projectIndex,
                                                             SZrString *uri,
                                                             SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrArray bindings;
    SZrLspMetadataProvider provider;
    TZrBool success = ZR_TRUE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);

    success = project_diagnostic_append_unresolved_imports(state,
                                                           &provider,
                                                           analyzer,
                                                           projectIndex,
                                                           uri,
                                                           &bindings,
                                                           result) &&
              project_diagnostic_append_member_diagnostics_recursive(state,
                                                                     &provider,
                                                                     analyzer,
                                                                     projectIndex,
                                                                     uri,
                                                                     &bindings,
                                                                     analyzer->ast,
                                                                     result) &&
              project_diagnostic_append_cycle_error(state, &provider, analyzer, projectIndex, uri, result);

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return success;
}
