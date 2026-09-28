#include "wasm_response.h"
#include "zr_vm_language_server/conf.h"

/* 所有 WASM 导出共用该失败封装，bridge 依据 success 与 code 转成 LSP 错误。 */
const char *ZrLanguageServer_Wasm_ErrorResponse(int code, const char *message) {
    cJSON *json = cJSON_CreateObject();
    char *result;
    if (json == ZR_NULL ||
        cJSON_AddBoolToObject(json, "success", ZR_FALSE) == ZR_NULL ||
        cJSON_AddNumberToObject(json, "code", code) == ZR_NULL ||
        cJSON_AddStringToObject(json, "error", message != ZR_NULL ? message : "WASM request failed") == ZR_NULL) {
        cJSON_Delete(json);
        return ZR_NULL;
    }
    result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return result;
}

/* 在此处转移 data 的所有权，调用导出只需管理其 native 临时结果。 */
const char *ZrLanguageServer_Wasm_SuccessResponse(cJSON *data) {
    cJSON *json;
    char *result;
    if (data == ZR_NULL) {
        return ZrLanguageServer_Wasm_ErrorResponse(ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE,
                                                  "Failed to serialize response data");
    }
    json = cJSON_CreateObject();
    if (json == ZR_NULL ||
        cJSON_AddBoolToObject(json, "success", ZR_TRUE) == ZR_NULL ||
        !cJSON_AddItemToObject(json, "data", data)) {
        cJSON_Delete(data);
        cJSON_Delete(json);
        return ZR_NULL;
    }
    result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return result;
}
