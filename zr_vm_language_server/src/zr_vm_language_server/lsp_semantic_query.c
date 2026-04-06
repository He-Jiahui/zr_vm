#include "lsp_semantic_query.h"
#include "lsp_semantic_import_chain.h"
#include "lsp_project_internal.h"

#include "zr_vm_library/file.h"
#include "zr_vm_parser/type_inference.h"

#include <ctype.h>
#include <string.h>

static void semantic_query_copy_type_text(SZrState *state,
                                          SZrInferredType *typeInfo,
                                          SZrLspResolvedTypeInfo *outInfo) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *typeText;

    if (state == ZR_NULL || typeInfo == ZR_NULL || outInfo == ZR_NULL) {
        return;
    }

    typeText = ZrParser_TypeNameString_Get(state, typeInfo, typeBuffer, sizeof(typeBuffer));
    if (typeText != ZR_NULL && typeText[0] != '\0') {
        outInfo->resolvedTypeText = ZrCore_String_Create(state, (TZrNativeString)typeText, strlen(typeText));
    }
}

static void semantic_query_copy_resolved_member_type(SZrState *state,
                                                     SZrLspResolvedMetadataMember *member,
                                                     SZrLspResolvedTypeInfo *outInfo) {
    const TZrChar *typeText;

    if (state == ZR_NULL || member == ZR_NULL || outInfo == ZR_NULL) {
        return;
    }

    switch (member->memberKind) {
        case ZR_LSP_METADATA_MEMBER_MODULE:
            outInfo->valueKind = ZR_LSP_RESOLVED_VALUE_KIND_MODULE;
            break;
        case ZR_LSP_METADATA_MEMBER_FUNCTION:
        case ZR_LSP_METADATA_MEMBER_METHOD:
            outInfo->valueKind = ZR_LSP_RESOLVED_VALUE_KIND_CALLABLE;
            break;
        case ZR_LSP_METADATA_MEMBER_TYPE:
            outInfo->valueKind = ZR_LSP_RESOLVED_VALUE_KIND_TYPE;
            break;
        case ZR_LSP_METADATA_MEMBER_CONSTANT:
        case ZR_LSP_METADATA_MEMBER_FIELD:
            outInfo->valueKind = ZR_LSP_RESOLVED_VALUE_KIND_SYMBOL;
            break;
        default:
            break;
    }

    typeText = member->resolvedTypeText != ZR_NULL
                   ? (member->resolvedTypeText->shortStringLength < ZR_VM_LONG_STRING_FLAG
                          ? ZrCore_String_GetNativeStringShort(member->resolvedTypeText)
                          : ZrCore_String_GetNativeString(member->resolvedTypeText))
                   : ZR_NULL;
    if (typeText != ZR_NULL && typeText[0] != '\0') {
        outInfo->resolvedTypeText = ZrCore_String_Create(state, (TZrNativeString)typeText, strlen(typeText));
    }
}

