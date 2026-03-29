//
// Exception runtime and compiler behavior tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_INFO(summary, details)                                                                                    \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                      \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                   \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason)                                                                       \
    do {                                                                                                               \
        double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                      \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason);                                     \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_MODULE_DIVIDER()                                                                                          \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

static void test_panic_handler(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
}

static SZrState *create_test_state(void) {
    SZrState *state = ZrTests_State_Create(test_panic_handler);
    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        ZrVmLibSystem_Register(state->global);
    }
    return state;
}

static void destroy_test_state(SZrState *state) {
    ZrTests_State_Destroy(state);
}

static TZrBool function_contains_opcode(SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrFunction *find_child_function_by_name(SZrFunction *function, const TZrChar *nameLiteral) {
    TZrUInt32 index;

    if (function == ZR_NULL || nameLiteral == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        SZrFunction *child = &function->childFunctionList[index];
        TZrNativeString functionName;

        if (child == ZR_NULL || child->functionName == ZR_NULL) {
            continue;
        }

        functionName = ZrCore_String_GetNativeStringShort(child->functionName);
        if (functionName != ZR_NULL && strcmp(functionName, nameLiteral) == 0) {
            return child;
        }
    }

    return ZR_NULL;
}

static TZrBool compile_source_to_function(SZrState *state,
                                          const TZrChar *source,
                                          const TZrChar *sourceNameLiteral,
                                          SZrFunction **function) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameLiteral == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceName = ZrCore_String_Create(state, sourceNameLiteral, strlen(sourceNameLiteral));
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }

    *function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    return *function != ZR_NULL;
}

static TZrBool execute_source_expect_int64(SZrState *state,
                                           const TZrChar *source,
                                           const TZrChar *sourceNameLiteral,
                                           TZrInt64 *result) {
    SZrFunction *function = ZR_NULL;
    TZrBool success;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }

    *result = 0;
    if (!compile_source_to_function(state, source, sourceNameLiteral, &function)) {
        return ZR_FALSE;
    }

    success = ZrTests_Function_ExecuteExpectInt64(state, function, result);
    ZrCore_Function_Free(state, function);
    return success;
}

// 测试初始化
void setUp(void) {}

void tearDown(void) {}

