//
// The root tests/CMakeLists.txt runs these compiler/runtime exception contracts in one Unity binary.
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

/* Active logging macros supplement Unity assertions; the unused TEST_FAIL_CUSTOM can set Unity's failure flag. */
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
        Unity.CurrentTestFailed = 1;                                                                                   \
        UNITY_OUTPUT_FLUSH();                                                                                          \
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

/** @brief Keep expected uncaught-exception tests inside the harness failure path instead of aborting the process. */
static void test_panic_handler(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
}

/** @brief Give each test a fresh parser and zr.system.exception registry for compile and runtime assertions.
 * @return A state owned by the test; the caller must pass it to destroy_test_state.
 */
static SZrState *create_test_state(void) {
    SZrState *state = ZrTests_State_Create(test_panic_handler);
    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        ZrVmLibSystem_Register(state->global);
    }
    return state;
}

/** @brief Release the per-test global, including GC-owned compiled functions and registered provider state. */
static void destroy_test_state(SZrState *state) {
    ZrTests_State_Destroy(state);
}

/** @brief Inspect compiler output when runtime behavior alone cannot prove the emitted exception boundary.
 * @note Used only by the metadata and ordinary-function tests; it does not prove control-flow execution.
 */
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

/** @brief Select the nested function whose closure or exception opcodes the tests need to inspect.
 * @return A borrowed child inside function; it becomes invalid when the parent function is freed.
 */
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

/** @brief Compile a named source fixture into the function inspected or executed by an exception test.
 * @note On success *function belongs to the caller until Function_Free or global teardown.
 */
static TZrBool compile_source_to_function(SZrState *state,
                                          const TZrChar *source,
                                          const TZrChar *sourceNameLiteral,
                                          SZrFunction **function) {
    SZrString *sourceName;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameLiteral == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameLiteral, strlen(sourceNameLiteral));
    if (sourceName == ZR_NULL) {
        return ZR_FALSE;
    }

    *function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    return *function != ZR_NULL;
}

/** @brief Exercise parser, compiler and VM together, then release the temporary compiled function.
 * @return False for compile, execution or integer-result failure; the caller owns state and result.
 */
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

/* Unity invokes tearDown even after an assertion aborts the current test body.
 * BUG: tearDown owns no state, so an assertion after create_test_state skips the
 * test body's destroy_test_state and leaks that VM global until process exit. */
void setUp(void) {}

void tearDown(void) {}

/** @brief Verify scalar throws are boxed into Error-compatible catch values. */
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

