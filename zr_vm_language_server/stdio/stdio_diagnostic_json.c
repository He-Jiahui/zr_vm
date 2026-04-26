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

static cJSON *serialize_diagnostic_data(const SZrLspDiagnostic *diagnostic, const char *uriText) {
    cJSON *data;
    char *codeText;

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
    if (diagnostic->code != ZR_NULL) {
        codeText = zr_string_to_c_string(diagnostic->code);
        if (codeText != NULL) {
            cJSON_AddStringToObject(data, ZR_LSP_FIELD_CODE, codeText);
            free(codeText);
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
