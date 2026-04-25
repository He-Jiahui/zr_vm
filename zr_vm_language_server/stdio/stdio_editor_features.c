#include "zr_vm_language_server_stdio_internal.h"

typedef TZrBool (*TZrLspLocationProvider)(SZrState *state,
                                          SZrLspContext *context,
                                          SZrString *uri,
                                          SZrLspPosition position,
                                          SZrArray *result);

static cJSON *serialize_folding_range(const SZrLspFoldingRange *range) {
    cJSON *json;
    char *kindText;

    if (range == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_START_LINE, range->startLine);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_START_CHARACTER, range->startCharacter);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_END_LINE, range->endLine);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_END_CHARACTER, range->endCharacter);
    kindText = zr_string_to_c_string(range->kind);
    if (kindText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_KIND, kindText);
    }
    free(kindText);
    return json;
}

static cJSON *serialize_folding_ranges_array(SZrArray *ranges) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || ranges == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < ranges->length; index++) {
        SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(ranges, index);
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_folding_range(*rangePtr));
        }
    }

    return json;
}

static cJSON *serialize_selection_range(const SZrLspSelectionRange *range) {
    cJSON *json;

    if (range == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(range->range));
    if (range->hasParent) {
        cJSON *parent = cJSON_CreateObject();
        if (parent != NULL) {
            cJSON_AddItemToObject(parent, ZR_LSP_FIELD_RANGE, serialize_range(range->parentRange));
            if (range->hasGrandParent) {
                cJSON *grandParent = cJSON_CreateObject();
                if (grandParent != NULL) {
                    cJSON_AddItemToObject(grandParent,
                                          ZR_LSP_FIELD_RANGE,
                                          serialize_range(range->grandParentRange));
                    cJSON_AddItemToObject(parent, ZR_LSP_FIELD_PARENT, grandParent);
                }
            }
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_PARENT, parent);
        }
    }
    return json;
}

static cJSON *serialize_selection_ranges_array(SZrArray *ranges) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || ranges == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < ranges->length; index++) {
        SZrLspSelectionRange **rangePtr = (SZrLspSelectionRange **)ZrCore_Array_Get(ranges, index);
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_selection_range(*rangePtr));
        }
    }

    return json;
}

static cJSON *serialize_document_link(const SZrLspDocumentLink *link) {
    cJSON *json;
    char *targetText;
    char *tooltipText;

    if (link == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(link->range));
    targetText = zr_string_to_c_string(link->target);
    tooltipText = zr_string_to_c_string(link->tooltip);
    if (targetText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_TARGET, targetText);
    }
    if (tooltipText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_TOOLTIP, tooltipText);
    }

    free(targetText);
    free(tooltipText);
    return json;
}

static cJSON *serialize_document_links_array(SZrArray *links) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || links == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < links->length; index++) {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(links, index);
        if (linkPtr != ZR_NULL && *linkPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_document_link(*linkPtr));
        }
    }

    return json;
}

static cJSON *serialize_code_lens(const SZrLspCodeLens *lens) {
    cJSON *json;
    cJSON *command;
    cJSON *arguments;
    char *titleText;
    char *commandText;
    char *argumentText;

    if (lens == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    command = cJSON_CreateObject();
    arguments = cJSON_CreateArray();
    if (json == NULL || command == NULL || arguments == NULL) {
        cJSON_Delete(json);
        cJSON_Delete(command);
        cJSON_Delete(arguments);
        return NULL;
    }

    titleText = zr_string_to_c_string(lens->commandTitle);
    commandText = zr_string_to_c_string(lens->command);
    argumentText = zr_string_to_c_string(lens->argument);

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(lens->range));
    cJSON_AddStringToObject(command, ZR_LSP_FIELD_TITLE, titleText != NULL ? titleText : "");
    cJSON_AddStringToObject(command, ZR_LSP_FIELD_COMMAND, commandText != NULL ? commandText : "");
    if (argumentText != NULL) {
        cJSON_AddItemToArray(arguments, cJSON_CreateString(argumentText));
    }
    if (lens->hasPositionArgument) {
        cJSON *position = cJSON_CreateObject();
        if (position != NULL) {
            cJSON_AddNumberToObject(position, ZR_LSP_FIELD_LINE, lens->positionArgument.line);
            cJSON_AddNumberToObject(position, ZR_LSP_FIELD_CHARACTER, lens->positionArgument.character);
            cJSON_AddItemToArray(arguments, position);
        }
    }
    cJSON_AddItemToObject(command, ZR_LSP_FIELD_ARGUMENTS, arguments);
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_COMMAND, command);

    free(titleText);
    free(commandText);
    free(argumentText);
    return json;
}

