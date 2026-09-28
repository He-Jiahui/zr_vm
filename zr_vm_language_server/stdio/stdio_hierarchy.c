#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

/* follow-up 请求回传 prepare 的 item；把显示字段、URI 和数据身份恢复为本次查询可用的临时视图。
 * URI 由服务端缓存持有，name/detail 由 VM 状态持有；栈上 item 不接管这些对象。 */
static int parse_hierarchy_item(SZrStdioServer *server, const cJSON *params, SZrLspHierarchyItem *outItem) {
    const cJSON *itemJson;
    const cJSON *nameJson;
    const cJSON *detailJson;
    const cJSON *kindJson;
    const cJSON *uriJson;
    const cJSON *selectionRangeJson;
    const cJSON *dataJson;
    const cJSON *symbolIdJson;
    const cJSON *typeIdJson;
    const cJSON *versionJson;
    TZrSize symbolIdValue;
    TZrSize typeIdValue;
    TZrSize versionValue;

    if (server == ZR_NULL || outItem == ZR_NULL) {
        return 0;
    }

    memset(outItem, 0, sizeof(SZrLspHierarchyItem));
    itemJson = get_object_item(params, ZR_LSP_FIELD_ITEM);
    nameJson = get_object_item(itemJson, ZR_LSP_FIELD_NAME);
    detailJson = get_object_item(itemJson, ZR_LSP_FIELD_DETAIL);
    kindJson = get_object_item(itemJson, ZR_LSP_FIELD_KIND);
    uriJson = get_object_item(itemJson, ZR_LSP_FIELD_URI);
    selectionRangeJson = get_object_item(itemJson, ZR_LSP_FIELD_SELECTION_RANGE);
    dataJson = get_object_item(itemJson, ZR_LSP_FIELD_DATA);
    symbolIdJson = get_object_item(dataJson, ZR_LSP_FIELD_SYMBOL_ID);
    typeIdJson = get_object_item(dataJson, ZR_LSP_FIELD_TYPE_ID);
    versionJson = get_object_item(dataJson, ZR_LSP_FIELD_VERSION);

    if (!cJSON_IsString(nameJson) ||
        !cJSON_IsString(uriJson) ||
        !cJSON_IsNumber(kindJson)) {
        return 0;
    }

    outItem->uri = server_get_cached_uri(server, uriJson->valuestring);
    if (!parse_range_for_uri(server,
                             outItem->uri,
                             get_object_item(itemJson, ZR_LSP_FIELD_RANGE),
                             &outItem->range)) {
        return 0;
    }

    outItem->name = ZrCore_String_Create(server->state,
                                         (TZrNativeString)nameJson->valuestring,
                                         strlen(nameJson->valuestring));
    outItem->detail = cJSON_IsString(detailJson)
                          ? ZrCore_String_Create(server->state,
                                                 (TZrNativeString)detailJson->valuestring,
                                                 strlen(detailJson->valuestring))
                          : ZR_NULL;
    /* BUG: kind 只验证为 JSON 数字，超出 int32 范围仍转为 TZrInt32；
     * 畸形 item 可触发未定义的浮点到整数转换。应先检查有限整数和范围。 */
    outItem->kind = (TZrInt32)kindJson->valuedouble;
    /* 身份三元组须完整且可无损收窄；缺失或不匹配当前语义目标时，后续查询拒绝 item。
     * TODO: 版本号仍经 cJSON double 解析，超过 2^53 时可能先舍入；见 stdio_lsp_parse.c，
     * 核查文档版本上界，并以原始请求验证旧 item 的拒绝行为。 */
    if (parse_size_value_strict(symbolIdJson, &symbolIdValue) &&
        parse_size_value_strict(typeIdJson, &typeIdValue) &&
        parse_size_value_strict(versionJson, &versionValue) &&
        symbolIdValue > 0U && typeIdValue > 0U &&
        (TZrSize)(TZrSymbolId)symbolIdValue == symbolIdValue &&
        (TZrSize)(TZrTypeId)typeIdValue == typeIdValue) {
        outItem->hasSemanticIdentity = ZR_TRUE;
        outItem->semanticId = (TZrSymbolId)symbolIdValue;
        outItem->semanticTypeId = (TZrTypeId)typeIdValue;
        outItem->semanticVersion = versionValue;
    }
    if (selectionRangeJson != NULL &&
        parse_range_for_uri(server, outItem->uri, selectionRangeJson, &outItem->selectionRange)) {
        return outItem->name != ZR_NULL && outItem->uri != ZR_NULL;
    }
    outItem->selectionRange = outItem->range;
    return outItem->name != ZR_NULL && outItem->uri != ZR_NULL;
}