static TZrBool semantic_query_append_location(SZrState *state,
                                              SZrArray *result,
                                              SZrString *uri,
                                              SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
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
    location->range = ZrLanguageServer_LspRange_FromFileRange(range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static TZrBool semantic_query_append_document_highlight(SZrState *state,
                                                        SZrArray *result,
                                                        SZrFileRange range,
                                                        TZrInt32 kind) {
    SZrLspDocumentHighlight *highlight;

    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight == ZR_NULL) {
        return ZR_FALSE;
    }

    highlight->range = ZrLanguageServer_LspRange_FromFileRange(range);
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

static SZrString *semantic_query_append_markdown_section(SZrState *state, SZrString *base, SZrString *appendix) {
    TZrNativeString baseText;
    TZrNativeString appendixText;
    TZrSize baseLength;
    TZrSize appendixLength;
    TZrChar buffer[ZR_LSP_MARKDOWN_BUFFER_SIZE];
    TZrSize used = 0;

    if (state == ZR_NULL || base == ZR_NULL || appendix == ZR_NULL) {
        return base;
    }

    if (base->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        baseText = ZrCore_String_GetNativeStringShort(base);
        baseLength = base->shortStringLength;
    } else {
        baseText = ZrCore_String_GetNativeString(base);
        baseLength = base->longStringLength;
    }

    if (appendix->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        appendixText = ZrCore_String_GetNativeStringShort(appendix);
        appendixLength = appendix->shortStringLength;
    } else {
        appendixText = ZrCore_String_GetNativeString(appendix);
        appendixLength = appendix->longStringLength;
    }

    if (baseText == ZR_NULL || appendixText == ZR_NULL || appendixLength == 0 ||
        baseLength + appendixLength + 3 >= sizeof(buffer) || strstr(baseText, appendixText) != ZR_NULL) {
        return base;
    }

    memcpy(buffer + used, baseText, baseLength);
    used += baseLength;
    memcpy(buffer + used, "\n\n", 2);
    used += 2;
    memcpy(buffer + used, appendixText, appendixLength);
    used += appendixLength;
    buffer[used] = '\0';
    return ZrCore_String_Create(state, buffer, used);
}

static TZrBool semantic_query_create_hover_from_content(SZrState *state,
                                                        SZrString *content,
                                                        SZrFileRange range,
                                                        SZrLspHover **result) {
    SZrLspHover *hover;

    if (state == ZR_NULL || content == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (hover == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_LspRange_FromFileRange(range);
    *result = hover;
    return ZR_TRUE;
}

static TZrBool semantic_query_append_local_symbol_references(SZrState *state,
                                                             SZrLspSemanticQuery *query,
                                                             TZrBool includeDeclaration,
                                                             SZrArray *result) {
    SZrArray references;

    if (state == ZR_NULL || query == ZR_NULL || query->analyzer == ZR_NULL || query->symbol == ZR_NULL ||
        query->analyzer->referenceTracker == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (includeDeclaration &&
        !semantic_query_append_location(state,
                                        result,
                                        query->symbol->location.source,
                                        ZrLanguageServer_Lsp_GetSymbolLookupRange(query->symbol))) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state,
                                                          query->analyzer->referenceTracker,
                                                          query->symbol,
                                                          &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < references.length; index++) {
        SZrReference **referencePtr = (SZrReference **)ZrCore_Array_Get(&references, index);
        if (referencePtr == ZR_NULL || *referencePtr == ZR_NULL ||
            ((*referencePtr)->type == ZR_REFERENCE_DEFINITION && !includeDeclaration)) {
            continue;
        }

        if (!semantic_query_append_location(state,
                                            result,
                                            (*referencePtr)->location.source,
                                            (*referencePtr)->location)) {
            ZrCore_Array_Free(state, &references);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

static TZrBool semantic_query_append_local_symbol_highlights(SZrState *state,
                                                             SZrLspSemanticQuery *query,
                                                             SZrArray *result) {
    SZrArray references;

    if (state == ZR_NULL || query == ZR_NULL || query->analyzer == ZR_NULL || query->symbol == ZR_NULL ||
        query->uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_Lsp_StringsEqual(query->symbol->location.source, query->uri) &&
        !semantic_query_append_document_highlight(state,
                                                  result,
                                                  ZrLanguageServer_Lsp_GetSymbolLookupRange(query->symbol),
                                                  3)) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state,
                                                          query->analyzer->referenceTracker,
                                                          query->symbol,
                                                          &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < references.length; index++) {
        SZrReference **referencePtr = (SZrReference **)ZrCore_Array_Get(&references, index);
        if (referencePtr == ZR_NULL || *referencePtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_StringsEqual((*referencePtr)->location.source, query->uri)) {
            continue;
        }

        if (!semantic_query_append_document_highlight(state,
                                                      result,
                                                      (*referencePtr)->location,
                                                      (*referencePtr)->type == ZR_REFERENCE_WRITE ? 3 : 2)) {
            ZrCore_Array_Free(state, &references);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

static TZrBool semantic_query_uri_to_native_path(SZrString *uri, TZrChar *buffer, TZrSize bufferSize) {
    return ZrLanguageServer_Lsp_FileUriToNativePath(uri, buffer, bufferSize);
}

static TZrBool semantic_query_try_get_analyzer_for_uri(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       SZrSemanticAnalyzer **outAnalyzer) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    TZrNativeString sourceBuffer = ZR_NULL;
    TZrSize sourceLength = 0;
    TZrBool loadedFromDisk = ZR_FALSE;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outAnalyzer != ZR_NULL) {
        *outAnalyzer = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        sourceBuffer = fileVersion->content;
        sourceLength = fileVersion->contentLength;
    } else if (state->global != ZR_NULL && semantic_query_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        sourceBuffer = ZrLibrary_File_ReadAll(state->global, nativePath);
        sourceLength = sourceBuffer != ZR_NULL ? strlen(sourceBuffer) : 0;
        loadedFromDisk = sourceBuffer != ZR_NULL;
    }

    if (sourceBuffer == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocumentCore(state,
                                                 context,
                                                 uri,
                                                 sourceBuffer,
                                                 sourceLength,
                                                 fileVersion != ZR_NULL ? fileVersion->version : 0,
                                                 ZR_FALSE)) {
        if (loadedFromDisk) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          sourceBuffer,
                                          sourceLength + 1,
                                          ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        }
        return ZR_FALSE;
    }

    if (loadedFromDisk) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceBuffer,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
    }

    return ZR_TRUE;
}

static TZrBool semantic_query_append_locations_as_highlights(SZrState *state,
                                                             SZrArray *locations,
                                                             SZrString *uri,
                                                             TZrInt32 kind,
                                                             SZrArray *result) {
    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_StringsEqual((*locationPtr)->uri, uri)) {
            continue;
        }

        if (!semantic_query_append_document_highlight(state,
                                                      result,
                                                      ZrLanguageServer_LspRange_ToFileRange((*locationPtr)->range, uri),
                                                      kind)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool semantic_query_append_imported_member_locations_for_uri(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrLspProjectIndex *projectIndex,
                                                                       SZrString *uri,
                                                                       SZrString *moduleName,
                                                                       SZrString *memberName,
                                                                       SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || moduleName == ZR_NULL ||
        memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLanguageServer_LspSemanticImportChain_AppendMatchingLocationsForUri(state,
                                                                                 context,
                                                                                 projectIndex,
                                                                                 uri,
                                                                                 moduleName,
                                                                                 memberName,
                                                                                 result);
}

static TZrBool semantic_query_append_project_imported_member_references(SZrState *state,
                                                                        SZrLspContext *context,
                                                                        SZrLspProjectIndex *projectIndex,
                                                                        SZrString *moduleName,
                                                                        SZrString *memberName,
                                                                        SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL ||
        memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL || (*recordPtr)->uri == ZR_NULL) {
            continue;
        }

        if (semantic_query_append_imported_member_locations_for_uri(state,
                                                                    context,
                                                                    projectIndex,
                                                                    (*recordPtr)->uri,
                                                                    moduleName,
                                                                    memberName,
                                                                    result)) {
            appended = ZR_TRUE;
        }
    }

    return appended;
}

static TZrBool semantic_query_append_imported_member_highlights(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrLspSemanticQuery *query,
                                                                SZrArray *result) {
    SZrArray locations;
    TZrBool appended;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL ||
        query->moduleName == ZR_NULL || query->memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &locations, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    appended = semantic_query_append_imported_member_locations_for_uri(state,
                                                                       context,
                                                                       query->projectIndex,
                                                                       query->uri,
                                                                       query->moduleName,
                                                                       query->memberName,
                                                                       &locations);
    if (!appended) {
        ZrCore_Array_Free(state, &locations);
        return ZR_FALSE;
    }

    appended = semantic_query_append_locations_as_highlights(state, &locations, query->uri, 2, result);
    ZrCore_Array_Free(state, &locations);
    return appended;
}

static TZrSize semantic_query_file_offset_from_range_start(const TZrChar *content,
                                                           TZrSize contentLength,
                                                           SZrFileRange range) {
    if (content == ZR_NULL || contentLength == 0) {
        return 0;
    }

    if (range.start.offset > 0 && range.start.offset <= contentLength) {
        return range.start.offset;
    }

    return ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(content,
                                                              contentLength,
                                                              range.start.line > 0 ? range.start.line - 1 : 0,
                                                              range.start.column > 0 ? range.start.column - 1 : 0);
}

static TZrBool semantic_query_file_ranges_equal(SZrFileRange left, SZrFileRange right) {
    return ZrLanguageServer_Lsp_StringsEqual(left.source, right.source) &&
           left.start.line == right.start.line &&
           left.start.column == right.start.column &&
           left.end.line == right.end.line &&
           left.end.column == right.end.column;
}

static TZrBool semantic_query_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source)) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset && position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line && range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line && position.end.column <= range.end.column));
}

