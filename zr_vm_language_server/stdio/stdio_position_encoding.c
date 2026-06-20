#include "zr_vm_language_server_stdio_internal.h"

static TZrBool position_encoding_is_utf8(const SZrStdioServer *server) {
    return server != ZR_NULL && server->positionEncoding == ZR_STDIO_POSITION_ENCODING_UTF8;
}

const char *position_encoding_name(const SZrStdioServer *server) {
    return position_encoding_is_utf8(server) ? ZR_LSP_POSITION_ENCODING_UTF8 : ZR_LSP_POSITION_ENCODING_UTF16;
}

static TZrSize utf8_codepoint_length(const char *content, size_t contentLength, size_t offset) {
    unsigned char first;

    if (content == NULL || offset >= contentLength) {
        return 0;
    }

    first = (unsigned char)content[offset];
    if (first < 0x80u) {
        return 1;
    }
    if ((first & 0xE0u) == 0xC0u && offset + 1 < contentLength) {
        return 2;
    }
    if ((first & 0xF0u) == 0xE0u && offset + 2 < contentLength) {
        return 3;
    }
    if ((first & 0xF8u) == 0xF0u && offset + 3 < contentLength) {
        return 4;
    }

    return 1;
}

static TZrInt32 utf16_units_for_utf8_codepoint(const char *content,
                                               size_t contentLength,
                                               size_t offset,
                                               TZrSize byteLength) {
    unsigned char first;
    TZrUInt32 codepoint;

    if (content == NULL || offset >= contentLength || byteLength == 0) {
        return 1;
    }

    first = (unsigned char)content[offset];
    if (byteLength == 4 && (first & 0xF8u) == 0xF0u) {
        codepoint = ((TZrUInt32)(first & 0x07u) << 18) |
                    ((TZrUInt32)((unsigned char)content[offset + 1] & 0x3Fu) << 12) |
                    ((TZrUInt32)((unsigned char)content[offset + 2] & 0x3Fu) << 6) |
                    (TZrUInt32)((unsigned char)content[offset + 3] & 0x3Fu);
        return codepoint >= 0x10000u ? 2 : 1;
    }

    return 1;
}

