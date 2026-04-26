#include "zr_vm_language_server_stdio_internal.h"

cJSON *serialize_position(SZrLspPosition position) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_LINE, position.line);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_CHARACTER, position.character);
    return json;
}

cJSON *serialize_range(SZrLspRange range) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_START, serialize_position(range.start));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_END, serialize_position(range.end));
    return json;
}

cJSON *serialize_location(const SZrLspLocation *location) {
    cJSON *json;
    char *uriText;

    if (location == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    uriText = zr_string_to_c_string(location->uri);
    if (uriText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_URI, uriText);
        free(uriText);
    } else {
        cJSON_AddNullToObject(json, ZR_LSP_FIELD_URI);
    }
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(location->range));
    return json;
}