static cJSON *serialize_code_lens_array(SZrArray *lenses) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || lenses == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < lenses->length; index++) {
        SZrLspCodeLens **lensPtr = (SZrLspCodeLens **)ZrCore_Array_Get(lenses, index);
        if (lensPtr != ZR_NULL && *lensPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_code_lens(*lensPtr));
        }
    }

    return json;
}

static cJSON *serialize_hierarchy_item(const SZrLspHierarchyItem *item) {
    cJSON *json;
    char *nameText;
    char *detailText;
    char *uriText;

    if (item == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    nameText = zr_string_to_c_string(item->name);
    detailText = zr_string_to_c_string(item->detail);
    uriText = zr_string_to_c_string(item->uri);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_NAME, nameText != NULL ? nameText : "");
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, item->kind);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(item->range));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_SELECTION_RANGE, serialize_range(item->selectionRange));
    if (detailText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_DETAIL, detailText);
    }

    free(nameText);
    free(detailText);
    free(uriText);
    return json;
}

static cJSON *serialize_hierarchy_items_array(SZrArray *items) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || items == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < items->length; index++) {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_hierarchy_item(*itemPtr));
        }
    }

    return json;
}

static cJSON *serialize_ranges_array(SZrArray *ranges) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || ranges == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < ranges->length; index++) {
        SZrLspRange *range = (SZrLspRange *)ZrCore_Array_Get(ranges, index);
        if (range != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_range(*range));
        }
    }

    return json;
}

static cJSON *serialize_hierarchy_call(const SZrLspHierarchyCall *call, TZrBool outgoing) {
    cJSON *json;

    if (call == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json,
                          outgoing ? ZR_LSP_FIELD_TO : ZR_LSP_FIELD_FROM,
                          serialize_hierarchy_item(call->item));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_FROM_RANGES, serialize_ranges_array((SZrArray *)&call->fromRanges));
    return json;
}

static cJSON *serialize_hierarchy_calls_array(SZrArray *calls, TZrBool outgoing) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || calls == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < calls->length; index++) {
        SZrLspHierarchyCall **callPtr = (SZrLspHierarchyCall **)ZrCore_Array_Get(calls, index);
        if (callPtr != ZR_NULL && *callPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_hierarchy_call(*callPtr, outgoing));
        }
    }

    return json;
}

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

