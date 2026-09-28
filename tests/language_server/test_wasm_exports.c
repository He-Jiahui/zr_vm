#include "wasm_exports.h"
#include "zr_vm_language_server/conf.h"
#include "cJSON/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 故障扫描的计数仅由本宿主进程持有，main 逐轮设置序号并汇总断言结果。 */
static int failures;
static size_t allocationOrdinal;
static size_t failureOrdinal;

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "Fail - %s\n", message);
        failures++;
    }
}

/* 给 cJSON 钩子按分配序号注入失败，以枚举导出 JSON 封装的各个分配点。 */
static void *fault_malloc(size_t size) {
    allocationOrdinal++;
    return allocationOrdinal == failureOrdinal ? NULL : malloc(size);
}

/* 宿主测试接管 WASM 导出返回的 JSON 文本并在解析后释放；解析结果由调用方删除。 */
static cJSON *take_response(const char *text) {
    cJSON *json;
    expect_true(text != NULL, "non-faulted export must return JSON");
    json = text == NULL ? NULL : cJSON_Parse(text);
    expect_true(cJSON_IsObject(json), "export response must be an object");
    cJSON_free((void *)text);
    return json;
}

/* 语法错误文本仍是已提交版本；旧版本重放不能覆盖修复后的快照。 */
static void test_document_update_snapshot_contract(void *context) {
    static const char uri[] = "file:///wasm-update-snapshot.zr";
    static const char broken[] = "\"unterminated";
    static const char repaired[] = "var value: int = 1;";
    cJSON *json = take_response(wasm_ZrLspUpdateDocument(
            context, uri, sizeof(uri) - 1, broken, sizeof(broken) - 1, 1));
    expect_true(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")),
                "committed text with a syntax error must be acknowledged");
    cJSON_Delete(json);
    json = take_response(wasm_ZrLspGetDiagnosticReport(context, uri, sizeof(uri) - 1));
    expect_true(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")),
                "committed syntax error must permit diagnostic queries");
    expect_true(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(
                        cJSON_GetObjectItemCaseSensitive(json, "data"), "items")) > 0,
                "unterminated source string must produce an editor diagnostic");
    cJSON_Delete(json);
    json = take_response(wasm_ZrLspUpdateDocument(
            context, uri, sizeof(uri) - 1, repaired, sizeof(repaired) - 1, 2));
    expect_true(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")),
                "a newer edit must repair the committed error snapshot");
    cJSON_Delete(json);
    json = take_response(wasm_ZrLspUpdateDocument(
            context, uri, sizeof(uri) - 1, broken, sizeof(broken) - 1, 2));
    expect_true(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(json, "success")),
                "a stale version must not acknowledge uncommitted replacement text");
    cJSON_Delete(json);
    json = take_response(wasm_ZrLspCloseDocument(context, uri, sizeof(uri) - 1));
    cJSON_Delete(json);
}

/* 宿主 CTest 检查实际 VM 导出和统一 JSON 响应，逐个分配故障点验证不会返回部分成功。 */
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
    test_document_update_snapshot_contract(context);
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

    json = take_response(wasm_ZrLspGetDefinition(context, uri, sizeof(uri) - 1, 0, 0));
    expect_true(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")) &&
                cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(json, "data")) &&
                cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(json, "data")) == 0,
                "definition on empty source must be an empty successful result");
    cJSON_Delete(json);
    json = take_response(wasm_ZrLspGetDocumentHighlights(context, uri, sizeof(uri) - 1, 0, 0));
    expect_true(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")) &&
                cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(json, "data")) &&
                cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(json, "data")) == 0,
                "highlights on empty source must be an empty successful result");
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
