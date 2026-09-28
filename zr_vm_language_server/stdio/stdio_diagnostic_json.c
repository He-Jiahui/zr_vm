#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_json_builder.h"

/** 将内部字符串复制进 JSON，避免结果持有已释放的诊断字符串。 */
static TZrBool diagnostic_json_add_string(cJSON *json, const char *field, SZrString *value) {
    char *text = zr_string_to_c_string(value);
    cJSON *item;

    if (value != ZR_NULL && text == NULL) {
        return ZR_FALSE;
    }
    item = cJSON_AddStringToObject(json, field, text != NULL ? text : "");
    free(text);
    return item != NULL;
}

/** 将关联位置和消息转为客户端可见项；新 JSON 的所有权交给上层数组。 */
static cJSON *serialize_diagnostic_related_information(
    const SZrLspDiagnosticRelatedInformation *relatedInformation) {
    cJSON *json;

    if (relatedInformation == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL ||
        !stdio_json_add_owned_item(json, ZR_LSP_FIELD_LOCATION,
                                   serialize_location(&relatedInformation->location)) ||
        !diagnostic_json_add_string(json, ZR_LSP_FIELD_MESSAGE, relatedInformation->message)) {
        cJSON_Delete(json);
        return NULL;
    }

    return json;
}

/** @brief 把诊断修复建议转为带编辑范围的附加数据，供客户端代码操作使用。
 * TODO: data.fixes 的范围仍是内部 UTF-16；核对协商 UTF-8 时客户端消费者的坐标契约。 */
static cJSON *serialize_diagnostic_fix(const SZrLspDiagnosticFix *fix) {
    cJSON *json;
    cJSON *edit;

    if (fix == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL ||
        !diagnostic_json_add_string(json, ZR_LSP_FIELD_TITLE, fix->title) ||
        (edit = cJSON_AddObjectToObject(json, ZR_LSP_FIELD_EDIT)) == NULL ||
        !stdio_json_add_owned_item(edit, ZR_LSP_FIELD_RANGE, serialize_range(fix->editRange)) ||
        !diagnostic_json_add_string(edit, ZR_LSP_FIELD_NEW_TEXT, fix->editText) ||
        cJSON_AddNumberToObject(json, ZR_LSP_FIELD_APPLICABILITY, fix->applicability) == NULL) {
        cJSON_Delete(json);
        return NULL;
    }
    return json;
}

/** @brief 为诊断附加 URI、描述符和可用修复，供后续客户端操作识别原始问题。
 * 仅在调用方提供 URI 时生成；成功返回独立 JSON，失败清理已创建字段。
 * TODO: data.range 同样保留内部 UTF-16；核对 UTF-8 协商下读取此字段的客户端坐标契约。 */
static cJSON *serialize_diagnostic_data(const SZrLspDiagnostic *diagnostic, const char *uriText) {
    cJSON *data;
    cJSON *fixes;
    const TZrChar *noFixReason;

    if (diagnostic == NULL || uriText == NULL) {
        return NULL;
    }

    data = cJSON_CreateObject();
    if (data == NULL ||
        cJSON_AddStringToObject(data, ZR_LSP_FIELD_URI, uriText) == NULL ||
        !stdio_json_add_owned_item(data, ZR_LSP_FIELD_RANGE, serialize_range(diagnostic->range)) ||
        cJSON_AddStringToObject(data, ZR_LSP_FIELD_SOURCE, ZR_LSP_DIAGNOSTIC_SOURCE_NAME) == NULL ||
        cJSON_AddNumberToObject(data, ZR_LSP_FIELD_DESCRIPTOR_ID, diagnostic->descriptorId) == NULL) {
        goto allocation_failed;
    }
    noFixReason = ZrLanguageServer_Lsp_DiagnosticNoFixReasonName(diagnostic->noFixReason);
    if (noFixReason != ZR_NULL &&
        cJSON_AddStringToObject(data, ZR_LSP_FIELD_NO_FIX_REASON, noFixReason) == NULL) {
        goto allocation_failed;
    }
    if (diagnostic->code != ZR_NULL && !diagnostic_json_add_string(data, ZR_LSP_FIELD_CODE, diagnostic->code)) {
        goto allocation_failed;
    }

    if (diagnostic->fixes.isValid && diagnostic->fixes.length > 0) {
        fixes = cJSON_AddArrayToObject(data, ZR_LSP_FIELD_FIXES);
        if (fixes == NULL) {
            goto allocation_failed;
        }
        for (TZrSize index = 0; index < diagnostic->fixes.length; index++) {
            const SZrLspDiagnosticFix *fix =
                (const SZrLspDiagnosticFix *)ZrCore_Array_Get((SZrArray *)&diagnostic->fixes, index);
            if (fix != NULL && !stdio_json_add_owned_array_item(fixes, serialize_diagnostic_fix(fix))) {
                goto allocation_failed;
            }
        }
    }
    return data;

allocation_failed:
    cJSON_Delete(data);
    return NULL;
}

