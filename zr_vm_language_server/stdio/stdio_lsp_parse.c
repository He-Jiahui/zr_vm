#include "zr_vm_language_server_stdio_internal.h"

#include <math.h>
#include <stdint.h>

const cJSON *get_object_item(const cJSON *json, const char *key) {
    if (json == NULL || key == NULL) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive((cJSON *)json, key);
}

TZrBool parse_size_value_strict(const cJSON *json, TZrSize *outValue) {
    double value;
    TZrSize parsed;

    if (!cJSON_IsNumber((cJSON *)json) || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    value = json->valuedouble;
    if (!isfinite(value) || value < 0 || value > (double)ZR_MAX_SIZE) {
        return ZR_FALSE;
    }

    parsed = (TZrSize)value;
    if ((double)parsed != value) {
        return ZR_FALSE;
    }

    *outValue = parsed;
    return ZR_TRUE;
}

static TZrBool parse_position_value(const cJSON *json, TZrInt32 *outValue) {
    double value;
    TZrInt32 parsed;

    if (!cJSON_IsNumber((cJSON *)json) || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    value = json->valuedouble;
    if (!isfinite(value) || value < 0 || value > (double)INT32_MAX) {
        return ZR_FALSE;
    }

    parsed = (TZrInt32)value;
    if ((double)parsed != value) {
        return ZR_FALSE;
    }

    *outValue = parsed;
    return ZR_TRUE;
}

TZrSize parse_size_value(const cJSON *json, TZrSize fallback) {
    TZrSize value;

    if (!parse_size_value_strict(json, &value)) {
        return fallback;
    }
    return value;
}

int parse_position(const cJSON *json, SZrLspPosition *outPosition) {
    const cJSON *line;
    const cJSON *character;

    if (json == NULL || outPosition == NULL) {
        return 0;
    }

    line = get_object_item(json, ZR_LSP_FIELD_LINE);
    character = get_object_item(json, ZR_LSP_FIELD_CHARACTER);
    if (!parse_position_value(line, &outPosition->line) ||
        !parse_position_value(character, &outPosition->character)) {
        return 0;
    }
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

    if (outRange->start.line > outRange->end.line ||
        (outRange->start.line == outRange->end.line &&
         outRange->start.character > outRange->end.character)) {
        return 0;
    }

    return 1;
}