static TZrBool semantic_query_find_import_binding_hit(SZrArray *bindings,
                                                      SZrFileRange queryRange,
                                                      SZrLspImportBinding **outBinding,
                                                      SZrFileRange *outLocation) {
    if (outBinding != ZR_NULL) {
        *outBinding = ZR_NULL;
    }
    if (outLocation != ZR_NULL) {
        memset(outLocation, 0, sizeof(*outLocation));
    }
    if (bindings == ZR_NULL || outBinding == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);

        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL) {
            continue;
        }

        if (semantic_query_range_contains_position((*bindingPtr)->modulePathLocation, queryRange)) {
            *outBinding = *bindingPtr;
            *outLocation = (*bindingPtr)->modulePathLocation;
            return ZR_TRUE;
        }

        if (semantic_query_range_contains_position((*bindingPtr)->aliasLocation, queryRange)) {
            *outBinding = *bindingPtr;
            *outLocation = (*bindingPtr)->aliasLocation;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool semantic_query_matches_external_type_member(SZrLspSemanticQuery *query,
                                                           SZrLspResolvedMetadataMember *candidate) {
    if (query == ZR_NULL || candidate == ZR_NULL || query->memberName == ZR_NULL || candidate->memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (query->resolvedMember.hasDeclaration && candidate->hasDeclaration &&
        query->resolvedMember.declarationUri != ZR_NULL && candidate->declarationUri != ZR_NULL &&
        ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.declarationUri, candidate->declarationUri) &&
        semantic_query_file_ranges_equal(query->resolvedMember.declarationRange, candidate->declarationRange)) {
        return ZR_TRUE;
    }

    return query->resolvedMember.memberKind == candidate->memberKind &&
           ZrLanguageServer_Lsp_StringsEqual(query->memberName, candidate->memberName) &&
           ZrLanguageServer_Lsp_StringsEqual(query->moduleName, candidate->module.moduleName) &&
           ((query->resolvedMember.ownerTypeDescriptor != ZR_NULL &&
             query->resolvedMember.ownerTypeDescriptor == candidate->ownerTypeDescriptor) ||
            ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.ownerTypeName, candidate->ownerTypeName));
}

static TZrBool semantic_query_append_external_type_member_locations_recursive(SZrState *state,
                                                                              SZrLspProjectIndex *projectIndex,
                                                                              SZrSemanticAnalyzer *analyzer,
                                                                              SZrAstNode *astRoot,
                                                                              SZrAstNode *node,
                                                                              const TZrChar *content,
                                                                              TZrSize contentLength,
                                                                              SZrLspSemanticQuery *query,
                                                                              SZrString *uri,
                                                                              SZrArray *result);

static TZrBool semantic_query_append_external_type_member_locations_in_node_array(SZrState *state,
                                                                                  SZrLspProjectIndex *projectIndex,
                                                                                  SZrSemanticAnalyzer *analyzer,
                                                                                  SZrAstNode *astRoot,
                                                                                  SZrAstNodeArray *nodes,
                                                                                  const TZrChar *content,
                                                                                  TZrSize contentLength,
                                                                                  SZrLspSemanticQuery *query,
                                                                                  SZrString *uri,
                                                                                  SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (semantic_query_append_external_type_member_locations_recursive(state,
                                                                           projectIndex,
                                                                           analyzer,
                                                                           astRoot,
                                                                           nodes->nodes[index],
                                                                           content,
                                                                           contentLength,
                                                                           query,
                                                                           uri,
                                                                           result)) {
            appended = ZR_TRUE;
        }
    }

    return appended;
}

static TZrBool semantic_query_try_append_primary_external_type_member_locations(SZrState *state,
                                                                                SZrLspProjectIndex *projectIndex,
                                                                                SZrSemanticAnalyzer *analyzer,
                                                                                SZrAstNode *astRoot,
                                                                                SZrAstNode *node,
                                                                                const TZrChar *content,
                                                                                TZrSize contentLength,
                                                                                SZrLspSemanticQuery *query,
                                                                                SZrString *uri,
                                                                                SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || analyzer == ZR_NULL || astRoot == ZR_NULL || node == ZR_NULL || content == ZR_NULL ||
        query == ZR_NULL || uri == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION ||
        node->data.primaryExpression.members == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
        SZrAstNode *memberNode = node->data.primaryExpression.members->nodes[index];
        SZrLspResolvedMetadataMember resolvedMember;
        TZrSize cursorOffset;

        if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
            memberNode->data.memberExpression.computed || memberNode->data.memberExpression.property == ZR_NULL ||
            memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
            continue;
        }

        memset(&resolvedMember, 0, sizeof(resolvedMember));
        cursorOffset = semantic_query_file_offset_from_range_start(content,
                                                                   contentLength,
                                                                   memberNode->data.memberExpression.property->location);
        if (cursorOffset >= contentLength ||
            !ZrLanguageServer_Lsp_TryResolveReceiverNativeMember(state,
                                                                 projectIndex,
                                                                 analyzer,
                                                                 astRoot,
                                                                 content,
                                                                 contentLength,
                                                                 cursorOffset,
                                                                 &resolvedMember) ||
            !semantic_query_matches_external_type_member(query, &resolvedMember)) {
            continue;
        }

        if (semantic_query_append_location(state,
                                           result,
                                           uri,
                                           memberNode->data.memberExpression.property->location)) {
            appended = ZR_TRUE;
        }
    }

    return appended;
}

