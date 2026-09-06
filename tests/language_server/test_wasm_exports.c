#include "wasm_exports.h"
#include "zr_vm_language_server/conf.h"
#include "cJSON/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static size_t allocationOrdinal;
static size_t failureOrdinal;

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "Fail - %s\n", message);
        failures++;
    }
}

static void *fault_malloc(size_t size) {
    allocationOrdinal++;
    return allocationOrdinal == failureOrdinal ? NULL : malloc(size);
}

static cJSON *take_response(const char *text) {
    cJSON *json;
    expect_true(text != NULL, "non-faulted export must return JSON");
    json = text == NULL ? NULL : cJSON_Parse(text);
    expect_true(cJSON_IsObject(json), "export response must be an object");
    cJSON_free((void *)text);
    return json;
}

int main(void) {
    static const char uri[] = "file:///wasm-response-empty.zr";
    void *context = wasm_ZrLspContextNew();
    cJSON *json;
    const cJSON *reports;
    const cJSON *report;
    const char *text;
    size_t count;
    size_t ordinal;
    cJSON_Hooks hooks = {fault_malloc, free};

    expect_true(context != NULL, "real core context must initialize");
    json = take_response(wasm_ZrLspGetHover(NULL, uri, sizeof(uri) - 1, 0, 0));
    expect_true(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(json, "success")),
                "invalid parameters must be an error, not success true");
    report = cJSON_GetObjectItemCaseSensitive(json, "code");
    expect_true(cJSON_IsNumber(report) && report->valueint == ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE,
                "invalid parameters must carry the explicit shared error code");
    cJSON_Delete(json);

    json = take_response(wasm_ZrLspUpdateDocument(context, uri, sizeof(uri) - 1, "", 0, 1));
    expect_true(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")), "empty document must open");
    cJSON_Delete(json);
    json = take_response(wasm_ZrLspGetHover(context, uri, sizeof(uri) - 1, 0, 0));
    expect_true(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")) &&
                (cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(json, "data")) ||
                 cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(json, "data"))),
                "real core hover must use the success envelope");
    cJSON_Delete(json);

    cJSON_InitHooks(&hooks);
    allocationOrdinal = 0;
    text = wasm_ZrLspGetWorkspaceDiagnosticReports(context);
    count = allocationOrdinal;
    cJSON_InitHooks(NULL);
    json = take_response(text);
    reports = cJSON_GetObjectItemCaseSensitive(json, "data");
    report = cJSON_GetArrayItem(reports, 0);
    expect_true(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")) &&
                cJSON_IsArray(reports) && cJSON_GetArraySize(reports) == 1 &&
                cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(report, "items")),
                "workspace report must contain the opened empty document");
    cJSON_Delete(json);

    for (ordinal = 1; ordinal <= count; ordinal++) {
        cJSON_InitHooks(&hooks);
        allocationOrdinal = 0;
        failureOrdinal = ordinal;
        text = wasm_ZrLspGetWorkspaceDiagnosticReports(context);
        failureOrdinal = 0;
        cJSON_InitHooks(NULL);
        if (text != NULL) {
            json = take_response(text);
            expect_true(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(json, "success")),
                        "workspace serialization failure must not return a partial success");
            report = cJSON_GetObjectItemCaseSensitive(json, "code");
            expect_true(cJSON_IsNumber(report) && report->valueint == ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE,
                        "workspace serialization failure must carry InternalError");
            cJSON_Delete(json);
        }
    }
    json = take_response(wasm_ZrLspCloseDocument(context, uri, sizeof(uri) - 1));
    cJSON_Delete(json);
    wasm_ZrLspContextFree(context);
    printf("WASM exports: real core hover and %zu workspace allocation faults, %d failures\n", count, failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
