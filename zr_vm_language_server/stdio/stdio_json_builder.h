#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_JSON_BUILDER_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_JSON_BUILDER_H

#include "cJSON/cJSON.h"

/** @brief 将新建 JSON 子树移交给对象；附加失败时也删除子树。
 *  @return 成功附加返回真；调用方在任一返回路径都不得再释放 item。
 */
static inline cJSON_bool stdio_json_add_owned_item(cJSON *object, const char *name, cJSON *item) {
    if (!cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return 0;
    }
    return 1;
}

/** @brief 将新建 JSON 子树移交给数组；失败时负责清理，避免调用方重复释放。 */
static inline cJSON_bool stdio_json_add_owned_array_item(cJSON *array, cJSON *item) {
    if (!cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return 0;
    }
    return 1;
}

#endif
