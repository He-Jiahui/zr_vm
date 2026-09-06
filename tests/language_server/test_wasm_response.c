#include "wasm_response.h"
#include "zr_vm_language_server/conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_TRACKED_ALLOCATIONS = 512 };
static void *allocations[MAX_TRACKED_ALLOCATIONS];
static size_t allocationOrdinal;
static size_t failureOrdinal;
static size_t injectedFailures;
static int failRemaining;
static int failures;

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "Fail - %s\n", message);
        failures++;
    }
}

static void *tracked_malloc(size_t size) {
    void *pointer;
    size_t index;
    allocationOrdinal++;
    if (failureOrdinal != 0 && (allocationOrdinal == failureOrdinal ||
                               (failRemaining && allocationOrdinal > failureOrdinal))) {
        injectedFailures++;
        return NULL;
    }
    pointer = malloc(size);
    if (pointer == NULL) {
        return NULL;
    }
    for (index = 0; index < MAX_TRACKED_ALLOCATIONS; index++) {
        if (allocations[index] == NULL) {
            allocations[index] = pointer;
            return pointer;
        }
    }
    abort();
}

static void tracked_free(void *pointer) {
    size_t index;
    if (pointer == NULL) {
        return;
    }
    for (index = 0; index < MAX_TRACKED_ALLOCATIONS; index++) {
        if (allocations[index] == pointer) {
            allocations[index] = NULL;
            free(pointer);
            return;
        }
    }
    fprintf(stderr, "untracked cJSON free\n");
    abort();
}

static void check_and_release_leaks(void) {
    size_t index;
    size_t leaked = 0;
    for (index = 0; index < MAX_TRACKED_ALLOCATIONS; index++) {
        if (allocations[index] != NULL) {
            leaked++;
            free(allocations[index]);
            allocations[index] = NULL;
        }
    }
    expect_true(leaked == 0, "response must consume input and release every allocation");
}

static size_t run_response(const char *input, int code, size_t failAt, int persistent) {
    cJSON *data;
    cJSON *expected;
    cJSON *response;
    const cJSON *value;
    const char *text;
    size_t ordinals;
    failureOrdinal = 0;
    data = input == NULL ? NULL : cJSON_Parse(input);
    expect_true(input == NULL || data != NULL, "fixture data must parse");

    allocationOrdinal = 0;
    injectedFailures = 0;
    failureOrdinal = failAt;
    failRemaining = persistent;
    text = code == 0 ? ZrLanguageServer_Wasm_SuccessResponse(data)
                     : ZrLanguageServer_Wasm_ErrorResponse(code, "same message for every code");
    ordinals = allocationOrdinal;
    failureOrdinal = 0;

    if (failAt != 0) {
        expect_true(injectedFailures != 0, "selected envelope allocation must be reached");
    } else {
        expect_true(text != NULL, "response must exist without allocation failure");
    }
    if (text != NULL) {
        response = cJSON_Parse(text);
        expect_true(cJSON_IsObject(response), "response must be a JSON object");
        value = cJSON_GetObjectItemCaseSensitive(response, "success");
        if (code == 0 && input != NULL) {
            expect_true(cJSON_IsTrue(value), "successful data must retain true success");
            expect_true(cJSON_GetArraySize(response) == 2, "success must contain only success and data");
            expected = cJSON_Parse(input);
            expect_true(cJSON_Compare(expected, cJSON_GetObjectItemCaseSensitive(response, "data"), 1),
                        "success must preserve explicit null, empty and nonempty data");
            cJSON_Delete(expected);
        } else {
            expect_true(cJSON_IsFalse(value), "missing data and errors must not become success");
            expect_true(cJSON_GetArraySize(response) == 3, "error must contain success, code and error");
            value = cJSON_GetObjectItemCaseSensitive(response, "code");
            expect_true(cJSON_IsNumber(value) && value->valueint ==
                                (code == 0 ? ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE : code),
                        "caller error code must survive independently of message text");
            value = cJSON_GetObjectItemCaseSensitive(response, "error");
            expect_true(cJSON_IsString(value) && value->valuestring[0] != '\0',
                        "errors must contain a message");
            if (code != 0 && cJSON_IsString(value)) {
                expect_true(strcmp(value->valuestring, "same message for every code") == 0,
                            "error message must be preserved");
            }
        }
        cJSON_Delete(response);
        cJSON_free((void *)text);
    }
    check_and_release_leaks();
    return ordinals;
}

int main(void) {
    const int codes[] = {ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE, ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE,
                         ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE, ZR_LSP_JSON_RPC_CONTENT_MODIFIED_CODE};
    const char *inputs[] = {"null", "[]", "{\"items\":[{\"label\":\"value\"}],\"version\":7}", NULL};
    cJSON_Hooks hooks = {tracked_malloc, tracked_free};
    size_t index;
    size_t ordinal;
    size_t count;
    size_t scenarios = 0;
    int persistent;
    cJSON_InitHooks(&hooks);
    for (index = 0; index < sizeof(codes) / sizeof(codes[0]); index++) {
        count = run_response(NULL, codes[index], 0, 0);
        scenarios++;
        for (persistent = 0; persistent <= 1; persistent++) {
            for (ordinal = 1; ordinal <= count; ordinal++) {
                run_response(NULL, codes[index], ordinal, persistent);
                scenarios++;
            }
        }
    }
    for (index = 0; index < sizeof(inputs) / sizeof(inputs[0]); index++) {
        count = run_response(inputs[index], 0, 0, 0);
        scenarios++;
        for (persistent = 0; persistent <= 1; persistent++) {
            for (ordinal = 1; ordinal <= count; ordinal++) {
                run_response(inputs[index], 0, ordinal, persistent);
                scenarios++;
            }
        }
    }
    cJSON_InitHooks(NULL);
    printf("WASM response: %zu scenarios, %d failures\n", scenarios, failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
