#include "semantic/lsp_semantic_query.h"
#include "semantic/lsp_semantic_definition_query.h"
#include "semantic/lsp_canonical_completion.h"
#include "semantic/lsp_canonical_hover.h"
#include "semantic/lsp_semantic_import_chain.h"
#include "semantic/lsp_semantic_reference_query.h"
#include "semantic/semantic_analyzer_internal.h"
#include "interface/lsp_semantic_cache_lru.h"
#include "project/lsp_project_internal.h"

#include "zr_vm_library/file.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/type_inference.h"

#include <ctype.h>
#include <string.h>

static TZrBool semantic_query_capture_document_version(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query) {
    SZrFileVersion *fileVersion;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL ||
        query->uri == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, query->uri);
    query->hasSemanticVersion = fileVersion != ZR_NULL;
    query->semanticVersion = fileVersion != ZR_NULL ? fileVersion->version : 0U;
    return ZR_TRUE;
}

static TZrBool semantic_query_document_version_is_current(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspSemanticQuery *query) {
    SZrFileVersion *fileVersion;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL ||
        query->uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!query->hasSemanticVersion) {
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, query->uri);
    return fileVersion != ZR_NULL && fileVersion->version == query->semanticVersion;
}

static TZrBool semantic_query_type_id_is_available(
        const SZrSemanticContext *semanticContext,
        TZrTypeId typeId) {
    TZrBool hasTypeReference = ZR_FALSE;

    if (semanticContext == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < semanticContext->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&semanticContext->referenceFacts, index);
        if (reference == ZR_NULL || reference->kind != ZR_SEMANTIC_REFERENCE_TYPE ||
            reference->typeId != typeId) {
            continue;
        }

        hasTypeReference = ZR_TRUE;
        if (reference->isResolved) {
            return ZR_TRUE;
        }
    }

    return !hasTypeReference;
}

static TZrBool semantic_query_copy_canonical_type_at(
        SZrState *state,
        const SZrSemanticContext *semanticContext,
        SZrFileRange range,
        SZrLspResolvedTypeInfo *outInfo) {
    SZrParserSemanticTypeQuery typeQuery;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (state == ZR_NULL || semanticContext == ZR_NULL || outInfo == ZR_NULL ||
        !ZrParser_SemanticQuery_CanonicalTypeAt(semanticContext, range, ZR_NULL, &typeQuery) ||
        typeQuery.typeId == ZR_SEMANTIC_ID_INVALID ||
        !((typeQuery.reference != ZR_NULL &&
           typeQuery.reference->kind == ZR_SEMANTIC_REFERENCE_TYPE &&
           typeQuery.reference->isResolved) ||
          (typeQuery.expression != ZR_NULL &&
           typeQuery.expression->exactness == ZR_SEMANTIC_FACT_EXACT &&
           typeQuery.expression->typeId == typeQuery.typeId &&
           ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(
                   &typeQuery.expression->inferredType) &&
           semantic_query_type_id_is_available(semanticContext, typeQuery.typeId))) ||
        !ZrParser_CanonicalType_Format(
                semanticContext, typeQuery.typeId, typeBuffer, sizeof(typeBuffer))) {
        return ZR_FALSE;
    }

    outInfo->resolvedTypeText =
            ZrCore_String_Create(state, (TZrNativeString)typeBuffer, strlen(typeBuffer));
    return outInfo->resolvedTypeText != ZR_NULL;
}

static TZrBool semantic_query_copy_canonical_symbol_type(
        SZrState *state,
        const SZrSemanticContext *semanticContext,
        const SZrParserSemanticSymbolQuery *symbol,
        SZrLspResolvedTypeInfo *outInfo) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (state == ZR_NULL || semanticContext == ZR_NULL || symbol == ZR_NULL ||
        outInfo == ZR_NULL || symbol->typeId == ZR_SEMANTIC_ID_INVALID ||
        !ZrParser_CanonicalType_Format(
                semanticContext, symbol->typeId, typeBuffer, sizeof(typeBuffer))) {
        return ZR_FALSE;
    }

    outInfo->resolvedTypeText = ZrCore_String_Create(
            state, (TZrNativeString)typeBuffer, strlen(typeBuffer));
    return outInfo->resolvedTypeText != ZR_NULL;
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

static TZrBool semantic_query_is_identifier_char(TZrChar value) {
    return isalnum((unsigned char)value) || value == '_';
}

static const TZrChar *semantic_query_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static SZrString *semantic_query_extract_identifier_at_offset(SZrState *state,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize offset) {
    TZrSize start;
    TZrSize end;

    if (state == ZR_NULL || content == ZR_NULL || contentLength == 0) {
        return ZR_NULL;
    }

    if (offset >= contentLength) {
        offset = contentLength - 1;
    }
    if (!semantic_query_is_identifier_char(content[offset]) &&
        offset > 0 &&
        semantic_query_is_identifier_char(content[offset - 1])) {
        offset--;
    }
    if (!semantic_query_is_identifier_char(content[offset])) {
        return ZR_NULL;
    }

    start = offset;
    while (start > 0 && semantic_query_is_identifier_char(content[start - 1])) {
        start--;
    }

    end = offset + 1;
    while (end < contentLength && semantic_query_is_identifier_char(content[end])) {
        end++;
    }

    if (end <= start) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)(content + start), end - start);
}

