#include <string.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_debug/coverage.h"
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

static SZrFunction *find_child_function_by_name(SZrFunction *function, const char *name) {
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->functionName != ZR_NULL) {
        const char *functionName = ZrCore_String_GetNativeString(function->functionName);
        if (functionName != ZR_NULL && strcmp(functionName, name) == 0) {
            return function;
        }
    }

    for (index = 0u; index < function->childFunctionLength; index++) {
        SZrFunction *match = find_child_function_by_name(&function->childFunctionList[index], name);
        if (match != ZR_NULL) {
            return match;
        }
    }

    return ZR_NULL;
}

static TZrBool has_active_line(const TZrUInt32 *lines, TZrSize count, TZrUInt32 line) {
    TZrSize index;

    for (index = 0u; index < count; index++) {
        if (lines[index] == line) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void test_core_active_lines_extract_unique_executable_lines(void) {
    const char *source =
            "func choose(flag: bool): int {\n"
            "    if (flag) {\n"
            "        return 10;\n"
            "    }\n"
            "    return 20;\n"
            "}\n"
            "return choose(true);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *chooseFunction;
    TZrUInt32 lines[16];
    TZrSize count;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "coverage_active_lines.zr");
    TEST_ASSERT_NOT_NULL(function);
    chooseFunction = find_child_function_by_name(function, "choose");
    TEST_ASSERT_NOT_NULL(chooseFunction);

    count = ZrCore_Debug_GetActiveLines(chooseFunction, ZR_NULL, 0u);
    TEST_ASSERT_TRUE(count >= 3u);
    TEST_ASSERT_EQUAL_UINT64(count, ZrCore_Debug_GetActiveLines(chooseFunction, lines, 16u));
    TEST_ASSERT_TRUE(has_active_line(lines, count, 2u));
    TEST_ASSERT_TRUE(has_active_line(lines, count, 3u));
    TEST_ASSERT_TRUE(has_active_line(lines, count, 5u));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_coverage_records_executed_and_uncovered_lines(void) {
    const char *source =
            "func choose(flag: bool): int {\n"
            "    if (flag) {\n"
            "        return 10;\n"
            "    }\n"
            "    return 20;\n"
            "}\n"
            "return choose(true);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    ZrDebugCoverage coverage;
    TZrInt64 result = 0;
    TZrSize index;
    TZrBool sawExecutedReturn = ZR_FALSE;
    TZrBool sawUncoveredReturn = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "coverage_lines.zr");
    TEST_ASSERT_NOT_NULL(function);

    ZrDebug_Coverage_Init(&coverage);
    TEST_ASSERT_TRUE(ZrDebug_Coverage_RegisterFunctionTree(&coverage, function));
    TEST_ASSERT_TRUE(ZrDebug_Coverage_Start(&coverage, state));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(10, result);

    ZrDebug_Coverage_Stop(&coverage);

    TEST_ASSERT_TRUE(ZrDebug_Coverage_GetLineCount(&coverage) >= 3u);
    for (index = 0u; index < ZrDebug_Coverage_GetLineCount(&coverage); index++) {
        const ZrDebugCoverageLine *line = ZrDebug_Coverage_GetLine(&coverage, index);
        TEST_ASSERT_NOT_NULL(line);
        if (strcmp(line->name, "choose") == 0 && line->line == 3u) {
            TEST_ASSERT_TRUE(line->executable);
            TEST_ASSERT_TRUE(line->executed);
            sawExecutedReturn = ZR_TRUE;
        }
        if (strcmp(line->name, "choose") == 0 && line->line == 5u) {
            TEST_ASSERT_TRUE(line->executable);
            TEST_ASSERT_FALSE(line->executed);
            sawUncoveredReturn = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(sawExecutedReturn, "coverage should mark the taken return line executed");
    TEST_ASSERT_TRUE_MESSAGE(sawUncoveredReturn, "coverage should keep the untaken return line uncovered");
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Debug_GetHookMask(state));

    ZrDebug_Coverage_Destroy(&coverage);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_core_active_lines_extract_unique_executable_lines);
    RUN_TEST(test_coverage_records_executed_and_uncovered_lines);
    return UNITY_END();
}
