#include "zr_vm_language_server_stdio_internal.h"
#include "unity.h"

typedef enum EJsonCase {
    JSON_POSITION,
    JSON_RANGE,
    JSON_LOCATION,
    JSON_DIAGNOSTIC,
    JSON_DIAGNOSTIC_ARRAY,
    JSON_DOCUMENT_REPORT,
    JSON_WORKSPACE_REPORT
} EJsonCase;

static const char g_uriText[] = "file:///diagnostic-json.zr";
static SZrStdioServer *g_server;
static SZrLspDiagnostic g_diagnostic;
static SZrLspLocation g_location;
static SZrArray g_diagnostics;
static cJSON *g_documentParams;
static cJSON *g_workspaceParams;
static cJSON *g_expected;
static size_t g_attempts;
static size_t g_failAt;
static size_t g_failures;
static size_t g_live;
static TZrBool g_persistent;

static void *json_malloc(size_t size) {
    void *pointer;

    g_attempts++;
    if (g_failAt != 0 && (g_attempts == g_failAt || (g_persistent && g_attempts > g_failAt))) {
        g_failures++;
        return ZR_NULL;
    }
    pointer = malloc(size);
    if (pointer != ZR_NULL) {
        g_live++;
    }
    return pointer;
}

static void json_free(void *pointer) {
    if (pointer != ZR_NULL) {
        g_live--;
        free(pointer);
    }
}

static SZrString *new_string(const char *text) {
    SZrString *value = ZrCore_String_Create(g_server->state, text, strlen(text));
    TEST_ASSERT_NOT_NULL(value);
    return value;
}

void setUp(void) {
    SZrLspDiagnosticRelatedInformation related = {0};
    SZrLspDiagnosticFix fix = {0};
    SZrLspDiagnostic *diagnostic = &g_diagnostic;

    g_live = 0;
    g_expected = ZR_NULL;
    g_documentParams = ZR_NULL;
    g_workspaceParams = ZR_NULL;
    memset(&g_diagnostic, 0, sizeof(g_diagnostic));
    memset(&g_diagnostics, 0, sizeof(g_diagnostics));
    g_server = ZrLanguageServer_StdioServer_New(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_server);
    g_location.uri = server_get_cached_uri(g_server, g_uriText);
    TEST_ASSERT_NOT_NULL(g_location.uri);
    g_location.range = (SZrLspRange){{3, 4}, {3, 9}};
    g_diagnostic.range = g_location.range;
    g_diagnostic.severity = 1;
    g_diagnostic.code = new_string("type_mismatch");
    g_diagnostic.message = new_string("diagnostic message");
    g_diagnostic.descriptorId = 2011;
    g_diagnostic.codeDescriptionHref = new_string("https://example.test/diagnostic");
    g_diagnostic.noFixReason = ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION;
    related.location = g_location;
    related.message = new_string("related message");
    fix.title = new_string("Apply cast");
    fix.editRange = g_location.range;
    fix.editText = new_string("int(value)");
    fix.applicability = ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE;
    ZrCore_Array_Init(g_server->state, &g_diagnostic.relatedInformation, sizeof(related), 1);
    ZrCore_Array_Push(g_server->state, &g_diagnostic.relatedInformation, &related);
    ZrCore_Array_Init(g_server->state, &g_diagnostic.fixes, sizeof(fix), 1);
    ZrCore_Array_Push(g_server->state, &g_diagnostic.fixes, &fix);
    ZrCore_Array_Init(g_server->state, &g_diagnostics, sizeof(diagnostic), 2);
    ZrCore_Array_Push(g_server->state, &g_diagnostics, &diagnostic);
    ZrCore_Array_Push(g_server->state, &g_diagnostics, &diagnostic);
    g_documentParams = cJSON_Parse("{\"textDocument\":{\"uri\":\"file:///diagnostic-json.zr\"}}");
    g_workspaceParams = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(g_documentParams);
    TEST_ASSERT_NOT_NULL(g_workspaceParams);
}

