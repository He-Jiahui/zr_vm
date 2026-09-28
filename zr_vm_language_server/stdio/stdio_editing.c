#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

#include <errno.h>
#include <stdint.h>

/** @brief resolve 阶段确认快照过期时给禁用代码操作使用的稳定说明。 */
#define ZR_LSP_CODE_ACTION_STALE_REASON \
    "Document changed since this code action was computed"

/**
 * @brief 读取代码操作 data 中的非负精确整数，供文档快照的长度、版本与代数校验使用。
 * @details 值来自先前 serialize_workspace_edit_document_snapshot 的 JSON 数字；
 *          限制在 JSON 安全整数及本机 TZrSize 范围内，避免 resolve 时比较失真。
 */
static TZrBool parse_code_action_snapshot_size(
        const cJSON *json,
        TZrSize *outValue) {
    TZrSize value;

    if (!cJSON_IsNumber((cJSON *)json) || json->valuedouble < 0.0 ||
        json->valuedouble > ZR_LSP_JSON_SAFE_INTEGER_MAX ||
        json->valuedouble > (double)SIZE_MAX ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    value = (TZrSize)json->valuedouble;
    if ((double)value != json->valuedouble) {
        return ZR_FALSE;
    }
    *outValue = value;
    return ZR_TRUE;
}

/**
 * @brief 还原代码操作 data 中以固定宽度字符串保存的 64 位指纹。
 * @details 与 serialize_snapshot_u64 成对使用，避免把完整指纹写成会丢精度的 JSON 数字。
 * TODO: 当前仅检查 16 字符长度和 strtoull 完整消费；后者仍接受前导空白或符号。
 *       核对 resolve 对非规范客户端 data 的约束，并为此类输入补协议测试。
 */
static TZrBool parse_code_action_snapshot_hash(
        const cJSON *json,
        TZrUInt64 *outValue) {
    char *end;
    unsigned long long value;

    if (!cJSON_IsString((cJSON *)json) || json->valuestring == NULL ||
        strlen(json->valuestring) != 16U || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    errno = 0;
    end = ZR_NULL;
    value = strtoull(json->valuestring, &end, 16);
    if (errno != 0 || end == json->valuestring || *end != '\0') {
        return ZR_FALSE;
    }
    *outValue = (TZrUInt64)value;
    return ZR_TRUE;
}

/**
 * @brief 还原文档、项目、提供者、语义和依赖五项身份，供过期代码操作判定。
 * @details 任一字段缺失即拒绝整组身份；调用方只在 data 包含 semanticIdentity 时调用。
 */
static TZrBool parse_code_action_semantic_identity(
        const cJSON *json,
        SZrLspSemanticSnapshotIdentity *outIdentity) {
    if (!cJSON_IsObject((cJSON *)json) || outIdentity == ZR_NULL) {
        return ZR_FALSE;
    }
    return parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_DOCUMENT_GENERATION),
                   &outIdentity->documentGeneration) &&
           parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_PROJECT_GENERATION),
                   &outIdentity->projectGeneration) &&
           parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_PROVIDER_GENERATION),
                   &outIdentity->providerGeneration) &&
           parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_SEMANTIC_GENERATION),
                   &outIdentity->semanticGeneration) &&
           parse_code_action_snapshot_hash(
                   get_object_item(json, ZR_LSP_FIELD_DEPENDENCY_FINGERPRINT),
                   &outIdentity->dependencyFingerprint);
}

/**
 * @brief 从服务端先前签发的 CodeAction.data 中恢复用于 resolve 的文档快照。
 * @details URI 经 server_get_cached_uri 对接当前缓存；随后由
 *          ValidateDocumentSnapshot 判定原操作是否仍对应当前文档和语义状态。
 *          data 缺失、字段越界或身份不完整均视为无效请求。
 */
