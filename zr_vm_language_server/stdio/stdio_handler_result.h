#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_HANDLER_RESULT_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_HANDLER_RESULT_H

#include "stdio_json_rpc.h"
#include "zr_vm_language_server/lsp_interface.h"

/* 参数校验等失败路径只移交状态，避免上层误以为仍有待发送的 JSON 树。 */
static inline SZrLspHandlerResult stdio_handler_error(EZrLspHandlerStatus status) {
    SZrLspHandlerResult response = {status, ZR_NULL};
    return response;
}

/* 处理器在返回前检查输入线程记录的取消意图，并将 result 所有权移交分发层。
 * 取消时本层释放结果；构造失败的 NULL 归类为内部错误，不能代替协议 JSON null。 */
static inline SZrLspHandlerResult stdio_handler_result_from_json(
        const SZrLspContext *context, cJSON *result) {
    SZrLspHandlerResult response;
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        cJSON_Delete(result);
        return stdio_handler_error(ZR_LSP_HANDLER_CANCELLED);
    }
    response.status = result != ZR_NULL ? ZR_LSP_HANDLER_OK : ZR_LSP_HANDLER_INTERNAL_ERROR;
    response.result = result;
    return response;
}

#endif
