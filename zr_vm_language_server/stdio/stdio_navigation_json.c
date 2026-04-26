#include "zr_vm_language_server_stdio_internal.h"

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

cJSON *serialize_inlay_hints_array(SZrArray *hints) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || hints == ZR_NULL) {
        return json;
    }

    for (index = 0; index < hints->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(hints, index);
        if (hintPtr != ZR_NULL && *hintPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_inlay_hint(*hintPtr));
        }
    }

    return json;
}

cJSON *serialize_document_highlight(const SZrLspDocumentHighlight *highlight) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(highlight->range));
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, highlight->kind);
    return json;
}

cJSON *serialize_locations_array(SZrArray *locations) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || locations == ZR_NULL) {
        return json;
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_location(*locationPtr));
        }
    }
    return json;
}

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
            cJSON_AddItemToArray(json, serialize_symbol_information(*symbolPtr));
        }
    }
    return json;
}

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
            cJSON_AddItemToArray(json, serialize_document_highlight(*highlightPtr));
        }
    }
    return json;
}
