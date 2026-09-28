#include "zr_vm_language_server_stdio_internal.h"

/** @brief 将大纲/工作区符号的展示身份和位置复制到 JSON；调用方仍持有原生符号。 */
cJSON *serialize_symbol_information(const SZrLspSymbolInformation *info) {
    cJSON *json;
    char *nameText;
    char *containerText;

    if (info == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    nameText = zr_string_to_c_string(info->name);
    if (nameText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_NAME, nameText);
        free(nameText);
    }
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, info->kind);
    /* BUG: serialize_location 分配失败返回 NULL 时，字段添加失败被忽略；
     * 仍返回缺少必需 location 的成功符号对象。 */
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_LOCATION, serialize_location(&info->location));

    if (info->containerName != ZR_NULL) {
        containerText = zr_string_to_c_string(info->containerName);
        if (containerText != NULL) {
            cJSON_AddStringToObject(json, ZR_LSP_FIELD_CONTAINER_NAME, containerText);
            free(containerText);
        }
    }

    return json;
}

/** @brief 把普通悬停的首段 Markdown 与范围交给标准 LSP hover 响应。 */
cJSON *serialize_hover(const SZrLspHover *hover) {
    cJSON *json;
    cJSON *contents;
    char *text = NULL;

    if (hover == ZR_NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    /* BUG: contents 分配失败时仍返回没有悬停内容的成功对象；
     * 应把该失败交给请求层，而非让客户端收到不完整 Hover。 */
    contents = cJSON_CreateObject();
    if (contents != NULL) {
        if (hover->contents.length > 0) {
            SZrString **textPtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&hover->contents, 0);
            if (textPtr != ZR_NULL && *textPtr != ZR_NULL) {
                text = zr_string_to_c_string(*textPtr);
            }
        }
        cJSON_AddStringToObject(contents, ZR_LSP_FIELD_KIND, ZR_LSP_MARKUP_KIND_MARKDOWN);
        cJSON_AddStringToObject(contents, ZR_LSP_FIELD_VALUE, text != NULL ? text : "");
        cJSON_AddItemToObject(json, ZR_LSP_FIELD_CONTENTS, contents);
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(hover->range));
    free(text);
    return json;
}

/** @brief 保留扩展富悬停的分段角色和范围，供编辑器专用视图消费。 */
cJSON *serialize_rich_hover(const SZrLspRichHover *hover) {
    cJSON *json;
    cJSON *sections;

    if (hover == ZR_NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    sections = cJSON_CreateArray();
    if (json == NULL || sections == NULL) {
        cJSON_Delete(json);
        cJSON_Delete(sections);
        return NULL;
    }

    /* 各段均是独立 JSON 值，发布后原生 section 可由上层统一回收。
     * BUG: 单段分配失败会被静默跳过，却仍返回缺段的成功响应。 */
    for (TZrSize index = 0; index < hover->sections.length; index++) {
        SZrLspRichHoverSection **sectionPtr =
            (SZrLspRichHoverSection **)ZrCore_Array_Get((SZrArray *)&hover->sections, index);
        cJSON *sectionJson;
        char *roleText;
        char *labelText;
        char *valueText;

        if (sectionPtr == ZR_NULL || *sectionPtr == ZR_NULL) {
            continue;
        }

        sectionJson = cJSON_CreateObject();
        if (sectionJson == NULL) {
            continue;
        }

        roleText = zr_string_to_c_string((*sectionPtr)->role);
        labelText = zr_string_to_c_string((*sectionPtr)->label);
        valueText = zr_string_to_c_string((*sectionPtr)->value);

        if (roleText != NULL) {
            cJSON_AddStringToObject(sectionJson, ZR_LSP_FIELD_ROLE, roleText);
        }
        if (labelText != NULL) {
            cJSON_AddStringToObject(sectionJson, ZR_LSP_FIELD_LABEL, labelText);
        }
        if (valueText != NULL) {
            cJSON_AddStringToObject(sectionJson, ZR_LSP_FIELD_VALUE, valueText);
        }

        free(roleText);
        free(labelText);
        free(valueText);
        cJSON_AddItemToArray(sections, sectionJson);
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_SECTIONS, sections);
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(hover->range));
    return json;
}

