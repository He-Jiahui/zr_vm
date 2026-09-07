#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_json_builder.h"

cJSON *serialize_position(SZrLspPosition position) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL ||
        cJSON_AddNumberToObject(json, ZR_LSP_FIELD_LINE, position.line) == NULL ||
        cJSON_AddNumberToObject(json, ZR_LSP_FIELD_CHARACTER, position.character) == NULL) {
        cJSON_Delete(json);
        return NULL;
    }
    return json;
}

cJSON *serialize_range(SZrLspRange range) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL ||
        !stdio_json_add_owned_item(json, ZR_LSP_FIELD_START, serialize_position(range.start)) ||
        !stdio_json_add_owned_item(json, ZR_LSP_FIELD_END, serialize_position(range.end))) {
        cJSON_Delete(json);
        return NULL;
    }
    return json;
}

cJSON *serialize_location(const SZrLspLocation *location) {
    cJSON *json;
    char *uriText;
    cJSON *uriJson;

    if (location == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    uriText = zr_string_to_c_string(location->uri);
    if (location->uri != ZR_NULL && uriText == NULL) {
        cJSON_Delete(json);
        return NULL;
    }
    uriJson = uriText != NULL ? cJSON_AddStringToObject(json, ZR_LSP_FIELD_URI, uriText)
                              : cJSON_AddNullToObject(json, ZR_LSP_FIELD_URI);
    free(uriText);
    if (uriJson == NULL ||
        !stdio_json_add_owned_item(json, ZR_LSP_FIELD_RANGE, serialize_range(location->range))) {
        cJSON_Delete(json);
        return NULL;
    }
    return json;
}
