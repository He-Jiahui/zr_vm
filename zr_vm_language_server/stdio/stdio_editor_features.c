#include "zr_vm_language_server_stdio_internal.h"

typedef TZrBool (*TZrLspLocationProvider)(SZrState *state,
                                          SZrLspContext *context,
                                          SZrString *uri,
                                          SZrLspPosition position,
                                          SZrArray *result);

static cJSON *handle_location_request(SZrStdioServer *server,
                                      const cJSON *params,
                                      TZrLspLocationProvider provider) {
    SZrArray locations = {0};
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (server == ZR_NULL || provider == NULL ||
        !get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return cJSON_CreateArray();
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &locations, sizeof(SZrLspLocation *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!provider(server->state, server->context, uri, position, &locations)) {
        free_locations_array(server->state, &locations);
        return cJSON_CreateArray();
    }

    result = serialize_locations_array(&locations);
    free_locations_array(server->state, &locations);
    return result;
}

cJSON *handle_folding_range_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray ranges = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &ranges, sizeof(SZrLspFoldingRange *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetFoldingRanges(server->state, server->context, uri, &ranges)) {
        ZrLanguageServer_Lsp_FreeFoldingRanges(server->state, &ranges);
        return cJSON_CreateArray();
    }

    result = serialize_folding_ranges_array(&ranges);
    ZrLanguageServer_Lsp_FreeFoldingRanges(server->state, &ranges);
    return result;
}

cJSON *handle_selection_range_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *positionsJson;
    SZrLspPosition *positions;
    int positionCount;
    SZrArray ranges = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }

    positionsJson = get_object_item(params, "positions");
    if (!cJSON_IsArray((cJSON *)positionsJson)) {
        return cJSON_CreateArray();
    }

    positionCount = cJSON_GetArraySize((cJSON *)positionsJson);
    if (positionCount <= 0) {
        return cJSON_CreateArray();
    }

    positions = (SZrLspPosition *)malloc(sizeof(SZrLspPosition) * (size_t)positionCount);
    if (positions == NULL) {
        return cJSON_CreateArray();
    }
    for (int index = 0; index < positionCount; index++) {
        if (!parse_position(cJSON_GetArrayItem((cJSON *)positionsJson, index), &positions[index])) {
            free(positions);
            return cJSON_CreateArray();
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
        return cJSON_CreateArray();
    }

    result = serialize_selection_ranges_array(&ranges);
    free(positions);
    ZrLanguageServer_Lsp_FreeSelectionRanges(server->state, &ranges);
    return result;
}

cJSON *handle_document_link_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray links = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &links, sizeof(SZrLspDocumentLink *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDocumentLinks(server->state, server->context, uri, &links)) {
        ZrLanguageServer_Lsp_FreeDocumentLinks(server->state, &links);
        return cJSON_CreateArray();
    }

    result = serialize_document_links_array(&links);
    ZrLanguageServer_Lsp_FreeDocumentLinks(server->state, &links);
    return result;
}

cJSON *handle_document_link_resolve_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    return params != NULL ? cJSON_Duplicate((cJSON *)params, 1) : cJSON_CreateObject();
}

cJSON *handle_declaration_request(SZrStdioServer *server, const cJSON *params) {
    return handle_location_request(server, params, ZrLanguageServer_Lsp_GetDeclaration);
}

cJSON *handle_type_definition_request(SZrStdioServer *server, const cJSON *params) {
    return handle_location_request(server, params, ZrLanguageServer_Lsp_GetTypeDefinition);
}

cJSON *handle_implementation_request(SZrStdioServer *server, const cJSON *params) {
    return handle_location_request(server, params, ZrLanguageServer_Lsp_GetImplementation);
}

cJSON *handle_code_lens_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray lenses = {0};
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }

    ZR_UNUSED_PARAMETER(uriText);
    ZrCore_Array_Init(server->state, &lenses, sizeof(SZrLspCodeLens *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetCodeLens(server->state, server->context, uri, &lenses)) {
        ZrLanguageServer_Lsp_FreeCodeLens(server->state, &lenses);
        return cJSON_CreateArray();
    }

    result = serialize_code_lens_array(&lenses);
    ZrLanguageServer_Lsp_FreeCodeLens(server->state, &lenses);
    return result;
}

cJSON *handle_code_lens_resolve_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    return params != NULL ? cJSON_Duplicate((cJSON *)params, 1) : cJSON_CreateObject();
}
