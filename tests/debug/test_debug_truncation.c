#include <string.h>

#include "debug_internal.h"
#include "runtime_support.h"
#include "unity.h"
#include "zr_vm_core/string.h"

static void test_copy_text_marks_truncated_plain_text(void) {
    TZrChar buffer[24];

    zr_debug_copy_text(buffer, sizeof(buffer), "abcdefghijklmnopqrstuvwxyz0123456789");

    TEST_ASSERT_EQUAL_UINT(sizeof(buffer) - 1u, strlen(buffer));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, "...[+"), "truncated text should include an omitted-byte marker");
    TEST_ASSERT_NOT_EQUAL(0, strcmp(buffer, "abcdefghijklmnopqrstuvw"));
}

static void test_copy_text_preserves_tail_for_paths(void) {
    TZrChar buffer[40];
    const TZrChar *path = "/very/long/generated/debug/source/path/with/useful_tail_file.zr";

    zr_debug_copy_text(buffer, sizeof(buffer), path);

    TEST_ASSERT_TRUE_MESSAGE(strncmp(buffer, "...[+", 5) == 0, "truncated paths should start with a marker");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, "useful_tail_file.zr"), "truncated paths should preserve the filename tail");
}

static void test_copy_text_tiny_buffer_remains_terminated(void) {
    TZrChar buffer[5];

    zr_debug_copy_text(buffer, sizeof(buffer), "abcdefghijklmnopqrstuvwxyz");

    TEST_ASSERT_EQUAL_UINT(sizeof(buffer) - 1u, strlen(buffer));
    TEST_ASSERT_EQUAL_CHAR('\0', buffer[sizeof(buffer) - 1u]);
}

static void test_long_string_value_preview_marks_truncation(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue value;
    TZrChar buffer[32];
    const TZrChar *longText = "value-preview-abcdefghijklmnopqrstuvwxyz-0123456789";

    TEST_ASSERT_NOT_NULL(state);
    memset(&value, 0, sizeof(value));
    value.type = ZR_VALUE_TYPE_STRING;
    value.value.object = ZR_CAST(SZrRawObject *, ZrCore_String_Create(state, (TZrNativeString)longText, strlen(longText)));
    TEST_ASSERT_NOT_NULL(value.value.object);

    zr_debug_format_value_text_safe(state, &value, buffer, sizeof(buffer));

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, "...[+"), "long string previews should advertise truncation");
    TEST_ASSERT_TRUE(strlen(buffer) < strlen(longText));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_long_string_value_preview_exposes_paged_chunks(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *longString;
    ZrDebugAgent agent;
    ZrDebugEvaluateResult result;
    ZrDebugValuePreview *chunks = ZR_NULL;
    TZrSize chunkCount = 0;
    TZrSize namedVariables = 0;
    TZrSize indexedVariables = 0;
    TZrChar error[ZR_DEBUG_TEXT_CAPACITY];
    const TZrChar *longText =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
            "ghijklmnopqrstuvghijklmnopqrstuvghijklmnopqrstuvghijklmnopqrstuv"
            "wxyzABCDEF012345wxyzABCDEF012345wxyzABCDEF012345wxyzABCDEF012345"
            "6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef012345"
            "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";

    TEST_ASSERT_NOT_NULL(state);
    memset(&agent, 0, sizeof(agent));
    memset(&result, 0, sizeof(result));
    error[0] = '\0';
    agent.state = state;
    agent.runMode = ZR_DEBUG_RUN_MODE_PAUSED;
    agent.nextVariableHandleId = ZR_DEBUG_VARIABLE_HANDLE_BASE;
    longString = ZrCore_String_Create(state, (TZrNativeString)longText, strlen(longText));
    TEST_ASSERT_NOT_NULL(longString);
    ZrCore_Value_InitAsRawObject(state, &state->global->zrObject, ZR_CAST_RAW_OBJECT_AS_SUPER(longString));
    state->global->zrObject.type = ZR_VALUE_TYPE_STRING;
    TEST_ASSERT_NOT_NULL(state->global->zrObject.value.object);

    TEST_ASSERT_TRUE(ZrDebug_Evaluate(&agent, 1, "zr", &result, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("", error);
    TEST_ASSERT_EQUAL_STRING("string", result.type_name);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(result.value_text, "...[+"), "long string preview should remain marked");
    TEST_ASSERT_TRUE_MESSAGE(result.variables_reference >= ZR_DEBUG_VARIABLE_HANDLE_BASE,
                             "truncated strings should expose a variables reference");
    TEST_ASSERT_EQUAL_UINT(0u, result.named_variables);
    TEST_ASSERT_TRUE(result.indexed_variables >= 5u);

    TEST_ASSERT_TRUE(ZrDebug_ReadVariables(&agent,
                                           result.variables_reference,
                                           1u,
                                           2u,
                                           &chunks,
                                           &chunkCount,
                                           &namedVariables,
                                           &indexedVariables));
    TEST_ASSERT_EQUAL_UINT(2u, chunkCount);
    TEST_ASSERT_EQUAL_UINT(0u, namedVariables);
    TEST_ASSERT_EQUAL_UINT(result.indexed_variables, indexedVariables);
    TEST_ASSERT_EQUAL_STRING("[64..128)", chunks[0].name);
    TEST_ASSERT_EQUAL_STRING("string", chunks[0].type_name);
    TEST_ASSERT_EQUAL_UINT(64u, strlen(chunks[0].value_text));
    TEST_ASSERT_NULL_MESSAGE(strstr(chunks[0].value_text, "...[+"), "string chunks should contain full text slices");
    TEST_ASSERT_EQUAL_STRING("[128..192)", chunks[1].name);

    ZrDebug_Free(chunks);
    ZrDebug_Free(agent.variableHandles);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_copy_text_marks_truncated_plain_text);
    RUN_TEST(test_copy_text_preserves_tail_for_paths);
    RUN_TEST(test_copy_text_tiny_buffer_remains_terminated);
    RUN_TEST(test_long_string_value_preview_marks_truncation);
    RUN_TEST(test_long_string_value_preview_exposes_paged_chunks);
    return UNITY_END();
}