static TZrBool semantic_query_append_external_type_member_locations_recursive(SZrState *state,
                                                                              SZrLspProjectIndex *projectIndex,
                                                                              SZrSemanticAnalyzer *analyzer,
                                                                              SZrAstNode *astRoot,
                                                                              SZrAstNode *node,
                                                                              const TZrChar *content,
                                                                              TZrSize contentLength,
                                                                              SZrLspSemanticQuery *query,
                                                                              SZrString *uri,
                                                                              SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (node == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    if (semantic_query_try_append_primary_external_type_member_locations(state,
                                                                         projectIndex,
                                                                         analyzer,
                                                                         astRoot,
                                                                         node,
                                                                         content,
                                                                         contentLength,
                                                                         query,
                                                                         uri,
                                                                         result)) {
        appended = ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.script.statements,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_BLOCK:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.block.body,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.compileTimeDeclaration.declaration,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_EXTERN_BLOCK:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.externBlock.declarations,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_STRUCT_DECLARATION:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.structDeclaration.members,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_CLASS_DECLARATION:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.classDeclaration.members,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_INTERFACE_DECLARATION:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.interfaceDeclaration.members,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_ENUM_DECLARATION:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.enumDeclaration.members,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.functionDeclaration.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_TEST_DECLARATION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.testDeclaration.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_STRUCT_METHOD:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.structMethod.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_STRUCT_META_FUNCTION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.structMetaFunction.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_CLASS_METHOD:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.classMethod.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_CLASS_META_FUNCTION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.classMetaFunction.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_PROPERTY_GET:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.propertyGet.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_PROPERTY_SET:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.propertySet.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_CLASS_PROPERTY:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.classProperty.modifier,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.variableDeclaration.value,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_STRUCT_FIELD:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.structField.init,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_CLASS_FIELD:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.classField.init,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_ENUM_MEMBER:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.enumMember.value,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_RETURN_STATEMENT:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.returnStatement.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_EXPRESSION_STATEMENT:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.expressionStatement.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_USING_STATEMENT:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.usingStatement.resource,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.usingStatement.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.breakContinueStatement.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_THROW_STATEMENT:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.throwStatement.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_OUT_STATEMENT:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.outStatement.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.tryCatchFinallyStatement.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.tryCatchFinallyStatement.catchClauses,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.tryCatchFinallyStatement.finallyBlock,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_CATCH_CLAUSE:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.catchClause.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.assignmentExpression.left,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.assignmentExpression.right,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_BINARY_EXPRESSION:
        case ZR_AST_LOGICAL_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->type == ZR_AST_BINARY_EXPRESSION
                                                                                      ? node->data.binaryExpression.left
                                                                                      : node->data.logicalExpression.left,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->type == ZR_AST_BINARY_EXPRESSION
                                                                                      ? node->data.binaryExpression.right
                                                                                      : node->data.logicalExpression.right,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_CONDITIONAL_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.conditionalExpression.test,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.conditionalExpression.consequent,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.conditionalExpression.alternate,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_UNARY_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.unaryExpression.argument,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_TYPE_CAST_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.typeCastExpression.expression,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_LAMBDA_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.lambdaExpression.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_IF_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.ifExpression.condition,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.ifExpression.thenExpr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.ifExpression.elseExpr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_SWITCH_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.switchExpression.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.switchExpression.cases,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.switchExpression.defaultCase,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_SWITCH_CASE:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.switchCase.value,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.switchCase.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_SWITCH_DEFAULT:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.switchDefault.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_FUNCTION_CALL:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.functionCall.args,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.constructExpression.target,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.constructExpression.args,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_PRIMARY_EXPRESSION:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.primaryExpression.property,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.primaryExpression.members,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_MEMBER_EXPRESSION:
            return node->data.memberExpression.computed
                       ? semantic_query_append_external_type_member_locations_recursive(state,
                                                                                         projectIndex,
                                                                                         analyzer,
                                                                                         astRoot,
                                                                                         node->data.memberExpression.property,
                                                                                         content,
                                                                                         contentLength,
                                                                                         query,
                                                                                         uri,
                                                                                         result) ||
                             appended
                       : appended;

        case ZR_AST_ARRAY_LITERAL:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.arrayLiteral.elements,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_OBJECT_LITERAL:
            return semantic_query_append_external_type_member_locations_in_node_array(state,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.objectLiteral.properties,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) || appended;

        case ZR_AST_KEY_VALUE_PAIR:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.keyValuePair.key,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.keyValuePair.value,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_WHILE_LOOP:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.whileLoop.cond,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.whileLoop.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_FOR_LOOP:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.forLoop.init,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.forLoop.cond,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.forLoop.step,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.forLoop.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_FOREACH_LOOP:
            return semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.foreachLoop.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.foreachLoop.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        default:
            return appended;
    }
}

static TZrBool semantic_query_append_external_type_member_locations_for_uri(SZrState *state,
                                                                            SZrLspContext *context,
                                                                            SZrLspProjectIndex *projectIndex,
                                                                            SZrString *uri,
                                                                            SZrLspSemanticQuery *query,
                                                                            SZrArray *result) {
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrFileVersion *fileVersion;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_query_try_get_analyzer_for_uri(state, context, uri, &analyzer) ||
        analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_FALSE;
    }

    return semantic_query_append_external_type_member_locations_recursive(state,
                                                                          projectIndex,
                                                                          analyzer,
                                                                          analyzer->ast,
                                                                          analyzer->ast,
                                                                          fileVersion->content,
                                                                          fileVersion->contentLength,
                                                                          query,
                                                                          uri,
                                                                          result);
}