static TZrBool find_line_bounds(const char *content,
                                size_t contentLength,
                                TZrInt32 targetLine,
                                size_t *outLineStart,
                                size_t *outLineEnd) {
    TZrInt32 line = 0;
    size_t lineStart = 0;
    size_t index;

    if (content == NULL || targetLine < 0 || outLineStart == NULL || outLineEnd == NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < contentLength; index++) {
        if (line == targetLine && (content[index] == '\n' || content[index] == '\r')) {
            *outLineStart = lineStart;
            *outLineEnd = index;
            return ZR_TRUE;
        }

        if (content[index] == '\n') {
            if (line == targetLine) {
                *outLineStart = lineStart;
                *outLineEnd = index;
                return ZR_TRUE;
            }
            line++;
            lineStart = index + 1;
        }
    }

    if (line == targetLine) {
        *outLineStart = lineStart;
        *outLineEnd = contentLength;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrLspPosition utf8_position_to_utf16_position(const char *content,
                                                      size_t contentLength,
                                                      SZrLspPosition position) {
    SZrLspPosition converted = position;
    size_t lineStart;
    size_t lineEnd;
    size_t targetOffset;
    size_t offset;
    TZrInt32 character = 0;

    if (position.character < 0 ||
        !find_line_bounds(content, contentLength, position.line, &lineStart, &lineEnd)) {
        return converted;
    }

    targetOffset = lineStart + (size_t)position.character;
    if (targetOffset > lineEnd) {
        targetOffset = lineEnd;
    }

    offset = lineStart;
    while (offset < targetOffset) {
        TZrSize codepointLength = utf8_codepoint_length(content, contentLength, offset);

        if (codepointLength == 0 || offset + codepointLength > targetOffset) {
            break;
        }

        character += utf16_units_for_utf8_codepoint(content, contentLength, offset, codepointLength);
        offset += codepointLength;
    }

    converted.character = character;
    return converted;
}

static SZrLspPosition utf16_position_to_utf8_position(const char *content,
                                                      size_t contentLength,
                                                      SZrLspPosition position) {
    SZrLspPosition converted = position;
    size_t lineStart;
    size_t lineEnd;
    size_t offset;
    TZrInt32 character = 0;

    if (position.character < 0 ||
        !find_line_bounds(content, contentLength, position.line, &lineStart, &lineEnd)) {
        return converted;
    }

    offset = lineStart;
    while (offset < lineEnd && character < position.character) {
        TZrSize codepointLength = utf8_codepoint_length(content, contentLength, offset);
        TZrInt32 utf16Units;

        if (codepointLength == 0 || offset + codepointLength > lineEnd) {
            break;
        }

        utf16Units = utf16_units_for_utf8_codepoint(content, contentLength, offset, codepointLength);
        if (character + utf16Units > position.character) {
            break;
        }

        character += utf16Units;
        offset += codepointLength;
    }

    converted.character = (TZrInt32)(offset - lineStart);
    return converted;
}

static TZrBool content_snapshot_for_uri(SZrStdioServer *server,
                                        SZrString *uri,
                                        SZrFileVersionContentSnapshot *outSnapshot) {
    SZrFileVersion *fileVersion;

    if (server == ZR_NULL || uri == ZR_NULL || outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    return ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, outSnapshot);
}

static TZrBool content_snapshot_for_uri_text(SZrStdioServer *server,
                                             const char *uriText,
                                             SZrFileVersionContentSnapshot *outSnapshot) {
    SZrString *uri;

    if (server == ZR_NULL || uriText == NULL || outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }

    uri = server_get_cached_uri(server, uriText);
    return content_snapshot_for_uri(server, uri, outSnapshot);
}

static SZrLspPosition client_position_to_internal(SZrStdioServer *server,
                                                  const char *content,
                                                  size_t contentLength,
                                                  SZrLspPosition position) {
    if (!position_encoding_is_utf8(server) || content == NULL) {
        return position;
    }

    return utf8_position_to_utf16_position(content, contentLength, position);
}

static SZrLspPosition internal_position_to_client(SZrStdioServer *server,
                                                  const char *content,
                                                  size_t contentLength,
                                                  SZrLspPosition position) {
    if (!position_encoding_is_utf8(server) || content == NULL) {
        return position;
    }

    return utf16_position_to_utf8_position(content, contentLength, position);
}

static SZrLspRange client_range_to_internal(SZrStdioServer *server,
                                            const char *content,
                                            size_t contentLength,
                                            SZrLspRange range) {
    range.start = client_position_to_internal(server, content, contentLength, range.start);
    range.end = client_position_to_internal(server, content, contentLength, range.end);
    return range;
}

int parse_position_for_uri(SZrStdioServer *server,
                           SZrString *uri,
                           const cJSON *json,
                           SZrLspPosition *outPosition) {
    SZrFileVersionContentSnapshot snapshot = {0};

    if (!parse_position(json, outPosition)) {
        return 0;
    }

    if (content_snapshot_for_uri(server, uri, &snapshot)) {
        *outPosition = client_position_to_internal(server,
                                                   snapshot.content,
                                                   snapshot.contentLength,
                                                   *outPosition);
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    }
    return 1;
}

int parse_range_for_uri(SZrStdioServer *server,
                        SZrString *uri,
                        const cJSON *json,
                        SZrLspRange *outRange) {
    SZrFileVersionContentSnapshot snapshot = {0};

    if (!parse_range(json, outRange)) {
        return 0;
    }

    if (content_snapshot_for_uri(server, uri, &snapshot)) {
        *outRange = client_range_to_internal(server,
                                             snapshot.content,
                                             snapshot.contentLength,
                                             *outRange);
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    }
    return 1;
}

int parse_range_for_content(SZrStdioServer *server,
                            const char *content,
                            size_t contentLength,
                            const cJSON *json,
                            SZrLspRange *outRange) {
    if (!parse_range(json, outRange)) {
        return 0;
    }

    *outRange = client_range_to_internal(server, content, contentLength, *outRange);
    return 1;
}

void negotiate_position_encoding(SZrStdioServer *server, const cJSON *params) {
    const cJSON *capabilities;
    const cJSON *general;
    const cJSON *encodings;
    const cJSON *encoding;

    if (server == ZR_NULL) {
        return;
    }

    server->positionEncoding = ZR_STDIO_POSITION_ENCODING_UTF16;
    capabilities = get_object_item(params, ZR_LSP_FIELD_CAPABILITIES);
    general = get_object_item(capabilities, ZR_LSP_FIELD_GENERAL);
    encodings = get_object_item(general, ZR_LSP_FIELD_POSITION_ENCODINGS);
    if (!cJSON_IsArray((cJSON *)encodings)) {
        return;
    }

    cJSON_ArrayForEach(encoding, encodings) {
        if (cJSON_IsString((cJSON *)encoding) &&
            encoding->valuestring != NULL &&
            strcmp(encoding->valuestring, ZR_LSP_POSITION_ENCODING_UTF8) == 0) {
            server->positionEncoding = ZR_STDIO_POSITION_ENCODING_UTF8;
            return;
        }
    }
}

static const char *uri_text_from_params(const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *uriJson;

    if (params == NULL) {
        return NULL;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    uriJson = get_object_item(textDocument, ZR_LSP_FIELD_URI);
    if (cJSON_IsString((cJSON *)uriJson)) {
        return cJSON_GetStringValue((cJSON *)uriJson);
    }

    uriJson = get_object_item(params, ZR_LSP_FIELD_URI);
    if (cJSON_IsString((cJSON *)uriJson)) {
        return cJSON_GetStringValue((cJSON *)uriJson);
    }

    return NULL;
}

static void set_position_character(cJSON *positionJson, SZrLspPosition position) {
    cJSON *characterJson;

    if (!cJSON_IsObject(positionJson)) {
        return;
    }

    characterJson = cJSON_GetObjectItemCaseSensitive(positionJson, ZR_LSP_FIELD_CHARACTER);
    if (cJSON_IsNumber(characterJson)) {
        cJSON_SetNumberValue(characterJson, position.character);
    }
}

static TZrBool read_position_object(cJSON *positionJson, SZrLspPosition *outPosition) {
    cJSON *lineJson;
    cJSON *characterJson;

    if (!cJSON_IsObject(positionJson) || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    lineJson = cJSON_GetObjectItemCaseSensitive(positionJson, ZR_LSP_FIELD_LINE);
    characterJson = cJSON_GetObjectItemCaseSensitive(positionJson, ZR_LSP_FIELD_CHARACTER);
    if (!cJSON_IsNumber(lineJson) || !cJSON_IsNumber(characterJson)) {
        return ZR_FALSE;
    }

    outPosition->line = (TZrInt32)lineJson->valuedouble;
    outPosition->character = (TZrInt32)characterJson->valuedouble;
    return ZR_TRUE;
}

static void encode_position_object_for_content(SZrStdioServer *server,
                                               cJSON *positionJson,
                                               const char *content,
                                               size_t contentLength) {
    SZrLspPosition position;

    if (!read_position_object(positionJson, &position)) {
        return;
    }

    position = internal_position_to_client(server, content, contentLength, position);
    set_position_character(positionJson, position);
}

static void encode_position_object(SZrStdioServer *server, cJSON *positionJson, const char *uriText) {
    SZrFileVersionContentSnapshot snapshot = {0};

    if (content_snapshot_for_uri_text(server, uriText, &snapshot)) {
        encode_position_object_for_content(server,
                                           positionJson,
                                           snapshot.content,
                                           snapshot.contentLength);
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    }
}

static TZrBool object_is_range(cJSON *json) {
    return cJSON_IsObject(json) &&
           cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_START)) &&
           cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_END));
}

