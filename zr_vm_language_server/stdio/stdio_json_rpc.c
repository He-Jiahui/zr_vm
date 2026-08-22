#include "stdio_json_rpc.h"

#include <string.h>

static TZrBool json_rpc_id_is_valid(const cJSON *id) {
    return id == ZR_NULL ||
           cJSON_IsString((cJSON *)id) ||
           cJSON_IsNumber((cJSON *)id) ||
           cJSON_IsNull((cJSON *)id);
}

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
