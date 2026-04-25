#include "zr_vm_language_server_stdio_internal.h"

typedef struct SZrInlineCompletionKeyword {
    const char *prefix;
    const char *insertText;
} SZrInlineCompletionKeyword;

static const SZrInlineCompletionKeyword ZR_INLINE_COMPLETION_KEYWORDS[] = {
    {"ret", "return "},
    {"fun", "func "},
    {"cla", "class "},
    {"pub", "pub "},
    {"pri", "pri "},
    {"sta", "static "},
    {"var", "var "},
};

static int inline_completion_offset_from_position(const char *content,
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

static int inline_completion_is_identifier_part(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
}

static cJSON *inline_completion_create_item(SZrLspPosition position,
                                            TZrInt32 prefixLength,
                                            const char *insertText) {
    cJSON *item;
    SZrLspRange range;

    if (insertText == NULL || prefixLength <= 0) {
        return NULL;
    }

    item = cJSON_CreateObject();
    if (item == NULL) {
        return NULL;
    }

    range.start.line = position.line;
    range.start.character = position.character - prefixLength;
    range.end = position;
    cJSON_AddStringToObject(item, ZR_LSP_FIELD_INSERT_TEXT, insertText);
    cJSON_AddItemToObject(item, ZR_LSP_FIELD_RANGE, serialize_range(range));
    return item;
}

cJSON *handle_inline_completion_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    const char *content;
    size_t contentLength;
    size_t offset;
    size_t prefixStart;
    size_t prefixLength;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return cJSON_CreateArray();
    }
    ZR_UNUSED_PARAMETER(uriText);

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return cJSON_CreateArray();
    }

    content = fileVersion->content;
    contentLength = (size_t)fileVersion->contentLength;
    if (!inline_completion_offset_from_position(content, contentLength, position, &offset)) {
        return cJSON_CreateArray();
    }

    prefixStart = offset;
    while (prefixStart > 0 && inline_completion_is_identifier_part(content[prefixStart - 1])) {
        prefixStart--;
    }
    prefixLength = offset - prefixStart;
    if (prefixLength == 0 || prefixLength > 16) {
        return cJSON_CreateArray();
    }

    result = cJSON_CreateArray();
    if (result == NULL) {
        return NULL;
    }

    for (size_t index = 0;
         index < sizeof(ZR_INLINE_COMPLETION_KEYWORDS) / sizeof(ZR_INLINE_COMPLETION_KEYWORDS[0]);
         index++) {
        const SZrInlineCompletionKeyword *keyword = &ZR_INLINE_COMPLETION_KEYWORDS[index];
        size_t keywordPrefixLength = strlen(keyword->prefix);

        if (prefixLength == keywordPrefixLength &&
            strncmp(content + prefixStart, keyword->prefix, keywordPrefixLength) == 0) {
            cJSON *item = inline_completion_create_item(position,
                                                        (TZrInt32)prefixLength,
                                                        keyword->insertText);
            if (item != NULL) {
                cJSON_AddItemToArray(result, item);
            }
            break;
        }
    }

    return result;
}
