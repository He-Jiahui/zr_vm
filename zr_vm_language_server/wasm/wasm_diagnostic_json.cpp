#include "wasm_diagnostic_json.h"

#ifdef __cplusplus
extern "C" {
#define class class_
#endif

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/lsp_interface.h"

#ifdef __cplusplus
#undef class
}
#endif

#include "cJSON/cJSON.h"

#include <cstring>

static char *wasm_diagnostic_copy_string(SZrState *state, const SZrString *value) {
    TZrNativeString nativeText;
    TZrSize length;
    TZrChar *text;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }
    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nativeText = ZrCore_String_GetNativeStringShort((SZrString *)value);
        length = value->shortStringLength;
    } else {
        nativeText = ZrCore_String_GetNativeString((SZrString *)value);
        length = value->longStringLength;
    }
    if (nativeText == ZR_NULL) {
        return ZR_NULL;
    }

    text = (TZrChar *)ZrCore_Memory_RawMalloc(
            state->global,
            (length + 1U) * sizeof(TZrChar));
    if (text != ZR_NULL) {
        std::memcpy(text, nativeText, length * sizeof(TZrChar));
        text[length] = '\0';
    }
    return text;
}

static void wasm_diagnostic_free_string(SZrState *state, char *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawFree(
            state->global,
            text,
            (std::strlen(text) + 1U) * sizeof(TZrChar));
}

static void wasm_diagnostic_add_string(
        SZrState *state,
        cJSON *object,
        const char *field,
        const SZrString *value) {
    char *text;

    if (object == ZR_NULL || field == ZR_NULL || value == ZR_NULL) {
        return;
    }
    text = wasm_diagnostic_copy_string(state, value);
    if (text != ZR_NULL) {
        cJSON_AddStringToObject(object, field, text);
    }
    wasm_diagnostic_free_string(state, text);
}

static cJSON *wasm_diagnostic_serialize_position(SZrLspPosition position) {
    cJSON *json = cJSON_CreateObject();

    if (json != ZR_NULL) {
        cJSON_AddNumberToObject(json, ZR_LSP_FIELD_LINE, position.line);
        cJSON_AddNumberToObject(json, ZR_LSP_FIELD_CHARACTER, position.character);
    }
    return json;
}

static cJSON *wasm_diagnostic_serialize_range(SZrLspRange range) {
    cJSON *json = cJSON_CreateObject();

    if (json != ZR_NULL) {
        cJSON_AddItemToObject(
                json,
                ZR_LSP_FIELD_START,
                wasm_diagnostic_serialize_position(range.start));
        cJSON_AddItemToObject(
                json,
                ZR_LSP_FIELD_END,
                wasm_diagnostic_serialize_position(range.end));
    }
    return json;
}

static cJSON *wasm_diagnostic_serialize_fix(
        SZrState *state,
        const SZrLspDiagnosticFix *fix) {
    cJSON *json;
    cJSON *edit;

    if (state == ZR_NULL || fix == ZR_NULL) {
        return cJSON_CreateNull();
    }
    json = cJSON_CreateObject();
    edit = cJSON_CreateObject();
    if (json == ZR_NULL || edit == ZR_NULL) {
        cJSON_Delete(json);
        cJSON_Delete(edit);
        return ZR_NULL;
    }

    wasm_diagnostic_add_string(state, json, ZR_LSP_FIELD_TITLE, fix->title);
    cJSON_AddItemToObject(
            edit,
            ZR_LSP_FIELD_RANGE,
            wasm_diagnostic_serialize_range(fix->editRange));
    wasm_diagnostic_add_string(state, edit, ZR_LSP_FIELD_NEW_TEXT, fix->editText);
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_EDIT, edit);
    cJSON_AddNumberToObject(
            json,
            ZR_LSP_FIELD_APPLICABILITY,
            fix->applicability);
    return json;
}

static cJSON *wasm_diagnostic_serialize_data(
        SZrState *state,
        const SZrLspDiagnostic *diagnostic,
        const SZrString *uri) {
    const TZrChar *noFixReason;
    cJSON *data;

    data = cJSON_CreateObject();
    if (data == ZR_NULL) {
        return ZR_NULL;
    }
    wasm_diagnostic_add_string(state, data, ZR_LSP_FIELD_URI, uri);
    cJSON_AddItemToObject(
            data,
            ZR_LSP_FIELD_RANGE,
            wasm_diagnostic_serialize_range(diagnostic->range));
    cJSON_AddStringToObject(
            data,
            ZR_LSP_FIELD_SOURCE,
            ZR_LSP_DIAGNOSTIC_SOURCE_NAME);
    cJSON_AddNumberToObject(
            data,
            ZR_LSP_FIELD_DESCRIPTOR_ID,
            diagnostic->descriptorId);
    wasm_diagnostic_add_string(state, data, ZR_LSP_FIELD_CODE, diagnostic->code);

    noFixReason = ZrLanguageServer_Lsp_DiagnosticNoFixReasonName(
            diagnostic->noFixReason);
    if (noFixReason != ZR_NULL) {
        cJSON_AddStringToObject(
                data,
                ZR_LSP_FIELD_NO_FIX_REASON,
                noFixReason);
    }
    if (diagnostic->fixes.isValid && diagnostic->fixes.length > 0U) {
        cJSON *fixes = cJSON_CreateArray();
        if (fixes != ZR_NULL) {
            for (TZrSize index = 0U; index < diagnostic->fixes.length; index++) {
                const SZrLspDiagnosticFix *fix =
                        (const SZrLspDiagnosticFix *)ZrCore_Array_Get(
                                (SZrArray *)&diagnostic->fixes,
                                index);
                if (fix != ZR_NULL) {
                    cJSON_AddItemToArray(
                            fixes,
                            wasm_diagnostic_serialize_fix(state, fix));
                }
            }
            cJSON_AddItemToObject(data, ZR_LSP_FIELD_FIXES, fixes);
        }
    }
    return data;
}

