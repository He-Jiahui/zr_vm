#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_JSON_RPC_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_JSON_RPC_H

#include "cJSON/cJSON.h"
#include "zr_vm_language_server/conf.h"

/** @brief 借用 cJSON 消息树中的字段，让输入线程与主分发循环作同一请求分类。 */
typedef struct SZrJsonRpcEnvelope {
    const cJSON *id;
    const TZrChar *method;
    const cJSON *params;
    TZrBool isRequest;
    TZrBool isNotification;
} SZrJsonRpcEnvelope;

/** @brief 将语义处理失败映射为 JSON-RPC 响应的分发状态。 */
typedef enum EZrLspHandlerStatus {
    ZR_LSP_HANDLER_OK = 0,
    ZR_LSP_HANDLER_INVALID_PARAMS,
    ZR_LSP_HANDLER_CANCELLED,
    ZR_LSP_HANDLER_CONTENT_MODIFIED,
    ZR_LSP_HANDLER_INTERNAL_ERROR,
} EZrLspHandlerStatus;

/** @brief 处理器移交给请求分发层的状态及可选结果树；错误状态须携带空结果。 */
typedef struct SZrLspHandlerResult {
    EZrLspHandlerStatus status;
    cJSON *result;
} SZrLspHandlerResult;

/** @brief 区分信封无效和参数无效，以保留请求 id 的错误响应语义。 */
typedef enum EZrJsonRpcEnvelopeStatus {
    ZR_JSON_RPC_ENVELOPE_OK = 0,
    ZR_JSON_RPC_ENVELOPE_INVALID_REQUEST,
    ZR_JSON_RPC_ENVELOPE_INVALID_PARAMS,
} EZrJsonRpcEnvelopeStatus;

/**
 * @brief 在预留请求 id 与派发处理器前验证 JSON-RPC 信封。
 * @pre message 的 cJSON 树在返回值及借用字段使用期间保持存活。
 * @note 无 id 是通知；显式 JSON null id 仍是请求；outErrorId 借用原树供错误响应使用。
 */
EZrJsonRpcEnvelopeStatus ZrLanguageServer_StdioJsonRpc_ParseEnvelope(
        const cJSON *message,
        SZrJsonRpcEnvelope *outEnvelope,
        const cJSON **outErrorId);

#endif
