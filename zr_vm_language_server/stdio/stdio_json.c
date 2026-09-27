#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_json_builder.h"

/** @brief 将内部位置写入 LSP 响应或补全项的协议坐标对象。
 *  @return 新建的 cJSON 节点；调用方接管，分配失败时返回 NULL。 */
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

/** @brief 为诊断、导航和编辑结果统一生成含起止位置的 LSP range。
 *  @note 子节点在附加失败时由构建器或本函数清理；成功后由调用方接管整棵 JSON 树。 */
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

/** @brief 把 LSP 查询返回的 URI 与范围转为可交给 JSON-RPC 响应的独立树。
 *  @pre location 非空时结构体有效；uri 非空时其 VM 字符串在序列化期间有效。
 *  @return location 为 NULL 时返回 JSON null；分配或字段构造失败时返回 NULL。 */
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