static TZrBool semantic_query_append_project_external_type_member_references(SZrState *state,
                                                                             SZrLspContext *context,
                                                                             SZrLspSemanticQuery *query,
                                                                             SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (query->projectIndex == ZR_NULL) {
        return semantic_query_append_external_type_member_locations_for_uri(state,
                                                                            context,
                                                                            ZR_NULL,
                                                                            query->uri,
                                                                            query,
                                                                            result);
    }

    for (TZrSize index = 0; index < query->projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&query->projectIndex->files, index);

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL || (*recordPtr)->uri == ZR_NULL) {
            continue;
        }

        if (semantic_query_append_external_type_member_locations_for_uri(state,
                                                                         context,
                                                                         query->projectIndex,
                                                                         (*recordPtr)->uri,
                                                                         query,
                                                                         result)) {
            appended = ZR_TRUE;
        }
    }

    return appended;
}

static TZrBool semantic_query_append_external_type_member_highlights(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrLspSemanticQuery *query,
                                                                     SZrArray *result) {
    SZrArray locations;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (query->resolvedMember.hasDeclaration &&
        query->resolvedMember.declarationUri != ZR_NULL &&
        ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.declarationUri, query->uri) &&
        !semantic_query_append_document_highlight(state,
                                                  result,
                                                  query->resolvedMember.declarationRange,
                                                  3)) {
        return ZR_FALSE;
    } else if (query->resolvedMember.hasDeclaration &&
               query->resolvedMember.declarationUri != ZR_NULL &&
               ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.declarationUri, query->uri)) {
        appended = ZR_TRUE;
    }

    ZrCore_Array_Init(state, &locations, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    appended = semantic_query_append_external_type_member_locations_for_uri(state,
                                                                            context,
                                                                            query->projectIndex,
                                                                            query->uri,
                                                                            query,
                                                                            &locations) || appended;
    if (locations.length > 0) {
        appended = semantic_query_append_locations_as_highlights(state, &locations, query->uri, 2, result) || appended;
    }

    ZrCore_Array_Free(state, &locations);
    return appended;
}

static TZrBool semantic_query_resolve_external_metadata_target(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrString *uri,
                                                               SZrLspPosition position,
                                                               SZrLspSemanticQuery *query) {
    SZrLspExternalMetadataDeclaration externalDeclaration;
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&externalDeclaration, 0, sizeof(externalDeclaration));
    if (!ZrLanguageServer_LspProject_ResolveExternalMetadataDeclaration(state,
                                                                       context,
                                                                       uri,
                                                                       position,
                                                                       &externalDeclaration) ||
        !externalDeclaration.hasDeclaration) {
        return ZR_FALSE;
    }

    query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION;
    query->projectIndex = externalDeclaration.projectIndex;
    query->moduleName = externalDeclaration.moduleName;
    query->memberName = externalDeclaration.memberName;
    query->sourceKind = (EZrLspImportedModuleSourceKind)externalDeclaration.sourceKind;
    query->resolvedTypeInfo.origin = (EZrLspImportedModuleSourceKind)externalDeclaration.sourceKind;
    query->resolvedMember.declarationUri = externalDeclaration.declarationUri;
    query->resolvedMember.declarationRange = externalDeclaration.declarationRange;
    query->resolvedMember.hasDeclaration = externalDeclaration.hasDeclaration;

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    if (externalDeclaration.memberName != ZR_NULL &&
        ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(&provider,
                                                                   query->analyzer,
                                                                   externalDeclaration.projectIndex,
                                                                   externalDeclaration.moduleName,
                                                                   externalDeclaration.memberName,
                                                                   &query->resolvedMember)) {
        query->resolvedModule = query->resolvedMember.module;
        semantic_query_copy_resolved_member_type(state, &query->resolvedMember, &query->resolvedTypeInfo);
    } else if (externalDeclaration.moduleName != ZR_NULL &&
               ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(&provider,
                                                                          query->analyzer,
                                                                          externalDeclaration.projectIndex,
                                                                          externalDeclaration.moduleName,
                                                                          &query->resolvedModule)) {
        query->resolvedTypeInfo.valueKind = ZR_LSP_RESOLVED_VALUE_KIND_MODULE;
        query->resolvedMember.module = query->resolvedModule;
    }

    return ZR_TRUE;
}

static TZrBool semantic_query_resolve_receiver_native_member_target(SZrState *state,
                                                                    SZrLspContext *context,
                                                                    SZrString *uri,
                                                                    SZrSemanticAnalyzer *analyzer,
                                                                    SZrLspSemanticQuery *query) {
    SZrFileVersion *fileVersion;
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || analyzer == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || analyzer->ast == ZR_NULL ||
        !ZrLanguageServer_Lsp_TryResolveReceiverNativeMember(state,
                                                             query->projectIndex,
                                                             analyzer,
                                                             analyzer->ast,
                                                             fileVersion->content,
                                                             fileVersion->contentLength,
                                                             query->queryRange.start.offset,
                                                             &query->resolvedMember)) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(&provider,
                                                                            query->projectIndex,
                                                                            &query->resolvedMember);

    query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER;
    query->moduleName = query->resolvedMember.module.moduleName;
    query->memberName = query->resolvedMember.memberName;
    query->resolvedModule = query->resolvedMember.module;
    query->sourceKind = query->resolvedModule.sourceKind;
    query->resolvedTypeInfo.origin = query->sourceKind;
    semantic_query_copy_resolved_member_type(state, &query->resolvedMember, &query->resolvedTypeInfo);
    return ZR_TRUE;
}

static TZrBool semantic_query_resolve_external_metadata_type_member_declaration_target(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspSemanticQuery *query) {
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    if (!ZrLanguageServer_LspMetadataProvider_FindNativeTypeMemberDeclaration(&provider,
                                                                              query->projectIndex,
                                                                              uri,
                                                                              position,
                                                                              &query->resolvedMember)) {
        return ZR_FALSE;
    }

    query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER;
    query->moduleName = query->resolvedMember.module.moduleName;
    query->memberName = query->resolvedMember.memberName;
    query->resolvedModule = query->resolvedMember.module;
    query->sourceKind = query->resolvedModule.sourceKind;
    query->resolvedTypeInfo.origin = query->sourceKind;
    semantic_query_copy_resolved_member_type(state, &query->resolvedMember, &query->resolvedTypeInfo);
    return ZR_TRUE;
}

