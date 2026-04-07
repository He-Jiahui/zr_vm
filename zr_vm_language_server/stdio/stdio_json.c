#include "zr_vm_language_server_stdio_internal.h"

cJSON *serialize_position(SZrLspPosition position) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_LINE, position.line);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_CHARACTER, position.character);
    return json;
}

cJSON *serialize_range(SZrLspRange range) {
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_START, serialize_position(range.start));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_END, serialize_position(range.end));
    return json;
}

cJSON *serialize_location(const SZrLspLocation *location) {
    cJSON *json;
    char *uriText;

    if (location == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    uriText = zr_string_to_c_string(location->uri);
    if (uriText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_URI, uriText);
        free(uriText);
    } else {
        cJSON_AddNullToObject(json, ZR_LSP_FIELD_URI);
    }
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(location->range));
    return json;
}

static cJSON *serialize_diagnostic_related_information(
    const SZrLspDiagnosticRelatedInformation *relatedInformation) {
    cJSON *json;
    char *messageText;

    if (relatedInformation == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json,
                          ZR_LSP_FIELD_LOCATION,
                          serialize_location(&relatedInformation->location));

    messageText = zr_string_to_c_string(relatedInformation->message);
    if (messageText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_MESSAGE, messageText);
        free(messageText);
    } else {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_MESSAGE, "");
    }

    return json;
}

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

cJSON *serialize_diagnostic(const SZrLspDiagnostic *diagnostic) {
    cJSON *json;
    cJSON *relatedArray;
    char *messageText;
    char *codeText;

    if (diagnostic == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(diagnostic->range));
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_SEVERITY, diagnostic->severity);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_SOURCE, ZR_LSP_DIAGNOSTIC_SOURCE_NAME);

    messageText = zr_string_to_c_string(diagnostic->message);
    if (messageText != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_MESSAGE, messageText);
        free(messageText);
    }

    if (diagnostic->code != ZR_NULL) {
        codeText = zr_string_to_c_string(diagnostic->code);
        if (codeText != NULL) {
            cJSON_AddStringToObject(json, ZR_LSP_FIELD_CODE, codeText);
            free(codeText);
        }
    }

    if (diagnostic->relatedInformation.length > 0) {
        relatedArray = cJSON_CreateArray();
        if (relatedArray != NULL) {
            for (TZrSize index = 0; index < diagnostic->relatedInformation.length; index++) {
                SZrLspDiagnosticRelatedInformation *relatedInformation =
                    (SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get((SZrArray *)&diagnostic->relatedInformation,
                                                                           index);
                if (relatedInformation != NULL) {
                    cJSON_AddItemToArray(relatedArray,
                                         serialize_diagnostic_related_information(relatedInformation));
                }
            }
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_RELATED_INFORMATION, relatedArray);
        }
    }

    return json;
}

cJSON *serialize_completion_item(const SZrLspCompletionItem *item) {
    cJSON *json;
    char *text;
    int insertTextFormat = ZR_LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT;

    if (item == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    text = zr_string_to_c_string(item->label);
    if (text != NULL) {
        cJSON_AddStringToObject(json, ZR_LSP_FIELD_LABEL, text);
        free(text);
    }
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, item->kind);

    if (item->detail != ZR_NULL) {
        text = zr_string_to_c_string(item->detail);
        if (text != NULL) {
            cJSON_AddStringToObject(json, ZR_LSP_FIELD_DETAIL, text);
            free(text);
        }
    }

    if (item->documentation != ZR_NULL) {
        cJSON *documentation = cJSON_CreateObject();
        text = zr_string_to_c_string(item->documentation);
        if (documentation != NULL && text != NULL) {
            cJSON_AddStringToObject(documentation, ZR_LSP_FIELD_KIND, ZR_LSP_MARKUP_KIND_MARKDOWN);
            cJSON_AddStringToObject(documentation, ZR_LSP_FIELD_VALUE, text);
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_DOCUMENTATION, documentation);
        } else {
            cJSON_Delete(documentation);
        }
        free(text);
    }

    if (item->insertText != ZR_NULL) {
        text = zr_string_to_c_string(item->insertText);
        if (text != NULL) {
            cJSON_AddStringToObject(json, ZR_LSP_FIELD_INSERT_TEXT, text);
            free(text);
        }
    }

    if (item->insertTextFormat != ZR_NULL) {
        text = zr_string_to_c_string(item->insertTextFormat);
        if (text != NULL) {
            if (strcmp(text, ZR_LSP_INSERT_TEXT_FORMAT_KIND_SNIPPET) == 0) {
                insertTextFormat = ZR_LSP_INSERT_TEXT_FORMAT_SNIPPET;
            }
            free(text);
        }
    }
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_INSERT_TEXT_FORMAT, insertTextFormat);

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

