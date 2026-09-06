#include "wasm_response.h"
#include "zr_vm_language_server/conf.h"

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