static TZrBool semantic_query_resolve_import_binding_target(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrLspProjectIndex *projectIndex,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrArray *bindings,
                                                            SZrFileRange queryRange,
                                                            SZrLspSemanticQuery *query) {
    SZrLspImportBinding *binding = ZR_NULL;
    SZrFileRange bindingRange;
    SZrLspMetadataProvider provider;
    SZrLspResolvedImportedModuleEntry moduleEntry;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || bindings == ZR_NULL || query == ZR_NULL ||
        (!semantic_query_find_import_binding_hit(bindings, queryRange, &binding, &bindingRange) &&
         !ZrLanguageServer_LspProject_FindImportBindingHit(analyzer->ast,
                                                          bindings,
                                                          queryRange,
                                                          &binding,
                                                          &bindingRange)) ||
        binding == ZR_NULL || binding->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(&provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    binding->moduleName,
                                                                    &query->resolvedModule)) {
        return ZR_FALSE;
    }

    memset(&moduleEntry, 0, sizeof(moduleEntry));
    ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry(&provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    binding->moduleName,
                                                                    &moduleEntry);

    query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION;
    query->moduleName = binding->moduleName;
    query->memberName = ZR_NULL;
    query->queryRange = bindingRange;
    query->sourceKind = query->resolvedModule.sourceKind;
    query->resolvedTypeInfo.origin = query->sourceKind;
    query->resolvedTypeInfo.valueKind = ZR_LSP_RESOLVED_VALUE_KIND_MODULE;
    query->resolvedMember.module = query->resolvedModule;
    query->resolvedMember.memberKind = ZR_LSP_METADATA_MEMBER_MODULE;
    query->resolvedMember.declarationUri = moduleEntry.declarationUri;
    query->resolvedMember.declarationRange = moduleEntry.declarationRange;
    query->resolvedMember.hasDeclaration = moduleEntry.hasDeclaration;
    return ZR_TRUE;
}

void ZrLanguageServer_LspSemanticQuery_Init(SZrLspSemanticQuery *query) {
    if (query == ZR_NULL) {
        return;
    }

    memset(query, 0, sizeof(*query));
}