static void test_throw_string_is_boxed_and_caught_by_base_error(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source =
            "try {\n"
            "    throw \"x\";\n"
            "} catch (e) {\n"
            "    if (e.message == \"x\" && e.exception == \"x\") {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    TZrInt64 result = 0;
    SZrState *state;

    TEST_START("Throw String Is Boxed As Error");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("throw string boxing",
              "Testing that throw \"x\" is boxed to Error semantics and caught by an untyped catch.");

    TEST_ASSERT_TRUE(execute_source_expect_int64(state, source, "exception_boxing_test.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Throw String Is Boxed As Error");
    TEST_DIVIDER();
}

static void test_derived_exception_prefers_first_matching_catch_clause(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source =
            "var exception = import(\"zr.system.exception\");\n"
            "try {\n"
            "    throw $exception.RuntimeError(\"boom\");\n"
            "} catch (e: RuntimeError) {\n"
            "    return 1;\n"
            "} catch (e: Error) {\n"
            "    return 2;\n"
            "}\n"
            "return 0;\n";
    TZrInt64 result = 0;
    SZrState *state;

    TEST_START("Derived Exception Prefers First Matching Catch");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("typed multi-catch ordering",
              "Testing RuntimeError matching and ordered catch clause dispatch through zr.system.exception.");

    TEST_ASSERT_TRUE(execute_source_expect_int64(state, source, "exception_multicatch_test.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Derived Exception Prefers First Matching Catch");
    TEST_DIVIDER();
}

static void test_finally_runs_for_normal_return_and_throw_paths(void) {
    SZrTestTimer timer = {0};
    const TZrChar *normalSource =
            "var value = 0;\n"
            "try {\n"
            "    value = 1;\n"
            "} finally {\n"
            "    value = value + 1;\n"
            "}\n"
            "return value;\n";
    const TZrChar *returnSource =
            "var marker = 0;\n"
            "run() {\n"
            "    try {\n"
            "        return 7;\n"
            "    } finally {\n"
            "        marker = 9;\n"
            "    }\n"
            "}\n"
            "var result = run();\n"
            "if (result == 7 && marker == 9) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
    const TZrChar *throwSource =
            "var marker = 0;\n"
            "try {\n"
            "    try {\n"
            "        throw \"boom\";\n"
            "    } finally {\n"
            "        marker = 5;\n"
            "    }\n"
            "} catch (e) {\n"
            "    return marker;\n"
            "}\n"
            "return 0;\n";
    TZrInt64 normalResult = 0;
    TZrInt64 returnResult = 0;
    TZrInt64 throwResult = 0;
    SZrState *state;

    TEST_START("Finally Runs For All Control Paths");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("finally semantics",
              "Testing finally execution on normal completion, return, and throw paths.");

    TEST_ASSERT_TRUE(execute_source_expect_int64(state, normalSource, "finally_normal_test.zr", &normalResult));
    TEST_ASSERT_EQUAL_INT64(2, normalResult);

    TEST_ASSERT_TRUE(execute_source_expect_int64(state, returnSource, "finally_return_test.zr", &returnResult));
    TEST_ASSERT_EQUAL_INT64(1, returnResult);

    TEST_ASSERT_TRUE(execute_source_expect_int64(state, throwSource, "finally_throw_test.zr", &throwResult));
    TEST_ASSERT_EQUAL_INT64(5, throwResult);

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Finally Runs For All Control Paths");
    TEST_DIVIDER();
}

static void test_named_function_finally_closure_and_sibling_function_metadata(void) {
    SZrTestTimer timer = {0};
    const TZrChar *closureSource =
            "var marker = 0;\n"
            "run() {\n"
            "    try {\n"
            "        return 7;\n"
            "    } finally {\n"
            "        marker = 9;\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    const TZrChar *siblingSource =
            "outer() {\n"
            "    return inner();\n"
            "}\n"
            "inner() {\n"
            "    return 1;\n"
            "}\n"
            "return outer();\n";
    SZrState *state;
    SZrFunction *closureFunction = ZR_NULL;
    SZrFunction *siblingFunction = ZR_NULL;
    SZrFunction *runChild;
    TZrInt64 siblingResult = 0;

    TEST_START("Named Function Metadata Supports Finally Closures And Sibling Calls");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_ASSERT_TRUE(compile_source_to_function(state, closureSource, "named_finally_closure_metadata.zr", &closureFunction));
    runChild = find_child_function_by_name(closureFunction, "run");
    TEST_ASSERT_NOT_NULL(runChild);
    TEST_ASSERT_EQUAL_UINT32(1, runChild->closureValueLength);
    TEST_ASSERT_TRUE(function_contains_opcode(runChild, ZR_INSTRUCTION_ENUM(SETUPVAL)));
    TEST_ASSERT_TRUE(function_contains_opcode(runChild, ZR_INSTRUCTION_ENUM(END_FINALLY)));

    TEST_ASSERT_TRUE(compile_source_to_function(state, siblingSource, "sibling_function_visibility.zr", &siblingFunction));
    TEST_ASSERT_TRUE(execute_source_expect_int64(state, siblingSource, "sibling_function_visibility.zr", &siblingResult));
    TEST_ASSERT_EQUAL_INT64(1, siblingResult);

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Named Function Metadata Supports Finally Closures And Sibling Calls");
    TEST_DIVIDER();
}

static void test_caught_error_exposes_stack_frames_in_throw_order(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source =
            "outer() {\n"
            "    inner();\n"
            "}\n"
            "inner() {\n"
            "    throw \"boom\";\n"
            "}\n"
            "try {\n"
            "    outer();\n"
            "} catch (e) {\n"
            "    if (e.stacks[0].functionName == \"inner\" &&\n"
            "        e.stacks[1].functionName == \"outer\" &&\n"
            "        e.stacks[0].sourceFile == \"stack_frames_test.zr\") {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "return 0;\n";
    TZrInt64 result = 0;
    SZrState *state;

    TEST_START("Caught Error Exposes Stack Frames");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("stack frames",
              "Testing that caught exceptions expose StackFrame values in throw-first order.");

    TEST_ASSERT_TRUE(execute_source_expect_int64(state, source, "stack_frames_test.zr", &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Caught Error Exposes Stack Frames");
    TEST_DIVIDER();
}

static void test_test_declaration_no_longer_wraps_throw_into_zero_one_contract(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source = "%test(\"uncaught_throw\") { throw \"boom\"; }";
    SZrCompileResult compileResult;
    SZrAstNode *ast;
    SZrString *sourceName;
    SZrState *state;
    TZrInt64 result = 0;

    TEST_START("Test Declaration Uses Real Exception Semantics");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("test declaration exception flow",
              "Testing that %test bodies compile without synthetic try/catch wrapping and escape uncaught throws.");

    memset(&compileResult, 0, sizeof(compileResult));
    sourceName = ZrCore_String_Create(state,
                                      "test_declaration_uncaught_exception.zr",
                                      strlen("test_declaration_uncaught_exception.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
    ZrParser_Ast_Free(state, ast);

    TEST_ASSERT_EQUAL_UINT64(1, compileResult.testFunctionCount);
    TEST_ASSERT_NOT_NULL(compileResult.testFunctions);
    TEST_ASSERT_NOT_NULL(compileResult.testFunctions[0]);
    TEST_ASSERT_FALSE(function_contains_opcode(compileResult.testFunctions[0], ZR_INSTRUCTION_ENUM(TRY)));
    TEST_ASSERT_FALSE(function_contains_opcode(compileResult.testFunctions[0], ZR_INSTRUCTION_ENUM(CATCH)));
    TEST_ASSERT_TRUE(function_contains_opcode(compileResult.testFunctions[0], ZR_INSTRUCTION_ENUM(THROW)));

    TEST_ASSERT_FALSE(ZrTests_Function_ExecuteExpectInt64(state, compileResult.testFunctions[0], &result));

    ZrParser_CompileResult_Free(state, &compileResult);
    if (compileResult.mainFunction != ZR_NULL) {
        ZrCore_Function_Free(state, compileResult.mainFunction);
    }
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Test Declaration Uses Real Exception Semantics");
    TEST_DIVIDER();
}

int main(void) {
    UNITY_BEGIN();

    TEST_MODULE_DIVIDER();
    printf("Exception Handling Tests\n");
    TEST_MODULE_DIVIDER();

    RUN_TEST(test_throw_string_is_boxed_and_caught_by_base_error);
    RUN_TEST(test_derived_exception_prefers_first_matching_catch_clause);
    RUN_TEST(test_finally_runs_for_normal_return_and_throw_paths);
    RUN_TEST(test_named_function_finally_closure_and_sibling_function_metadata);
    RUN_TEST(test_caught_error_exposes_stack_frames_in_throw_order);
    RUN_TEST(test_test_declaration_no_longer_wraps_throw_into_zero_one_contract);

    return UNITY_END();
}