/** @brief Check ordered typed-catch dispatch against the registered zr.system.exception prototypes. */
static void test_derived_exception_prefers_first_matching_catch_clause(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source =
            "let exception = import(\"zr.system.exception\");\n"
            "try {\n"
            "    throw new exception.RuntimeError(\"boom\");\n"
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

/** @brief Check that qualified catch syntax records the member type, not the imported module name.
 * @note This test inspects compiler metadata only; it does not execute the qualified handler.
 */
static void test_qualified_exception_catch_uses_member_type_name(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source =
            "let exception = import(\"zr.system.exception\");\n"
            "try {\n"
            "    throw \"boom\";\n"
            "} catch (e: exception.RuntimeError) {\n"
            "    return 1;\n"
            "} catch (e: Error) {\n"
            "    return 2;\n"
            "}\n"
            "return 0;\n";
    SZrFunction *function = ZR_NULL;
    SZrState *state;
    TZrNativeString typeName;

    TEST_START("Qualified Exception Catch Uses Member Type Name");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("qualified typed catch",
              "Testing that catch (e: exception.RuntimeError) matches the RuntimeError prototype, not the module.");

    TEST_ASSERT_TRUE(compile_source_to_function(state, source, "exception_qualified_catch_test.zr", &function));
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(2, function->catchClauseCount);
    TEST_ASSERT_NOT_NULL(function->catchClauseList);
    TEST_ASSERT_NOT_NULL(function->catchClauseList[0].typeName);
    typeName = ZrCore_String_GetNativeString(function->catchClauseList[0].typeName);
    TEST_ASSERT_NOT_NULL(typeName);
    TEST_ASSERT_EQUAL_STRING("RuntimeError", typeName);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Qualified Exception Catch Uses Member Type Name");
    TEST_DIVIDER();
}

/** @brief Run independent fixtures for normal completion, return and throw unwinding through finally. */
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
            "fn run(): int {\n"
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

/** @brief Inspect nested finally capture metadata and run a separate sibling-call visibility fixture.
 * TODO: The closure fixture is compiled but never run, so its captured marker update is unverified.
 */
static void test_named_function_finally_closure_and_sibling_function_metadata(void) {
    SZrTestTimer timer = {0};
    const TZrChar *closureSource =
            "var marker = 0;\n"
            "fn run(): int {\n"
            "    try {\n"
            "        return 7;\n"
            "    } finally {\n"
            "        marker = 9;\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    const TZrChar *siblingSource =
            "fn outer(): int {\n"
            "    return inner();\n"
            "}\n"
            "fn inner(): int {\n"
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

/** @brief Ensure return through catch/finally leaves no handler on the caller's following loop. */
static void test_return_from_catch_discards_frame_exception_handlers(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source =
            "fn guarded(flag: int): int {\n"
            "    var marker = 0;\n"
            "    try {\n"
            "        try {\n"
            "            if (flag != 0) {\n"
            "                throw \"boom\";\n"
            "            }\n"
            "            return 0;\n"
            "        } finally {\n"
            "            marker = marker + 7;\n"
            "        }\n"
            "    } catch (e) {\n"
            "        return marker + 1;\n"
            "    }\n"
            "}\n"
            "var value = guarded(1);\n"
            "var after = 0;\n"
            "while (after < 3) {\n"
            "    after = after + 1;\n"
            "}\n"
            "return value + after;\n";
    TZrInt64 result = 0;
    SZrState *state;

    TEST_START("Return From Catch Discards Frame Exception Handlers");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("catch return handler cleanup",
              "Testing that returning from a catch after an inner finally leaves no stale handlers on the caller path.");

    TEST_ASSERT_TRUE(execute_source_expect_int64(state, source, "catch_return_handler_cleanup.zr", &result));
    TEST_ASSERT_EQUAL_INT64(11, result);

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Return From Catch Discards Frame Exception Handlers");
    TEST_DIVIDER();
}

/** @brief Check the caught Error exposes the throwing frame before its caller and keeps source identity. */
static void test_caught_error_exposes_stack_frames_in_throw_order(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source =
            "fn outer(): void {\n"
            "    inner();\n"
            "}\n"
            "fn inner(): void {\n"
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

/** @brief Keep ordinary functions outside test-only exception wrapping, both in bytecode and execution.
 * TODO: Execution asserts only failure; capture and assert the escaping error value to prove its identity.
 */
static void test_ordinary_function_throw_is_not_wrapped(void) {
    SZrTestTimer timer = {0};
    const TZrChar *source =
            "fn uncaughtThrow(): int { throw \"boom\"; }\n"
            "return uncaughtThrow();\n";
    SZrFunction *function;
    SZrFunction *throwingFunction;
    SZrState *state;
    TZrInt64 result = 0;

    TEST_START("Test Declaration Uses Real Exception Semantics");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("ordinary function exception flow",
              "Testing that ordinary functions use real exception flow without a synthetic test wrapper.");

    TEST_ASSERT_TRUE(compile_source_to_function(
            state, source, "ordinary_function_uncaught_exception.zr", &function));
    throwingFunction = find_child_function_by_name(function, "uncaughtThrow");
    TEST_ASSERT_NOT_NULL(throwingFunction);
    TEST_ASSERT_FALSE(function_contains_opcode(throwingFunction, ZR_INSTRUCTION_ENUM(TRY)));
    TEST_ASSERT_FALSE(function_contains_opcode(throwingFunction, ZR_INSTRUCTION_ENUM(CATCH)));
    TEST_ASSERT_TRUE(function_contains_opcode(throwingFunction, ZR_INSTRUCTION_ENUM(THROW)));

    TEST_ASSERT_FALSE(ZrTests_Function_ExecuteExpectInt64(state, function, &result));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Test Declaration Uses Real Exception Semantics");
    TEST_DIVIDER();
}

/** @brief Run all eight exception contracts through Unity; the root CMake target provides the harness. */
int main(void) {
    UNITY_BEGIN();

    TEST_MODULE_DIVIDER();
    printf("Exception Handling Tests\n");
    TEST_MODULE_DIVIDER();

    RUN_TEST(test_throw_string_is_boxed_and_caught_by_base_error);
    RUN_TEST(test_derived_exception_prefers_first_matching_catch_clause);
    RUN_TEST(test_qualified_exception_catch_uses_member_type_name);
    RUN_TEST(test_finally_runs_for_normal_return_and_throw_paths);
    RUN_TEST(test_named_function_finally_closure_and_sibling_function_metadata);
    RUN_TEST(test_return_from_catch_discards_frame_exception_handlers);
    RUN_TEST(test_caught_error_exposes_stack_frames_in_throw_order);
    RUN_TEST(test_ordinary_function_throw_is_not_wrapped);

    return UNITY_END();
}