static void encode_range_object(SZrStdioServer *server, cJSON *rangeJson, const char *uriText) {
    cJSON *startJson;
    cJSON *endJson;
    SZrFileVersionContentSnapshot snapshot = {0};

    if (!object_is_range(rangeJson)) {
        return;
    }
    if (!content_snapshot_for_uri_text(server, uriText, &snapshot)) {
        return;
    }

    startJson = cJSON_GetObjectItemCaseSensitive(rangeJson, ZR_LSP_FIELD_START);
    endJson = cJSON_GetObjectItemCaseSensitive(rangeJson, ZR_LSP_FIELD_END);
    encode_position_object_for_content(server, startJson, snapshot.content, snapshot.contentLength);
    encode_position_object_for_content(server, endJson, snapshot.content, snapshot.contentLength);
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
}

static void encode_folding_range_fields(SZrStdioServer *server, cJSON *json, const char *uriText) {
    cJSON *startLineJson;
    cJSON *startCharacterJson;
    cJSON *endLineJson;
    cJSON *endCharacterJson;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrLspPosition start;
    SZrLspPosition end;

    startLineJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_START_LINE);
    startCharacterJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_START_CHARACTER);
    endLineJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_END_LINE);
    endCharacterJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_END_CHARACTER);
    if (!cJSON_IsNumber(startLineJson) ||
        !cJSON_IsNumber(startCharacterJson) ||
        !cJSON_IsNumber(endLineJson) ||
        !cJSON_IsNumber(endCharacterJson)) {
        return;
    }

    if (!content_snapshot_for_uri_text(server, uriText, &snapshot)) {
        return;
    }

    start.line = (TZrInt32)startLineJson->valuedouble;
    start.character = (TZrInt32)startCharacterJson->valuedouble;
    end.line = (TZrInt32)endLineJson->valuedouble;
    end.character = (TZrInt32)endCharacterJson->valuedouble;
    start = internal_position_to_client(server, snapshot.content, snapshot.contentLength, start);
    end = internal_position_to_client(server, snapshot.content, snapshot.contentLength, end);
    cJSON_SetNumberValue(startCharacterJson, start.character);
    cJSON_SetNumberValue(endCharacterJson, end.character);
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
}