/* 签名和参数的可选文档统一用 Markdown 对象，与标签文本分开编码。 */
static cJSON *serialize_markdown_content(SZrString *value) {
    cJSON *json;
    char *text;

    if (value == ZR_NULL) {
        return NULL;
    }

    text = zr_string_to_c_string(value);
    if (text == NULL) {
        return NULL;
    }

    json = cJSON_CreateObject();
    if (json != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_KIND, ZR_LSP_MARKUP_KIND_MARKDOWN);
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_VALUE, text);
    }

    free(text);
    return json;
}

/** @brief 发布当前调用的候选签名、参数文档和活动索引；上层仍持有原生 help。 */
cJSON *serialize_signature_help(const SZrLspSignatureHelp *help) {
    cJSON *json;
    cJSON *signatures;

    if (help == ZR_NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    signatures = cJSON_CreateArray();
    if (json == NULL || signatures == NULL) {
        cJSON_Delete(json);
        cJSON_Delete(signatures);
        return NULL;
    }

    /* BUG: 分配失败时跳过个别签名/参数却仍返回成功对象；
     * 活动索引可能指向缺失的候选，必须传播失败。 */
    for (TZrSize signatureIndex = 0; signatureIndex < help->signatures.length; signatureIndex++) {
        SZrLspSignatureInformation **signaturePtr =
            (SZrLspSignatureInformation **)ZrCore_Array_Get((SZrArray *)&help->signatures, signatureIndex);
        cJSON *signatureJson;
        char *labelText;

        if (signaturePtr == ZR_NULL || *signaturePtr == ZR_NULL) {
            continue;
        }

        signatureJson = cJSON_CreateObject();
        labelText = zr_string_to_c_string((*signaturePtr)->label);
        if (signatureJson == NULL || labelText == NULL) {
            cJSON_Delete(signatureJson);
            free(labelText);
            continue;
        }

        cJSON_AddStringToObject(signatureJson, ZR_LSP_FIELD_LABEL, labelText);
        free(labelText);

        if ((*signaturePtr)->documentation != ZR_NULL) {
            cJSON *documentation = serialize_markdown_content((*signaturePtr)->documentation);
            if (documentation != NULL) {
                cJSON_AddItemToObject(signatureJson, ZR_LSP_FIELD_DOCUMENTATION, documentation);
            }
        }

        if ((*signaturePtr)->parameters.length > 0) {
            cJSON *parameters = cJSON_CreateArray();
            if (parameters != NULL) {
                for (TZrSize parameterIndex = 0; parameterIndex < (*signaturePtr)->parameters.length; parameterIndex++) {
                    SZrLspParameterInformation **parameterPtr =
                        (SZrLspParameterInformation **)ZrCore_Array_Get(&(*signaturePtr)->parameters, parameterIndex);
                    cJSON *parameterJson;
                    char *parameterLabel;

                    if (parameterPtr == ZR_NULL || *parameterPtr == ZR_NULL) {
                        continue;
                    }

                    parameterJson = cJSON_CreateObject();
                    parameterLabel = zr_string_to_c_string((*parameterPtr)->label);
                    if (parameterJson == NULL || parameterLabel == NULL) {
                        cJSON_Delete(parameterJson);
                        free(parameterLabel);
                        continue;
                    }

                    cJSON_AddStringToObject(parameterJson, ZR_LSP_FIELD_LABEL, parameterLabel);
                    free(parameterLabel);

                    if ((*parameterPtr)->documentation != ZR_NULL) {
                        cJSON *documentation = serialize_markdown_content((*parameterPtr)->documentation);
                        if (documentation != NULL) {
                            cJSON_AddItemToObject(parameterJson, ZR_LSP_FIELD_DOCUMENTATION, documentation);
                        }
                    }

                    cJSON_AddItemToArray(parameters, parameterJson);
                }
                cJSON_AddItemToObject(signatureJson, ZR_LSP_FIELD_PARAMETERS, parameters);
            }
        }

        cJSON_AddItemToArray(signatures, signatureJson);
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_SIGNATURES, signatures);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_ACTIVE_SIGNATURE, help->activeSignature);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_ACTIVE_PARAMETER, help->activeParameter);
    return json;
}

