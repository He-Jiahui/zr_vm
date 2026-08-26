#include "zr_vm_language_server_stdio_internal.h"

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

static cJSON *serialize_diagnostic_fix(const SZrLspDiagnosticFix *fix) {
    cJSON *json;
    cJSON *edit;
    char *titleText;
    char *editText;

    if (fix == NULL) {
        return cJSON_CreateNull();
    }

    json = cJSON_CreateObject();
    edit = cJSON_CreateObject();
    if (json == NULL || edit == NULL) {
        cJSON_Delete(json);
        cJSON_Delete(edit);
        return NULL;
    }

    titleText = zr_string_to_c_string(fix->title);
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_TITLE, titleText != NULL ? titleText : "");
    free(titleText);

    editText = zr_string_to_c_string(fix->editText);
    cJSON_AddItemToObject(edit, ZR_LSP_FIELD_RANGE, serialize_range(fix->editRange));
    cJSON_AddStringToObject(edit, ZR_LSP_FIELD_NEW_TEXT, editText != NULL ? editText : "");
    free(editText);

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_EDIT, edit);
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_APPLICABILITY, fix->applicability);
    return json;
}

static cJSON *serialize_diagnostic_data(const SZrLspDiagnostic *diagnostic, const char *uriText) {
    cJSON *data;
    cJSON *fixes;
    char *codeText;
    const TZrChar *noFixReason;

    if (diagnostic == NULL || uriText == NULL) {
        return NULL;
    }

    data = cJSON_CreateObject();
    if (data == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(data, ZR_LSP_FIELD_URI, uriText);
    cJSON_AddItemToObject(data, ZR_LSP_FIELD_RANGE, serialize_range(diagnostic->range));
    cJSON_AddStringToObject(data, ZR_LSP_FIELD_SOURCE, ZR_LSP_DIAGNOSTIC_SOURCE_NAME);
    cJSON_AddNumberToObject(data, ZR_LSP_FIELD_DESCRIPTOR_ID, diagnostic->descriptorId);
    noFixReason = ZrLanguageServer_Lsp_DiagnosticNoFixReasonName(diagnostic->noFixReason);
    if (noFixReason != ZR_NULL) {
        cJSON_AddStringToObject(data, ZR_LSP_FIELD_NO_FIX_REASON, noFixReason);
    }
    if (diagnostic->code != ZR_NULL) {
        codeText = zr_string_to_c_string(diagnostic->code);
        if (codeText != NULL) {
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_CODE, codeText);
            free(codeText);
        }
    }

    if (diagnostic->fixes.isValid && diagnostic->fixes.length > 0) {
        fixes = cJSON_CreateArray();
        if (fixes != NULL) {
            for (TZrSize index = 0; index < diagnostic->fixes.length; index++) {
                const SZrLspDiagnosticFix *fix =
                    (const SZrLspDiagnosticFix *)ZrCore_Array_Get((SZrArray *)&diagnostic->fixes, index);
                if (fix != NULL) {
                    cJSON_AddItemToArray(fixes, serialize_diagnostic_fix(fix));
                }
            }
            cJSON_AddItemToObject(data, ZR_LSP_FIELD_FIXES, fixes);
        }
    }
    return data;
}

static cJSON *serialize_diagnostic_for_uri(const SZrLspDiagnostic *diagnostic, const char *uriText) {
    cJSON *json;
    cJSON *relatedArray;
    cJSON *data;
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

    if (diagnostic->codeDescriptionHref != ZR_NULL) {
        char *hrefText = zr_string_to_c_string(diagnostic->codeDescriptionHref);
        cJSON *codeDescription = cJSON_CreateObject();
        if (hrefText != NULL && codeDescription != NULL) {
            cJSON_AddStringToObject(codeDescription, ZR_LSP_FIELD_HREF, hrefText);
            cJSON_AddItemToObject(json, ZR_LSP_FIELD_CODE_DESCRIPTION, codeDescription);
        } else {
            cJSON_Delete(codeDescription);
        }
        free(hrefText);
    }

    data = serialize_diagnostic_data(diagnostic, uriText);
    if (data != NULL) {
        cJSON_AddItemToObject(json, ZR_LSP_FIELD_DATA, data);
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

cJSON *serialize_diagnostic(const SZrLspDiagnostic *diagnostic) {
    return serialize_diagnostic_for_uri(diagnostic, NULL);
}

cJSON *serialize_diagnostics_array_for_uri(SZrArray *diagnostics, const char *uriText) {
    cJSON *json = cJSON_CreateArray();
    TZrSize index;

    if (json == NULL || diagnostics == ZR_NULL) {
        return json;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_diagnostic_for_uri(*diagnosticPtr, uriText));
        }
    }
    return json;
}

cJSON *serialize_diagnostics_array(SZrArray *diagnostics) {
    return serialize_diagnostics_array_for_uri(diagnostics, NULL);
}