void tearDown(void) {
    cJSON_InitHooks(ZR_NULL);
    cJSON_Delete(g_expected);
    cJSON_Delete(g_documentParams);
    cJSON_Delete(g_workspaceParams);
    if (g_server != ZR_NULL) {
        ZrCore_Array_Free(g_server->state, &g_diagnostics);
        ZrCore_Array_Free(g_server->state, &g_diagnostic.relatedInformation);
        ZrCore_Array_Free(g_server->state, &g_diagnostic.fixes);
        ZrLanguageServer_StdioServer_Free(g_server);
        g_server = ZR_NULL;
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_live, "serializer must release every JSON allocation");
}

static void expect_number(const cJSON *object, const char *field, int expected) {
    const cJSON *value = get_object_item(object, field);
    TEST_ASSERT_TRUE(cJSON_IsNumber(value));
    TEST_ASSERT_EQUAL_INT(expected, value->valueint);
}

static void expect_range(const cJSON *range) {
    const cJSON *start = get_object_item(range, "start");
    const cJSON *end = get_object_item(range, "end");
    expect_number(start, "line", 3);
    expect_number(start, "character", 4);
    expect_number(end, "line", 3);
    expect_number(end, "character", 9);
}

static void expect_location(const cJSON *location) {
    TEST_ASSERT_EQUAL_STRING(g_uriText, cJSON_GetStringValue(get_object_item(location, "uri")));
    expect_range(get_object_item(location, "range"));
}

static void expect_diagnostic(const cJSON *json, TZrBool withUri) {
    const cJSON *related = get_object_item(json, "relatedInformation");
    const cJSON *data = get_object_item(json, "data");
    const cJSON *fixes;
    const cJSON *fix;

    expect_range(get_object_item(json, "range"));
    expect_number(json, "severity", 1);
    TEST_ASSERT_EQUAL_STRING(ZR_LSP_DIAGNOSTIC_SOURCE_NAME, cJSON_GetStringValue(get_object_item(json, "source")));
    TEST_ASSERT_EQUAL_STRING("type_mismatch", cJSON_GetStringValue(get_object_item(json, "code")));
    TEST_ASSERT_EQUAL_STRING("diagnostic message", cJSON_GetStringValue(get_object_item(json, "message")));
    TEST_ASSERT_EQUAL_STRING("https://example.test/diagnostic",
                            cJSON_GetStringValue(get_object_item(get_object_item(json, "codeDescription"), "href")));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(related));
    expect_location(get_object_item(cJSON_GetArrayItem(related, 0), "location"));
    TEST_ASSERT_EQUAL_STRING("related message",
                            cJSON_GetStringValue(get_object_item(cJSON_GetArrayItem(related, 0), "message")));
    if (!withUri) {
        TEST_ASSERT_NULL(data);
        return;
    }
    TEST_ASSERT_EQUAL_STRING(g_uriText, cJSON_GetStringValue(get_object_item(data, "uri")));
    TEST_ASSERT_EQUAL_STRING("type_mismatch", cJSON_GetStringValue(get_object_item(data, "code")));
    TEST_ASSERT_EQUAL_STRING("requires_user_decision", cJSON_GetStringValue(get_object_item(data, "noFixReason")));
    expect_number(data, "descriptorId", 2011);
    expect_range(get_object_item(data, "range"));
    fixes = get_object_item(data, "fixes");
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(fixes));
    fix = cJSON_GetArrayItem(fixes, 0);
    TEST_ASSERT_EQUAL_STRING("Apply cast", cJSON_GetStringValue(get_object_item(fix, "title")));
    expect_number(fix, "applicability", ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE);
    expect_range(get_object_item(get_object_item(fix, "edit"), "range"));
    TEST_ASSERT_EQUAL_STRING("int(value)",
                            cJSON_GetStringValue(get_object_item(get_object_item(fix, "edit"), "newText")));
}

