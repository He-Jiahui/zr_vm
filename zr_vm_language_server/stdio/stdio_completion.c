#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

/** @brief 将补全请求的内部位置定位到文本快照中的字节偏移，供前缀替换使用。
 *  BUG: parse_position_for_uri 交来的 character 是 UTF-16 列，而这里每个 UTF-8
 *  字节都递增列号；非 ASCII 前缀后的补全会定位到错误字节，进而生成错误 textEdit。
 *  同一解析路径也认可单独 CR 换行，本扫描只认 LF；CR 后第二行会退回零宽范围。
 */
static int completion_offset_from_position(const char *content,
                                           size_t contentLength,
                                           SZrLspPosition position,
                                           size_t *outOffset) {
    TZrInt32 line = 0;
    TZrInt32 character = 0;

    if (content == NULL || outOffset == NULL || position.line < 0 || position.character < 0) {
        return 0;
    }

    for (size_t offset = 0; offset < contentLength; offset++) {
        if (line == position.line && character == position.character) {
            *outOffset = offset;
            return 1;
        }

        if (content[offset] == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
    }

    if (line == position.line && character == position.character) {
        *outOffset = contentLength;
        return 1;
    }

    return 0;
}

/** @brief 限定补全替换前缀为 ASCII 标识符片段；不能把标点或空白并入编辑。 */
static int completion_is_identifier_part(char ch) {
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_';
}

/** @brief 从当前文档快照推导候选项的替换范围；快照不可用时退回零宽光标范围。
 *  借入的快照只在本函数内读取并释放，返回范围供普通补全和 resolve 共用。
 */
static SZrLspRange completion_prefix_range(SZrStdioServer *server,
                                           SZrString *uri,
                                           SZrLspPosition position) {
    SZrLspRange range;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const char *content;
    size_t contentLength;
    size_t offset;
    size_t prefixStart;
    TZrInt32 prefixLength;

    range.start = position;
    range.end = position;

    fileVersion = get_file_version_for_uri(server, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        return range;
    }

    content = snapshot.content;
    contentLength = snapshot.contentLength;
    if (!completion_offset_from_position(content, contentLength, position, &offset)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return range;
    }

    prefixStart = offset;
    while (prefixStart > 0 && completion_is_identifier_part(content[prefixStart - 1])) {
        prefixStart--;
    }

    prefixLength = (TZrInt32)(offset - prefixStart);
    if (prefixLength > 0 && range.start.character >= prefixLength) {
        range.start.character -= prefixLength;
    }
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    return range;
}

/** @brief 取得候选项将插入的文本；缺少 insertText 时回退到 label。
 *  返回值借自 JSON 项，调用方只能在项存活期间使用。
 */
static const char *completion_item_new_text(cJSON *item) {
    const cJSON *insertText = get_object_item(item, ZR_LSP_FIELD_INSERT_TEXT);
    const cJSON *label = get_object_item(item, ZR_LSP_FIELD_LABEL);

    if (cJSON_IsString((cJSON *)insertText) && insertText->valuestring != NULL) {
        return insertText->valuestring;
    }
    if (cJSON_IsString((cJSON *)label) && label->valuestring != NULL) {
        return label->valuestring;
    }
    return NULL;
}

/** @brief 为补全候选附加覆盖当前前缀的 textEdit，使选择项可替换已输入文字。
 *  新 JSON 节点归 item 所有；无插入文本或分配失败时保留原项。
 */
static void add_completion_text_edit(cJSON *item, SZrLspRange range) {
    cJSON *textEdit;
    const char *newText;

    if (!cJSON_IsObject(item)) {
        return;
    }

    newText = completion_item_new_text(item);
    if (newText == NULL) {
        return;
    }

    textEdit = cJSON_CreateObject();
    if (textEdit == NULL) {
        return;
    }

    cJSON_AddItemToObject(textEdit, ZR_LSP_FIELD_RANGE, serialize_range(range));
    cJSON_AddStringToObject(textEdit, ZR_LSP_FIELD_NEW_TEXT, newText);
    cJSON_AddItemToObject(item, ZR_LSP_FIELD_TEXT_EDIT, textEdit);
}

/** @brief resolve 时用显示标签寻找重新计算的候选，临时 C 字符串在比较后释放。 */
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

/** @brief 将重新取得的原生候选投影为 resolve 响应，并保留初次响应的定位数据。 */
static cJSON *serialize_resolved_completion_item(const cJSON *data,
                                                 SZrLspCompletionItem *item,
                                                 SZrLspRange range) {
    cJSON *resolved = serialize_completion_item(item);

    if (resolved == NULL) {
        return NULL;
    }

    add_completion_text_edit(resolved, range);
    if (data != NULL) {
        cJSON_AddItemToObject(resolved, ZR_LSP_FIELD_DATA, cJSON_Duplicate((cJSON *)data, 1));
    }
    return resolved;
}

/** @brief 响应普通补全：查询语义候选，附上替换范围与 resolve 所需 URI/位置。
 *  原生候选数组序列化后立即释放；JSON 结果所有权转交请求层。
 */
SZrLspHandlerResult handle_completion_request(SZrStdioServer *server, const cJSON *params) {
    SZrArray completions = {0};
    SZrLspPosition position;
    SZrLspRange prefixRange;
    const char *uriText;
    SZrString *uri;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    prefixRange = completion_prefix_range(server, uri, position);
    if (!ZrLanguageServer_Lsp_GetCompletion(server->state, server->context, uri, position, &completions)) {
        free_completion_items_array(server->state, &completions);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    /* resolve 不保存原生候选，只凭此处写入的数据重新查询当前语义事实。 */
    result = serialize_completion_items_array(&completions);
    if (cJSON_IsArray(result)) {
        int count = cJSON_GetArraySize(result);
        for (int index = 0; index < count; index++) {
            cJSON *item = cJSON_GetArrayItem(result, index);
            cJSON *data;

            if (!cJSON_IsObject(item)) {
                continue;
            }

            add_completion_text_edit(item, prefixRange);
            data = cJSON_CreateObject();
            if (data == NULL) {
                continue;
            }
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_URI, uriText);
            cJSON_AddItemToObject(data, ZR_LSP_FIELD_POSITION, serialize_position(position));
            cJSON_AddItemToObject(item, ZR_LSP_FIELD_DATA, data);
        }
    }
    free_completion_items_array(server->state, &completions);
    return stdio_handler_result_from_json(server->context, result);
}

/** @brief 从候选项携带的 URI/内部位置重算语义补全，并回填匹配项。
 *  查找失败时原样复制客户端项；原生数组必须在响应前释放。
 */
SZrLspHandlerResult handle_completion_item_resolve_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *labelJson;
    const cJSON *data;
    const cJSON *uriJson;
    const cJSON *positionJson;
    const char *uriText;
    const char *label;
    SZrString *uri;
    SZrLspPosition position;
    SZrLspRange prefixRange;
    SZrArray completions = {0};
    TZrBool matched = ZR_FALSE;
    cJSON *result;

    if (server == ZR_NULL || !cJSON_IsObject((cJSON *)params)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    labelJson = get_object_item(params, ZR_LSP_FIELD_LABEL);
    data = get_object_item(params, ZR_LSP_FIELD_DATA);
    uriJson = get_object_item(data, ZR_LSP_FIELD_URI);
    positionJson = get_object_item(data, ZR_LSP_FIELD_POSITION);
    if (!cJSON_IsString((cJSON *)labelJson) ||
        !cJSON_IsString((cJSON *)uriJson) ||
        !parse_position(positionJson, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    label = cJSON_GetStringValue((cJSON *)labelJson);
    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    uri = server_get_cached_uri(server, uriText);
    if (label == NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    if (uri == ZR_NULL) {
        return stdio_handler_error(ZR_LSP_HANDLER_INTERNAL_ERROR);
    }

    prefixRange = completion_prefix_range(server, uri, position);
    if (!ZrLanguageServer_Lsp_GetCompletion(server->state, server->context, uri, position, &completions)) {
        free_completion_items_array(server->state, &completions);
        return stdio_handler_result_from_json(server->context, cJSON_Duplicate((cJSON *)params, 1));
    }

    /* TODO: 当前只按 label 选第一项，未核对同名不同 kind/来源的候选是否可并存；
     * 需用重载、同名导入和本地遮蔽场景确认 resolve 能否保持原候选身份。
     */
    result = ZR_NULL;
    for (TZrSize index = 0; index < completions.length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(&completions, index);

        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL && completion_item_label_matches(*itemPtr, label)) {
            matched = ZR_TRUE;
            result = serialize_resolved_completion_item(data, *itemPtr, prefixRange);
            break;
        }
    }

    free_completion_items_array(server->state, &completions);
    return stdio_handler_result_from_json(
            server->context, matched ? result : cJSON_Duplicate((cJSON *)params, 1));
}
