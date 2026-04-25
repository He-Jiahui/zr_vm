#include "zr_vm_language_server_stdio_internal.h"

static int inline_value_is_identifier_start(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

static int inline_value_is_identifier_part(char ch) {
    return inline_value_is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

static int inline_value_position_in_range(SZrLspRange range, TZrInt32 line, TZrInt32 character) {
    if (line < range.start.line || line > range.end.line) {
        return 0;
    }
    if (line == range.start.line && character < range.start.character) {
        return 0;
    }
    if (line == range.end.line && character > range.end.character) {
        return 0;
    }
    return 1;
}

static int inline_value_is_keyword_at(const char *content,
                                      size_t lineStart,
                                      size_t lineEnd,
                                      size_t offset,
                                      const char *keyword) {
    size_t keywordLength;

    if (content == NULL || keyword == NULL) {
        return 0;
    }

    keywordLength = strlen(keyword);
    if (offset + keywordLength > lineEnd) {
        return 0;
    }
    if (offset > lineStart && inline_value_is_identifier_part(content[offset - 1])) {
        return 0;
    }
    if (strncmp(content + offset, keyword, keywordLength) != 0) {
        return 0;
    }
    if (offset + keywordLength < lineEnd &&
        inline_value_is_identifier_part(content[offset + keywordLength])) {
        return 0;
    }
    return 1;
}

static cJSON *inline_value_create_variable_lookup(TZrInt32 line,
                                                  TZrInt32 startCharacter,
                                                  const char *nameStart,
                                                  size_t nameLength) {
    cJSON *json;
    SZrLspRange range;
    char *name;

    if (nameStart == NULL || nameLength == 0) {
        return NULL;
    }

    json = cJSON_CreateObject();
    name = duplicate_string_range(nameStart, nameLength);
    if (json == NULL || name == NULL) {
        cJSON_Delete(json);
        free(name);
        return NULL;
    }

    range.start.line = line;
    range.start.character = startCharacter;
    range.end.line = line;
    range.end.character = startCharacter + (TZrInt32)nameLength;
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(range));
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_VARIABLE_NAME, name);
    cJSON_AddBoolToObject(json, ZR_LSP_FIELD_CASE_SENSITIVE_LOOKUP, 1);

    free(name);
    return json;
}

static void inline_value_scan_line(const char *content,
                                   size_t lineStart,
                                   size_t lineEnd,
                                   TZrInt32 line,
                                   SZrLspRange requestRange,
                                   cJSON *result) {
    size_t offset = lineStart;
    TZrInt32 character = 0;
    size_t commentStart = lineEnd;

    for (size_t scan = lineStart; scan + 1 < lineEnd; scan++) {
        if (content[scan] == '/' && content[scan + 1] == '/') {
            commentStart = scan;
            break;
        }
    }

    while (offset < commentStart) {
        size_t keywordEnd;
        size_t nameStart;
        size_t nameEnd;
        TZrInt32 nameCharacter;
        cJSON *value;

        if (!inline_value_position_in_range(requestRange, line, character) ||
            !inline_value_is_keyword_at(content, lineStart, commentStart, offset, "var")) {
            offset++;
            character++;
            continue;
        }

        keywordEnd = offset + 3;
        if (keywordEnd >= commentStart || (content[keywordEnd] != ' ' && content[keywordEnd] != '\t')) {
            offset++;
            character++;
            continue;
        }

        nameStart = keywordEnd;
        while (nameStart < commentStart && (content[nameStart] == ' ' || content[nameStart] == '\t')) {
            nameStart++;
        }
        if (nameStart >= commentStart || !inline_value_is_identifier_start(content[nameStart])) {
            offset++;
            character++;
            continue;
        }

        nameCharacter = character + (TZrInt32)(nameStart - offset);
        nameEnd = nameStart + 1;
        while (nameEnd < commentStart && inline_value_is_identifier_part(content[nameEnd])) {
            nameEnd++;
        }

        if (inline_value_position_in_range(requestRange, line, nameCharacter)) {
            value = inline_value_create_variable_lookup(line,
                                                        nameCharacter,
                                                        content + nameStart,
                                                        nameEnd - nameStart);
            if (value != NULL) {
                cJSON_AddItemToArray(result, value);
            }
        }

        character += (TZrInt32)(nameEnd - offset);
        offset = nameEnd;
    }
}

cJSON *handle_inline_value_request(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;
    SZrLspRange requestRange;
    SZrFileVersion *fileVersion;
    const char *content;
    size_t contentLength;
    size_t lineStart = 0;
    TZrInt32 line = 0;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri) ||
        !parse_range(get_object_item(params, ZR_LSP_FIELD_RANGE), &requestRange)) {
        return cJSON_CreateArray();
    }
    ZR_UNUSED_PARAMETER(uriText);

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return cJSON_CreateArray();
    }

    result = cJSON_CreateArray();
    if (result == NULL) {
        return NULL;
    }

    content = fileVersion->content;
    contentLength = (size_t)fileVersion->contentLength;
    for (size_t offset = 0; offset <= contentLength; offset++) {
        if (offset == contentLength || content[offset] == '\n') {
            if (line >= requestRange.start.line && line <= requestRange.end.line) {
                inline_value_scan_line(content, lineStart, offset, line, requestRange, result);
            }
            line++;
            lineStart = offset + 1;
        }
    }

    return result;
}