static SZrLspHandlerResult serialize_case(EJsonCase kind) {
    SZrLspHandlerResult response = {ZR_LSP_HANDLER_OK, ZR_NULL};

    switch (kind) {
        case JSON_POSITION: response.result = serialize_position(g_location.range.start); break;
        case JSON_RANGE: response.result = serialize_range(g_location.range); break;
        case JSON_LOCATION: response.result = serialize_location(&g_location); break;
        case JSON_DIAGNOSTIC: response.result = serialize_diagnostic(&g_diagnostic); break;
        case JSON_DIAGNOSTIC_ARRAY:
            response.result = serialize_diagnostics_array_for_uri(&g_diagnostics, g_uriText);
            break;
        case JSON_DOCUMENT_REPORT:
            response = handle_text_document_diagnostic_request(g_server, g_documentParams);
            break;
        case JSON_WORKSPACE_REPORT:
            response = handle_workspace_diagnostic_request(g_server, g_workspaceParams);
            break;
    }
    return response;
}

static void expect_complete(EJsonCase kind, const cJSON *json) {
    switch (kind) {
        case JSON_POSITION:
            expect_number(json, "line", 3);
            expect_number(json, "character", 4);
            break;
        case JSON_RANGE: expect_range(json); break;
        case JSON_LOCATION: expect_location(json); break;
        case JSON_DIAGNOSTIC: expect_diagnostic(json, ZR_FALSE); break;
        case JSON_DIAGNOSTIC_ARRAY:
            TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(json));
            expect_diagnostic(cJSON_GetArrayItem(json, 0), ZR_TRUE);
            expect_diagnostic(cJSON_GetArrayItem(json, 1), ZR_TRUE);
            break;
        case JSON_DOCUMENT_REPORT:
        case JSON_WORKSPACE_REPORT: TEST_ASSERT_TRUE(cJSON_Compare(json, g_expected, 1)); break;
    }
}

static size_t run_case(EJsonCase kind, size_t failAt, TZrBool persistent) {
    cJSON_Hooks hooks = {json_malloc, json_free};
    SZrLspHandlerResult response;
    TZrBool hasResult;
    char description[100];

    g_attempts = 0;
    g_failAt = failAt;
    g_failures = 0;
    g_persistent = persistent;
    cJSON_InitHooks(&hooks);
    response = serialize_case(kind);
    hasResult = response.result != ZR_NULL;
    if (failAt == 0) {
        cJSON_InitHooks(ZR_NULL);
        TEST_ASSERT_NOT_NULL(response.result);
        expect_complete(kind, response.result);
        cJSON_InitHooks(&hooks);
    }
    cJSON_Delete(response.result);
    cJSON_InitHooks(ZR_NULL);
    snprintf(description, sizeof(description), "JSON case %d allocation %zu persistent=%d", kind, failAt, persistent);
    if (failAt != 0) {
        TEST_ASSERT_TRUE_MESSAGE(g_failures > 0, description);
        TEST_ASSERT_FALSE_MESSAGE(hasResult, description);
        if (kind == JSON_DOCUMENT_REPORT || kind == JSON_WORKSPACE_REPORT) {
            TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_LSP_HANDLER_INTERNAL_ERROR, response.status, description);
        }
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_OK, response.status);
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0, g_live, description);
    return g_attempts;
}

static void sweep_case(EJsonCase kind) {
    size_t count = run_case(kind, 0, ZR_FALSE);

    TEST_ASSERT_TRUE(count > 0);
    for (size_t index = 1; index <= count; index++) {
        run_case(kind, index, ZR_FALSE);
        run_case(kind, index, ZR_TRUE);
    }
    printf("diagnostic JSON case %d: %zu allocation points\n", kind, count);
}