void ZrLanguageServer_LspSemanticQuery_Free(SZrState *state, SZrLspSemanticQuery *query) {
    ZR_UNUSED_PARAMETER(state);

    if (query == ZR_NULL) {
        return;
    }

    memset(query, 0, sizeof(*query));
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspSemanticQuery *query) {
    SZrSemanticAnalyzer *analyzer;
    SZrLspProjectIndex *projectIndex;
    SZrFilePosition filePosition;
    SZrArray bindings;
    SZrLspSemanticImportChainHit importChainHit;
    SZrLspMetadataProvider provider;
    SZrLspResolvedImportedModuleEntry moduleEntry;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspSemanticQuery_Init(query);
    query->uri = uri;
    query->position = position;

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    query->queryRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    query->projectIndex = projectIndex;
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    query->analyzer = analyzer;

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
        if (ZrLanguageServer_LspSemanticImportChain_ResolveAtRange(state,
                                                                   context,
                                                                   projectIndex,
                                                                   analyzer,
                                                                   &bindings,
                                                                   query->queryRange,
                                                                   &importChainHit)) {
            if (importChainHit.memberName == ZR_NULL) {
                ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
                memset(&moduleEntry, 0, sizeof(moduleEntry));
                if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(&provider,
                                                                                analyzer,
                                                                                projectIndex,
                                                                                importChainHit.moduleName,
                                                                                &query->resolvedModule)) {
                    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
                    return ZR_FALSE;
                }

                ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry(&provider,
                                                                                analyzer,
                                                                                projectIndex,
                                                                                importChainHit.moduleName,
                                                                                &moduleEntry);

                query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION;
                query->moduleName = importChainHit.moduleName;
                query->memberName = ZR_NULL;
                query->queryRange = importChainHit.location;
                query->sourceKind = query->resolvedModule.sourceKind;
                query->resolvedTypeInfo.origin = query->sourceKind;
                query->resolvedTypeInfo.valueKind = ZR_LSP_RESOLVED_VALUE_KIND_MODULE;
                query->resolvedMember.module = query->resolvedModule;
                query->resolvedMember.memberKind = ZR_LSP_METADATA_MEMBER_MODULE;
                query->resolvedMember.declarationUri = moduleEntry.declarationUri;
                query->resolvedMember.declarationRange = moduleEntry.declarationRange;
                query->resolvedMember.hasDeclaration = moduleEntry.hasDeclaration;
            } else {
                query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER;
                query->moduleName = importChainHit.moduleName;
                query->memberName = importChainHit.memberName;
                query->resolvedMember = importChainHit.resolvedMember;
                query->sourceKind = query->resolvedMember.module.sourceKind;
                query->resolvedModule = query->resolvedMember.module;
                query->resolvedTypeInfo.origin = query->sourceKind;
                semantic_query_copy_resolved_member_type(state, &query->resolvedMember, &query->resolvedTypeInfo);
            }
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return ZR_TRUE;
        }
        if (semantic_query_resolve_import_binding_target(state,
                                                         context,
                                                         projectIndex,
                                                         analyzer,
                                                         &bindings,
                                                         query->queryRange,
                                                         query)) {
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return ZR_TRUE;
        }
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);

    if (semantic_query_resolve_external_metadata_type_member_declaration_target(state, context, uri, position, query)) {
        return ZR_TRUE;
    }

    if (semantic_query_resolve_receiver_native_member_target(state, context, uri, analyzer, query)) {
        return ZR_TRUE;
    }

    if (semantic_query_resolve_external_metadata_target(state, context, uri, position, query)) {
        return ZR_TRUE;
    }

    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    query->symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, query->queryRange);
    if (query->symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL;
    query->resolvedTypeInfo.origin = ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
    query->resolvedTypeInfo.valueKind = ZR_LSP_RESOLVED_VALUE_KIND_SYMBOL;
    {
        SZrInferredType resolvedType;

        ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
        if (ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(state,
                                                                    analyzer,
                                                                    query->queryRange,
                                                                    &resolvedType)) {
            semantic_query_copy_type_text(state, &resolvedType, &query->resolvedTypeInfo);
            if (query->symbol->type == ZR_SYMBOL_CLASS || query->symbol->type == ZR_SYMBOL_STRUCT ||
                query->symbol->type == ZR_SYMBOL_INTERFACE || query->symbol->type == ZR_SYMBOL_ENUM) {
                query->resolvedTypeInfo.valueKind = ZR_LSP_RESOLVED_VALUE_KIND_TYPE;
            }
        } else {
            semantic_query_copy_type_text(state, query->symbol->typeInfo, &query->resolvedTypeInfo);
        }
        ZrParser_InferredType_Free(state, &resolvedType);
    }
    return ZR_TRUE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_BuildHover(SZrState *state,
                                                                            SZrLspContext *context,
                                                                            SZrLspSemanticQuery *query,
                                                                            SZrLspHover **result) {
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER) {
        ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
        return ZrLanguageServer_LspMetadataProvider_CreateImportedMemberHover(&provider,
                                                                             query->analyzer,
                                                                             &query->resolvedMember,
                                                                             query->queryRange,
                                                                             result);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION) {
        ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
        if (query->memberName != ZR_NULL) {
            return ZrLanguageServer_LspMetadataProvider_CreateImportedMemberHover(&provider,
                                                                                 query->analyzer,
                                                                                 &query->resolvedMember,
                                                                                 query->queryRange,
                                                                                 result);
        }
        return ZrLanguageServer_LspMetadataProvider_CreateImportedModuleHover(&provider,
                                                                              &query->resolvedModule,
                                                                              query->queryRange,
                                                                              result);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER) {
        ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
        return ZrLanguageServer_LspMetadataProvider_CreateImportedMemberHover(&provider,
                                                                             query->analyzer,
                                                                             &query->resolvedMember,
                                                                             query->queryRange,
                                                                             result);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL && query->analyzer != ZR_NULL) {
        SZrHoverInfo *hoverInfo = ZR_NULL;
        SZrFileVersion *fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, query->uri);
        SZrString *content = ZR_NULL;

        if (ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state,
                                                           query->analyzer,
                                                           query->queryRange,
                                                           &hoverInfo) &&
            hoverInfo != ZR_NULL && hoverInfo->contents != ZR_NULL) {
            content = hoverInfo->contents;
        } else if (query->symbol != ZR_NULL && fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
            content = ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(state,
                                                                            query->symbol,
                                                                            fileVersion->content,
                                                                            fileVersion->contentLength);
        }

        if (content == ZR_NULL) {
            if (hoverInfo != ZR_NULL) {
                ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
            }
            return ZR_FALSE;
        }

        if (query->symbol != ZR_NULL && fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
            SZrString *comment = ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(state,
                                                                                    query->symbol,
                                                                                    fileVersion->content,
                                                                                    fileVersion->contentLength);
            content = semantic_query_append_markdown_section(state, content, comment);
        }

        if (hoverInfo != ZR_NULL) {
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        }
        return semantic_query_create_hover_from_content(state,
                                                        content,
                                                        query->symbol != ZR_NULL
                                                            ? ZrLanguageServer_Lsp_GetSymbolLookupRange(query->symbol)
                                                            : query->queryRange,
                                                        result);
    }

    return ZR_FALSE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrArray completions;
    SZrFileVersion *fileVersion;
    TZrBool hasStructuredCompletions = ZR_FALSE;
    SZrString *hoveredSymbolName = ZR_NULL;
    SZrString *resolvedTypeText = ZR_NULL;
    SZrLspSemanticQuery semanticQuery;
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);

    ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery)) {
        hoveredSymbolName = semanticQuery.symbol != ZR_NULL ? semanticQuery.symbol->name : semanticQuery.memberName;
        resolvedTypeText = semanticQuery.resolvedTypeInfo.resolvedTypeText;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL && analyzer->ast != ZR_NULL) {
        hasStructuredCompletions = ZrLanguageServer_Lsp_TryCollectTokenPrefixCompletions(state,
                                                                                         fileVersion->content,
                                                                                         fileVersion->contentLength,
                                                                                         filePos.offset,
                                                                                         &completions);
        if (!hasStructuredCompletions) {
            SZrArray bindings;
            SZrLspResolvedImportedModule completionModule;
            SZrLspProjectIndex *completionProjectIndex = semanticQuery.projectIndex;

            if (completionProjectIndex == ZR_NULL) {
                completionProjectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
            }

            memset(&completionModule, 0, sizeof(completionModule));
            ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
            ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
            if (ZrLanguageServer_LspSemanticImportChain_ResolveCompletionModuleAtOffset(state,
                                                                                        context,
                                                                                        completionProjectIndex,
                                                                                        analyzer,
                                                                                        &bindings,
                                                                                        fileVersion->content,
                                                                                        fileVersion->contentLength,
                                                                                        filePos.offset,
                                                                                        &completionModule)) {
                ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
                hasStructuredCompletions = ZrLanguageServer_LspMetadataProvider_AppendImportedModuleCompletions(
                    &provider,
                    analyzer,
                    &completionModule,
                    &completions);
            }
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        }
        if (!hasStructuredCompletions &&
            (semanticQuery.kind == ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER ||
             (semanticQuery.kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION &&
              semanticQuery.memberName == ZR_NULL &&
              semanticQuery.resolvedModule.moduleName != ZR_NULL))) {
            ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
            hasStructuredCompletions = ZrLanguageServer_LspMetadataProvider_AppendImportedModuleCompletions(
                &provider,
                analyzer,
                &semanticQuery.resolvedModule,
                &completions);
        }
        if (!hasStructuredCompletions) {
            hasStructuredCompletions = ZrLanguageServer_Lsp_TryCollectReceiverCompletions(state,
                                                                                          context,
                                                                                          semanticQuery.projectIndex,
                                                                                          analyzer,
                                                                                          uri,
                                                                                          analyzer->ast,
                                                                                          fileVersion->content,
                                                                                          fileVersion->contentLength,
                                                                                          filePos.offset,
                                                                                          &completions);
        }
    }

    if (!hasStructuredCompletions &&
        !ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, fileRange, &completions)) {
        ZrCore_Array_Free(state, &completions);
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < completions.length; index++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(&completions, index);
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL) {
            continue;
        }

        if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
            ZrLanguageServer_Lsp_EnrichCompletionItemMetadata(state,
                                                              analyzer,
                                                              *itemPtr,
                                                              hoveredSymbolName,
                                                              resolvedTypeText,
                                                              fileVersion->content,
                                                              fileVersion->contentLength);
        }
        ZrCore_Array_Push(state, result, itemPtr);
    }

    ZrCore_Array_Free(state, &completions);
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    return ZR_TRUE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDefinitions(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER) {
        if (query->resolvedMember.hasDeclaration && query->resolvedMember.declarationUri != ZR_NULL) {
            return semantic_query_append_location(state,
                                                  result,
                                                  query->resolvedMember.declarationUri,
                                                  query->resolvedMember.declarationRange);
        }
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION) {
        return query->resolvedMember.hasDeclaration &&
               query->resolvedMember.declarationUri != ZR_NULL &&
               semantic_query_append_location(state,
                                              result,
                                              query->resolvedMember.declarationUri,
                                              query->resolvedMember.declarationRange);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER) {
        return query->resolvedMember.hasDeclaration &&
               query->resolvedMember.declarationUri != ZR_NULL &&
               semantic_query_append_location(state,
                                              result,
                                              query->resolvedMember.declarationUri,
                                              query->resolvedMember.declarationRange);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL) {
        return query->symbol != ZR_NULL &&
               semantic_query_append_location(state,
                                              result,
                                              query->symbol->location.source,
                                              ZrLanguageServer_Lsp_GetSymbolLookupRange(query->symbol));
    }

    return ZR_FALSE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendReferences(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    TZrBool includeDeclaration,
    SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER) {
        TZrBool appended = ZR_FALSE;

        if (includeDeclaration &&
            query->resolvedMember.hasDeclaration &&
            query->resolvedMember.declarationUri != ZR_NULL &&
            !semantic_query_append_location(state,
                                            result,
                                            query->resolvedMember.declarationUri,
                                            query->resolvedMember.declarationRange)) {
            return ZR_FALSE;
        }

        if (query->projectIndex != ZR_NULL) {
            appended = semantic_query_append_project_imported_member_references(state,
                                                                                context,
                                                                                query->projectIndex,
                                                                                query->moduleName,
                                                                                query->memberName,
                                                                                result);
        } else if (query->uri != ZR_NULL) {
            appended = semantic_query_append_imported_member_locations_for_uri(state,
                                                                               context,
                                                                               query->projectIndex,
                                                                               query->uri,
                                                                               query->moduleName,
                                                                               query->memberName,
                                                                               result);
        }

        return appended || result->length > 0;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION) {
        SZrLspExternalMetadataDeclaration resolved;

        memset(&resolved, 0, sizeof(resolved));
        resolved.projectIndex = query->projectIndex;
        resolved.moduleName = query->moduleName;
        resolved.memberName = query->memberName;
        resolved.declarationUri = query->resolvedMember.declarationUri;
        resolved.declarationRange = query->resolvedMember.declarationRange;
        resolved.hasDeclaration = query->resolvedMember.hasDeclaration;
        resolved.sourceKind = query->resolvedModule.sourceKind;
        return ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationReferences(state,
                                                                                       context,
                                                                                       &resolved,
                                                                                       query->uri,
                                                                                       includeDeclaration,
                                                                                       result);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER) {
        if (includeDeclaration &&
            query->resolvedMember.hasDeclaration &&
            query->resolvedMember.declarationUri != ZR_NULL &&
            !semantic_query_append_location(state,
                                            result,
                                            query->resolvedMember.declarationUri,
                                            query->resolvedMember.declarationRange)) {
            return ZR_FALSE;
        }

        return semantic_query_append_project_external_type_member_references(state, context, query, result) ||
               result->length > 0;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL) {
        TZrBool appended = semantic_query_append_local_symbol_references(state, query, includeDeclaration, result);

        if (!appended || query->projectIndex == ZR_NULL || query->symbol == ZR_NULL ||
            query->symbol->accessModifier != ZR_ACCESS_PUBLIC) {
            return appended;
        }

        {
            SZrLspProjectFileRecord *record =
                ZrLanguageServer_LspProject_FindRecordByUri(query->projectIndex, query->symbol->location.source);
            if (record == ZR_NULL || record->moduleName == ZR_NULL) {
                return appended;
            }

            return semantic_query_append_project_imported_member_references(state,
                                                                            context,
                                                                            query->projectIndex,
                                                                            record->moduleName,
                                                                            query->symbol->name,
                                                                            result) || appended;
        }
    }

    return ZR_FALSE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER) {
        return semantic_query_append_imported_member_highlights(state, context, query, result);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION) {
        SZrLspExternalMetadataDeclaration resolved;

        memset(&resolved, 0, sizeof(resolved));
        resolved.projectIndex = query->projectIndex;
        resolved.moduleName = query->moduleName;
        resolved.memberName = query->memberName;
        resolved.declarationUri = query->resolvedMember.declarationUri;
        resolved.declarationRange = query->resolvedMember.declarationRange;
        resolved.hasDeclaration = query->resolvedMember.hasDeclaration;
        resolved.sourceKind = query->resolvedModule.sourceKind;
        return ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationHighlights(state,
                                                                                       context,
                                                                                       &resolved,
                                                                                       query->uri,
                                                                                       result);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER) {
        return semantic_query_append_external_type_member_highlights(state, context, query, result);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL) {
        return semantic_query_append_local_symbol_highlights(state, query, result);
    }

    return ZR_FALSE;
}
