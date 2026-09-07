#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_JSON_BUILDER_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_JSON_BUILDER_H

#include "cJSON/cJSON.h"

/* Both helpers consume item, including when attachment fails. */
static inline cJSON_bool stdio_json_add_owned_item(cJSON *object, const char *name, cJSON *item) {
    if (!cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return 0;
    }
    return 1;
}

static inline cJSON_bool stdio_json_add_owned_array_item(cJSON *array, cJSON *item) {
    if (!cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return 0;
    }
    return 1;
}

#endif