/** 生成单条标准诊断；有 URI 时带扩展 data，并保留关联信息。 */
static cJSON *serialize_diagnostic_for_uri(const SZrLspDiagnostic *diagnostic, const char *uriText) {
    cJSON *json;
    cJSON *relatedArray;

    if (diagnostic == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL ||
        !stdio_json_add_owned_item(json, ZR_LSP_FIELD_RANGE, serialize_range(diagnostic->range)) ||
        cJSON_AddNumberToObject(json, ZR_LSP_FIELD_SEVERITY, diagnostic->severity) == NULL ||
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_SOURCE, ZR_LSP_DIAGNOSTIC_SOURCE_NAME) == NULL ||
        !diagnostic_json_add_string(json, ZR_LSP_FIELD_MESSAGE, diagnostic->message)) {
        goto allocation_failed;
    }

    if (diagnostic->code != ZR_NULL && !diagnostic_json_add_string(json, ZR_LSP_FIELD_CODE, diagnostic->code)) {
        goto allocation_failed;
    }

    if (diagnostic->codeDescriptionHref != ZR_NULL) {
        cJSON *codeDescription = cJSON_AddObjectToObject(json, ZR_LSP_FIELD_CODE_DESCRIPTION);
        if (codeDescription == NULL ||
            !diagnostic_json_add_string(codeDescription, ZR_LSP_FIELD_HREF, diagnostic->codeDescriptionHref)) {
            goto allocation_failed;
        }
    }

    if (uriText != NULL &&
        !stdio_json_add_owned_item(json, ZR_LSP_FIELD_DATA, serialize_diagnostic_data(diagnostic, uriText))) {
        goto allocation_failed;
    }

    if (diagnostic->relatedInformation.length > 0) {
        relatedArray = cJSON_AddArrayToObject(json, ZR_LSP_FIELD_RELATED_INFORMATION);
        if (relatedArray == NULL) {
            goto allocation_failed;
        }
        for (TZrSize index = 0; index < diagnostic->relatedInformation.length; index++) {
            SZrLspDiagnosticRelatedInformation *relatedInformation =
                (SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get((SZrArray *)&diagnostic->relatedInformation,
                                                                       index);
            if (relatedInformation != NULL &&
                !stdio_json_add_owned_array_item(relatedArray,
                                                 serialize_diagnostic_related_information(relatedInformation))) {
                goto allocation_failed;
            }
        }
    }

    return json;

allocation_failed:
    cJSON_Delete(json);
    return NULL;
}

/** 不带 URI 上下文的单条序列化入口，主要供直接 JSON 调用者使用。 */
cJSON *serialize_diagnostic(const SZrLspDiagnostic *diagnostic) {
    return serialize_diagnostic_for_uri(diagnostic, NULL);
}

/** @brief 将某 URI 的诊断数组转为独立 JSON，供 push 与两种 pull 报告复用。
 * 调用方持有返回 JSON；任一元素序列化失败则整体失败，输入诊断仍归调用方。 */
cJSON *serialize_diagnostics_array_for_uri(SZrArray *diagnostics, const char *uriText) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || diagnostics == ZR_NULL) {
        return json;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL) {
            if (!stdio_json_add_owned_array_item(json, serialize_diagnostic_for_uri(*diagnosticPtr, uriText))) {
                cJSON_Delete(json);
                return NULL;
            }
        }
    }
    return json;
}

/** 无 URI 上下文的数组包装入口，复用单 URI 序列化的内存失败处理。 */
cJSON *serialize_diagnostics_array(SZrArray *diagnostics) {
    return serialize_diagnostics_array_for_uri(diagnostics, NULL);
}
