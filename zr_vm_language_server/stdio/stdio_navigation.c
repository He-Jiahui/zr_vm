#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

/** @brief 将源码位置上的普通悬停查询投影为 LSP 响应，查询无目标时返回 null。
 * @note 参数位置须先按协商编码解析；JSON 建立后即可归还查询结果。 */
SZrLspHandlerResult handle_hover_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspHover *hover = ZR_NULL;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    if (!ZrLanguageServer_Lsp_GetHover(server->state, server->context, uri, position, &hover) || hover == ZR_NULL) {
        if (hover != ZR_NULL) {
            free_hover(server->state, hover);
        }
        return stdio_handler_result_from_json(server->context, cJSON_CreateNull());
    }

    result = serialize_hover(hover);
    free_hover(server->state, hover);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 为扩展专用请求提供分段悬停，保持与普通 hover 不同的响应和释放契约。 */
SZrLspHandlerResult handle_rich_hover_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspRichHover *hover = ZR_NULL;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    if (!ZrLanguageServer_Lsp_GetRichHover(server->state, server->context, uri, position, &hover) ||
        hover == ZR_NULL) {
        free_rich_hover(server->state, hover);
        return stdio_handler_result_from_json(server->context, cJSON_CreateNull());
    }

    result = serialize_rich_hover(hover);
    free_rich_hover(server->state, hover);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 将调用点的语义签名与参数文档发给编辑器，缺少可用签名时返回 null。 */
SZrLspHandlerResult handle_signature_help_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrLspSignatureHelp *help = ZR_NULL;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    if (!ZrLanguageServer_Lsp_GetSignatureHelp(server->state, server->context, uri, position, &help) ||
        help == ZR_NULL) {
        free_signature_help(server->state, help);
        return stdio_handler_result_from_json(server->context, cJSON_CreateNull());
    }

    result = serialize_signature_help(help);
    free_signature_help(server->state, help);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 在客户端指定范围内发布从规范声明事实得到的类型提示。
 * @note 范围按文档 URI 转成内部坐标；序列化后归还原生提示数组。 */
SZrLspHandlerResult handle_inlay_hint_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *rangeJson;
    const char *uriText;
    SZrString *uri;
    SZrLspRange range;
    SZrArray hints = {0};
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    rangeJson = get_object_item(params, ZR_LSP_FIELD_RANGE);
    if (!parse_range_for_uri(server, uri, rangeJson, &range)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    if (!ZrLanguageServer_Lsp_GetInlayHints(server->state, server->context, uri, range, &hints)) {
        free_inlay_hints_array(server->state, &hints);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_inlay_hints_array(&hints);
    free_inlay_hints_array(server->state, &hints);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 用语义定义位置支持源码和原生虚拟声明页的跳转；无目标时给空数组。 */
SZrLspHandlerResult handle_definition_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    if (!ZrLanguageServer_Lsp_GetDefinition(server->state, server->context, uri, position, &locations)) {
        free_locations_array(server->state, &locations);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_locations_array(&locations);
    free_locations_array(server->state, &locations);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 响应扩展对只读原生声明 URI 的取文请求；普通文件仍由文档同步管理。
 * @note 文本归 VM 状态持有，只释放临时 C 字符串副本。 */
SZrLspHandlerResult handle_native_declaration_document_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *uriJson;
    const char *uriText;
    SZrString *uri;
    SZrString *documentText = ZR_NULL;
    char *renderedText;
    cJSON *result;

    if (server == ZR_NULL || params == NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    uriJson = get_object_item(params, ZR_LSP_FIELD_URI);
    if (!cJSON_IsString((cJSON *)uriJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    uri = server_get_cached_uri(server, uriText);
    if (uri == ZR_NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INTERNAL_ERROR);
    }
    if (!ZrLanguageServer_Lsp_GetNativeDeclarationDocument(server->state, server->context, uri, &documentText) ||
        documentText == ZR_NULL) {
        return stdio_handler_result_from_json(server->context, cJSON_CreateNull());
    }

    renderedText = zr_string_to_c_string(documentText);
    result = renderedText != NULL ? cJSON_CreateString(renderedText) : ZR_NULL;
    free(renderedText);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 依客户端 includeDeclaration 选项返回语义身份对应的跨文件引用。
 * @note 位置数组先复制进 JSON 再释放；请求层可能把该数组作为 partial result 发出。 */
SZrLspHandlerResult handle_references_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const cJSON *contextJson;
    const cJSON *includeDeclarationJson;
    const char *uriText;
    SZrString *uri;
    TZrBool includeDeclaration = ZR_FALSE;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    /* includeDeclaration 必须来自合法的 references context；不能把缺字段当作 false。 */
    contextJson = get_object_item(params, ZR_LSP_FIELD_CONTEXT);
    if (!cJSON_IsObject((cJSON *)contextJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    includeDeclarationJson = get_object_item(contextJson, ZR_LSP_FIELD_INCLUDE_DECLARATION);
    if (!cJSON_IsBool((cJSON *)includeDeclarationJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    includeDeclaration = cJSON_IsTrue((cJSON *)includeDeclarationJson) ? ZR_TRUE : ZR_FALSE;

    if (!ZrLanguageServer_Lsp_FindReferences(
            server->state,
            server->context,
            uri,
            position,
            includeDeclaration,
            &locations)) {
        free_locations_array(server->state, &locations);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_locations_array(&locations);
    free_locations_array(server->state, &locations);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 生成当前文档大纲；未取消的查询失败返回空数组，取消由结果封装层传播。 */
SZrLspHandlerResult handle_document_symbols_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray symbols = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(server->state, server->context, uri, &symbols)) {
        free_symbols_array(server->state, &symbols);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_symbols_array(&symbols);
    free_symbols_array(server->state, &symbols);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 将工作区查询文本交给项目索引及打开文档的合并搜索，再发布符号数组。 */
SZrLspHandlerResult handle_workspace_symbols_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray symbols = {0};
    const cJSON *queryJson;
    const char *queryText = "";
    SZrString *query;
    cJSON *result;

    if (server == ZR_NULL || !cJSON_IsObject((cJSON *)params)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    queryJson = get_object_item(params, ZR_LSP_FIELD_QUERY);
    if (!cJSON_IsString((cJSON *)queryJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    queryText = cJSON_GetStringValue((cJSON *)queryJson);
    if (queryText == NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    /* 搜索词是请求 JSON 的暂借字节；语义查询需要 VM 字符串跨过参数解析边界。 */
    query = ZrCore_String_Create(server->state, (TZrNativeString)queryText, (TZrSize)strlen(queryText));
    if (query == ZR_NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INTERNAL_ERROR);
    }

    if (!ZrLanguageServer_Lsp_GetWorkspaceSymbols(server->state, server->context, query, &symbols)) {
        free_symbols_array(server->state, &symbols);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_symbols_array(&symbols);
    free_symbols_array(server->state, &symbols);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 为当前文档的同一语义目标返回高亮位置，供编辑器局部标记引用。 */
SZrLspHandlerResult handle_document_highlights_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray highlights = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    if (!ZrLanguageServer_Lsp_GetDocumentHighlights(server->state, server->context, uri, position, &highlights)) {
        free_highlights_array(server->state, &highlights);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_highlights_array(&highlights);
    free_highlights_array(server->state, &highlights);
    return stdio_handler_result_from_json(server->context, result);
}
