#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

/* 把项目层的借用摘要转成扩展树视图消费的 JSON；临时 C 字符串只活到 cJSON 复制字段为止。
 * TODO: cJSON 字段添加失败目前未检查；需用分配失败注入核对是否会返回缺字段的树节点。 */
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

/* GetProjectModules 的摘要数组由调用方继续持有，这里只建立独立的协议结果树。
 * TODO: 数组追加结果未检查；需确认分配失败时是否会静默遗漏后续模块。 */
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

/* zr/projectModules 为项目浏览器按指定 .zrp 懒扫描源图；失败给空树，摘要无论成败均由本层归还。 */
SZrLspHandlerResult handle_project_modules_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *uriJson;
    const char *uriText;
    SZrString *projectUri;
    SZrArray modules = {0};
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

    projectUri = server_get_cached_uri(server, uriText);
    if (projectUri == ZR_NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INTERNAL_ERROR);
    }

    ZrCore_Array_Init(server->state, &modules, sizeof(SZrLspProjectModuleSummary *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetProjectModules(server->state, server->context, projectUri, &modules)) {
        ZrLanguageServer_Lsp_FreeProjectModules(server->state, &modules);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = serialize_project_modules_array(&modules);
    ZrLanguageServer_Lsp_FreeProjectModules(server->state, &modules);
    return stdio_handler_result_from_json(server->context, result);
}

/* 编辑器在初始化后改变项目选择时更新多工程消歧提示；null/空 URI 表示撤销，缺失字段不覆盖旧选择。
 * TODO: 非空但非法的 URI 会先清除旧选择再被 setter 拒绝；核对客户端误发时是否应保留原提示。 */
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
