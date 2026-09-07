#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

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