/* 单个类型提示只借用 LSP 结果；标签与位置复制到独立 JSON 后由上层释放原生对象。 */
static cJSON *serialize_inlay_hint(const SZrLspInlayHint *hint) {
    cJSON *json;
    char *labelText;

    if (hint == ZR_NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    /* BUG: position 编码失败时字段添加器拒绝 NULL，此函数仍返回无位置的提示对象。 */
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_POSITION, serialize_position(hint->position));
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, hint->kind);
    cJSON_AddBoolToObject(json, ZR_LSP_FIELD_PADDING_LEFT, hint->paddingLeft ? 1 : 0);
    cJSON_AddBoolToObject(json, ZR_LSP_FIELD_PADDING_RIGHT, hint->paddingRight ? 1 : 0);

    labelText = zr_string_to_c_string(hint->label);
    if (labelText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_LABEL, labelText);
        free(labelText);
    } else {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_LABEL, "");
    }

    return json;
}

/** @brief 将查询范围内的提示数组变成 LSP JSON；空查询保持数组形状。 */
cJSON *serialize_inlay_hints_array(SZrArray *hints) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || hints == ZR_NULL) {
        return json;
    }

    for (index = 0; index < hints->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(hints, index);
        if (hintPtr != ZR_NULL && *hintPtr != ZR_NULL) {
            /* BUG: 单项编码分配失败返回 NULL 时，AddItemToArray 失败却仍返回成功数组；
             * 客户端会少收到已查询到的提示，应把该失败传播为整个请求错误。 */
            cJSON_AddItemToArray(json, serialize_inlay_hint(*hintPtr));
        }
    }

    return json;
}

/** @brief 编码同文档高亮的范围和类别，供引用或写入标记显示。
 * @pre highlight 非空；数组调用方先过滤空指针。 */
cJSON *serialize_document_highlight(const SZrLspDocumentHighlight *highlight) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    /* BUG: range 编码失败后仍返回无范围的成功高亮对象。 */
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(highlight->range));
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, highlight->kind);
    return json;
}

/** @brief 复制定义、实现和引用查询的位置数组，允许上层立即释放原生元素。 */
cJSON *serialize_locations_array(SZrArray *locations) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || locations == ZR_NULL) {
        return json;
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            /* BUG: serialize_location 因资源不足返回 NULL 时，此处忽略添加失败；
             * definition/references/implementation 会成功返回缺少目标的数组。 */
            cJSON_AddItemToArray(json, serialize_location(*locationPtr));
        }
    }
    return json;
}

/** @brief 复制文档或工作区符号列表，保留各符号的容器与位置。 */
cJSON *serialize_symbols_array(SZrArray *symbols) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || symbols == ZR_NULL) {
        return json;
    }

    for (index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            /* BUG: 单个符号编码失败不会让整个文档/工作区查询失败，
             * AddItemToArray 拒绝 NULL 后仍返回缺失该符号的成功数组。 */
            cJSON_AddItemToArray(json, serialize_symbol_information(*symbolPtr));
        }
    }
    return json;
}

/** @brief 复制同文档高亮列表；查询为空时仍返回空数组。 */
cJSON *serialize_highlights_array(SZrArray *highlights) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || highlights == ZR_NULL) {
        return json;
    }

    for (index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
            /* BUG: 单项高亮编码失败时 NULL 被数组添加器拒绝，
             * 这里仍返回成功结果，客户端会漏看已查到的高亮范围。 */
            cJSON_AddItemToArray(json, serialize_document_highlight(*highlightPtr));
        }
    }
    return json;
}
