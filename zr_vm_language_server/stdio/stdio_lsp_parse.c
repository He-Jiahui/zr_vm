#include "zr_vm_language_server_stdio_internal.h"

const cJSON *get_object_item(const cJSON *json, const char *key) {
    if (json == NULL || key == NULL) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive((cJSON *)json, key);
}

TZrSize parse_size_value(const cJSON *json, TZrSize fallback) {
    if (!cJSON_IsNumber((cJSON *)json) || json->valuedouble < 0) {
        return fallback;
    }
    return (TZrSize)json->valuedouble;
}

int parse_position(const cJSON *json, SZrLspPosition *outPosition) {
    const cJSON *line;
    const cJSON *character;

    if (json == NULL || outPosition == NULL) {
        return 0;
    }

    line = get_object_item(json, ZR_LSP_FIELD_LINE);
    character = get_object_item(json, ZR_LSP_FIELD_CHARACTER);
    if (!cJSON_IsNumber(line) || !cJSON_IsNumber(character)) {
        return 0;
    }

    outPosition->line = (TZrInt32)line->valuedouble;
    outPosition->character = (TZrInt32)character->valuedouble;
    return 1;
}

int parse_range(const cJSON *json, SZrLspRange *outRange) {
    const cJSON *start;
    const cJSON *end;

    if (json == NULL || outRange == NULL) {
        return 0;
    }

    start = get_object_item(json, ZR_LSP_FIELD_START);
    end = get_object_item(json, ZR_LSP_FIELD_END);
    if (!parse_position(start, &outRange->start) || !parse_position(end, &outRange->end)) {
        return 0;
    }

    return 1;
}
