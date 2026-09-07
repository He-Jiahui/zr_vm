#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_json_builder.h"

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
            if (!stdio_json_add_owned_array_item(json, serialize_diagnostic_for_uri(*diagnosticPtr, uriText))) {
                cJSON_Delete(json);
                return NULL;
            }
        }
    }
    return json;
}

cJSON *serialize_diagnostics_array(SZrArray *diagnostics) {
    return serialize_diagnostics_array_for_uri(diagnostics, NULL);
}