void add_advanced_editor_capabilities(cJSON *capabilities) {
    cJSON *codeActionProvider;
    cJSON *codeActionKinds;
    cJSON *onTypeFormattingProvider;
    cJSON *onTypeMoreTriggers;
    cJSON *documentLinkProvider;
    cJSON *codeLensProvider;
    cJSON *executeCommandProvider;
    cJSON *executeCommands;
    cJSON *diagnosticProvider;

    if (capabilities == NULL) {
        return;
    }

    codeActionProvider = cJSON_CreateObject();
    codeActionKinds = cJSON_CreateArray();
    if (codeActionProvider != NULL && codeActionKinds != NULL) {
        cJSON_AddItemToArray(codeActionKinds, cJSON_CreateString(ZR_LSP_CODE_ACTION_KIND_QUICK_FIX));
        cJSON_AddItemToArray(codeActionKinds, cJSON_CreateString(ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS));
        cJSON_AddItemToObject(codeActionProvider, ZR_LSP_FIELD_CODE_ACTION_KINDS, codeActionKinds);
        cJSON_AddBoolToObject(codeActionProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_CODE_ACTION_PROVIDER, codeActionProvider);
    } else {
        cJSON_Delete(codeActionProvider);
        cJSON_Delete(codeActionKinds);
    }

    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_FORMATTING_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER, 1);
    onTypeFormattingProvider = cJSON_CreateObject();
    onTypeMoreTriggers = cJSON_CreateArray();
    if (onTypeFormattingProvider != NULL && onTypeMoreTriggers != NULL) {
        cJSON_AddStringToObject(onTypeFormattingProvider, ZR_LSP_FIELD_FIRST_TRIGGER_CHARACTER, "}");
        cJSON_AddItemToArray(onTypeMoreTriggers, cJSON_CreateString(";"));
        cJSON_AddItemToObject(onTypeFormattingProvider,
                              ZR_LSP_FIELD_MORE_TRIGGER_CHARACTER,
                              onTypeMoreTriggers);
        cJSON_AddItemToObject(capabilities,
                              ZR_LSP_FIELD_DOCUMENT_ON_TYPE_FORMATTING_PROVIDER,
                              onTypeFormattingProvider);
    } else {
        cJSON_Delete(onTypeFormattingProvider);
        cJSON_Delete(onTypeMoreTriggers);
    }
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_FOLDING_RANGE_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_SELECTION_RANGE_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_LINKED_EDITING_RANGE_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_MONIKER_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_INLINE_VALUE_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_INLINE_COMPLETION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_COLOR_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_DECLARATION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_TYPE_DEFINITION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_IMPLEMENTATION_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_CALL_HIERARCHY_PROVIDER, 1);
    cJSON_AddBoolToObject(capabilities, ZR_LSP_FIELD_TYPE_HIERARCHY_PROVIDER, 1);

    documentLinkProvider = cJSON_CreateObject();
    if (documentLinkProvider != NULL) {
        cJSON_AddBoolToObject(documentLinkProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER, documentLinkProvider);
    }

    codeLensProvider = cJSON_CreateObject();
    if (codeLensProvider != NULL) {
        cJSON_AddBoolToObject(codeLensProvider, ZR_LSP_FIELD_RESOLVE_PROVIDER, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_CODE_LENS_PROVIDER, codeLensProvider);
    }

    executeCommandProvider = cJSON_CreateObject();
    executeCommands = cJSON_CreateArray();
    if (executeCommandProvider != NULL && executeCommands != NULL) {
        cJSON_AddItemToArray(executeCommands, cJSON_CreateString(ZR_LSP_COMMAND_RUN_CURRENT_PROJECT));
        cJSON_AddItemToArray(executeCommands, cJSON_CreateString(ZR_LSP_COMMAND_SHOW_REFERENCES));
        cJSON_AddItemToObject(executeCommandProvider, ZR_LSP_FIELD_COMMANDS, executeCommands);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_EXECUTE_COMMAND_PROVIDER, executeCommandProvider);
    } else {
        cJSON_Delete(executeCommandProvider);
        cJSON_Delete(executeCommands);
    }

    diagnosticProvider = cJSON_CreateObject();
    if (diagnosticProvider != NULL) {
        cJSON_AddBoolToObject(diagnosticProvider, ZR_LSP_FIELD_INTER_FILE_DEPENDENCIES, 1);
        cJSON_AddBoolToObject(diagnosticProvider, ZR_LSP_FIELD_WORKSPACE_DIAGNOSTICS, 1);
        cJSON_AddItemToObject(capabilities, ZR_LSP_FIELD_DIAGNOSTIC_PROVIDER, diagnosticProvider);
    }
}

static TZrBool completion_item_label_matches(SZrLspCompletionItem *item, const char *label) {
    char *itemLabel;
    TZrBool matches;

    if (item == ZR_NULL || label == NULL) {
        return ZR_FALSE;
    }

    itemLabel = zr_string_to_c_string(item->label);
    matches = itemLabel != NULL && strcmp(itemLabel, label) == 0;
    free(itemLabel);
    return matches;
}

static cJSON *serialize_resolved_completion_item(const cJSON *params,
                                                 const cJSON *data,
                                                 SZrLspCompletionItem *item) {
    cJSON *resolved = serialize_completion_item(item);

    if (resolved == NULL) {
        return params != NULL ? cJSON_Duplicate((cJSON *)params, 1) : cJSON_CreateObject();
    }

    if (data != NULL) {
        cJSON_AddItemToObject(resolved, ZR_LSP_FIELD_DATA, cJSON_Duplicate((cJSON *)data, 1));
    }
    return resolved;
}