static TZrBool parse_code_action_document_snapshot(
        SZrStdioServer *server,
        const cJSON *params,
        SZrLspWorkspaceEditDocumentSnapshot *outSnapshot) {
    const cJSON *data;
    const cJSON *uriJson;
    const cJSON *snapshotJson;
    const cJSON *isOpenDocumentJson;
    const cJSON *semanticIdentityJson;

    if (server == ZR_NULL || outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }
    data = get_object_item(params, ZR_LSP_FIELD_DATA);
    uriJson = get_object_item(data, ZR_LSP_FIELD_URI);
    snapshotJson = get_object_item(data, ZR_LSP_FIELD_SNAPSHOT);
    isOpenDocumentJson =
            get_object_item(snapshotJson, ZR_LSP_FIELD_IS_OPEN_DOCUMENT);
    if (!cJSON_IsString((cJSON *)uriJson) || uriJson->valuestring == NULL ||
        !cJSON_IsObject((cJSON *)snapshotJson) ||
        !cJSON_IsBool((cJSON *)isOpenDocumentJson)) {
        return ZR_FALSE;
    }

    memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->uri = server_get_cached_uri(server, uriJson->valuestring);
    outSnapshot->isOpenDocument = cJSON_IsTrue((cJSON *)isOpenDocumentJson)
                                      ? ZR_TRUE
                                      : ZR_FALSE;
    if (outSnapshot->uri == ZR_NULL ||
        !parse_code_action_snapshot_hash(
                get_object_item(snapshotJson, ZR_LSP_FIELD_CONTENT_HASH),
                &outSnapshot->contentHash) ||
        !parse_code_action_snapshot_size(
                get_object_item(snapshotJson, ZR_LSP_FIELD_CONTENT_LENGTH),
                &outSnapshot->contentLength) ||
        !parse_code_action_snapshot_size(
                get_object_item(snapshotJson, ZR_LSP_FIELD_VERSION),
                &outSnapshot->version) ||
        !parse_code_action_snapshot_size(
                get_object_item(snapshotJson, ZR_LSP_FIELD_CONTENT_GENERATION),
                &outSnapshot->contentGeneration)) {
        return ZR_FALSE;
    }

    semanticIdentityJson = get_object_item(snapshotJson, ZR_LSP_FIELD_SEMANTIC_IDENTITY);
    if (semanticIdentityJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!parse_code_action_semantic_identity(
                semanticIdentityJson, &outSnapshot->semanticIdentity)) {
        return ZR_FALSE;
    }
    outSnapshot->hasSemanticIdentity = ZR_TRUE;
    return ZR_TRUE;
}

/**
 * @brief 将已过期的代码操作转为带 disabled.reason 的响应，防止客户端应用旧 edit。
 * @details resolve 收到失效快照时调用；返回新建 JSON，由响应路径接管并释放。
 */
static cJSON *disable_stale_code_action(const cJSON *params) {
    cJSON *result;
    cJSON *disabled;

    result = params != NULL
                 ? cJSON_Duplicate((cJSON *)params, 1)
                 : cJSON_CreateObject();
    if (result == NULL || !cJSON_IsObject(result)) {
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_DeleteItemFromObjectCaseSensitive(result, ZR_LSP_FIELD_EDIT);
    cJSON_DeleteItemFromObjectCaseSensitive(result, ZR_LSP_FIELD_DISABLED);
    disabled = cJSON_CreateObject();
    if (disabled == NULL ||
        cJSON_AddStringToObject(disabled, ZR_LSP_FIELD_REASON, ZR_LSP_CODE_ACTION_STALE_REASON) == NULL ||
        !cJSON_AddItemToObject(result, ZR_LSP_FIELD_DISABLED, disabled)) {
        cJSON_Delete(disabled);
        cJSON_Delete(result);
        return NULL;
    }
    return result;
}

/**
 * @brief 处理 textDocument/formatting，将接口层编辑结果交给统一响应编码路径。
 * @details 请求分发器取得活动语义快照后调用；接口层拥有 edits，序列化完成后统一释放。
 *          接口层无结果时返回空数组，协议参数错误则交由分发器形成错误响应。
 */
SZrLspHandlerResult handle_formatting_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray edits = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetFormatting(server->state, server->context, uri, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    ZR_UNUSED_PARAMETER(uriText);
    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return stdio_handler_result_from_json(server->context, result);
}

/**
 * @brief 处理单一区间格式化，先按 URI 将客户端 range 转成接口层内部位置。
 * @details 与全文格式化共用文本编辑序列化和释放路径；只接受可解析的文档 URI 与 range。
 */
SZrLspHandlerResult handle_range_formatting_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray edits = {0};
    SZrLspRange range;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    if (!parse_range_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_RANGE), &range)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    ZR_UNUSED_PARAMETER(uriText);
    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return stdio_handler_result_from_json(server->context, result);
}

/**
 * @brief 处理多区间格式化，对每个请求区间调用单区间接口并汇总 TextEdit 数组。
 * @details 仅在客户端声明 rangesFormatting 能力时由分发器路由；每轮接口层 edits
 *          在进入下一轮前释放，最终 JSON 数组交由响应路径接管。
 */
