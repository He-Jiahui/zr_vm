#include "zr_vm_language_server_stdio_internal.h"

static int linked_editing_location_matches_uri(const SZrLspLocation *location, const char *uriText) {
    char *locationUriText;
    int matches;

    if (location == ZR_NULL || location->uri == ZR_NULL || uriText == NULL) {
        return 0;
    }

    locationUriText = zr_string_to_c_string(location->uri);
    matches = locationUriText != NULL && strcmp(locationUriText, uriText) == 0;
    free(locationUriText);
    return matches;
}

static int linked_editing_range_is_non_empty(SZrLspRange range) {
    return range.start.line < range.end.line ||
           (range.start.line == range.end.line && range.start.character < range.end.character);
}

static int linked_editing_range_equals(SZrLspRange left, SZrLspRange right) {
    return left.start.line == right.start.line &&
           left.start.character == right.start.character &&
           left.end.line == right.end.line &&
           left.end.character == right.end.character;
}

static int linked_editing_ranges_contains(SZrLspRange *ranges, int rangeCount, SZrLspRange range) {
    for (int index = 0; index < rangeCount; index++) {
        if (linked_editing_range_equals(ranges[index], range)) {
            return 1;
        }
    }

    return 0;
}

static int linked_editing_is_identifier_start(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

static int linked_editing_is_identifier_part(char ch) {
    return linked_editing_is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

static int linked_editing_offset_is_in_code(const char *content,
                                            size_t contentLength,
                                            size_t targetOffset) {
    int inLineComment = 0;
    int inBlockComment = 0;
    int inString = 0;
    int escaped = 0;

    if (content == NULL || targetOffset >= contentLength) {
        return 0;
    }

    for (size_t offset = 0; offset <= targetOffset && offset < contentLength; offset++) {
        char ch = content[offset];
        char next = offset + 1 < contentLength ? content[offset + 1] : '\0';

        if (inLineComment) {
            if (offset == targetOffset) {
                return 0;
            }
            if (ch == '\n' || ch == '\r') {
                inLineComment = 0;
            }
            continue;
        }

        if (inBlockComment) {
            if (offset == targetOffset) {
                return 0;
            }
            if (ch == '*' && next == '/') {
                if (offset + 1 >= targetOffset) {
                    return 0;
                }
                inBlockComment = 0;
                offset++;
            }
            continue;
        }

        if (inString) {
            if (offset == targetOffset) {
                return 0;
            }
            if (escaped) {
                escaped = 0;
                continue;
            }
            if (ch == '\\') {
                escaped = 1;
                continue;
            }
            if (ch == '"') {
                inString = 0;
            }
            continue;
        }

        if (offset == targetOffset) {
            return 1;
        }
        if (ch == '/' && next == '/') {
            if (offset + 1 >= targetOffset) {
                return 0;
            }
            inLineComment = 1;
            offset++;
            continue;
        }
        if (ch == '/' && next == '*') {
            if (offset + 1 >= targetOffset) {
                return 0;
            }
            inBlockComment = 1;
            offset++;
            continue;
        }
        if (ch == '"') {
            inString = 1;
        }
    }

    return 0;
}

static int linked_editing_offset_from_position(const char *content,
                                               size_t contentLength,
                                               SZrLspPosition position,
                                               size_t *outOffset) {
    TZrInt32 line = 0;
    TZrInt32 character = 0;

    if (content == NULL || outOffset == NULL || position.line < 0 || position.character < 0) {
        return 0;
    }

    for (size_t offset = 0; offset < contentLength; offset++) {
        if (line == position.line && character == position.character) {
            *outOffset = offset;
            return 1;
        }
        if (content[offset] == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
    }

    if (line == position.line && character == position.character) {
        *outOffset = contentLength;
        return 1;
    }

    return 0;
}

static SZrLspPosition linked_editing_position_from_offset(const char *content,
                                                          size_t contentLength,
                                                          size_t targetOffset) {
    SZrLspPosition position;
    position.line = 0;
    position.character = 0;

    if (content == NULL) {
        return position;
    }

    for (size_t offset = 0; offset < contentLength && offset < targetOffset; offset++) {
        if (content[offset] == '\n') {
            position.line++;
            position.character = 0;
        } else {
            position.character++;
        }
    }

    return position;
}

static cJSON *linked_editing_ranges_from_document(SZrStdioServer *server,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  int *outRangeCount) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const char *content;
    size_t contentLength;
    size_t offset;
    size_t wordStart;
    size_t wordEnd;
    size_t wordLength;
    cJSON *ranges;

    if (outRangeCount != NULL) {
        *outRangeCount = 0;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        return cJSON_CreateArray();
    }

    content = snapshot.content;
    contentLength = snapshot.contentLength;
    if (!linked_editing_offset_from_position(content, contentLength, position, &offset)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return cJSON_CreateArray();
    }
    if (offset >= contentLength || !linked_editing_is_identifier_part(content[offset])) {
        if (offset == 0 || !linked_editing_is_identifier_part(content[offset - 1])) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
            return cJSON_CreateArray();
        }
        offset--;
    }
    if (!linked_editing_offset_is_in_code(content, contentLength, offset)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return cJSON_CreateArray();
    }

    wordStart = offset;
    while (wordStart > 0 && linked_editing_is_identifier_part(content[wordStart - 1])) {
        wordStart--;
    }
    wordEnd = offset + 1;
    while (wordEnd < contentLength && linked_editing_is_identifier_part(content[wordEnd])) {
        wordEnd++;
    }

    wordLength = wordEnd - wordStart;
    if (wordLength == 0 || !linked_editing_is_identifier_start(content[wordStart])) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return cJSON_CreateArray();
    }

    ranges = cJSON_CreateArray();
    if (ranges == NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return NULL;
    }

    for (size_t cursor = 0; cursor + wordLength <= contentLength; cursor++) {
        int beforeOk = cursor == 0 || !linked_editing_is_identifier_part(content[cursor - 1]);
        int afterOk = cursor + wordLength >= contentLength ||
                      !linked_editing_is_identifier_part(content[cursor + wordLength]);
        if (beforeOk &&
            afterOk &&
            linked_editing_offset_is_in_code(content, contentLength, cursor) &&
            memcmp(content + cursor, content + wordStart, wordLength) == 0) {
            SZrLspRange range;
            range.start = linked_editing_position_from_offset(content, contentLength, cursor);
            range.end = linked_editing_position_from_offset(content, contentLength, cursor + wordLength);
            cJSON_AddItemToArray(ranges, serialize_range(range));
            if (outRangeCount != NULL) {
                (*outRangeCount)++;
            }
        }
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    return ranges;
}

cJSON *handle_linked_editing_range_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;
    cJSON *ranges;
    SZrLspRange semanticRanges[ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY];
    int rangeCount = 0;
    TZrBool hasSemanticReferences;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return cJSON_CreateNull();
    }

    hasSemanticReferences =
        ZrLanguageServer_Lsp_FindReferences(server->state, server->context, uri, position, ZR_TRUE, &locations);

    result = cJSON_CreateObject();
    ranges = cJSON_CreateArray();
    if (result == NULL || ranges == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(ranges);
        if (hasSemanticReferences) {
            free_locations_array(server->state, &locations);
        }
        return cJSON_CreateNull();
    }

    if (hasSemanticReferences) {
        for (TZrSize index = 0; index < locations.length; index++) {
            SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(&locations, index);
            if (locationPtr != ZR_NULL &&
                *locationPtr != ZR_NULL &&
                linked_editing_location_matches_uri(*locationPtr, uriText)) {
                SZrLspRange range = (*locationPtr)->range;
                if (linked_editing_range_is_non_empty(range) &&
                    rangeCount < (int)ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY &&
                    !linked_editing_ranges_contains(semanticRanges, rangeCount, range)) {
                    semanticRanges[rangeCount] = range;
                    cJSON_AddItemToArray(ranges, serialize_range(range));
                    rangeCount++;
                }
            }
        }
    }

    if (rangeCount < 2) {
        cJSON_Delete(ranges);
        ranges = linked_editing_ranges_from_document(server, uri, position, &rangeCount);
    }

    if (hasSemanticReferences) {
        free_locations_array(server->state, &locations);
    }
    if (ranges == NULL || rangeCount < 2) {
        cJSON_Delete(result);
        cJSON_Delete(ranges);
        return cJSON_CreateNull();
    }

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_RANGES, ranges);
    cJSON_AddStringToObject(result, ZR_LSP_FIELD_WORD_PATTERN, "[A-Za-z_][A-Za-z0-9_]*");
    return result;
}