cJSON *handle_completion_item_resolve_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *labelJson;
    const cJSON *data;
    const cJSON *uriJson;
    const cJSON *positionJson;
    const char *uriText;
    const char *label;
    SZrString *uri;
    SZrLspPosition position;
    SZrArray completions = {0};
    cJSON *result;

    if (server == ZR_NULL || params == NULL) {
        return cJSON_CreateObject();
    }

    labelJson = get_object_item(params, ZR_LSP_FIELD_LABEL);
    data = get_object_item(params, ZR_LSP_FIELD_DATA);
    uriJson = get_object_item(data, ZR_LSP_FIELD_URI);
    positionJson = get_object_item(data, ZR_LSP_FIELD_POSITION);
    if (!cJSON_IsString((cJSON *)labelJson) ||
        !cJSON_IsString((cJSON *)uriJson) ||
        !parse_position(positionJson, &position)) {
        return cJSON_Duplicate((cJSON *)params, 1);
    }

    label = cJSON_GetStringValue((cJSON *)labelJson);
    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    uri = server_get_cached_uri(server, uriText);
    if (label == NULL || uri == ZR_NULL) {
        return cJSON_Duplicate((cJSON *)params, 1);
    }

    if (!ZrLanguageServer_Lsp_GetCompletion(server->state, server->context, uri, position, &completions)) {
        return cJSON_Duplicate((cJSON *)params, 1);
    }

    result = ZR_NULL;
    for (TZrSize index = 0; index < completions.length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(&completions, index);

        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL && completion_item_label_matches(*itemPtr, label)) {
            result = serialize_resolved_completion_item(params, data, *itemPtr);
            break;
        }
    }

    free_completion_items_array(server->state, &completions);
    return result != NULL ? result : cJSON_Duplicate((cJSON *)params, 1);
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

static void build_diagnostic_result_id(SZrStdioServer *server,
                                       SZrString *uri,
                                       char *buffer,
                                       size_t bufferSize) {
    SZrFileVersion *fileVersion;
    const TZrChar *content;
    unsigned long long hash = 1469598103934665603ULL;
    TZrSize version = 0;
    TZrSize length = 0;

    if (buffer == NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL) {
        content = fileVersion->content;
        version = fileVersion->version;
        length = fileVersion->contentLength;
        if (content != ZR_NULL) {
            for (TZrSize index = 0; index < length; index++) {
                hash ^= (unsigned char)content[index];
                hash *= 1099511628211ULL;
            }
        }
    }

    snprintf(buffer,
             bufferSize,
             "zr:%llu:%llu:%llx",
             (unsigned long long)version,
             (unsigned long long)length,
             hash);
}

static TZrBool workspace_previous_result_id_matches(const cJSON *previousResultIds,
                                                    const char *uriText,
                                                    const char *resultId) {
    if (!cJSON_IsArray((cJSON *)previousResultIds) || uriText == NULL || resultId == NULL) {
        return ZR_FALSE;
    }

    for (const cJSON *entry = previousResultIds->child; entry != NULL; entry = entry->next) {
        const cJSON *entryUri = get_object_item(entry, ZR_LSP_FIELD_URI);
        const cJSON *entryValue = get_object_item(entry, ZR_LSP_FIELD_VALUE);
        if (cJSON_IsString((cJSON *)entryUri) &&
            cJSON_IsString((cJSON *)entryValue) &&
            strcmp(entryUri->valuestring, uriText) == 0 &&
            strcmp(entryValue->valuestring, resultId) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

cJSON *handle_text_document_diagnostic_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray diagnostics = {0};
    const char *uriText;
    SZrString *uri;
    const cJSON *previousResultIdJson;
    char resultId[96];
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);
            cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, cJSON_CreateArray());
        }
        return result;
    }

    ZR_UNUSED_PARAMETER(uriText);
    build_diagnostic_result_id(server, uri, resultId, sizeof(resultId));
    previousResultIdJson = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID);
    if (cJSON_IsString((cJSON *)previousResultIdJson) &&
        strcmp(previousResultIdJson->valuestring, resultId) == 0) {
        result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddStringToObject(result,
                                    ZR_LSP_FIELD_KIND,
                                    ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED);
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
        }
        return result;
    }

    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        free_diagnostics_array(server->state, &diagnostics);
        result = cJSON_CreateObject();
        if (result != NULL) {
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);
            cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
            cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, cJSON_CreateArray());
        }
        return result;
    }

    result = cJSON_CreateObject();
    if (result != NULL) {
        cJSON_AddStringToObject(result, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);
        cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
        cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, serialize_diagnostics_array(&diagnostics));
    }
    free_diagnostics_array(server->state, &diagnostics);
    return result;
}

