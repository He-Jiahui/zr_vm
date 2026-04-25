#include "lsp_editor_features_internal.h"
#include "lsp_virtual_documents.h"

#include <string.h>

static void lsp_document_links_free_locations(SZrState *state, SZrArray *locations) {
    if (state == ZR_NULL || locations == ZR_NULL) {
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

static const TZrChar *lsp_document_links_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static TZrBool lsp_document_links_append(SZrState *state,
                                         SZrArray *result,
                                         SZrLspRange range,
                                         SZrString *target) {
    SZrLspDocumentLink *link;

    if (state == ZR_NULL || result == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentLink *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    link = (SZrLspDocumentLink *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentLink));
    if (link == ZR_NULL) {
        return ZR_FALSE;
    }
    link->range = range;
    link->target = target;
    link->tooltip = lsp_editor_create_string(state, "Open Zr import target", strlen("Open Zr import target"));
    ZrCore_Array_Push(state, result, &link);
    return ZR_TRUE;
}

static TZrBool lsp_document_links_uri_ends_with(const TZrChar *uriText, const TZrChar *suffix) {
    TZrSize uriLength;
    TZrSize suffixLength;

    if (uriText == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    uriLength = strlen(uriText);
    suffixLength = strlen(suffix);
    return uriLength >= suffixLength &&
           memcmp(uriText + uriLength - suffixLength, suffix, suffixLength) == 0;
}

static TZrBool lsp_document_links_find_json_string_value(const TZrChar *content,
                                                        TZrSize contentLength,
                                                        const TZrChar *key,
                                                        TZrSize *outValueStart,
                                                        TZrSize *outValueEnd) {
    TZrSize keyLength;
    TZrSize cursor = 0;

    if (content == ZR_NULL || key == ZR_NULL || outValueStart == ZR_NULL || outValueEnd == ZR_NULL) {
        return ZR_FALSE;
    }

    keyLength = strlen(key);
    while (cursor + keyLength + 2 < contentLength) {
        TZrSize keyStart;
        TZrSize quoteOffset;
        TZrSize closeOffset;

        if (content[cursor] != '"') {
            cursor++;
            continue;
        }

        keyStart = cursor + 1;
        if (keyStart + keyLength >= contentLength ||
            memcmp(content + keyStart, key, keyLength) != 0 ||
            content[keyStart + keyLength] != '"') {
            cursor++;
            continue;
        }

        quoteOffset = keyStart + keyLength + 1;
        while (quoteOffset < contentLength && content[quoteOffset] != ':') {
            quoteOffset++;
        }
        if (quoteOffset >= contentLength) {
            return ZR_FALSE;
        }
        quoteOffset++;
        while (quoteOffset < contentLength &&
               (content[quoteOffset] == ' ' || content[quoteOffset] == '\t' ||
                content[quoteOffset] == '\r' || content[quoteOffset] == '\n')) {
            quoteOffset++;
        }
        if (quoteOffset >= contentLength || content[quoteOffset] != '"') {
            cursor = quoteOffset;
            continue;
        }

        closeOffset = quoteOffset + 1;
        while (closeOffset < contentLength) {
            if (content[closeOffset] == '"' && (closeOffset == quoteOffset + 1 || content[closeOffset - 1] != '\\')) {
                *outValueStart = quoteOffset + 1;
                *outValueEnd = closeOffset;
                return ZR_TRUE;
            }
            closeOffset++;
        }
        return ZR_FALSE;
    }

    return ZR_FALSE;
}

static TZrBool lsp_document_links_uri_append_escaped(TZrChar *buffer,
                                                     TZrSize bufferSize,
                                                     TZrSize *offset,
                                                     const TZrChar *text,
                                                     TZrSize textLength) {
    static const TZrChar hex[] = "0123456789ABCDEF";

    if (buffer == ZR_NULL || offset == ZR_NULL || text == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < textLength; index++) {
        TZrChar ch = text[index] == '\\' ? '/' : text[index];

        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '/' || ch == '.' || ch == '_' || ch == '-' || ch == '~' || ch == ':') {
            if (*offset + 1 >= bufferSize) {
                return ZR_FALSE;
            }
            buffer[(*offset)++] = ch;
        } else {
            if (*offset + 3 >= bufferSize) {
                return ZR_FALSE;
            }
            buffer[(*offset)++] = '%';
            buffer[(*offset)++] = hex[((unsigned char)ch >> 4) & 0x0F];
            buffer[(*offset)++] = hex[(unsigned char)ch & 0x0F];
        }
    }

    buffer[*offset] = '\0';
    return ZR_TRUE;
}

static TZrBool lsp_document_links_build_relative_uri(TZrChar *buffer,
                                                     TZrSize bufferSize,
                                                     const TZrChar *baseUri,
                                                     const TZrChar *pathText,
                                                     TZrSize pathLength) {
    TZrSize baseLength;
    TZrSize baseDirLength = 0;
    TZrSize offset;

    if (buffer == ZR_NULL || bufferSize == 0 || baseUri == ZR_NULL || pathText == ZR_NULL || pathLength == 0) {
        return ZR_FALSE;
    }

    if (pathLength >= 7 && memcmp(pathText, "file://", 7) == 0) {
        if (pathLength >= bufferSize) {
            return ZR_FALSE;
        }
        memcpy(buffer, pathText, pathLength);
        buffer[pathLength] = '\0';
        return ZR_TRUE;
    }

    baseLength = strlen(baseUri);
    for (TZrSize index = 0; index < baseLength; index++) {
        if (baseUri[index] == '/') {
            baseDirLength = index + 1;
        }
    }

    if (baseDirLength == 0 || baseDirLength >= bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer, baseUri, baseDirLength);
    offset = baseDirLength;
    return lsp_document_links_uri_append_escaped(buffer, bufferSize, &offset, pathText, pathLength);
}

static TZrBool lsp_document_links_append_zrp_path(SZrState *state,
                                                  SZrArray *result,
                                                  const TZrChar *content,
                                                  TZrSize contentLength,
                                                  const TZrChar *baseUri,
                                                  TZrSize valueStart,
                                                  TZrSize valueEnd,
                                                  const TZrChar *targetText,
                                                  TZrSize targetLength) {
    TZrChar targetBuffer[ZR_VM_PATH_LENGTH_MAX];
    SZrLspRange range;
    SZrString *target;

    if (!lsp_document_links_build_relative_uri(targetBuffer,
                                               sizeof(targetBuffer),
                                               baseUri,
                                               targetText,
                                               targetLength)) {
        return ZR_TRUE;
    }

    range = lsp_editor_range_from_offsets(content, contentLength, valueStart, valueEnd);
    target = lsp_editor_create_string(state, targetBuffer, strlen(targetBuffer));
    return lsp_document_links_append(state, result, range, target);
}

static TZrBool lsp_document_links_append_zrp_links(SZrState *state,
                                                   SZrArray *result,
                                                   SZrString *uri,
                                                   const TZrChar *content,
                                                   TZrSize contentLength) {
    const TZrChar *uriText = lsp_document_links_string_text(uri);
    TZrSize sourceStart = 0;
    TZrSize sourceEnd = 0;
    TZrSize binaryStart = 0;
    TZrSize binaryEnd = 0;
    TZrSize entryStart = 0;
    TZrSize entryEnd = 0;
    TZrSize dependencyStart = 0;
    TZrSize dependencyEnd = 0;
    TZrSize localStart = 0;
    TZrSize localEnd = 0;
    TZrBool hasSource;

    if (state == ZR_NULL || result == ZR_NULL || uriText == ZR_NULL ||
        !lsp_document_links_uri_ends_with(uriText, ".zrp")) {
        return ZR_TRUE;
    }

    hasSource =
        lsp_document_links_find_json_string_value(content, contentLength, "source", &sourceStart, &sourceEnd);
    if (hasSource &&
        !lsp_document_links_append_zrp_path(state,
                                            result,
                                            content,
                                            contentLength,
                                            uriText,
                                            sourceStart,
                                            sourceEnd,
                                            content + sourceStart,
                                            sourceEnd - sourceStart)) {
        return ZR_FALSE;
    }

    if (lsp_document_links_find_json_string_value(content, contentLength, "binary", &binaryStart, &binaryEnd) &&
        !lsp_document_links_append_zrp_path(state,
                                            result,
                                            content,
                                            contentLength,
                                            uriText,
                                            binaryStart,
                                            binaryEnd,
                                            content + binaryStart,
                                            binaryEnd - binaryStart)) {
        return ZR_FALSE;
    }

    if (lsp_document_links_find_json_string_value(content,
                                                  contentLength,
                                                  "dependency",
                                                  &dependencyStart,
                                                  &dependencyEnd) &&
        !lsp_document_links_append_zrp_path(state,
                                            result,
                                            content,
                                            contentLength,
                                            uriText,
                                            dependencyStart,
                                            dependencyEnd,
                                            content + dependencyStart,
                                            dependencyEnd - dependencyStart)) {
        return ZR_FALSE;
    }

    if (lsp_document_links_find_json_string_value(content, contentLength, "local", &localStart, &localEnd) &&
        !lsp_document_links_append_zrp_path(state,
                                            result,
                                            content,
                                            contentLength,
                                            uriText,
                                            localStart,
                                            localEnd,
                                            content + localStart,
                                            localEnd - localStart)) {
        return ZR_FALSE;
    }

    if (lsp_document_links_find_json_string_value(content, contentLength, "entry", &entryStart, &entryEnd)) {
        TZrChar entryPath[ZR_VM_PATH_LENGTH_MAX];
        TZrSize offset = 0;

        if (hasSource) {
            TZrSize sourceLength = sourceEnd - sourceStart;
            if (sourceLength + 1 < sizeof(entryPath)) {
                memcpy(entryPath, content + sourceStart, sourceLength);
                offset = sourceLength;
                if (offset > 0 && entryPath[offset - 1] != '/' && entryPath[offset - 1] != '\\') {
                    entryPath[offset++] = '/';
                }
            }
        }
        if (offset + (entryEnd - entryStart) + 4 < sizeof(entryPath)) {
            memcpy(entryPath + offset, content + entryStart, entryEnd - entryStart);
            offset += entryEnd - entryStart;
            if (offset < 3 ||
                entryPath[offset - 3] != '.' ||
                entryPath[offset - 2] != 'z' ||
                entryPath[offset - 1] != 'r') {
                entryPath[offset++] = '.';
                entryPath[offset++] = 'z';
                entryPath[offset++] = 'r';
            }
            entryPath[offset] = '\0';
            if (!lsp_document_links_append_zrp_path(state,
                                                    result,
                                                    content,
                                                    contentLength,
                                                    uriText,
                                                    entryStart,
                                                    entryEnd,
                                                    entryPath,
                                                    offset)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool lsp_document_links_append_virtual_module_links(SZrState *state,
                                                              SZrArray *result,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength) {
    static const TZrChar prefix[] = "pub module ";
    TZrSize cursor = 0;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL ||
        content == ZR_NULL || !ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri)) {
        return ZR_TRUE;
    }

    while (cursor < contentLength) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize trimStart;

        while (lineEnd < contentLength && content[lineEnd] != '\n') {
            lineEnd++;
        }

        trimStart = lineStart;
        while (trimStart < lineEnd && (content[trimStart] == ' ' || content[trimStart] == '\t')) {
            trimStart++;
        }

        if (lineEnd - trimStart > strlen(prefix) &&
            memcmp(content + trimStart, prefix, strlen(prefix)) == 0) {
            TZrSize nameStart = trimStart + strlen(prefix);
            TZrSize nameEnd = nameStart;
            TZrSize targetStart;
            TZrSize targetEnd;
            TZrChar targetBuffer[ZR_VM_PATH_LENGTH_MAX];
            SZrLspRange range;
            SZrString *target;

            while (nameEnd < lineEnd &&
                   content[nameEnd] != ':' &&
                   content[nameEnd] != ' ' &&
                   content[nameEnd] != '\t') {
                nameEnd++;
            }

            targetStart = nameEnd;
            while (targetStart < lineEnd && content[targetStart] != ':') {
                targetStart++;
            }
            if (targetStart >= lineEnd) {
                cursor = lineEnd < contentLength ? lineEnd + 1 : lineEnd;
                continue;
            }
            targetStart++;
            while (targetStart < lineEnd &&
                   (content[targetStart] == ' ' || content[targetStart] == '\t')) {
                targetStart++;
            }
            targetEnd = targetStart;
            while (targetEnd < lineEnd &&
                   content[targetEnd] != ';' &&
                   content[targetEnd] != ' ' &&
                   content[targetEnd] != '\t' &&
                   content[targetEnd] != '\r') {
                targetEnd++;
            }

            if (nameStart < nameEnd &&
                targetStart < targetEnd &&
                snprintf(targetBuffer,
                         sizeof(targetBuffer),
                         "zr-decompiled:/%.*s.zr",
                         (int)(targetEnd - targetStart),
                         content + targetStart) > 0) {
                range = lsp_editor_range_from_offsets(content, contentLength, nameStart, nameEnd);
                target = lsp_editor_create_string(state, targetBuffer, strlen(targetBuffer));
                if (!lsp_document_links_append(state, result, range, target)) {
                    return ZR_FALSE;
                }
            }
        }

        cursor = lineEnd < contentLength ? lineEnd + 1 : lineEnd;
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetDocumentLinks(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrString *virtualDocumentText = ZR_NULL;
    const TZrChar *content;
    TZrSize contentLength;
    TZrSize cursor = 0;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentLink *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        if (!ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri) ||
            !ZrLanguageServer_Lsp_GetNativeDeclarationDocument(state, context, uri, &virtualDocumentText) ||
            virtualDocumentText == ZR_NULL) {
            return ZR_FALSE;
        }
        content = lsp_document_links_string_text(virtualDocumentText);
        contentLength = content != ZR_NULL ? strlen(content) : 0;
        return lsp_document_links_append_virtual_module_links(state, result, uri, content, contentLength);
    }

    content = fileVersion->content;
    contentLength = fileVersion->contentLength;
    if (!lsp_document_links_append_zrp_links(state, result, uri, content, contentLength)) {
        return ZR_FALSE;
    }
    if (!lsp_document_links_append_virtual_module_links(state, result, uri, content, contentLength)) {
        return ZR_FALSE;
    }

    while (cursor + 8 < contentLength) {
        const TZrChar *match = strstr(content + cursor, "%import");
        TZrSize matchOffset;
        TZrSize quoteOffset;
        TZrSize closeOffset;
        SZrArray definitions = {0};
        SZrLspRange linkRange;
        SZrLspPosition queryPosition;

        if (match == ZR_NULL) {
            break;
        }
        matchOffset = (TZrSize)(match - content);
        quoteOffset = matchOffset;
        while (quoteOffset < contentLength && content[quoteOffset] != '"') {
            quoteOffset++;
        }
        if (quoteOffset >= contentLength) {
            cursor = matchOffset + 7;
            continue;
        }
        closeOffset = quoteOffset + 1;
        while (closeOffset < contentLength && content[closeOffset] != '"') {
            closeOffset++;
        }
        if (closeOffset >= contentLength) {
            cursor = quoteOffset + 1;
            continue;
        }

        linkRange = lsp_editor_range_from_offsets(content, contentLength, quoteOffset + 1, closeOffset);
        queryPosition = linkRange.start;
        if (ZrLanguageServer_Lsp_GetDefinition(state, context, uri, queryPosition, &definitions) &&
            definitions.length > 0) {
            SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(&definitions, 0);
            if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL && (*locationPtr)->uri != ZR_NULL) {
                if (!lsp_document_links_append(state, result, linkRange, (*locationPtr)->uri)) {
                    lsp_document_links_free_locations(state, &definitions);
                    return ZR_FALSE;
                }
            }
        }
        lsp_document_links_free_locations(state, &definitions);
        cursor = closeOffset + 1;
    }

    return ZR_TRUE;
}