static TZrSize semantic_query_lsp_offset_from_position(const TZrChar *content,
                                                       TZrSize contentLength,
                                                       SZrLspPosition position) {
    TZrSize offset = 0;
    TZrInt32 line = 0;
    TZrInt32 character = 0;

    if (content == ZR_NULL || position.line < 0 || position.character < 0) {
        return 0;
    }

    while (offset < contentLength && line < position.line) {
        if (content[offset] == '\n') {
            line++;
        }
        offset++;
    }

    while (offset < contentLength && character < position.character &&
           content[offset] != '\n' && content[offset] != '\r') {
        offset++;
        character++;
    }

    return offset < contentLength ? offset : contentLength > 0 ? contentLength - 1 : 0;
}

static SZrFileRange semantic_query_range_from_offset(const TZrChar *content,
                                                     TZrSize contentLength,
                                                     TZrSize startOffset,
                                                     TZrSize length,
                                                     SZrString *uri) {
    SZrFileRange range;
    TZrSize offset = 0;
    TZrInt32 line = 1;
    TZrInt32 column = 1;

    while (offset < contentLength && offset < startOffset) {
        if (content[offset] == '\n') {
            line++;
            column = 1;
        } else if (content[offset] != '\r') {
            column++;
        }
        offset++;
    }

    range.start.line = line;
    range.start.column = column;
    range.start.offset = startOffset;

    while (offset < contentLength && offset < startOffset + length) {
        if (content[offset] == '\n') {
            line++;
            column = 1;
        } else if (content[offset] != '\r') {
            column++;
        }
        offset++;
    }

    range.end.line = line;
    range.end.column = column;
    range.end.offset = startOffset + length;
    range.source = uri;
    return range;
}

static TZrBool semantic_query_position_is_code_span(SZrLspContext *context,
                                                    SZrString *uri,
                                                    SZrLspPosition position) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrFilePosition filePosition;
    TZrBool result;

    if (context == ZR_NULL || context->state == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(context->state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                          snapshot.content,
                                                                          snapshot.contentLength);
    result = ZrLanguageServer_Lsp_IsOffsetInCodeSpan(snapshot.content,
                                                     snapshot.contentLength,
                                                     filePosition.offset);
    ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
    return result;
}

static TZrBool semantic_query_append_location(SZrState *state,
                                              SZrLspContext *context,
                                              SZrArray *result,
                                              SZrString *uri,
                                              SZrFileRange range,
                                              EZrLspImportedModuleSourceKind sourceKind) {
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
    if (sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA) {
        if (!ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates(context,
                                                                        uri,
                                                                        range,
                                                                        &location->range)) {
            ZrCore_Memory_RawFree(state->global, location, sizeof(SZrLspLocation));
            return ZR_FALSE;
        }
    } else if (sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
        if (!ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates(range, &location->range)) {
            ZrCore_Memory_RawFree(state->global, location, sizeof(SZrLspLocation));
            return ZR_FALSE;
        }
    } else {
        location->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, range);
    }
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static TZrBool semantic_query_append_document_highlight(SZrState *state,
                                                        SZrLspContext *context,
                                                        SZrString *uri,
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

    highlight->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, range);
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

static TZrBool semantic_query_append_lsp_document_highlight(SZrState *state,
                                                            SZrArray *result,
                                                            SZrLspRange range,
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

    highlight->range = range;
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

static TZrBool semantic_query_uri_to_native_path(SZrString *uri, TZrChar *buffer, TZrSize bufferSize) {
    return ZrLanguageServer_Lsp_FileUriToNativePath(uri, buffer, bufferSize);
}

static TZrBool semantic_query_path_has_extension(const TZrChar *path, const TZrChar *extension) {
    TZrSize pathLength;
    TZrSize extensionLength;

    if (path == ZR_NULL || extension == ZR_NULL) {
        return ZR_FALSE;
    }

    pathLength = strlen(path);
    extensionLength = strlen(extension);
    return pathLength >= extensionLength && strcmp(path + pathLength - extensionLength, extension) == 0;
}

static const TZrChar *semantic_query_dynamic_library_extension(void) {
#if defined(ZR_VM_PLATFORM_IS_WIN) || defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

static TZrBool semantic_query_uri_is_binary_metadata_uri(SZrString *uri) {
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (uri == ZR_NULL || !semantic_query_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        return ZR_FALSE;
    }

    return semantic_query_path_has_extension(nativePath, ZR_VM_BINARY_MODULE_FILE_EXTENSION) ||
           semantic_query_path_has_extension(nativePath, ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION);
}

static TZrBool semantic_query_uri_is_native_plugin_metadata_uri(SZrString *uri) {
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (uri == ZR_NULL || !semantic_query_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        return ZR_FALSE;
    }

    return semantic_query_path_has_extension(nativePath, semantic_query_dynamic_library_extension());
}

static TZrBool semantic_query_try_get_analyzer_for_uri(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       SZrSemanticAnalyzer **outAnalyzer) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    TZrNativeString sourceBuffer = ZR_NULL;
    TZrSize sourceLength = 0;
    TZrSize sourceVersion = 0;
    TZrBool loadedFromDisk = ZR_FALSE;
    TZrBool hasSnapshot = ZR_FALSE;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outAnalyzer != ZR_NULL) {
        *outAnalyzer = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL &&
        (analyzer->ast != ZR_NULL || analyzer->semanticContext != ZR_NULL)) {
        *outAnalyzer = analyzer;
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        sourceBuffer = snapshot.content;
        sourceLength = snapshot.contentLength;
        sourceVersion = snapshot.version;
        hasSnapshot = ZR_TRUE;
    } else if (state->global != ZR_NULL && semantic_query_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        sourceBuffer = ZrLibrary_File_ReadAll(state->global, nativePath);
        sourceLength = sourceBuffer != ZR_NULL ? strlen(sourceBuffer) : 0;
        sourceVersion = fileVersion != ZR_NULL ? fileVersion->version : 0;
        loadedFromDisk = sourceBuffer != ZR_NULL;
    }

    if (sourceBuffer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocumentCore(state,
                                                 context,
                                                 uri,
                                                 sourceBuffer,
                                                 sourceLength,
                                                 sourceVersion,
                                                 ZR_FALSE)) {
        if (hasSnapshot) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        }
        if (loadedFromDisk) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          sourceBuffer,
                                          sourceLength + 1,
                                          ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        }
        return ZR_FALSE;
    }

    if (hasSnapshot) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    }
    if (loadedFromDisk) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceBuffer,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    *outAnalyzer = analyzer;
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

        if (!semantic_query_append_lsp_document_highlight(state, result, (*locationPtr)->range, kind)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static void semantic_query_free_locations(SZrState *state, SZrArray *locations) {
    if (state == ZR_NULL || locations == ZR_NULL || !locations->isValid) {
        return;
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *locationPtr, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
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
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);

        if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
            return ZR_FALSE;
        }

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
    if (query->resolvedMember.module.moduleName != ZR_NULL &&
        !ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.module.moduleName, query->moduleName)) {
        appended = semantic_query_append_imported_member_locations_for_uri(state,
                                                                           context,
                                                                           query->projectIndex,
                                                                           query->uri,
                                                                           query->resolvedMember.module.moduleName,
                                                                           query->memberName,
                                                                           &locations) ||
                   appended;
    }
    if (!appended) {
        semantic_query_free_locations(state, &locations);
        return ZR_FALSE;
    }

    appended = semantic_query_append_locations_as_highlights(state, &locations, query->uri, 2, result);
    semantic_query_free_locations(state, &locations);
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
    return (ZrLanguageServer_Lsp_StringsEqual(left.source, right.source) ||
            left.source == ZR_NULL || right.source == ZR_NULL) &&
           left.start.line == right.start.line &&
           left.start.column == right.start.column &&
           left.end.line == right.end.line &&
           left.end.column == right.end.column;
}