static cJSON *serialize_workspace_diagnostic_report_for_uri(SZrStdioServer *server,
                                                            SZrString *uri,
                                                            const cJSON *previousResultIds) {
    SZrArray diagnostics = {0};
    cJSON *report;
    char *uriText;
    char resultId[96];
    SZrFileVersion *fileVersion;

    report = cJSON_CreateObject();
    if (report == NULL) {
        return NULL;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    uriText = zr_string_to_c_string(uri);
    build_diagnostic_result_id(server, uri, resultId, sizeof(resultId));
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    if (fileVersion != ZR_NULL) {
        cJSON_AddNumberToObject(report, ZR_LSP_FIELD_VERSION, (double)fileVersion->version);
    } else {
        cJSON_AddNullToObject(report, ZR_LSP_FIELD_VERSION);
    }
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_RESULT_ID, resultId);
    if (workspace_previous_result_id_matches(previousResultIds, uriText, resultId)) {
        cJSON_AddStringToObject(report,
                                ZR_LSP_FIELD_KIND,
                                ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED);
        free(uriText);
        return report;
    }
    cJSON_AddStringToObject(report, ZR_LSP_FIELD_KIND, ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL);

    ZrCore_Array_Init(server->state, &diagnostics, sizeof(SZrLspDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (uri == ZR_NULL || !ZrLanguageServer_Lsp_GetDiagnostics(server->state, server->context, uri, &diagnostics)) {
        free_diagnostics_array(server->state, &diagnostics);
        cJSON_AddItemToObject(report, ZR_LSP_FIELD_ITEMS, cJSON_CreateArray());
        free(uriText);
        return report;
    }

    cJSON_AddItemToObject(report, ZR_LSP_FIELD_ITEMS, serialize_diagnostics_array(&diagnostics));
    free_diagnostics_array(server->state, &diagnostics);
    free(uriText);
    return report;
}

cJSON *handle_workspace_diagnostic_request(SZrStdioServer *server, const cJSON *params) {
    cJSON *result;
    cJSON *items;
    SZrHashSet *fileMap;
    const cJSON *previousResultIds = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_IDS);
    result = cJSON_CreateObject();
    items = cJSON_CreateArray();
    if (result == NULL || items == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(items);
        return NULL;
    }

    if (server != ZR_NULL &&
        server->context != ZR_NULL &&
        server->context->parser != ZR_NULL) {
        fileMap = &server->context->parser->uriToFileMap;
        if (fileMap->isValid && fileMap->buckets != ZR_NULL) {
            for (TZrSize bucketIndex = 0; bucketIndex < fileMap->capacity; bucketIndex++) {
                SZrHashKeyValuePair *pair = fileMap->buckets[bucketIndex];
                while (pair != ZR_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                        SZrFileVersion *fileVersion =
                                (SZrFileVersion *)pair->value.value.nativeObject.nativePointer;
                        if (fileVersion != ZR_NULL && fileVersion->uri != ZR_NULL) {
                            cJSON *report =
                                    serialize_workspace_diagnostic_report_for_uri(server,
                                                                                  fileVersion->uri,
                                                                                  previousResultIds);
                            if (report != NULL) {
                                cJSON_AddItemToArray(items, report);
                            }
                        }
                    }
                    pair = pair->next;
                }
            }
        }
    }

    if (result != NULL) {
        cJSON_AddItemToObject(result, ZR_LSP_FIELD_ITEMS, items);
    }
    return result;
}
