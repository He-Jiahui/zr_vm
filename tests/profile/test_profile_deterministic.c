#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_debug/profile.h"
#include "zr_vm_parser.h"

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceLabel) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceLabel);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void test_deterministic_profile_counts_calls_and_returns(void) {
    const char *source =
            "func leaf(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "func mid(value: int): int {\n"
            "    return leaf(value) + leaf(value + 1);\n"
            "}\n"
            "var first = mid(1);\n"
            "var second = mid(2);\n"
            "return first + second;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugProfile profile;
    const ZrDebugProfileEntry *midEntry;
    const ZrDebugProfileEntry *leafEntry;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "profile_deterministic.zr");
    TEST_ASSERT_NOT_NULL(function);

    ZrDebug_Profile_Init(&profile);
    TEST_ASSERT_TRUE(ZrDebug_Profile_Start(&profile, state));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(12, result);

    ZrDebug_Profile_Stop(&profile);

    midEntry = ZrDebug_Profile_FindByName(&profile, "mid");
    leafEntry = ZrDebug_Profile_FindByName(&profile, "leaf");
    TEST_ASSERT_NOT_NULL(midEntry);
    TEST_ASSERT_NOT_NULL(leafEntry);
    TEST_ASSERT_EQUAL_UINT64(2u, midEntry->call_count);
    TEST_ASSERT_EQUAL_UINT64(2u, midEntry->return_count);
    TEST_ASSERT_EQUAL_UINT64(4u, leafEntry->call_count);
    TEST_ASSERT_EQUAL_UINT64(4u, leafEntry->return_count);
    TEST_ASSERT_TRUE(midEntry->total_time_ns >= midEntry->self_time_ns);
    TEST_ASSERT_TRUE(leafEntry->total_time_ns >= leafEntry->self_time_ns);
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Debug_GetHookMask(state));

    ZrDebug_Profile_Destroy(&profile);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_sampling_profile_records_hot_function_lines(void) {
    const char *source =
            "func leaf(value: int): int {\n"
            "    return value + 1;\n"
            "}\n"
            "var total = 0;\n"
            "for (var index = 0; index < 8; index = index + 1) {\n"
            "    total = total + leaf(index);\n"
            "}\n"
            "return total;";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugProfile profile;
    TZrInt64 result = 0;
    TZrSize index;
    TZrBool sawLeafSample = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "profile_sampling.zr");
    TEST_ASSERT_NOT_NULL(function);

    ZrDebug_Profile_Init(&profile);
    TEST_ASSERT_TRUE(ZrDebug_Profile_StartWithSampling(&profile, state, 1u));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(36, result);

    ZrDebug_Profile_Stop(&profile);

    TEST_ASSERT_GREATER_THAN_UINT64(0u, ZrDebug_Profile_GetSampleCount(&profile));
    for (index = 0u; index < ZrDebug_Profile_GetSampleCount(&profile); index++) {
        const ZrDebugProfileSample *sample = ZrDebug_Profile_GetSample(&profile, index);
        TEST_ASSERT_NOT_NULL(sample);
        if (strcmp(sample->name, "leaf") == 0) {
            TEST_ASSERT_GREATER_THAN_UINT64(0u, sample->sample_count);
            TEST_ASSERT_TRUE(sample->line > 0u);
            sawLeafSample = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(sawLeafSample, "sampling profiler should record at least one leaf line");
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Debug_GetHookMask(state));

    ZrDebug_Profile_Destroy(&profile);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_deterministic_profile_counts_calls_and_returns);
    RUN_TEST(test_sampling_profile_records_hot_function_lines);
    return UNITY_END();
}