static TZrBool semantic_query_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source) &&
        range.source != ZR_NULL && position.source != ZR_NULL) {
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
    if (query == ZR_NULL || candidate == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!query->resolvedMember.hasDeclaration || !candidate->hasDeclaration ||
        query->resolvedMember.declarationUri == ZR_NULL || candidate->declarationUri == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.declarationUri, candidate->declarationUri) &&
           semantic_query_file_ranges_equal(query->resolvedMember.declarationRange, candidate->declarationRange);
}

static TZrBool semantic_query_try_resolve_receiver_external_type_member(SZrState *state,
                                                                        SZrLspContext *context,
                                                                        SZrLspProjectIndex *projectIndex,
                                                                        SZrSemanticAnalyzer *analyzer,
                                                                        SZrString *uri,
                                                                        SZrAstNode *ast,
                                                                        const TZrChar *content,
                                                                        TZrSize contentLength,
                                                                        TZrSize cursorOffset,
                                                                        SZrLspResolvedMetadataMember *outResolved) {
    if (state == ZR_NULL || analyzer == ZR_NULL || uri == ZR_NULL || ast == ZR_NULL ||
        content == ZR_NULL || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_Lsp_TryResolveReceiverProjectMember(state,
                                                             context,
                                                             projectIndex,
                                                             analyzer,
                                                             uri,
                                                             ast,
                                                             content,
                                                             contentLength,
                                                             cursorOffset,
                                                             outResolved)) {
        return ZR_TRUE;
    }

    return ZrLanguageServer_Lsp_TryResolveReceiverNativeMember(state,
                                                               projectIndex,
                                                               analyzer,
                                                               uri,
                                                               ast,
                                                               content,
                                                               contentLength,
                                                               cursorOffset,
                                                               outResolved);
}

static TZrBool semantic_query_append_external_type_member_locations_recursive(SZrState *state,
                                                                              SZrLspContext *context,
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
                                                                                  SZrLspContext *context,
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
        if (semantic_query_append_external_type_member_locations_recursive(state, context,
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
                                                                                SZrLspContext *context,
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
            !semantic_query_try_resolve_receiver_external_type_member(state,
                                                                      ZR_NULL,
                                                                      projectIndex,
                                                                      analyzer,
                                                                      uri,
                                                                      astRoot,
                                                                      content,
                                                                      contentLength,
                                                                      cursorOffset,
                                                                      &resolvedMember) ||
            !semantic_query_matches_external_type_member(query, &resolvedMember)) {
            continue;
        }

        {
            const TZrChar *resolvedName = semantic_query_string_text(resolvedMember.memberName);
            SZrFileRange location = semantic_query_range_from_offset(content,
                                                                     contentLength,
                                                                     cursorOffset,
                                                                     resolvedName != ZR_NULL ? strlen(resolvedName) : 0,
                                                                     uri);
            if (semantic_query_append_location(state,
                                               context,
                                               result,
                                               uri,
                                               location,
                                               ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED)) {
                appended = ZR_TRUE;
            }
        }
    }

    return appended;
}

