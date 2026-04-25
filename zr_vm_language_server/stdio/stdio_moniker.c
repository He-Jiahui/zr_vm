#include "zr_vm_language_server_stdio_internal.h"

static int moniker_is_identifier_start(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

static int moniker_is_identifier_part(char ch) {
    return moniker_is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

static int moniker_offset_from_position(const char *content,
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

static cJSON *moniker_create_for_word(const char *uriText,
                                      const char *wordStart,
                                      size_t wordLength) {
    cJSON *moniker = cJSON_CreateObject();
    char *identifier;
    size_t uriLength;

    if (moniker == NULL || uriText == NULL || wordStart == NULL || wordLength == 0) {
        cJSON_Delete(moniker);
        return NULL;
    }

    uriLength = strlen(uriText);
    identifier = (char *)malloc(uriLength + 1 + wordLength + 1);
    if (identifier == NULL) {
        cJSON_Delete(moniker);
        return NULL;
    }

    memcpy(identifier, uriText, uriLength);
    identifier[uriLength] = '#';
    memcpy(identifier + uriLength + 1, wordStart, wordLength);
    identifier[uriLength + 1 + wordLength] = '\0';

    cJSON_AddStringToObject(moniker, ZR_LSP_FIELD_SCHEME, "zr");
    cJSON_AddStringToObject(moniker, ZR_LSP_FIELD_IDENTIFIER, identifier);
    cJSON_AddStringToObject(moniker, ZR_LSP_FIELD_UNIQUE, "document");
    cJSON_AddStringToObject(moniker, ZR_LSP_FIELD_KIND, "local");

    free(identifier);
    return moniker;
}

cJSON *handle_moniker_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    const char *content;
    size_t contentLength;
    size_t offset;
    size_t wordStart;
    size_t wordEnd;
    cJSON *result;
    cJSON *moniker;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return cJSON_CreateArray();
    }

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return cJSON_CreateArray();
    }

    content = fileVersion->content;
    contentLength = (size_t)fileVersion->contentLength;
    if (!moniker_offset_from_position(content, contentLength, position, &offset)) {
        return cJSON_CreateArray();
    }
    if (offset >= contentLength || !moniker_is_identifier_part(content[offset])) {
        if (offset == 0 || !moniker_is_identifier_part(content[offset - 1])) {
            return cJSON_CreateArray();
        }
        offset--;
    }

    wordStart = offset;
    while (wordStart > 0 && moniker_is_identifier_part(content[wordStart - 1])) {
        wordStart--;
    }
    if (!moniker_is_identifier_start(content[wordStart])) {
        return cJSON_CreateArray();
    }

    wordEnd = offset + 1;
    while (wordEnd < contentLength && moniker_is_identifier_part(content[wordEnd])) {
        wordEnd++;
    }

    result = cJSON_CreateArray();
    moniker = moniker_create_for_word(uriText, content + wordStart, wordEnd - wordStart);
    if (result == NULL || moniker == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(moniker);
        return cJSON_CreateArray();
    }

    cJSON_AddItemToArray(result, moniker);
    return result;
}
