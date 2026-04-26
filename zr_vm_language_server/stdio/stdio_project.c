#include "zr_vm_language_server_stdio_internal.h"

static cJSON *serialize_project_module_summary(const SZrLspProjectModuleSummary *summary) {
    cJSON *json;
    char *moduleNameText;
    char *displayNameText;
    char *descriptionText;
    char *navigationUriText;

    if (summary == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_SOURCE_KIND, summary->sourceKind);
    cJSON_AddBoolToObject(json, ZR_LSP_FIELD_IS_ENTRY, summary->isEntry ? 1 : 0);
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(summary->range));

    moduleNameText = zr_string_to_c_string(summary->moduleName);
    displayNameText = zr_string_to_c_string(summary->displayName);
    descriptionText = zr_string_to_c_string(summary->description);
    navigationUriText = zr_string_to_c_string(summary->navigationUri);

    if (moduleNameText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_MODULE_NAME, moduleNameText);
    }
    if (displayNameText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_DISPLAY_NAME, displayNameText);
    }
    if (descriptionText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_DESCRIPTION, descriptionText);
    }
    if (navigationUriText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_NAVIGATION_URI, navigationUriText);
    }

    free(moduleNameText);
    free(displayNameText);
    free(descriptionText);
    free(navigationUriText);
    return json;
}

static cJSON *serialize_project_modules_array(SZrArray *modules) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || modules == ZR_NULL) {
        return json;
    }

    for (TZrSize index = 0; index < modules->length; index++) {
        SZrLspProjectModuleSummary **summaryPtr =
            (SZrLspProjectModuleSummary **)ZrCore_Array_Get(modules, index);
        if (summaryPtr != ZR_NULL && *summaryPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_project_module_summary(*summaryPtr));
        }
    }

    return json;
}

cJSON *handle_project_modules_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *uriJson;
    const char *uriText;
    SZrString *projectUri;
    SZrArray modules = {0};
    cJSON *result;

    if (server == ZR_NULL || params == NULL) {
        return NULL;
    }

    uriJson = get_object_item(params, ZR_LSP_FIELD_URI);
    if (!cJSON_IsString((cJSON *)uriJson)) {
        return NULL;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return NULL;
    }

    projectUri = server_get_cached_uri(server, uriText);
    if (projectUri == ZR_NULL) {
        return cJSON_CreateArray();
    }

    ZrCore_Array_Init(server->state, &modules, sizeof(SZrLspProjectModuleSummary *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetProjectModules(server->state, server->context, projectUri, &modules)) {
        ZrLanguageServer_Lsp_FreeProjectModules(server->state, &modules);
        return cJSON_CreateArray();
    }

    result = serialize_project_modules_array(&modules);
    ZrLanguageServer_Lsp_FreeProjectModules(server->state, &modules);
    return result;
}

void handle_zr_selected_project_notification(SZrStdioServer *server, const cJSON *params) {
    const cJSON *uriJson;
    const char *uriText;
    SZrString *cachedUri;

    if (server == ZR_NULL || server->context == ZR_NULL || params == ZR_NULL) {
        return;
    }

    uriJson = get_object_item(params, ZR_LSP_FIELD_URI);
    if (cJSON_IsNull((cJSON *)uriJson)) {
        ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, ZR_NULL);
        return;
    }

    if (!cJSON_IsString((cJSON *)uriJson)) {
        return;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == ZR_NULL || uriText[0] == '\0') {
        ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, ZR_NULL);
        return;
    }

    cachedUri = server_get_cached_uri(server, uriText);
    if (cachedUri == ZR_NULL) {
        return;
    }

    ZrLanguageServer_LspContext_SetClientSelectedZrpUri(server->state, server->context, cachedUri);
}