static cJSON *wasm_diagnostic_serialize_related(
        SZrState *state,
        const SZrLspDiagnosticRelatedInformation *related) {
    cJSON *json;
    cJSON *location;

    if (state == ZR_NULL || related == ZR_NULL) {
        return cJSON_CreateNull();
    }
    json = cJSON_CreateObject();
    location = cJSON_CreateObject();
    if (json == ZR_NULL || location == ZR_NULL) {
        cJSON_Delete(json);
        cJSON_Delete(location);
        return ZR_NULL;
    }

    wasm_diagnostic_add_string(
            state,
            location,
            ZR_LSP_FIELD_URI,
            related->location.uri);
    cJSON_AddItemToObject(
            location,
            ZR_LSP_FIELD_RANGE,
            wasm_diagnostic_serialize_range(related->location.range));
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_LOCATION, location);
    wasm_diagnostic_add_string(
            state,
            json,
            ZR_LSP_FIELD_MESSAGE,
            related->message);
    return json;
}

static cJSON *wasm_diagnostic_serialize_one(
        SZrState *state,
        const SZrLspDiagnostic *diagnostic,
        const SZrString *uri) {
    cJSON *json;

    if (state == ZR_NULL || diagnostic == ZR_NULL) {
        return cJSON_CreateNull();
    }
    json = cJSON_CreateObject();
    if (json == ZR_NULL) {
        return ZR_NULL;
    }

    cJSON_AddItemToObject(
            json,
            ZR_LSP_FIELD_RANGE,
            wasm_diagnostic_serialize_range(diagnostic->range));
    cJSON_AddNumberToObject(
            json,
            ZR_LSP_FIELD_SEVERITY,
            diagnostic->severity);
    cJSON_AddStringToObject(
            json,
            ZR_LSP_FIELD_SOURCE,
            ZR_LSP_DIAGNOSTIC_SOURCE_NAME);
    wasm_diagnostic_add_string(state, json, ZR_LSP_FIELD_CODE, diagnostic->code);
    wasm_diagnostic_add_string(
            state,
            json,
            ZR_LSP_FIELD_MESSAGE,
            diagnostic->message);

    if (diagnostic->codeDescriptionHref != ZR_NULL) {
        cJSON *codeDescription = cJSON_CreateObject();
        if (codeDescription != ZR_NULL) {
            wasm_diagnostic_add_string(
                    state,
                    codeDescription,
                    ZR_LSP_FIELD_HREF,
                    diagnostic->codeDescriptionHref);
            cJSON_AddItemToObject(
                    json,
                    ZR_LSP_FIELD_CODE_DESCRIPTION,
                    codeDescription);
        }
    }
    cJSON_AddItemToObject(
            json,
            ZR_LSP_FIELD_DATA,
            wasm_diagnostic_serialize_data(state, diagnostic, uri));

    if (diagnostic->relatedInformation.isValid &&
        diagnostic->relatedInformation.length > 0U) {
        cJSON *relatedArray = cJSON_CreateArray();
        if (relatedArray != ZR_NULL) {
            for (TZrSize index = 0U;
                 index < diagnostic->relatedInformation.length;
                 index++) {
                const SZrLspDiagnosticRelatedInformation *related =
                        (const SZrLspDiagnosticRelatedInformation *)ZrCore_Array_Get(
                                (SZrArray *)&diagnostic->relatedInformation,
                                index);
                if (related != ZR_NULL) {
                    cJSON_AddItemToArray(
                            relatedArray,
                            wasm_diagnostic_serialize_related(state, related));
                }
            }
            cJSON_AddItemToObject(
                    json,
                    ZR_LSP_FIELD_RELATED_INFORMATION,
                    relatedArray);
        }
    }
    return json;
}

cJSON *ZrLanguageServer_Wasm_SerializeDiagnostics(
        SZrState *state,
        SZrArray *diagnostics,
        const SZrString *uri) {
    cJSON *json = cJSON_CreateArray();

    if (json == ZR_NULL || state == ZR_NULL || diagnostics == ZR_NULL) {
        return json;
    }
    for (TZrSize index = 0U; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr =
                (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL) {
            cJSON_AddItemToArray(
                    json,
                    wasm_diagnostic_serialize_one(state, *diagnosticPtr, uri));
        }
    }
    return json;
}
