#include "zr_vm_language_server_stdio_internal.h"

#include <inttypes.h>

/**
 * @brief 将接口层 TextEdit 转为协议对象，供格式化和 WorkspaceEdit 共用。
 * @details serialize_range 保留内部位置，最终响应由统一的位置编码转换路径处理；
 *          返回的新 JSON 节点归调用方所有，不接管 edit 或其 newText。
 */
cJSON *serialize_text_edit(const SZrLspTextEdit *edit) {
    cJSON *json;
    char *text;

    if (edit == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(edit->range));
    text = zr_string_to_c_string(edit->newText);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_NEW_TEXT, text != NULL ? text : "");
    free(text);
    return json;
}

/**
 * @brief 把接口层持有的 TextEdit 指针数组序列化为协议数组。
 * @details 接口层仍拥有原数组，由调用方按来源用 FreeTextEdits 或 FreeCodeActions 释放；
 *          JSON 数组交给响应或父节点管理。
 */
cJSON *serialize_text_edits_array(SZrArray *edits) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || edits == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < edits->length; index++) {
        SZrLspTextEdit **editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(edits, index);
        if (editPtr != ZR_NULL && *editPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_text_edit(*editPtr));
        }
    }

    return json;
}

/**
 * @brief 为已打开文档构造带版本约束的 TextDocumentEdit。
 * @details serialize_workspace_edit 选用 documentChanges 时调用；版本来自刚捕获的
 *          文档快照，以便客户端拒绝把编辑应用到另一版本。
 */