/* call/type prepare 共用位置解析和结果所有权边界；返回的 item 携带后续查询所需身份。 */
static SZrLspHandlerResult handle_prepare_hierarchy_request(SZrStdioServer *server,
                                               const cJSON *params,
                                               TZrBool typeHierarchy) {
    SZrArray items = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;
    TZrBool success;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &items, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    success = typeHierarchy
                  ? ZrLanguageServer_Lsp_PrepareTypeHierarchy(server->state,
                                                              server->context,
                                                              uri,
                                                              position,
                                                              &items)
                  : ZrLanguageServer_Lsp_PrepareCallHierarchy(server->state,
                                                              server->context,
                                                              uri,
                                                              position,
                                                              &items);
    if (!success) {
        ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_hierarchy_items_array(&items);
    ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 为调用层级生成可回传的函数 item；调用方之后可查询入边和出边。 */
SZrLspHandlerResult handle_prepare_call_hierarchy_request(SZrStdioServer *server, const cJSON *params) {
    return handle_prepare_hierarchy_request(server, params, ZR_FALSE);
}

/** @brief 按 prepare item 的语义身份查找调用者，失效身份得到空数组。 */
SZrLspHandlerResult handle_call_hierarchy_incoming_calls_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray calls = {0};
    SZrLspHierarchyItem item;
    cJSON *result;

    if (!parse_hierarchy_item(server, params, &item)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    ZrCore_Array_Init(server->state, &calls, sizeof(SZrLspHierarchyCall *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(server->state, server->context, &item, &calls)) {
        ZrLanguageServer_Lsp_FreeHierarchyCalls(server->state, &calls);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    /* fromRanges 属于调用者，JSON 的 from 键须与 outgoing 的 to 键区分。 */
    result = serialize_hierarchy_calls_array(&calls, ZR_FALSE);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(server->state, &calls);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 按 prepare item 的语义身份查找被调用者，保持调用边方向。 */
SZrLspHandlerResult handle_call_hierarchy_outgoing_calls_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray calls = {0};
    SZrLspHierarchyItem item;
    cJSON *result;

    if (!parse_hierarchy_item(server, params, &item)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    ZrCore_Array_Init(server->state, &calls, sizeof(SZrLspHierarchyCall *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(server->state, server->context, &item, &calls)) {
        ZrLanguageServer_Lsp_FreeHierarchyCalls(server->state, &calls);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    result = serialize_hierarchy_calls_array(&calls, ZR_TRUE);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(server->state, &calls);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 为类型层级生成带版本身份的类型 item，供后续查询直接回传。 */
SZrLspHandlerResult handle_prepare_type_hierarchy_request(SZrStdioServer *server, const cJSON *params) {
    return handle_prepare_hierarchy_request(server, params, ZR_TRUE);
}

/** @brief 从同版本的类型身份枚举父类型；旧版本 item 由语义层拒绝。 */
SZrLspHandlerResult handle_type_hierarchy_supertypes_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray items = {0};
    SZrLspHierarchyItem item;
    cJSON *result;

    if (!parse_hierarchy_item(server, params, &item)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    ZrCore_Array_Init(server->state, &items, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(server->state, server->context, &item, &items)) {
        ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    result = serialize_hierarchy_items_array(&items);
    ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 从同版本的类型身份枚举子类型，避免仅凭展示名称匹配。 */
SZrLspHandlerResult handle_type_hierarchy_subtypes_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray items = {0};
    SZrLspHierarchyItem item;
    cJSON *result;

    if (!parse_hierarchy_item(server, params, &item)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    ZrCore_Array_Init(server->state, &items, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(server->state, server->context, &item, &items)) {
        ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    result = serialize_hierarchy_items_array(&items);
    ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
    return stdio_handler_result_from_json(server->context, result);
}
