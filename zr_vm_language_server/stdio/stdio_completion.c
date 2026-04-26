#include "zr_vm_language_server_stdio_internal.h"

static int completion_offset_from_position(const char *content,
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

static int completion_is_identifier_part(char ch) {
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_';
}

static SZrLspRange completion_prefix_range(SZrStdioServer *server,
                                           SZrString *uri,
                                           SZrLspPosition position) {
    SZrLspRange range;
    SZrFileVersion *fileVersion;
    const char *content;
    size_t contentLength;
    size_t offset;
    size_t prefixStart;
    TZrInt32 prefixLength;

    range.start = position;
    range.end = position;

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return range;
    }

    content = fileVersion->content;
    contentLength = (size_t)fileVersion->contentLength;
    if (!completion_offset_from_position(content, contentLength, position, &offset)) {
        return range;
    }

    prefixStart = offset;
    while (prefixStart > 0 && completion_is_identifier_part(content[prefixStart - 1])) {
        prefixStart--;
    }

    prefixLength = (TZrInt32)(offset - prefixStart);
    if (prefixLength > 0 && range.start.character >= prefixLength) {
        range.start.character -= prefixLength;
    }
    return range;
}

static const char *completion_item_new_text(cJSON *item) {
    const cJSON *insertText = get_object_item(item, ZR_LSP_FIELD_INSERT_TEXT);
    const cJSON *label = get_object_item(item, ZR_LSP_FIELD_LABEL);

    if (cJSON_IsString((cJSON *)insertText) && insertText->valuestring != NULL) {
        return insertText->valuestring;
    }
    if (cJSON_IsString((cJSON *)label) && label->valuestring != NULL) {
        return label->valuestring;
    }
    return NULL;
}

static void add_completion_text_edit(cJSON *item, SZrLspRange range) {
    cJSON *textEdit;
    const char *newText;

    if (!cJSON_IsObject(item)) {
        return;
    }

    newText = completion_item_new_text(item);
    if (newText == NULL) {
        return;
    }

    textEdit = cJSON_CreateObject();
    if (textEdit == NULL) {
        return;
    }

    cJSON_AddItemToObject(textEdit, ZR_LSP_FIELD_RANGE, serialize_range(range));
    cJSON_AddStringToObject(textEdit, ZR_LSP_FIELD_NEW_TEXT, newText);
    cJSON_AddItemToObject(item, ZR_LSP_FIELD_TEXT_EDIT, textEdit);
}

static TZrBool completion_item_label_matches(SZrLspCompletionItem *item, const char *label) {
    char *itemLabel;
    TZrBool matches;

    if (item == ZR_NULL || label == NULL) {
        return ZR_FALSE;
    }

    itemLabel = zr_string_to_c_string(item->label);
    matches = itemLabel != NULL && strcmp(itemLabel, label) == 0;
    free(itemLabel);
    return matches;
}

static cJSON *serialize_resolved_completion_item(const cJSON *params,
                                                 const cJSON *data,
                                                 SZrLspCompletionItem *item,
                                                 SZrLspRange range) {
    cJSON *resolved = serialize_completion_item(item);

    if (resolved == NULL) {
        return params != NULL ? cJSON_Duplicate((cJSON *)params, 1) : cJSON_CreateObject();
    }

    add_completion_text_edit(resolved, range);
    if (data != NULL) {
        cJSON_AddItemToObject(resolved, ZR_LSP_FIELD_DATA, cJSON_Duplicate((cJSON *)data, 1));
    }
    return resolved;
}

cJSON *handle_completion_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray completions = {0};
    SZrLspPosition position;
    SZrLspRange prefixRange;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return NULL;
    }

    prefixRange = completion_prefix_range(server, uri, position);
    if (!ZrLanguageServer_Lsp_GetCompletion(server->state, server->context, uri, position, &completions)) {
        return cJSON_CreateArray();
    }

    result = serialize_completion_items_array(&completions);
    if (cJSON_IsArray(result)) {
        int count = cJSON_GetArraySize(result);
        for (int index = 0; index < count; index++) {
            cJSON *item = cJSON_GetArrayItem(result, index);
            cJSON *data;

            if (!cJSON_IsObject(item)) {
                continue;
            }

            add_completion_text_edit(item, prefixRange);
            data = cJSON_CreateObject();
            if (data == NULL) {
                continue;
            }
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_URI, uriText);
            cJSON_AddItemToObject(data, ZR_LSP_FIELD_POSITION, serialize_position(position));
            cJSON_AddItemToObject(item, ZR_LSP_FIELD_DATA, data);
        }
    }
    free_completion_items_array(server->state, &completions);
    return result;
}

cJSON *handle_completion_item_resolve_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *labelJson;
    const cJSON *data;
    const cJSON *uriJson;
    const cJSON *positionJson;
    const char *uriText;
    const char *label;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspRange prefixRange;
    SZrArray completions = {0};
    cJSON *result;

    if (server == ZR_NULL || params == NULL) {
        return cJSON_CreateObject();
    }

    labelJson = get_object_item(params, ZR_LSP_FIELD_LABEL);
    data = get_object_item(params, ZR_LSP_FIELD_DATA);
    uriJson = get_object_item(data, ZR_LSP_FIELD_URI);
    positionJson = get_object_item(data, ZR_LSP_FIELD_POSITION);
    if (!cJSON_IsString((cJSON *)labelJson) ||
        !cJSON_IsString((cJSON *)uriJson) ||
        !parse_position(positionJson, &position)) {
        return cJSON_Duplicate((cJSON *)params, 1);
    }

    label = cJSON_GetStringValue((cJSON *)labelJson);
    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    uri = server_get_cached_uri(server, uriText);
    if (label == NULL || uri == ZR_NULL) {
        return cJSON_Duplicate((cJSON *)params, 1);
    }

    prefixRange = completion_prefix_range(server, uri, position);
    if (!ZrLanguageServer_Lsp_GetCompletion(server->state, server->context, uri, position, &completions)) {
        return cJSON_Duplicate((cJSON *)params, 1);
    }

    result = ZR_NULL;
    for (TZrSize index = 0; index < completions.length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(&completions, index);

        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL && completion_item_label_matches(*itemPtr, label)) {
            result = serialize_resolved_completion_item(params, data, *itemPtr, prefixRange);
            break;
        }
    }

    free_completion_items_array(server->state, &completions);
    return result != NULL ? result : cJSON_Duplicate((cJSON *)params, 1);
}
