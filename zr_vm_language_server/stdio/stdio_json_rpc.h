#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_JSON_RPC_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_JSON_RPC_H

#include "cJSON/cJSON.h"
#include "zr_vm_language_server/conf.h"

typedef struct SZrJsonRpcEnvelope {
    const cJSON *id;
    const TZrChar *method;
    const cJSON *params;
    TZrBool isRequest;
    TZrBool isNotification;
} SZrJsonRpcEnvelope;

typedef enum EZrLspHandlerStatus {
    ZR_LSP_HANDLER_OK = 0,
    ZR_LSP_HANDLER_INVALID_PARAMS,
    ZR_LSP_HANDLER_CANCELLED,
    ZR_LSP_HANDLER_CONTENT_MODIFIED,
    ZR_LSP_HANDLER_INTERNAL_ERROR,
} EZrLspHandlerStatus;

typedef struct SZrLspHandlerResult {
    EZrLspHandlerStatus status;
    cJSON *result;
} SZrLspHandlerResult;

typedef enum EZrJsonRpcEnvelopeStatus {
    ZR_JSON_RPC_ENVELOPE_OK = 0,
    ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST,
    ZR_JSON_RPC_ENVELOPE_INVALID_PARAMS,
} EZrJsonRpcEnvelopeStatus;

EZrJsonRpcEnvelopeStatus ZrLanguageServer_StdioJsonRpc_ParseEnvelope(
        const cJSON *message,
        SZrJsonRpcEnvelope *outEnvelope,
        const cJSON **outErrorId);

#endif