static void prepare_report(EJsonCase kind, TZrBool unchanged) {
    const char source[] = "fn main(): int {\n var amount: int = 3.75;\n return 0;\n}\n";
    SZrLspHandlerResult response;
    const cJSON *reports;
    const cJSON *report;

    TEST_ASSERT_TRUE(ZrLanguageServer_Lsp_UpdateDocument(
            g_server->state, g_server->context, g_location.uri, source, strlen(source), 1));
    response = serialize_case(kind);
    g_expected = response.result;
    TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_OK, response.status);
    TEST_ASSERT_NOT_NULL(g_expected);
    reports = get_object_item(g_expected, "items");
    report = kind == JSON_DOCUMENT_REPORT ? g_expected : cJSON_GetArrayItem(reports, 0);
    TEST_ASSERT_NOT_NULL(report);
    TEST_ASSERT_TRUE(cJSON_GetArraySize(get_object_item(report, "items")) > 0);
    if (unchanged) {
        const char *resultId = cJSON_GetStringValue(get_object_item(report, "resultId"));
        TEST_ASSERT_NOT_NULL(resultId);
        if (kind == JSON_DOCUMENT_REPORT) {
            TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(g_documentParams, "previousResultId", resultId));
        } else {
            cJSON *previousIds = cJSON_AddArrayToObject(g_workspaceParams, "previousResultIds");
            cJSON *previous = cJSON_CreateObject();
            TEST_ASSERT_NOT_NULL(previousIds);
            TEST_ASSERT_NOT_NULL(previous);
            TEST_ASSERT_TRUE(cJSON_AddItemToArray(previousIds, previous));
            TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(previous, "uri", g_uriText));
            TEST_ASSERT_NOT_NULL(cJSON_AddStringToObject(previous, "value", resultId));
        }
        cJSON_Delete(g_expected);
        response = serialize_case(kind);
        g_expected = response.result;
        TEST_ASSERT_EQUAL_INT(ZR_LSP_HANDLER_OK, response.status);
        report = kind == JSON_DOCUMENT_REPORT ? g_expected : cJSON_GetArrayItem(get_object_item(g_expected, "items"), 0);
        TEST_ASSERT_EQUAL_STRING("unchanged", cJSON_GetStringValue(get_object_item(report, "kind")));
        TEST_ASSERT_NULL(get_object_item(report, "items"));
    }
}

static void test_position_allocation_failures(void) { sweep_case(JSON_POSITION); }
static void test_range_allocation_failures(void) { sweep_case(JSON_RANGE); }
static void test_location_allocation_failures(void) { sweep_case(JSON_LOCATION); }
static void test_diagnostic_allocation_failures(void) { sweep_case(JSON_DIAGNOSTIC); }
static void test_diagnostic_array_allocation_failures(void) { sweep_case(JSON_DIAGNOSTIC_ARRAY); }
static void test_document_full_report_allocation_failures(void) {
    prepare_report(JSON_DOCUMENT_REPORT, ZR_FALSE);
    sweep_case(JSON_DOCUMENT_REPORT);
}
static void test_document_unchanged_report_allocation_failures(void) {
    prepare_report(JSON_DOCUMENT_REPORT, ZR_TRUE);
    sweep_case(JSON_DOCUMENT_REPORT);
}
static void test_workspace_full_report_allocation_failures(void) {
    prepare_report(JSON_WORKSPACE_REPORT, ZR_FALSE);
    sweep_case(JSON_WORKSPACE_REPORT);
}
static void test_workspace_unchanged_report_allocation_failures(void) {
    prepare_report(JSON_WORKSPACE_REPORT, ZR_TRUE);
    sweep_case(JSON_WORKSPACE_REPORT);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_position_allocation_failures);
    RUN_TEST(test_range_allocation_failures);
    RUN_TEST(test_location_allocation_failures);
    RUN_TEST(test_diagnostic_allocation_failures);
    RUN_TEST(test_diagnostic_array_allocation_failures);
    RUN_TEST(test_document_full_report_allocation_failures);
    RUN_TEST(test_document_unchanged_report_allocation_failures);
    RUN_TEST(test_workspace_full_report_allocation_failures);
    RUN_TEST(test_workspace_unchanged_report_allocation_failures);
    return UNITY_END();
}
