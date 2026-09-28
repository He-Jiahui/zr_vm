#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

/** @brief 为 full 与 range 请求共用一次语义 token 生成，并按请求形态封装结果。
 *  请求分发器通常已持有活动语义快照；直接调用时自行获取并释放。full 的结果进入
 *  URI 最近结果缓存，供后续 delta 比对；range 结果不改变该缓存。
 */
static SZrLspHandlerResult create_semantic_tokens_response(SZrStdioServer *server,
                                              const cJSON *params,
                                              const SZrLspRange *range) {
    const char *uriText;
    SZrString *uri;
    SZrArray tokens = {0};
    cJSON *result;
    char resultId[64];
    SZrLspSemanticSnapshot *snapshot;
    TZrBool ownsSnapshot = ZR_FALSE;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    /* 沿用请求层的快照身份，令 resultId 与随后将校验的语义视图同代；仅独立调用时持有新快照。 */
    snapshot = ZrLanguageServer_LspSemanticSnapshot_GetActive(server->context);
    if (snapshot == ZR_NULL) {
        snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(server->state, server->context, uri);
        ownsSnapshot = ZR_TRUE;
    }
    ZrCore_Array_Init(server->state, &tokens, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(server->state, server->context, uri, &tokens)) {
        ZrCore_Array_Free(server->state, &tokens);
        if (ownsSnapshot) {
            ZrLanguageServer_LspSemanticSnapshot_Release(server->state, snapshot);
        }
        return stdio_handler_result_from_json(server->context, cJSON_CreateNull());
    }

    /* BUG: 首次语法错误使 AST 缺失时，Acquire 可返回 NULL，但文本 token 仍生成成功。
     * 此处 resultId 退化为 zr-snapshot:0:<长度>；同长度的下一版 token 若内容变化，
     * delta 的同 ID 快速路径会给客户端空编辑，留下过期高亮。
     */
    ZrLanguageServer_LspSemanticSnapshot_FormatResultId(snapshot, tokens.length, resultId, sizeof(resultId));
    result = range != ZR_NULL ? serialize_semantic_tokens_range_result(&tokens, *range)
                              : serialize_semantic_tokens_result(&tokens, resultId);
    /* delta 的比较基线应是 full/delta 的完整流，故 range 不能覆盖它。 */
    if (range == ZR_NULL && result != NULL) {
        upsert_semantic_token_snapshot(server, uriText, resultId, &tokens);
    }
    ZrCore_Array_Free(server->state, &tokens);
    if (ownsSnapshot) {
        ZrLanguageServer_LspSemanticSnapshot_Release(server->state, snapshot);
    }
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 处理客户端声明的 full 能力，返回含 resultId 的完整 token 流。 */
SZrLspHandlerResult handle_semantic_tokens_full_request(SZrStdioServer *server, const cJSON *params) {
    return create_semantic_tokens_response(server, params, ZR_NULL);
}

/** @brief 对客户端持有的上次完整结果生成 delta，优先使用同 URI 缓存求最小编辑。
 *  previousResultId 必须是字符串；最近缓存不匹配时由序列化层按 ID 中的长度回退。
 */
SZrLspHandlerResult handle_semantic_tokens_full_delta_request(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    const cJSON *previousResultIdJson;
    const char *previousResultIdText = NULL;
    SZrString *uri;
    SZrArray tokens = {0};
    TZrSize previousLength;
    SZrSemanticTokenSnapshot *previousSnapshot;
    cJSON *result;
    char resultId[64];
    SZrLspSemanticSnapshot *snapshot;
    TZrBool ownsSnapshot = ZR_FALSE;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    previousResultIdJson = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID);
    if (!cJSON_IsString((cJSON *)previousResultIdJson) ||
        cJSON_GetStringValue((cJSON *)previousResultIdJson) == NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    snapshot = ZrLanguageServer_LspSemanticSnapshot_GetActive(server->context);
    if (snapshot == ZR_NULL) {
        snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(server->state, server->context, uri);
        ownsSnapshot = ZR_TRUE;
    }
    /* 旧长度只用于缓存未命中的全量替换；命中时以缓存中的真实数组长度为准。 */
    previousResultIdText = cJSON_GetStringValue((cJSON *)previousResultIdJson);
    previousLength = semantic_tokens_previous_result_length(params);
    ZrCore_Array_Init(server->state, &tokens, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetSemanticTokens(server->state, server->context, uri, &tokens)) {
        ZrCore_Array_Free(server->state, &tokens);
        if (ownsSnapshot) {
            ZrLanguageServer_LspSemanticSnapshot_Release(server->state, snapshot);
        }
        return stdio_handler_result_from_json(server->context, cJSON_CreateNull());
    }

    /* BUG: 无 AST 时 snapshot 可为 NULL；两版 token 长度相同会复用固定 ID，
     * 下方序列化器据此返回空编辑，即使文本扫描所得 token 数值已改变。
     */
    ZrLanguageServer_LspSemanticSnapshot_FormatResultId(snapshot, tokens.length, resultId, sizeof(resultId));
    previousSnapshot = find_semantic_token_snapshot(server, uriText);
    result =
        serialize_semantic_tokens_delta_result(&tokens, previousLength, previousResultIdText, previousSnapshot, resultId);
    if (result != NULL) {
        upsert_semantic_token_snapshot(server, uriText, resultId, &tokens);
    }
    ZrCore_Array_Free(server->state, &tokens);
    if (ownsSnapshot) {
        ZrLanguageServer_LspSemanticSnapshot_Release(server->state, snapshot);
    }
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 把客户端范围坐标转换为内部位置后，返回该范围的独立 token 流。
 *  range 不建立可用于 full/delta 的 resultId 基线。
 */
SZrLspHandlerResult handle_semantic_tokens_range_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspRange range;

    const char *uriText;
    SZrString *uri;

    if (!get_uri_from_text_document(server, params, &uriText, &uri) ||
        !parse_range_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_RANGE), &range)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZR_UNUSED_PARAMETER(uriText);
    return create_semantic_tokens_response(server, params, &range);
}