static TZrBool semantic_query_append_external_type_member_locations_recursive(SZrState *state,
                                                                              SZrLspContext *context,
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
                                                                         context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.functionDeclaration.body,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) || appended;

        case ZR_AST_STRUCT_METHOD:
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.usingStatement.resource,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.tryCatchFinallyStatement.block,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_in_node_array(state, context,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.tryCatchFinallyStatement.catchClauses,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.assignmentExpression.left,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.conditionalExpression.test,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.conditionalExpression.consequent,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.ifExpression.condition,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.ifExpression.thenExpr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.switchExpression.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_in_node_array(state, context,
                                                                                      projectIndex,
                                                                                      analyzer,
                                                                                      astRoot,
                                                                                      node->data.switchExpression.cases,
                                                                                      content,
                                                                                      contentLength,
                                                                                      query,
                                                                                      uri,
                                                                                      result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.switchCase.value,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.constructExpression.target,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.primaryExpression.property,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
                       ? semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_in_node_array(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.keyValuePair.key,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.whileLoop.cond,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.forLoop.init,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.forLoop.cond,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.forLoop.step,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
            return semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                                  projectIndex,
                                                                                  analyzer,
                                                                                  astRoot,
                                                                                  node->data.foreachLoop.expr,
                                                                                  content,
                                                                                  contentLength,
                                                                                  query,
                                                                                  uri,
                                                                                  result) ||
                   semantic_query_append_external_type_member_locations_recursive(state, context,
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
    SZrFileVersionContentSnapshot snapshot = {0};
    TZrBool appended;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_query_try_get_analyzer_for_uri(state, context, uri, &analyzer) ||
        analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }

    appended = semantic_query_append_external_type_member_locations_recursive(state, context,
                                                                              projectIndex,
                                                                              analyzer,
                                                                              analyzer->ast,
                                                                              analyzer->ast,
                                                                              snapshot.content,
                                                                              snapshot.contentLength,
                                                                              query,
                                                                              uri,
                                                                              result);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return appended;
}

static TZrBool semantic_query_append_project_external_type_member_references(SZrState *state,
                                                                             SZrLspContext *context,
                                                                             SZrLspSemanticQuery *query,
                                                                             SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
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

    if (query->resolvedMember.module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE ||
        query->resolvedMember.module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER) {
        if (query->uri != ZR_NULL &&
            semantic_query_append_external_type_member_locations_for_uri(state,
                                                                         context,
                                                                         query->projectIndex,
                                                                         query->uri,
                                                                         query,
                                                                         result)) {
            appended = ZR_TRUE;
        }
        if (query->resolvedMember.declarationUri != ZR_NULL &&
            !ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.declarationUri, query->uri) &&
            semantic_query_append_external_type_member_locations_for_uri(state,
                                                                         context,
                                                                         query->projectIndex,
                                                                         query->resolvedMember.declarationUri,
                                                                         query,
                                                                         result)) {
            appended = ZR_TRUE;
        }
        return appended;
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
                                                  context,
                                                  query->uri,
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
    SZrString *preciseMemberDeclarationUri;
    SZrFileRange preciseMemberDeclarationRange;
    TZrBool hasPreciseMemberDeclaration;

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
    preciseMemberDeclarationUri = externalDeclaration.declarationUri;
    preciseMemberDeclarationRange = externalDeclaration.declarationRange;
    hasPreciseMemberDeclaration = externalDeclaration.memberName != ZR_NULL && externalDeclaration.hasDeclaration;

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    if (externalDeclaration.memberName != ZR_NULL &&
        ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(&provider,
                                                                   query->analyzer,
                                                                   externalDeclaration.projectIndex,
                                                                   externalDeclaration.moduleName,
                                                                   externalDeclaration.memberName,
                                                                   &query->resolvedMember)) {
        query->resolvedModule = query->resolvedMember.module;
        if (hasPreciseMemberDeclaration) {
            query->resolvedMember.declarationUri = preciseMemberDeclarationUri;
            query->resolvedMember.declarationRange = preciseMemberDeclarationRange;
            query->resolvedMember.hasDeclaration = ZR_TRUE;
        }
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

static TZrBool semantic_query_resolve_receiver_type_member_target(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri,
                                                                  SZrSemanticAnalyzer *analyzer,
                                                                  SZrLspSemanticQuery *query) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrLspMetadataProvider provider;
    TZrBool resolved;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || analyzer == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_query_position_is_code_span(context, uri, query->position)) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (analyzer->ast == ZR_NULL ||
        !ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    resolved = semantic_query_try_resolve_receiver_external_type_member(state,
                                                                        context,
                                                                        query->projectIndex,
                                                                        analyzer,
                                                                        uri,
                                                                        analyzer->ast,
                                                                        snapshot.content,
                                                                        snapshot.contentLength,
                                                                        query->queryRange.start.offset,
                                                                        &query->resolvedMember);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    if (!resolved) {
        return ZR_FALSE;
    }

    if (!query->resolvedMember.hasDeclaration &&
        (query->resolvedMember.module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE ||
         query->resolvedMember.module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER)) {
        ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
        ZrLanguageServer_LspMetadataProvider_ResolveProjectTypeMemberDeclaration(&provider,
                                                                                 query->projectIndex,
                                                                                 query->resolvedMember.ownerTypeName,
                                                                                 query->resolvedMember.memberName,
                                                                                 &query->resolvedMember);
    }

    if (!query->resolvedMember.hasDeclaration &&
        (query->resolvedMember.module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN ||
         query->resolvedMember.module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN)) {
        ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
        ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(&provider,
                                                                                query->projectIndex,
                                                                                &query->resolvedMember);
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

static TZrBool semantic_query_resolve_import_binding_module_target(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   SZrLspProjectIndex *projectIndex,
                                                                   SZrSemanticAnalyzer *analyzer,
                                                                   SZrLspImportBinding *binding,
                                                                   SZrFileRange bindingRange,
                                                                   SZrLspSemanticQuery *query) {
    SZrLspMetadataProvider provider;
    SZrLspResolvedImportedModuleEntry moduleEntry;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || binding == ZR_NULL ||
        binding->moduleName == ZR_NULL || query == ZR_NULL) {
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

static TZrBool semantic_query_resolve_import_binding_target(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrLspProjectIndex *projectIndex,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrArray *bindings,
                                                            SZrFileRange queryRange,
                                                            SZrLspSemanticQuery *query) {
    SZrLspImportBinding *binding = ZR_NULL;
    SZrFileRange bindingRange;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || bindings == ZR_NULL || query == ZR_NULL ||
        (!semantic_query_find_import_binding_hit(bindings, queryRange, &binding, &bindingRange) &&
         !ZrLanguageServer_LspProject_FindImportBindingHit(analyzer->ast,
                                                          bindings,
                                                          queryRange,
                                                          &binding,
                                                          &bindingRange))) {
        return ZR_FALSE;
    }

    return semantic_query_resolve_import_binding_module_target(state,
                                                              context,
                                                              projectIndex,
                                                              analyzer,
                                                              binding,
                                                              bindingRange,
                                                              query);
}

static TZrBool semantic_query_symbol_matches_import_binding(SZrSymbol *symbol, SZrLspImportBinding *binding) {
    if (symbol == ZR_NULL || binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (symbol->name != ZR_NULL && binding->aliasName != ZR_NULL &&
        ZrLanguageServer_Lsp_StringsEqual(symbol->name, binding->aliasName)) {
        return ZR_TRUE;
    }

    return semantic_query_file_ranges_equal(symbol->selectionRange, binding->aliasLocation) ||
           semantic_query_file_ranges_equal(symbol->location, binding->aliasLocation);
}

static TZrBool semantic_query_resolve_import_alias_symbol_target(SZrState *state,
                                                                 SZrLspContext *context,
                                                                 SZrLspProjectIndex *projectIndex,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrSymbol *symbol,
                                                                 SZrLspSemanticQuery *query) {
    SZrArray bindings;
    TZrBool resolved = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL ||
        symbol == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    for (TZrSize index = 0; index < bindings.length && !resolved; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL ||
            !semantic_query_symbol_matches_import_binding(symbol, *bindingPtr)) {
            continue;
        }

        resolved = semantic_query_resolve_import_binding_module_target(state,
                                                                       context,
                                                                       projectIndex,
                                                                       analyzer,
                                                                       *bindingPtr,
                                                                       (*bindingPtr)->aliasLocation,
                                                                       query);
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return resolved;
}

static TZrBool semantic_query_resolve_import_alias_token_target(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrLspProjectIndex *projectIndex,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrString *uri,
                                                                SZrLspSemanticQuery *query) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    TZrSize cursorOffset;
    SZrString *name;
    SZrSymbol *symbol;
    SZrArray bindings;
    TZrBool resolved = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL ||
        uri == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }

    cursorOffset = semantic_query_lsp_offset_from_position(snapshot.content,
                                                           snapshot.contentLength,
                                                           query->position);
    if (!ZrLanguageServer_Lsp_IsOffsetInCodeSpan(snapshot.content,
                                                 snapshot.contentLength,
                                                 cursorOffset)) {
        goto cleanup;
    }
    name = semantic_query_extract_identifier_at_offset(state,
                                                       snapshot.content,
                                                       snapshot.contentLength,
                                                       cursorOffset);
    if (name == ZR_NULL) {
        goto cleanup;
    }
    symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, query->queryRange);

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    for (TZrSize index = 0; index < bindings.length && !resolved; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->aliasName == ZR_NULL ||
            (!ZrLanguageServer_Lsp_StringsEqual((*bindingPtr)->aliasName, name) &&
             !semantic_query_symbol_matches_import_binding(symbol, *bindingPtr))) {
            continue;
        }

        resolved = semantic_query_resolve_import_binding_module_target(state,
                                                                       context,
                                                                       projectIndex,
                                                                       analyzer,
                                                                       *bindingPtr,
                                                                       query->queryRange,
                                                                       query);
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
cleanup:
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return resolved;
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
    TZrBool resolved;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || query == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspSemanticQuery_Init(query);
    query->uri = uri;
    query->position = position;
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    query->queryRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);

    if (semantic_query_uri_is_binary_metadata_uri(uri) ||
        semantic_query_uri_is_native_plugin_metadata_uri(uri)) {
        query->projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
        query->analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
        resolved = semantic_query_resolve_external_metadata_target(
                state, context, uri, position, query);
        return resolved && semantic_query_capture_document_version(state, context, query);
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    query->projectIndex = projectIndex;
    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!semantic_query_try_get_analyzer_for_uri(state, context, uri, &analyzer) ||
        analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        return ZR_FALSE;
    }
    query->analyzer = analyzer;

    if (analyzer->ast != ZR_NULL) {
        if (semantic_query_resolve_receiver_type_member_target(state, context, uri, analyzer, query)) {
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return semantic_query_capture_document_version(state, context, query);
        }

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
            return semantic_query_capture_document_version(state, context, query);
        }
        if (semantic_query_resolve_import_binding_target(state,
                                                         context,
                                                         projectIndex,
                                                         analyzer,
                                                         &bindings,
                                                         query->queryRange,
                                                         query)) {
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return semantic_query_capture_document_version(state, context, query);
        }
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);

    if (analyzer->ast != ZR_NULL) {
        if (semantic_query_resolve_import_alias_token_target(state, context, projectIndex, analyzer, uri, query)) {
            return semantic_query_capture_document_version(state, context, query);
        }
    }

    if (semantic_query_resolve_external_metadata_type_member_declaration_target(state, context, uri, position, query)) {
        return semantic_query_capture_document_version(state, context, query);
    }

    if (semantic_query_resolve_external_metadata_target(state, context, uri, position, query)) {
        return semantic_query_capture_document_version(state, context, query);
    }

    {
        SZrParserSemanticSymbolQuery canonicalSymbol;
        SZrParserSemanticQueryFacts canonicalFacts;

        memset(&canonicalSymbol, 0, sizeof(canonicalSymbol));
        memset(&canonicalFacts, 0, sizeof(canonicalFacts));
        (void)ZrParser_SemanticQuery_FactsAt(
                analyzer->semanticContext,
                query->queryRange,
                ZR_NULL,
                &canonicalFacts);
        if (ZrParser_SemanticQuery_SymbolAt(
                    analyzer->semanticContext,
                    query->queryRange,
                    ZR_NULL,
                    &canonicalSymbol)) {
            query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL;
            query->hasCanonicalSymbol = ZR_TRUE;
            query->canonicalSymbol = canonicalSymbol;
            query->canonicalReferenceRange = canonicalSymbol.referenceRange;
            if (canonicalFacts.reference != ZR_NULL &&
                canonicalFacts.reference->isResolved) {
                query->canonicalReferenceRange = canonicalFacts.reference->range;
            }
            query->resolvedTypeInfo.origin = ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
            query->resolvedTypeInfo.valueKind =
                    canonicalSymbol.kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE
                        ? ZR_LSP_RESOLVED_VALUE_KIND_TYPE
                        : canonicalSymbol.kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION
                              ? ZR_LSP_RESOLVED_VALUE_KIND_CALLABLE
                              : ZR_LSP_RESOLVED_VALUE_KIND_SYMBOL;
            semantic_query_copy_canonical_symbol_type(
                    state,
                    analyzer->semanticContext,
                    &canonicalSymbol,
                    &query->resolvedTypeInfo);
            if (analyzer->ast != ZR_NULL) {
                query->symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(
                        analyzer, query->queryRange);
            }
            return semantic_query_capture_document_version(state, context, query);
        }
        if (canonicalFacts.reference != ZR_NULL &&
            canonicalFacts.reference->kind != ZR_SEMANTIC_REFERENCE_TYPE &&
            (!canonicalFacts.reference->isResolved ||
             canonicalFacts.reference->symbolId == ZR_SEMANTIC_ID_INVALID)) {
            return ZR_FALSE;
        }
    }

    if (analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_query_position_is_code_span(context, uri, position)) {
        return ZR_FALSE;
    }

    query->symbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, query->queryRange);
    if (query->symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    {
        SZrArray symbolBindings;
        SZrFileRange symbolRange = ZrLanguageServer_Lsp_GetSymbolLookupRange(query->symbol);

        ZrCore_Array_Init(state, &symbolBindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &symbolBindings);
        if (semantic_query_resolve_import_binding_target(state,
                                                         context,
                                                         projectIndex,
                                                         analyzer,
                                                         &symbolBindings,
                                                         symbolRange,
                                                         query) ||
            semantic_query_resolve_import_binding_target(state,
                                                         context,
                                                         projectIndex,
                                                         analyzer,
                                                         &symbolBindings,
                                                         query->symbol->selectionRange,
                                                         query) ||
            semantic_query_resolve_import_binding_target(state,
                                                         context,
                                                         projectIndex,
                                                         analyzer,
                                                         &symbolBindings,
                                                         query->symbol->location,
                                                         query)) {
            ZrLanguageServer_LspProject_FreeImportBindings(state, &symbolBindings);
            return semantic_query_capture_document_version(state, context, query);
        }
        ZrLanguageServer_LspProject_FreeImportBindings(state, &symbolBindings);
    }

    if (semantic_query_resolve_import_alias_symbol_target(state,
                                                          context,
                                                          projectIndex,
                                                          analyzer,
                                                          query->symbol,
                                                          query)) {
        return semantic_query_capture_document_version(state, context, query);
    }

    query->kind = ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL;
    query->resolvedTypeInfo.origin = ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
    query->resolvedTypeInfo.valueKind = ZR_LSP_RESOLVED_VALUE_KIND_SYMBOL;
    if (semantic_query_copy_canonical_type_at(
                state, analyzer->semanticContext, query->queryRange, &query->resolvedTypeInfo) &&
        (query->symbol->type == ZR_SYMBOL_CLASS || query->symbol->type == ZR_SYMBOL_STRUCT ||
         query->symbol->type == ZR_SYMBOL_INTERFACE || query->symbol->type == ZR_SYMBOL_ENUM)) {
        query->resolvedTypeInfo.valueKind = ZR_LSP_RESOLVED_VALUE_KIND_TYPE;
    }
    return semantic_query_capture_document_version(state, context, query);
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_BuildHover(SZrState *state,
                                                                            SZrLspContext *context,
                                                                            SZrLspSemanticQuery *query,
                                                                            SZrLspHover **result) {
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL ||
        !semantic_query_document_version_is_current(state, context, query)) {
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

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL &&
        query->hasCanonicalSymbol && query->analyzer != ZR_NULL &&
        query->analyzer->semanticContext != ZR_NULL) {
        TZrBool built = ZrLanguageServer_LspCanonicalHover_BuildSymbol(
                state,
                context,
                query->uri,
                query->analyzer->semanticContext,
                &query->canonicalSymbol,
                query->canonicalReferenceRange,
                query->analyzer->ast,
                query->symbol,
                result);
        return built;
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
    SZrSemanticAnalyzer *metadataAnalyzer;
    SZrSemanticAnalyzer *fallbackAnalyzer = ZR_NULL;
    SZrLspSemanticQuery semanticQuery;
    SZrLspMetadataProvider provider;
    SZrFileVersionContentSnapshot snapshot = {0};
    TZrBool hasSnapshot = ZR_FALSE;
    TZrBool receiverCompletionFailClosed = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    metadataAnalyzer = analyzer;

    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);

    ZrLanguageServer_LspSemanticQuery_Init(&semanticQuery);
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    hasSnapshot = ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot);
    if (hasSnapshot && snapshot.usesFallbackAst) {
        /*
         * Completion still needs the live cursor offset even when the parser fell back to the
         * previous AST. Receiver/import completion scans the current buffer text first, then
         * reuses the last analyzable semantic state; forcing offset 0 collapses the request into
         * generic file-scope completions.
         */
        filePos = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                         snapshot.content,
                                                                         snapshot.contentLength);
        fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    }
    if (hasSnapshot && analyzer->ast != ZR_NULL &&
        ZrLanguageServer_Lsp_ShouldFailClosedReceiverCompletion(analyzer,
                                                                 analyzer->ast,
                                                                 snapshot.content,
                                                                 snapshot.contentLength,
                                                                 filePos.offset)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
        return ZR_TRUE;
    }

    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(state, context, uri, position, &semanticQuery)) {
        hoveredSymbolName = semanticQuery.symbol != ZR_NULL ? semanticQuery.symbol->name : semanticQuery.memberName;
        resolvedTypeText = semanticQuery.resolvedTypeInfo.resolvedTypeText;
    }
    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (hasSnapshot && analyzer->ast != ZR_NULL) {
        hasStructuredCompletions = ZrLanguageServer_Lsp_TryCollectTokenPrefixCompletions(state,
                                                                                         snapshot.content,
                                                                                         snapshot.contentLength,
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
                                                                                        snapshot.content,
                                                                                        snapshot.contentLength,
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
                                                                                          snapshot.content,
                                                                                          snapshot.contentLength,
                                                                                          filePos.offset,
                                                                                          &completions,
                                                                                          &receiverCompletionFailClosed);
        }
    }

    if (!hasStructuredCompletions && !receiverCompletionFailClosed) {
        hasStructuredCompletions =
                ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols(
                        state,
                        analyzer->semanticContext,
                        fileRange,
                        &completions);
    }

    if (completions.length == 0 && !receiverCompletionFailClosed &&
        fileVersion != ZR_NULL &&
        hasSnapshot &&
        fileVersion->ast != ZR_NULL) {
        SZrAstNode *analysisRoot =
            ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
                fileVersion->ast,
                fileRange);
        fallbackAnalyzer =
            ZrLanguageServer_SemanticAnalyzer_GetOrCreateScopedQueryAnalyzer(
                state,
                analyzer);
        if (fallbackAnalyzer != ZR_NULL &&
            (analysisRoot != ZR_NULL
                 ? ZrLanguageServer_SemanticAnalyzer_AnalyzeScope(
                       state,
                       fallbackAnalyzer,
                       fileVersion->ast,
                       analysisRoot)
                 : ZrLanguageServer_SemanticAnalyzer_Analyze(
                       state,
                       fallbackAnalyzer,
                       fileVersion->ast))) {
            metadataAnalyzer = fallbackAnalyzer;
            hasStructuredCompletions = ZrLanguageServer_Lsp_TryCollectReceiverCompletions(state,
                                                                                          context,
                                                                                          ZR_NULL,
                                                                                          fallbackAnalyzer,
                                                                                          uri,
                                                                                          fileVersion->ast,
                                                                                          snapshot.content,
                                                                                           snapshot.contentLength,
                                                                                           filePos.offset,
                                                                                           &completions,
                                                                                           &receiverCompletionFailClosed);
            if (!hasStructuredCompletions && !receiverCompletionFailClosed) {
                hasStructuredCompletions =
                        ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols(
                                state,
                                fallbackAnalyzer->semanticContext,
                                fileRange,
                                &completions);
            }
        }
    }

    for (TZrSize index = 0; index < completions.length; index++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(&completions, index);
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL) {
            continue;
        }

        if (hasSnapshot) {
            ZrLanguageServer_Lsp_EnrichCompletionItemMetadata(state,
                                                              metadataAnalyzer,
                                                              *itemPtr,
                                                              hoveredSymbolName,
                                                              resolvedTypeText,
                                                              snapshot.content,
                                                              snapshot.contentLength);
        }
        ZrCore_Array_Push(state, result, itemPtr);
    }

    ZrCore_Array_Free(state, &completions);
    if (hasSnapshot) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &semanticQuery);
    ZrLanguageServer_LspSemanticCacheLru_Touch(context, analyzer);
    ZrLanguageServer_LspSemanticCacheLru_Enforce(state, context);
    return ZR_TRUE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDefinitions(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL ||
        !semantic_query_document_version_is_current(state, context, query)) {
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER) {
        if (query->resolvedMember.hasDeclaration && query->resolvedMember.declarationUri != ZR_NULL) {
            return semantic_query_append_location(state,
                                                  context,
                                                  result,
                                                  query->resolvedMember.declarationUri,
                                                  query->resolvedMember.declarationRange,
                                                  query->sourceKind);
        }
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION) {
        return query->resolvedMember.hasDeclaration &&
               query->resolvedMember.declarationUri != ZR_NULL &&
               semantic_query_append_location(state,
                                              context,
                                              result,
                                              query->resolvedMember.declarationUri,
                                              query->resolvedMember.declarationRange,
                                              query->sourceKind);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER) {
        return query->resolvedMember.hasDeclaration &&
               query->resolvedMember.declarationUri != ZR_NULL &&
               semantic_query_append_location(state,
                                              context,
                                              result,
                                              query->resolvedMember.declarationUri,
                                              query->resolvedMember.declarationRange,
                                              query->sourceKind);
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL) {
        return ZrLanguageServer_LspSemanticDefinitionQuery_AppendReachingDefinition(
                state, context, query, result);
    }

    return ZR_FALSE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendReferences(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    TZrBool includeDeclaration,
    SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL ||
        !semantic_query_document_version_is_current(state, context, query)) {
        return ZR_FALSE;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER) {
        TZrBool appended = ZR_FALSE;

        if (includeDeclaration &&
            query->resolvedMember.hasDeclaration &&
            query->resolvedMember.declarationUri != ZR_NULL &&
            !semantic_query_append_location(state,
                                            context,
                                            result,
                                            query->resolvedMember.declarationUri,
                                            query->resolvedMember.declarationRange,
                                            query->sourceKind)) {
            return ZR_FALSE;
        }

        if (query->projectIndex != ZR_NULL) {
            appended = semantic_query_append_project_imported_member_references(state,
                                                                                context,
                                                                                query->projectIndex,
                                                                                query->moduleName,
                                                                                query->memberName,
                                                                                result);
            if (query->resolvedMember.module.moduleName != ZR_NULL &&
                !ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.module.moduleName, query->moduleName)) {
                appended = semantic_query_append_project_imported_member_references(state,
                                                                                    context,
                                                                                    query->projectIndex,
                                                                                    query->resolvedMember.module.moduleName,
                                                                                    query->memberName,
                                                                                    result) ||
                           appended;
            }
        } else if (query->uri != ZR_NULL) {
            appended = semantic_query_append_imported_member_locations_for_uri(state,
                                                                               context,
                                                                               query->projectIndex,
                                                                               query->uri,
                                                                               query->moduleName,
                                                                               query->memberName,
                                                                               result);
            if (query->resolvedMember.module.moduleName != ZR_NULL &&
                !ZrLanguageServer_Lsp_StringsEqual(query->resolvedMember.module.moduleName, query->moduleName)) {
                appended = semantic_query_append_imported_member_locations_for_uri(state,
                                                                                   context,
                                                                                   query->projectIndex,
                                                                                   query->uri,
                                                                                   query->resolvedMember.module.moduleName,
                                                                                   query->memberName,
                                                                                   result) ||
                           appended;
            }
        }

        return appended || result->length > 0;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION) {
        SZrLspExternalMetadataDeclaration resolved;

        if (query->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN &&
            query->memberName != ZR_NULL) {
            if (includeDeclaration &&
                query->resolvedMember.hasDeclaration &&
                query->resolvedMember.declarationUri != ZR_NULL &&
                !semantic_query_append_location(state,
                                                context,
                                                result,
                                                query->resolvedMember.declarationUri,
                                                query->resolvedMember.declarationRange,
                                                query->sourceKind)) {
                return ZR_FALSE;
            }

            return semantic_query_append_project_external_type_member_references(state, context, query, result) ||
                   result->length > 0;
        }

        memset(&resolved, 0, sizeof(resolved));
        resolved.projectIndex = query->projectIndex;
        resolved.moduleName = query->moduleName;
        resolved.memberName = query->memberName;
        resolved.declarationUri = query->resolvedMember.declarationUri;
        resolved.declarationRange = query->resolvedMember.declarationRange;
        resolved.hasDeclaration = query->resolvedMember.hasDeclaration;
        resolved.sourceKind = query->sourceKind;
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
                                            context,
                                            result,
                                            query->resolvedMember.declarationUri,
                                            query->resolvedMember.declarationRange,
                                            query->sourceKind)) {
            return ZR_FALSE;
        }

        return semantic_query_append_project_external_type_member_references(state, context, query, result) ||
               result->length > 0;
    }

    if (query->kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL) {
        return ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences(
                state,
                context,
                query,
                includeDeclaration,
                result);
    }

    return ZR_FALSE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL || result == ZR_NULL ||
        !semantic_query_document_version_is_current(state, context, query)) {
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
        resolved.sourceKind = query->sourceKind;
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
        return ZrLanguageServer_LspSemanticReferenceQuery_AppendHighlights(
                state, context, query, result);
    }

    return ZR_FALSE;
}