SZrLspHandlerResult handle_ranges_formatting_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *rangesJson;
    const cJSON *rangeJson;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    rangesJson = get_object_item(params, ZR_LSP_FIELD_RANGES);
    if (!cJSON_IsArray(rangesJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    result = cJSON_CreateArray();
    if (result == NULL) {
        return stdio_handler_result_from_json(server->context, ZR_NULL);
    }

    /* 每个区间独立转换位置并释放结果；输出仍是一个协议 TextEdit 数组。
     * BUG: 相同或重叠的非空输入区间可生成重叠的整行替换编辑；这里不去重，
     * 客户端收到违反 TextEdit[] 非重叠约束的数组，无法可靠一次性应用。
     */
    cJSON_ArrayForEach(rangeJson, rangesJson) {
        SZrArray edits = {0};
        SZrLspRange range;

        if (!parse_range_for_uri(server, uri, rangeJson, &range)) {
            cJSON_Delete(result);
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }

        ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        if (ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
            for (TZrSize index = 0; index < edits.length; index++) {
                SZrLspTextEdit **editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(&edits, index);
                if (editPtr != ZR_NULL && *editPtr != ZR_NULL) {
                    cJSON_AddItemToArray(result, serialize_text_edit(*editPtr));
                }
            }
        }
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    }

    ZR_UNUSED_PARAMETER(uriText);
    return stdio_handler_result_from_json(server->context, result);
}

/**
 * @brief 处理文档输入触发的格式化，仅对 initialize 声明的 `}` 和 `;` 请求格式化当前行前缀。
 * @details 客户端位置先转内部编码；触发字符超出服务端声明范围时按无效参数返回。
 */
SZrLspHandlerResult handle_on_type_formatting_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *chJson;
    SZrArray edits = {0};
    const char *uriText;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspRange range;
    cJSON *result;

    chJson = get_object_item(params, ZR_LSP_FIELD_CH);
    if (!cJSON_IsString(chJson) ||
        (strcmp(chJson->valuestring, "}") != 0 && strcmp(chJson->valuestring, ";") != 0) ||
        !get_uri_from_text_document(server, params, &uriText, &uri) ||
        !parse_position_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_POSITION), &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZR_UNUSED_PARAMETER(uriText);
    range.start = position;
    range.start.character = 0;
    range.end = position;

    ZrCore_Array_Init(server->state, &edits, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetRangeFormatting(server->state, server->context, uri, range, &edits)) {
        ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_text_edits_array(&edits);
    ZrLanguageServer_Lsp_FreeTextEdits(server->state, &edits);
    return stdio_handler_result_from_json(server->context, result);
}

/**
 * @brief 处理 textDocument/codeAction，并给每个操作携带可供 resolve 校验的文档快照。
 * @details 先核对 context，再捕获快照、取得接口层操作并复验快照；序列化阶段
 *          根据 context.only 筛选操作。接口层 actions 在成功和失败路径均释放。
 */
SZrLspHandlerResult handle_code_action_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray actions = {0};
    SZrLspRange range = {{0, 0}, {0, 0}};
    SZrLspWorkspaceEditDocumentSnapshot documentSnapshot = {0};
    const cJSON *contextJson;
    const cJSON *diagnosticsJson;
    const cJSON *onlyJson;
    cJSON *itemJson;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    contextJson = get_object_item(params, ZR_LSP_FIELD_CONTEXT);
    if (!cJSON_IsObject((cJSON *)contextJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    diagnosticsJson = get_object_item(contextJson, ZR_LSP_FIELD_DIAGNOSTICS);
    if (!cJSON_IsArray((cJSON *)diagnosticsJson)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    cJSON_ArrayForEach(itemJson, (cJSON *)diagnosticsJson) {
        if (!cJSON_IsObject(itemJson)) {
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }
    }
    onlyJson = get_object_item(contextJson, ZR_LSP_FIELD_ONLY);
    if (onlyJson != ZR_NULL) {
        if (!cJSON_IsArray((cJSON *)onlyJson)) {
            return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
        }
        cJSON_ArrayForEach(itemJson, (cJSON *)onlyJson) {
            if (!cJSON_IsString(itemJson) || itemJson->valuestring == ZR_NULL) {
                return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
            }
        }
    }
    if (!ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshot(
                server->state,
                server->context,
                uri,
                &documentSnapshot)) {
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    if (!parse_range_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_RANGE), &range)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZrCore_Array_Init(server->state, &actions, sizeof(SZrLspCodeAction *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCodeActions(server->state, server->context, uri, range, &actions)) {
        ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    /* 计算操作期间文档或语义身份若变化，不签发可能落到旧状态的编辑。 */
    if (!ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot(
                server->state,
                server->context,
                &documentSnapshot)) {
        ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    result = serialize_code_actions_array(
            uriText, &documentSnapshot, &actions, params);
    ZrLanguageServer_Lsp_FreeCodeActions(server->state, &actions);
    return stdio_handler_result_from_json(server->context, result);
}

/**
 * @brief 处理 codeAction/resolve，依据 data 中的快照决定保留操作还是禁用旧编辑。
 * @details 当前操作在首轮响应即含 edit；resolve 只复验有效期，不重新计算操作。
 *          返回的是输入的深拷贝或去掉 edit 的禁用深拷贝，均由响应路径释放。
 */
SZrLspHandlerResult handle_code_action_resolve_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspWorkspaceEditDocumentSnapshot documentSnapshot = {0};

    if (server == ZR_NULL || !cJSON_IsObject((cJSON *)params) ||
        !parse_code_action_document_snapshot(
                server, params, &documentSnapshot)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    if (!ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshot(
                server->state,
                server->context,
                &documentSnapshot)) {
        return stdio_handler_result_from_json(server->context, disable_stale_code_action(params));
    }
    return stdio_handler_result_from_json(server->context, cJSON_Duplicate((cJSON *)params, 1));
}
