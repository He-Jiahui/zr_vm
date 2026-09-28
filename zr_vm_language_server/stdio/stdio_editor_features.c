#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

/* 将实现位置查询接入与定义、引用一致的 stdio Location 序列化和释放路径。 */
typedef TZrBool (*TZrLspLocationProvider)(SZrState *state,
                                          SZrLspContext *context,
                                          SZrString *uri,
                                          SZrLspPosition position,
                                          SZrArray *result);

/* provider 的原生数组只在本次请求有效；响应 JSON 在统一发送层完成坐标编码。 */
static SZrLspHandlerResult handle_location_request(SZrStdioServer *server,
                                      const cJSON *params,
                                      TZrLspLocationProvider provider) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (server == ZR_NULL || provider == NULL ||
        !get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &locations, sizeof(SZrLspLocation *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    /* TODO: provider 的 false 与无匹配位置在响应中都变成 []；核对内存失败等内部错误是否应保留独立状态。 */
    if (!provider(server->state, server->context, uri, position, &locations)) {
        free_locations_array(server->state, &locations);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_locations_array(&locations);
    free_locations_array(server->state, &locations);
    return stdio_handler_result_from_json(server->context, result);
}

/* 请求分发器选择折叠能力后调用；失败按当前协议适配约定返回空数组。 */
SZrLspHandlerResult handle_folding_range_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray ranges = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &ranges, sizeof(SZrLspFoldingRange *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetFoldingRanges(server->state, server->context, uri, &ranges)) {
        ZrLanguageServer_Lsp_FreeFoldingRanges(server->state, &ranges);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_folding_ranges_array(&ranges);
    ZrLanguageServer_Lsp_FreeFoldingRanges(server->state, &ranges);
    return stdio_handler_result_from_json(server->context, result);
}

/* 逐个按文档 URI 解码客户端位置，保持输入顺序，再移交 selectionRange JSON 树。 */
SZrLspHandlerResult handle_selection_range_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *positionsJson;
    SZrLspPosition *positions;
    int positionCount;
    SZrArray ranges = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    positionsJson = get_object_item(params, "positions");
    if (!cJSON_IsArray((cJSON *)positionsJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    positionCount = cJSON_GetArraySize((cJSON *)positionsJson);
    if (positionCount <= 0) {
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    positions = (SZrLspPosition *)malloc(sizeof(SZrLspPosition) * (size_t)positionCount);
    if (positions == NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INTERNAL_ERROR);
    }
    for (int index = 0; index < positionCount; index++) {
        if (!parse_position_for_uri(server,
                                    uri,
                                    cJSON_GetArrayItem((cJSON *)positionsJson, index),
                                    &positions[index])) {
            free(positions);
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &ranges, sizeof(SZrLspSelectionRange *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetSelectionRanges(server->state,
                                                 server->context,
                                                 uri,
                                                 positions,
                                                 (TZrSize)positionCount,
                                                 &ranges)) {
        free(positions);
        ZrLanguageServer_Lsp_FreeSelectionRanges(server->state, &ranges);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_selection_ranges_array(&ranges);
    free(positions);
    ZrLanguageServer_Lsp_FreeSelectionRanges(server->state, &ranges);
    return stdio_handler_result_from_json(server->context, result);
}

/* documentLink 的普通和虚拟目标共用此响应入口，原生链接在序列化后立即释放。 */
SZrLspHandlerResult handle_document_link_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray links = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &links, sizeof(SZrLspDocumentLink *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDocumentLinks(server->state, server->context, uri, &links)) {
        ZrLanguageServer_Lsp_FreeDocumentLinks(server->state, &links);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_document_links_array(&links);
    ZrLanguageServer_Lsp_FreeDocumentLinks(server->state, &links);
    return stdio_handler_result_from_json(server->context, result);
}

/* implementation 与其他导航请求共享输入解析，但结果来自独立语义关系查询器。 */
SZrLspHandlerResult handle_implementation_request(SZrStdioServer *server, const cJSON *params) {
    return handle_location_request(server, params, ZrLanguageServer_Lsp_GetImplementation);
}

/* CodeLens 查询生成可执行命令；返回前复制成 JSON，避免暴露原生项的生命周期。 */
SZrLspHandlerResult handle_code_lens_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray lenses = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &lenses, sizeof(SZrLspCodeLens *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCodeLens(server->state, server->context, uri, &lenses)) {
        ZrLanguageServer_Lsp_FreeCodeLens(server->state, &lenses);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_code_lens_array(&lenses);
    ZrLanguageServer_Lsp_FreeCodeLens(server->state, &lenses);
    return stdio_handler_result_from_json(server->context, result);
}
