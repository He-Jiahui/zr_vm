#include "zr_vm_language_server_stdio_internal.h"

static int parse_hierarchy_item(SZrStdioServer *server, const cJSON *params, SZrLspHierarchyItem *outItem) {
    const cJSON *itemJson;
    const cJSON *nameJson;
    const cJSON *detailJson;
    const cJSON *kindJson;
    const cJSON *uriJson;
    const cJSON *selectionRangeJson;

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

    if (!cJSON_IsString(nameJson) ||
        !cJSON_IsString(uriJson) ||
        !cJSON_IsNumber(kindJson) ||
        !parse_range(get_object_item(itemJson, ZR_LSP_FIELD_RANGE), &outItem->range)) {
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
    outItem->kind = (TZrInt32)kindJson->valuedouble;
    outItem->uri = server_get_cached_uri(server, uriJson->valuestring);
    if (selectionRangeJson != NULL && parse_range(selectionRangeJson, &outItem->selectionRange)) {
        return outItem->name != ZR_NULL && outItem->uri != ZR_NULL;
    }
    outItem->selectionRange = outItem->range;
    return outItem->name != ZR_NULL && outItem->uri != ZR_NULL;
}

static cJSON *handle_prepare_hierarchy_request(SZrStdioServer *server,
                                               const cJSON *params,
                                               TZrBool typeHierarchy) {
    SZrArray items = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;
    TZrBool success;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return cJSON_CreateArray();
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
        return cJSON_CreateArray();
    }

    result = serialize_hierarchy_items_array(&items);
    ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
    return result;
}

cJSON *handle_prepare_call_hierarchy_request(SZrStdioServer *server, const cJSON *params) {
    return handle_prepare_hierarchy_request(server, params, ZR_FALSE);
}

cJSON *handle_call_hierarchy_incoming_calls_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray calls = {0};
    SZrLspHierarchyItem item;
    cJSON *result;

    if (!parse_hierarchy_item(server, params, &item)) {
        return cJSON_CreateArray();
    }
    ZrCore_Array_Init(server->state, &calls, sizeof(SZrLspHierarchyCall *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(server->state, server->context, &item, &calls)) {
        ZrLanguageServer_Lsp_FreeHierarchyCalls(server->state, &calls);
        return cJSON_CreateArray();
    }
    result = serialize_hierarchy_calls_array(&calls, ZR_FALSE);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(server->state, &calls);
    return result;
}

cJSON *handle_call_hierarchy_outgoing_calls_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray calls = {0};
    SZrLspHierarchyItem item;
    cJSON *result;

    if (!parse_hierarchy_item(server, params, &item)) {
        return cJSON_CreateArray();
    }
    ZrCore_Array_Init(server->state, &calls, sizeof(SZrLspHierarchyCall *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(server->state, server->context, &item, &calls)) {
        ZrLanguageServer_Lsp_FreeHierarchyCalls(server->state, &calls);
        return cJSON_CreateArray();
    }
    result = serialize_hierarchy_calls_array(&calls, ZR_TRUE);
    ZrLanguageServer_Lsp_FreeHierarchyCalls(server->state, &calls);
    return result;
}

cJSON *handle_prepare_type_hierarchy_request(SZrStdioServer *server, const cJSON *params) {
    return handle_prepare_hierarchy_request(server, params, ZR_TRUE);
}

cJSON *handle_type_hierarchy_supertypes_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray items = {0};
    SZrLspHierarchyItem item;
    cJSON *result;

    if (!parse_hierarchy_item(server, params, &item)) {
        return cJSON_CreateArray();
    }
    ZrCore_Array_Init(server->state, &items, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(server->state, server->context, &item, &items)) {
        ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
        return cJSON_CreateArray();
    }
    result = serialize_hierarchy_items_array(&items);
    ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
    return result;
}

cJSON *handle_type_hierarchy_subtypes_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray items = {0};
    SZrLspHierarchyItem item;
    cJSON *result;

    if (!parse_hierarchy_item(server, params, &item)) {
        return cJSON_CreateArray();
    }
    ZrCore_Array_Init(server->state, &items, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(server->state, server->context, &item, &items)) {
        ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
        return cJSON_CreateArray();
    }
    result = serialize_hierarchy_items_array(&items);
    ZrLanguageServer_Lsp_FreeHierarchyItems(server->state, &items);
    return result;
}