cJSON *serialize_diagnostics_array(SZrArray *diagnostics) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || diagnostics == ZR_NULL) {
        return json;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_diagnostic(*diagnosticPtr));
        }
    }
    return json;
}

cJSON *serialize_completion_items_array(SZrArray *items) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || items == ZR_NULL) {
        return json;
    }

    for (index = 0; index < items->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_completion_item(*itemPtr));
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

void free_locations_array(SZrState *state, SZrArray *locations) {
    TZrSize index;

    if (state == ZR_NULL || locations == ZR_NULL) {
        return;
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *locationPtr, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
}

void free_symbols_array(SZrState *state, SZrArray *symbols) {
    TZrSize index;

    if (state == ZR_NULL || symbols == ZR_NULL) {
        return;
    }

    for (index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *symbolPtr, sizeof(SZrLspSymbolInformation));
        }
    }
    ZrCore_Array_Free(state, symbols);
}

void free_diagnostics_array(SZrState *state, SZrArray *diagnostics) {
    TZrSize index;

    if (state == ZR_NULL || diagnostics == ZR_NULL) {
        return;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL) {
            ZrCore_Array_Free(state, &(*diagnosticPtr)->relatedInformation);
            ZrCore_Memory_RawFree(state->global, *diagnosticPtr, sizeof(SZrLspDiagnostic));
        }
    }
    ZrCore_Array_Free(state, diagnostics);
}

void free_completion_items_array(SZrState *state, SZrArray *items) {
    TZrSize index;

    if (state == ZR_NULL || items == ZR_NULL) {
        return;
    }

    for (index = 0; index < items->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *itemPtr, sizeof(SZrLspCompletionItem));
        }
    }
    ZrCore_Array_Free(state, items);
}

void free_highlights_array(SZrState *state, SZrArray *highlights) {
    TZrSize index;

    if (state == ZR_NULL || highlights == ZR_NULL) {
        return;
    }

    for (index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *highlightPtr, sizeof(SZrLspDocumentHighlight));
        }
    }
    ZrCore_Array_Free(state, highlights);
}

void free_hover(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

void free_signature_help(SZrState *state, SZrLspSignatureHelp *help) {
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
}

int parse_position(const cJSON *json, SZrLspPosition *outPosition) {
    const cJSON *line;
    const cJSON *character;

    if (json == NULL || outPosition == NULL) {
        return 0;
    }

    line = get_object_item(json, ZR_LSP_FIELD_LINE);
    character = get_object_item(json, ZR_LSP_FIELD_CHARACTER);
    if (!cJSON_IsNumber(line) || !cJSON_IsNumber(character)) {
        return 0;
    }

    outPosition->line = (TZrInt32)line->valuedouble;
    outPosition->character = (TZrInt32)character->valuedouble;
    return 1;
}

int parse_range(const cJSON *json, SZrLspRange *outRange) {
    const cJSON *start;
    const cJSON *end;

    if (json == NULL || outRange == NULL) {
        return 0;
    }

    start = get_object_item(json, ZR_LSP_FIELD_START);
    end = get_object_item(json, ZR_LSP_FIELD_END);
    if (!parse_position(start, &outRange->start) || !parse_position(end, &outRange->end)) {
        return 0;
    }

    return 1;
}
