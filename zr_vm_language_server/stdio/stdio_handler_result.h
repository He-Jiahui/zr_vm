#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_HANDLER_RESULT_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_HANDLER_RESULT_H

#include "stdio_json_rpc.h"
#include "zr_vm_language_server/lsp_interface.h"

static inline SZrLspHandlerResult stdio_handler_error(EZrLspHandlerStatus status) {
    SZrLspHandlerResult response = {status, ZR_NULL};
    return response;
}

/* Takes ownership of result; errors never expose an owned JSON value to the caller. */
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