static cJSON *serialize_versioned_document_change(const char *uriText,
                                                  TZrSize version,
                                                  SZrArray *edits) {
    cJSON *documentChange = cJSON_CreateObject();
    cJSON *textDocument = cJSON_CreateObject();

    if (documentChange == NULL || textDocument == NULL) {
        cJSON_Delete(documentChange);
        cJSON_Delete(textDocument);
        return NULL;
    }

    cJSON_AddStringToObject(textDocument, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
    cJSON_AddNumberToObject(textDocument, ZR_LSP_FIELD_VERSION, (double)version);
    cJSON_AddItemToObject(documentChange, ZR_LSP_FIELD_TEXT_DOCUMENT, textDocument);
    cJSON_AddItemToObject(documentChange, ZR_LSP_FIELD_EDITS, serialize_text_edits_array(edits));
    return documentChange;
}

/**
 * @brief 把 64 位快照指纹写为固定宽度十六进制字符串。
 * @details CodeAction.data 后续由 parse_code_action_snapshot_hash 读回；
 *          字符串表示避免 JSON 数字对高位指纹造成精度损失。
 */
static TZrBool serialize_snapshot_u64(
        cJSON *json,
        const char *fieldName,
        TZrUInt64 value) {
    char text[17];

    if (json == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }
    snprintf(text, sizeof(text), "%016" PRIx64, (uint64_t)value);
    return cJSON_AddStringToObject(json, fieldName, text) ? ZR_TRUE : ZR_FALSE;
}

/**
 * @brief 将五项语义身份一并放入 CodeAction.data 的文档快照。
 * @details 与 resolve 阶段 parse_code_action_semantic_identity 成对，字段必须全体成功；
 *          JSON 节点成功附着后归父对象管理，失败时由此函数释放。
 */
static TZrBool serialize_snapshot_semantic_identity(
        cJSON *json,
        const SZrLspSemanticSnapshotIdentity *identity) {
    cJSON *semanticIdentity;

    if (json == ZR_NULL || identity == ZR_NULL) {
        return ZR_FALSE;
    }
    semanticIdentity = cJSON_CreateObject();
    if (semanticIdentity == ZR_NULL ||
        !serialize_snapshot_u64(
                semanticIdentity,
                ZR_LSP_FIELD_DOCUMENT_GENERATION,
                identity->documentGeneration) ||
        !serialize_snapshot_u64(
                semanticIdentity,
                ZR_LSP_FIELD_PROJECT_GENERATION,
                identity->projectGeneration) ||
        !serialize_snapshot_u64(
                semanticIdentity,
                ZR_LSP_FIELD_PROVIDER_GENERATION,
                identity->providerGeneration) ||
        !serialize_snapshot_u64(
                semanticIdentity,
                ZR_LSP_FIELD_SEMANTIC_GENERATION,
                identity->semanticGeneration) ||
        !serialize_snapshot_u64(
                semanticIdentity,
                ZR_LSP_FIELD_DEPENDENCY_FINGERPRINT,
                identity->dependencyFingerprint) ||
        !cJSON_AddItemToObject(
                json, ZR_LSP_FIELD_SEMANTIC_IDENTITY, semanticIdentity)) {
        cJSON_Delete(semanticIdentity);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 为 CodeAction.data 编码可在 resolve 时复验的文档快照。
 * @details 长度、版本和内容代数限制为 JSON 安全整数；内容及语义指纹使用
 *          十六进制字符串。新建 JSON 归调用方管理，不接管输入快照。
 */
static cJSON *serialize_workspace_edit_document_snapshot(
        const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot) {
    cJSON *json;
    char contentHash[17];

    if (documentSnapshot == ZR_NULL) {
        return ZR_NULL;
    }
    if ((double)documentSnapshot->contentLength >
                ZR_LSP_JSON_SAFE_INTEGER_MAX ||
        (double)documentSnapshot->version > ZR_LSP_JSON_SAFE_INTEGER_MAX ||
        (double)documentSnapshot->contentGeneration >
                ZR_LSP_JSON_SAFE_INTEGER_MAX) {
        return ZR_NULL;
    }
    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    snprintf(contentHash,
             sizeof(contentHash),
             "%016" PRIx64,
             (uint64_t)documentSnapshot->contentHash);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_CONTENT_HASH, contentHash);
    cJSON_AddNumberToObject(json,
                            ZR_LSP_FIELD_CONTENT_LENGTH,
                            (double)documentSnapshot->contentLength);
    cJSON_AddNumberToObject(json,
                            ZR_LSP_FIELD_VERSION,
                            (double)documentSnapshot->version);
    cJSON_AddNumberToObject(json,
                            ZR_LSP_FIELD_CONTENT_GENERATION,
                            (double)documentSnapshot->contentGeneration);
    cJSON_AddBoolToObject(json,
                          ZR_LSP_FIELD_IS_OPEN_DOCUMENT,
                          documentSnapshot->isOpenDocument ? 1 : 0);
    if (documentSnapshot->hasSemanticIdentity &&
        !serialize_snapshot_semantic_identity(
                json, &documentSnapshot->semanticIdentity)) {
        cJSON_Delete(json);
        return ZR_NULL;
    }
    return json;
}

/**
 * @brief 依文档打开状态选择 WorkspaceEdit 的版本化或普通编辑形式。
 * @details 打开的文档使用 documentChanges 携带版本；关闭的文档使用 URI 键控
 *          changes。编辑数组只读取不接管，返回节点由 CodeAction 持有。
 */
static cJSON *serialize_workspace_edit(const char *uriText,
                                       SZrArray *edits,
                                       const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot) {
    cJSON *json = cJSON_CreateObject();

    if (json == NULL) {
        cJSON_Delete(json);
        return NULL;
    }

    /* 已打开文档需让客户端按快照版本校验；关闭文档无法提供协议版本。 */
    if (documentSnapshot != ZR_NULL && documentSnapshot->isOpenDocument) {
        cJSON *documentChanges = cJSON_CreateArray();
        cJSON *documentChange = serialize_versioned_document_change(
                uriText, documentSnapshot->version, edits);

        if (documentChanges == NULL || documentChange == NULL) {
            cJSON_Delete(documentChanges);
            cJSON_Delete(documentChange);
            cJSON_Delete(json);
            return NULL;
        }
        cJSON_AddItemToArray(documentChanges, documentChange);
        cJSON_AddItemToObject(json, ZR_LSP_FIELD_DOCUMENT_CHANGES, documentChanges);
        return json;
    }

    {
        cJSON *changes = cJSON_CreateObject();
        if (changes == NULL) {
            cJSON_Delete(json);
            return NULL;
        }
        cJSON_AddItemToObject(changes,
                              uriText != NULL ? uriText : "",
                              serialize_text_edits_array(edits));
        cJSON_AddItemToObject(json, ZR_LSP_FIELD_CHANGES, changes);
    }
    return json;
}

/**
 * @brief 将接口层 CodeAction 封装为可应用、可在 resolve 时判断过期的协议操作。
 * @details data 保存 URI、操作信息和快照，edit 按文档状态选择 WorkspaceEdit 形式；
 *          新建 JSON 归数组或响应管理，接口层 action 仍由 FreeCodeActions 释放。
 */
static cJSON *serialize_code_action(const char *uriText,
                                    const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot,
                                    const SZrLspCodeAction *action) {
    cJSON *json;
    cJSON *snapshotJson;
    char *titleText;
    char *kindText;

    if (action == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    titleText = zr_string_to_c_string(action->title);
    kindText = zr_string_to_c_string(action->kind);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_TITLE, titleText != NULL ? titleText : "");
    if (kindText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_KIND, kindText);
    }
    cJSON_AddBoolToObject(json, ZR_LSP_FIELD_IS_PREFERRED, action->isPreferred ? 1 : 0);
    /* data 与首轮响应绑定；resolve 只据此校验，不依赖客户端再次传回内部指针。 */
    {
        cJSON *data = cJSON_CreateObject();
        if (data != NULL) {
            snapshotJson = serialize_workspace_edit_document_snapshot(
                    documentSnapshot);
            if (snapshotJson == NULL) {
                cJSON_Delete(data);
                cJSON_Delete(json);
                free(titleText);
                free(kindText);
                return NULL;
            }
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_URI, uriText != NULL ? uriText : "");
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_TITLE, titleText != NULL ? titleText : "");
            if (kindText != NULL) {
                cJSON_AddStringToObject(data, ZR_LSP_FIELD_KIND, kindText);
            }
            cJSON_AddBoolToObject(data, ZR_LSP_FIELD_IS_PREFERRED, action->isPreferred ? 1 : 0);
            cJSON_AddItemToObject(
                    data,
                    ZR_LSP_FIELD_SNAPSHOT,
                    snapshotJson);
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_DATA, data);
        } else {
            cJSON_Delete(json);
            free(titleText);
            free(kindText);
            return NULL;
        }
    }
    cJSON_AddItemToObject(json,
                          ZR_LSP_FIELD_EDIT,
                          serialize_workspace_edit(
                                  uriText,
                                  (SZrArray *)&action->edits,
                                  documentSnapshot));

    free(titleText);
    free(kindText);
    return json;
}

