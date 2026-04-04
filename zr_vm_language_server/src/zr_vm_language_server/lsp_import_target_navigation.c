#include "lsp_module_metadata.h"
#include "lsp_project_internal.h"

#include "zr_vm_core/memory.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static TZrBool import_target_append_location(SZrState *state,
                                             SZrArray *result,
                                             SZrString *uri,
                                             SZrFileRange range) {
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

static TZrBool import_target_create_hover(SZrState *state,
                                          const TZrChar *markdown,
                                          SZrFileRange range,
                                          SZrLspHover **result) {
    SZrLspHover *hover;
    SZrString *content;

    if (state == ZR_NULL || markdown == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    content = ZrCore_String_Create(state, (TZrNativeString)markdown, strlen(markdown));
    if (hover == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_LspRange_FromFileRange(range);
    *result = hover;
    return ZR_TRUE;
}

static TZrBool import_target_normalize_module_key(const TZrChar *modulePath,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize) {
    TZrSize length;
    TZrSize writeIndex = 0;

    if (modulePath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    length = strlen(modulePath);
    while (length > 0 && (modulePath[length - 1] == '/' || modulePath[length - 1] == '\\')) {
        length--;
    }

    if (length >= 4 && memcmp(modulePath + length - 4, ".zro", 4) == 0) {
        length -= 4;
    } else if (length >= 4 && memcmp(modulePath + length - 4, ".zri", 4) == 0) {
        length -= 4;
    } else if (length >= 3 && memcmp(modulePath + length - 3, ".zr", 3) == 0) {
        length -= 3;
    }

    while (length > 0 && (*modulePath == '/' || *modulePath == '\\')) {
        modulePath++;
        length--;
    }

    if (length == 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < length && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = modulePath[index];
        buffer[writeIndex++] = current == '\\' ? '/' : current;
    }

    buffer[writeIndex] = '\0';
    return writeIndex > 0;
}

static SZrFilePosition import_target_file_position_from_offset(const TZrChar *content,
                                                               TZrSize contentLength,
                                                               TZrSize offset) {
    TZrInt32 line = 1;
    TZrInt32 column = 1;

    if (content == ZR_NULL) {
        return ZrParser_FilePosition_Create(offset, line, column);
    }

    if (offset > contentLength) {
        offset = contentLength;
    }

    for (TZrSize index = 0; index < offset; index++) {
        if (content[index] == '\n') {
            line++;
            column = 1;
        } else if (content[index] != '\r') {
            column++;
        }
    }

    return ZrParser_FilePosition_Create(offset, line, column);
}

static TZrBool import_target_has_import_prefix(const TZrChar *content, TZrSize quoteOffset) {
    TZrSize probe = quoteOffset;
    TZrSize start;

    if (content == ZR_NULL || quoteOffset == 0) {
        return ZR_FALSE;
    }

    while (probe > 0 && isspace((unsigned char)content[probe - 1])) {
        probe--;
    }
    if (probe == 0 || content[probe - 1] != '(') {
        return ZR_FALSE;
    }

    probe--;
    while (probe > 0 && isspace((unsigned char)content[probe - 1])) {
        probe--;
    }

    start = probe;
    while (start > 0 &&
           (isalpha((unsigned char)content[start - 1]) || content[start - 1] == '%')) {
        start--;
    }

    return probe - start == 7 && memcmp(content + start, "%import", 7) == 0;
}

static TZrBool import_target_find_literal_at_position(SZrState *state,
                                                      const TZrChar *content,
                                                      TZrSize contentLength,
                                                      SZrString *uri,
                                                      SZrFileRange position,
                                                      SZrString **outModuleName,
                                                      SZrFileRange *outLocation) {
    TZrSize cursorOffset;
    TZrSize startQuote;
    TZrSize endQuote;
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize moduleLength;

    if (state == ZR_NULL || content == ZR_NULL || uri == ZR_NULL || outModuleName == ZR_NULL ||
        outLocation == ZR_NULL || position.start.offset > contentLength) {
        return ZR_FALSE;
    }

    *outModuleName = ZR_NULL;
    memset(outLocation, 0, sizeof(*outLocation));

    if (contentLength == 0) {
        return ZR_FALSE;
    }

    cursorOffset = position.start.offset < contentLength ? position.start.offset : contentLength - 1;
    startQuote = cursorOffset;
    while (1) {
        TZrChar current = content[startQuote];
        if (current == '"') {
            break;
        }
        if (current == '\n' || current == '\r') {
            return ZR_FALSE;
        }
        if (startQuote == 0) {
            return ZR_FALSE;
        }
        startQuote--;
    }

    endQuote = startQuote + 1;
    while (endQuote < contentLength && content[endQuote] != '"' &&
           content[endQuote] != '\n' && content[endQuote] != '\r') {
        endQuote++;
    }
    if (endQuote >= contentLength || content[endQuote] != '"' || cursorOffset <= startQuote || cursorOffset >= endQuote ||
        !import_target_has_import_prefix(content, startQuote)) {
        return ZR_FALSE;
    }

    moduleLength = endQuote - startQuote - 1;
    if (moduleLength == 0 || moduleLength >= sizeof(normalizedModule)) {
        return ZR_FALSE;
    }

    memcpy(normalizedModule, content + startQuote + 1, moduleLength);
    normalizedModule[moduleLength] = '\0';
    if (!import_target_normalize_module_key(normalizedModule, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    *outModuleName = ZrCore_String_Create(state, normalizedModule, strlen(normalizedModule));
    if (*outModuleName == ZR_NULL) {
        return ZR_FALSE;
    }

    *outLocation = ZrParser_FileRange_Create(import_target_file_position_from_offset(content,
                                                                                      contentLength,
                                                                                      startQuote + 1),
                                             import_target_file_position_from_offset(content,
                                                                                      contentLength,
                                                                                      endQuote),
                                             uri);
    return ZR_TRUE;
}

static const TZrChar *import_target_describe_source_kind(SZrState *state,
                                                         SZrLspProjectIndex *projectIndex,
                                                         SZrString *moduleName) {
    SZrLspResolvedImportedModule resolved;

    ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                             ZR_NULL,
                                                             projectIndex,
                                                             moduleName,
                                                             &resolved);

    return ZrLanguageServer_LspModuleMetadata_SourceKindLabel(resolved.sourceKind);
}

static SZrFileRange import_target_module_entry_range(SZrString *uri) {
    SZrFilePosition start = ZrParser_FilePosition_Create(0, 1, 1);
    return ZrParser_FileRange_Create(start, start, uri);
}

TZrBool ZrLanguageServer_Lsp_TryGetImportTargetDefinition(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri,
                                                          SZrLspPosition position,
                                                          SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrString *moduleName;
    SZrFileRange bindingRange;
    SZrLspProjectIndex *projectIndex;
    SZrLspResolvedImportedModule resolved;
    SZrString *binaryUri = ZR_NULL;
    SZrString *nativeUri = ZR_NULL;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL ||
        !import_target_find_literal_at_position(state,
                                               fileVersion->content,
                                               fileVersion->contentLength,
                                               uri,
                                               fileRange,
                                               &moduleName,
                                               &bindingRange) ||
        moduleName == ZR_NULL || projectIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                             ZR_NULL,
                                                             projectIndex,
                                                             moduleName,
                                                             &resolved);
    if (resolved.sourceRecord != ZR_NULL && resolved.sourceRecord->uri != ZR_NULL) {
        return import_target_append_location(state,
                                             result,
                                             resolved.sourceRecord->uri,
                                             import_target_module_entry_range(resolved.sourceRecord->uri));
    }

    if (resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
        ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(state, projectIndex, moduleName, &binaryUri) &&
        binaryUri != ZR_NULL) {
        return import_target_append_location(state,
                                             result,
                                             binaryUri,
                                             import_target_module_entry_range(binaryUri));
    }

    if (ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(state, projectIndex, moduleName, &nativeUri) &&
        nativeUri != ZR_NULL) {
        return import_target_append_location(state,
                                             result,
                                             nativeUri,
                                             import_target_module_entry_range(nativeUri));
    }

    return ZR_FALSE;
}

TZrBool ZrLanguageServer_Lsp_TryGetImportTargetHover(SZrState *state,
                                                     SZrLspContext *context,
                                                     SZrString *uri,
                                                     SZrLspPosition position,
                                                     SZrLspHover **result) {
    SZrFileVersion *fileVersion;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrString *moduleName;
    SZrFileRange bindingRange;
    SZrLspProjectIndex *projectIndex;
    const TZrChar *moduleText;
    const TZrChar *sourceKind;
    TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL ||
        !import_target_find_literal_at_position(state,
                                               fileVersion->content,
                                               fileVersion->contentLength,
                                               uri,
                                               fileRange,
                                               &moduleName,
                                               &bindingRange) ||
        moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleText = moduleName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                     ? ZrCore_String_GetNativeStringShort(moduleName)
                     : ZrCore_String_GetNativeString(moduleName);
    sourceKind = import_target_describe_source_kind(state, projectIndex, moduleName);

    snprintf(buffer,
             sizeof(buffer),
             "module <%s>\n\nSource: %s",
             moduleText != ZR_NULL ? moduleText : "",
             sourceKind);
    return import_target_create_hover(state, buffer, bindingRange, result);
}