static void apply_encoding_to_json_node(SZrStdioServer *server, cJSON *json, const char *currentUriText) {
    cJSON *child;
    const cJSON *uriJson;
    const char *localUriText = currentUriText;

    if (!position_encoding_is_utf8(server) || json == NULL) {
        return;
    }

    if (cJSON_IsArray(json)) {
        cJSON_ArrayForEach(child, json) {
            apply_encoding_to_json_node(server, child, currentUriText);
        }
        return;
    }

    if (!cJSON_IsObject(json)) {
        return;
    }

    uriJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_URI);
    if (cJSON_IsString((cJSON *)uriJson) && uriJson->valuestring != NULL) {
        localUriText = uriJson->valuestring;
    }

    if (object_is_range(json)) {
        encode_range_object(server, json, localUriText);
        return;
    }

    {
        SZrLspPosition ignoredPosition;
        if (read_position_object(json, &ignoredPosition)) {
            encode_position_object(server, json, localUriText);
            return;
        }
    }

    encode_folding_range_fields(server, json, localUriText);
    cJSON_ArrayForEach(child, json) {
        if (child->string != NULL &&
            (strcmp(child->string, ZR_LSP_FIELD_DATA) == 0 ||
             strcmp(child->string, ZR_LSP_FIELD_RANGE) == 0 ||
             strcmp(child->string, ZR_LSP_FIELD_POSITION) == 0)) {
            if (strcmp(child->string, ZR_LSP_FIELD_RANGE) == 0) {
                encode_range_object(server, child, localUriText);
            } else if (strcmp(child->string, ZR_LSP_FIELD_POSITION) == 0) {
                encode_position_object(server, child, localUriText);
            }
            continue;
        }

        apply_encoding_to_json_node(server, child, localUriText);
    }
}

static TZrBool result_method_uses_client_owned_positions(const char *method) {
    return method != NULL &&
           (strcmp(method, ZR_LSP_METHOD_INLAY_HINT_RESOLVE) == 0 ||
            strcmp(method, ZR_LSP_METHOD_CODE_ACTION_RESOLVE) == 0 ||
            strcmp(method, ZR_LSP_METHOD_DOCUMENT_LINK_RESOLVE) == 0 ||
            strcmp(method, ZR_LSP_METHOD_CODE_LENS_RESOLVE) == 0 ||
            strcmp(method, ZR_LSP_METHOD_WORKSPACE_SYMBOL_RESOLVE) == 0);
}

void apply_position_encoding_to_response(SZrStdioServer *server,
                                         const char *method,
                                         const cJSON *requestParams,
                                         cJSON *response) {
    if (result_method_uses_client_owned_positions(method)) {
        return;
    }

    apply_encoding_to_json_node(server, response, uri_text_from_params(requestParams));
}

void apply_position_encoding_to_json_for_uri(SZrStdioServer *server,
                                             const char *uriText,
                                             cJSON *json) {
    apply_encoding_to_json_node(server, json, uriText);
}