/**
 * @brief 按代码操作 kind 的层级规则匹配 context.only。
 * @details 请求父 kind 时接受相同 kind 或以“父 kind.”开头的子 kind，
 *          避免把同字首但不属于该层级的 kind 误认为匹配。
 */
static int code_action_kind_matches_filter(const char *actionKind, const char *requestedKind) {
    size_t requestedLength;

    if (actionKind == NULL || requestedKind == NULL) {
        return 0;
    }
    if (strcmp(actionKind, requestedKind) == 0) {
        return 1;
    }

    requestedLength = strlen(requestedKind);
    return strncmp(actionKind, requestedKind, requestedLength) == 0 &&
           actionKind[requestedLength] == '.';
}

/**
 * @brief 判断一个接口层操作是否满足本次请求的 context.only 过滤条件。
 * @details 空缺或空数组沿用全部操作；非空数组按层级 kind 匹配，临时 kind 字符串
 *          在函数返回前释放，输入 action 与 params 均不由此函数持有。
 */
static int code_action_allowed_by_context_only(const SZrLspCodeAction *action, const cJSON *params) {
    const cJSON *context;
    const cJSON *only;
    const cJSON *requestedKind;
    char *kindText;
    int allowed = 0;

    context = get_object_item(params, ZR_LSP_FIELD_CONTEXT);
    only = get_object_item(context, ZR_LSP_FIELD_ONLY);
    if (!cJSON_IsArray(only) || cJSON_GetArraySize(only) == 0) {
        return 1;
    }

    kindText = zr_string_to_c_string(action != NULL ? action->kind : ZR_NULL);
    cJSON_ArrayForEach(requestedKind, only) {
        if (cJSON_IsString(requestedKind) &&
            code_action_kind_matches_filter(kindText, requestedKind->valuestring)) {
            allowed = 1;
            break;
        }
    }
    free(kindText);
    return allowed;
}

/**
 * @brief 过滤并序列化代码操作列表，供 codeAction 请求处理器生成响应。
 * @details 输入操作、快照和请求参数只借用；返回 JSON 数组由响应路径接管，
 *          任一操作序列化失败时销毁已生成数组并返回空指针。
 */
cJSON *serialize_code_actions_array(const char *uriText,
                                    const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot,
                                    SZrArray *actions,
                                    const cJSON *params) {
    cJSON *json = cJSON_CreateArray();

    if (json == NULL || actions == NULL) {
        return json;
    }

    for (TZrSize index = 0; index < actions->length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(actions, index);
        if (actionPtr != ZR_NULL &&
            *actionPtr != ZR_NULL &&
            code_action_allowed_by_context_only(*actionPtr, params)) {
            cJSON *actionJson = serialize_code_action(
                    uriText, documentSnapshot, *actionPtr);
            if (actionJson == NULL) {
                cJSON_Delete(json);
                return NULL;
            }
            cJSON_AddItemToArray(json, actionJson);
        }
    }

    return json;
}
