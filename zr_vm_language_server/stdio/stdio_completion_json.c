#include "zr_vm_language_server_stdio_internal.h"

static void add_string_to_array(cJSON *array, const char *value) {
    cJSON *item;

    if (array == NULL || value == NULL) {
        return;
    }

    item = cJSON_CreateString(value);
    if (item != NULL) {
        cJSON_AddItemToArray(array, item);
    }
}

cJSON *create_completion_commit_characters_array(void) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL) {
        return NULL;
    }

    add_string_to_array(json, ZR_LSP_COMPLETION_COMMIT_CHARACTER_SEMICOLON);
    add_string_to_array(json, ZR_LSP_COMPLETION_COMMIT_CHARACTER_COMMA);
    add_string_to_array(json, ZR_LSP_COMPLETION_COMMIT_CHARACTER_DOT);
    add_string_to_array(json, ZR_LSP_COMPLETION_COMMIT_CHARACTER_OPEN_PAREN);
    return json;
}

cJSON *serialize_completion_item(const SZrLspCompletionItem *item) {
    cJSON *json;
    char *text;
    int insertTextFormat = ZR_LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT;

    if (item == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    text = zr_string_to_c_string(item->label);
    if (text != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_LABEL, text);
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_FILTER_TEXT, text);
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_SORT_TEXT, text);
        free(text);
    }
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, item->kind);
    cJSON_AddItemToObject(json,
                          ZR_LSP_FIELD_COMMIT_CHARACTERS,
                          create_completion_commit_characters_array());

    if (item->detail != ZR_NULL) {
        text = zr_string_to_c_string(item->detail);
        if (text != NULL) {
            cJSON *labelDetails = cJSON_CreateObject();

            cJSON_AddStringToObject(json, ZR_LSP_FIELD_DETAIL, text);
            if (labelDetails != NULL) {
                cJSON_AddStringToObject(labelDetails, ZR_LSP_FIELD_DETAIL, text);
                cJSON_AddItemToObject(json, ZR_LSP_FIELD_LABEL_DETAILS, labelDetails);
            }
            free(text);
        }
    }

    if (item->documentation != ZR_NULL) {
        cJSON *documentation = cJSON_CreateObject();
        text = zr_string_to_c_string(item->documentation);
        if (documentation != NULL && text != NULL) {
            cJSON_AddStringToObject(documentation, ZR_LSP_FIELD_KIND, ZR_LSP_MARKUP_KIND_MARKDOWN);
            cJSON_AddStringToObject(documentation, ZR_LSP_FIELD_VALUE, text);
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_DOCUMENTATION, documentation);
        } else {
            cJSON_Delete(documentation);
        }
        free(text);
    }

    if (item->insertText != ZR_NULL) {
        text = zr_string_to_c_string(item->insertText);
        if (text != NULL) {
            cJSON_AddStringToObject(json, ZR_LSP_FIELD_INSERT_TEXT, text);
            free(text);
        }
    }

    if (item->insertTextFormat != ZR_NULL) {
        text = zr_string_to_c_string(item->insertTextFormat);
        if (text != NULL) {
            if (strcmp(text, ZR_LSP_INSERT_TEXT_FORMAT_KIND_SNIPPET) == 0) {
                insertTextFormat = ZR_LSP_INSERT_TEXT_FORMAT_SNIPPET;
            }
            free(text);
        }
    }
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_INSERT_TEXT_FORMAT, insertTextFormat);

    return json;
}

cJSON *serialize_completion_items_array(SZrArray *items) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || items == ZR_NULL) {
        return json;
    }

    for (index = 0; index < items->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_completion_item(*itemPtr));
        }
    }
    return json;
}
