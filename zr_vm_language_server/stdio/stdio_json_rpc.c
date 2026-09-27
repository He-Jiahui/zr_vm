#include "stdio_json_rpc.h"

#include <math.h>
#include <string.h>

/* id 经 cJSON 的 double 表示后仅接受可精确往返的安全整数，避免请求预留错配。 */
static TZrBool json_rpc_number_id_is_valid(const cJSON *id) {
    double value;

    if (!cJSON_IsNumber((cJSON *)id)) {
        return ZR_FALSE;
    }
    value = id->valuedouble;
    if (!isfinite(value) || value < -ZR_LSP_JSON_SAFE_INTEGER_MAX ||
        value > ZR_LSP_JSON_SAFE_INTEGER_MAX) {
        return ZR_FALSE;
    }
    return value == (double)(long long)value;
}

/* 缺少 id 与显式 null 是两种不同信封，不能在通知分类前合并。 */
static TZrBool json_rpc_id_is_valid(const cJSON *id) {
    return id == ZR_NULL ||
           cJSON_IsString((cJSON *)id) ||
           json_rpc_number_id_is_valid(id) ||
           cJSON_IsNull((cJSON *)id);
}

/* 输入线程先调用以预留/取消请求，主循环再调用以选择响应或通知路径。 */
EZrJsonRpcEnvelopeStatus ZrLanguageServer_StdioJsonRpc_ParseEnvelope(
        const cJSON *message,
        SZrJsonRpcEnvelope *outEnvelope,
        const cJSON **outErrorId) {
    const cJSON *id;
    const cJSON *jsonRpc;
    const cJSON *method;
    const cJSON *params;

    if (outEnvelope != ZR_NULL) {
        memset(outEnvelope, 0, sizeof(*outEnvelope));
    }
    if (outErrorId != ZR_NULL) {
        *outErrorId = ZR_NULL;
    }
    if (!cJSON_IsObject((cJSON *)message) || outEnvelope == ZR_NULL) {
        return ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST;
    }

    /* 即使后续字段不合格，也尽量保留合法 id 供 Invalid Request 回复使用。 */
    id = cJSON_GetObjectItemCaseSensitive((cJSON *)message, ZR_LSP_JSON_RPC_FIELD_ID);
    if (json_rpc_id_is_valid(id)) {
        outEnvelope->id = id;
        outEnvelope->isRequest = id != ZR_NULL;
        outEnvelope->isNotification = id == ZR_NULL;
        if (outErrorId != ZR_NULL) {
            *outErrorId = id;
        }
    } else {
        return ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST;
    }

    jsonRpc = cJSON_GetObjectItemCaseSensitive((cJSON *)message, ZR_LSP_JSON_RPC_FIELD_JSONRPC);
    method = cJSON_GetObjectItemCaseSensitive((cJSON *)message, ZR_LSP_JSON_RPC_FIELD_METHOD);
    if (!cJSON_IsString((cJSON *)jsonRpc) ||
        strcmp(cJSON_GetStringValue((cJSON *)jsonRpc), ZR_LSP_JSON_RPC_VERSION) != 0 ||
        !cJSON_IsString((cJSON *)method)) {
        return ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST;
    }

    params = cJSON_GetObjectItemCaseSensitive((cJSON *)message, ZR_LSP_JSON_RPC_FIELD_PARAMS);
    outEnvelope->method = cJSON_GetStringValue((cJSON *)method);
    outEnvelope->params = params;
    if (params != ZR_NULL &&
        !cJSON_IsObject((cJSON *)params) &&
        !cJSON_IsArray((cJSON *)params)) {
        return ZR_JSON_RPC_ENVELOPE_INVALID_PARAMS;
    }

    return ZR_JSON_RPC_ENVELOPE_OK;
}
