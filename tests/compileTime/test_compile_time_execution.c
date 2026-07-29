//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "unity.h"
#include "runtime_support.h"
#include "module_fixture_support.h"
#include "test_support.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compile_tool.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.h"

#define TEST_START(summary) ZR_TEST_START(summary)
#define TEST_INFO(summary, details) ZR_TEST_INFO(summary, details)
#define TEST_PASS_CUSTOM(timer, summary) ZR_TEST_PASS(timer, summary)
#define TEST_FAIL_CUSTOM(timer, summary, reason) do { \
    (timer).endTime = clock(); \
    ZR_TEST_FAIL(timer, summary, reason); \
} while (0)
#define TEST_DIVIDER() ZR_TEST_DIVIDER()
#define TEST_MODULE_DIVIDER() ZR_TEST_MODULE_DIVIDER()

typedef struct STestCompileResult {
    SZrFunction *mainFunction;
    SZrFunction **testFunctions;
    TZrSize testFunctionCount;
} STestCompileResult;

static void test_compile_result_free(SZrState *state, STestCompileResult *result) {
    if (state != ZR_NULL && result != ZR_NULL &&
        result->testFunctions != ZR_NULL && result->testFunctionCount > 0) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                result->testFunctions,
                result->testFunctionCount * sizeof(SZrFunction *),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        result->testFunctions = ZR_NULL;
        result->testFunctionCount = 0;
    }
}

static SZrState* create_test_state(void) {
    return ZrTests_State_Create(ZR_NULL);
}

static void destroy_test_state(SZrState* state) {
    ZrTests_State_Destroy(state);
}

typedef ZrTestsFixtureSource SZrCompileTimeImportFixture;
typedef ZrTestsFixtureReader SZrCompileTimeImportReader;

#define compile_time_import_reader_read ZrTests_Fixture_ReaderRead
#define compile_time_import_reader_close ZrTests_Fixture_ReaderClose

static const SZrCompileTimeImportFixture* gCompileTimeImportFixtures = ZR_NULL;
static TZrSize gCompileTimeImportFixtureCount = 0;
static const TZrChar* gCompileTimeImportBinaryModuleName = ZR_NULL;
static const TZrChar* gCompileTimeImportBinaryPath = ZR_NULL;

static TZrByte* build_compile_time_import_binary_fixture(SZrState* state,
                                                         const TZrChar* moduleSource,
                                                         const TZrChar* binaryPath,
                                                         TZrSize* outLength) {
    return ZrTests_Fixture_BuildBinaryFile(state, moduleSource, binaryPath, ZR_TRUE, outLength);
}

static TZrBool compile_time_import_source_loader(SZrState* state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo* io) {
    return ZrTests_Fixture_SourceLoaderFromArray(state,
                                                 sourcePath,
                                                 md5,
                                                 io,
                                                 gCompileTimeImportFixtures,
                                                 gCompileTimeImportFixtureCount);
}

static TZrBool compile_time_import_binary_file_loader(SZrState* state,
                                                      const TZrChar* binaryPath,
                                                      SZrIo* io) {
    SZrLibrary_File_Reader* reader;

    if (state == ZR_NULL || binaryPath == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    reader = ZrLibrary_File_OpenRead(state->global, (TZrNativeString)binaryPath, ZR_TRUE);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io->isBinary = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool compile_time_import_mixed_source_loader(SZrState* state,
                                                       TZrNativeString sourcePath,
                                                       TZrNativeString md5,
                                                       SZrIo* io) {
    ZR_UNUSED_PARAMETER(md5);

    if (gCompileTimeImportBinaryModuleName != ZR_NULL &&
        gCompileTimeImportBinaryPath != ZR_NULL &&
        sourcePath != ZR_NULL &&
        strcmp(sourcePath, gCompileTimeImportBinaryModuleName) == 0) {
        return compile_time_import_binary_file_loader(state, gCompileTimeImportBinaryPath, io);
    }

    return compile_time_import_source_loader(state, sourcePath, md5, io);
}

static TZrBool execute_test_function(SZrState* state, SZrFunction* testFunc, TZrInt64 expectedValue, const TZrChar* testName) {
    TZrInt64 actualValue = 0;

    ZR_UNUSED_PARAMETER(testName);

    if (!ZrTests_Function_ExecuteExpectInt64(state, testFunc, &actualValue)) {
        return ZR_FALSE;
    }

    if (expectedValue >= 0) {
        TEST_ASSERT_EQUAL_INT64(expectedValue, actualValue);
    }

    return ZR_TRUE;
}

static SZrFunction* find_fixture_function(SZrFunction* mainFunction) {
    TZrUInt32 index;

    if (mainFunction == ZR_NULL || mainFunction->childFunctionList == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < mainFunction->childFunctionLength; index++) {
        SZrFunction* child = &mainFunction->childFunctionList[index];
        TZrNativeString name;

        if (child->functionName == ZR_NULL) {
            continue;
        }

        name = ZrCore_String_GetNativeStringShort(child->functionName);
        if (name != ZR_NULL && strcmp(name, "__fixture") == 0) {
            return child;
        }
    }

    return ZR_NULL;
}

static TZrBool compile_fixture_with_entry(SZrState* state, SZrAstNode* ast, STestCompileResult* result) {
    SZrFunction* fixtureFunction;

    if (state == ZR_NULL || ast == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(result, 0, sizeof(*result));
    result->mainFunction = ZrParser_Compiler_Compile(state, ast);
    if (result->mainFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    fixtureFunction = find_fixture_function(result->mainFunction);
    if (fixtureFunction == ZR_NULL) {
        ZrCore_Function_Free(state, result->mainFunction);
        result->mainFunction = ZR_NULL;
        return ZR_FALSE;
    }

    result->testFunctions = (SZrFunction**)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction*),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (result->testFunctions == ZR_NULL) {
        ZrCore_Function_Free(state, result->mainFunction);
        result->mainFunction = ZR_NULL;
        return ZR_FALSE;
    }

    result->testFunctions[0] = fixtureFunction;
    result->testFunctionCount = 1;
    return ZR_TRUE;
}


static void reset_loaded_module_registry(SZrState* state) {
    SZrObject* registry;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    registry = ZrCore_Object_New(state, ZR_NULL);
    if (registry == ZR_NULL) {
        return;
    }

    ZrCore_Object_Init(state, registry);
    ZrCore_Value_InitAsRawObject(state, &state->global->loadedModulesRegistry, ZR_CAST_RAW_OBJECT_AS_SUPER(registry));
    state->global->loadedModulesRegistry.type = ZR_VALUE_TYPE_OBJECT;
}

// 测试初始化和清理
void setUp(void) {
}

void tearDown(void) {
}

static void assert_compile_time_compile_failure(SZrState* state, const TZrChar* source, const TZrChar* sourceNameText) {
    SZrString* sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    STestCompileResult compileResult;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(compile_fixture_with_entry(state, ast, &compileResult));

    ZrParser_Ast_Free(state, ast);
}

// ==================== 编译期执行测试 ====================

// 测试1: 编译期变量声明和使用
static void test_compile_time_variables(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Variables";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time variable declaration and usage", 
              "Testing comptime var MAX_SIZE = 100");
    
    const TZrChar* source = 
        "module test;\n"
        "comptime var MAX_SIZE = 100;\n"
        "comptime var MIN_SIZE = 1;\n"
        "comptime var DEFAULT_VALUE = 42;\n"
        "var runtimeVar = DEFAULT_VALUE;\n"
        "fn __fixture(): int {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_vars.zr", 26);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 42, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }
        }
    }
    
    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试2: 编译期函数声明和调用
static void test_compile_time_functions(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Functions";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time function declaration and call", 
              "Testing comptime function calculateSum");
    
    const TZrChar* source = 
        "module test;\n"
        "comptime fn calculateSum(a: int, b: int): int {\n"
        "    return a + b;\n"
        "}\n"
        "comptime var computedValue = calculateSum(10, 20);\n"
        "var runtimeVar = computedValue;\n"
        "fn __fixture(): int {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_funcs.zr", 27);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 30, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }
        }
    }
    
    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试3: 编译期表达式计算
static void test_compile_time_expressions(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Expressions";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time expression evaluation", 
              "Testing comptime var complexExpr = (1+2)*3*4 - 10");
    
    const TZrChar* source = 
        "module test;\n"
        "comptime fn calculateSum(a: int, b: int): int {\n"
        "    return a + b;\n"
        "}\n"
        "comptime fn multiply(x: int, y: int): int {\n"
        "    return x * y;\n"
        "}\n"
        "comptime var complexExpr = (calculateSum(1, 2) * multiply(3, 4)) - 10;\n"
        "var runtimeVar = complexExpr;\n"
        "fn __fixture(): int {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_expr.zr", 26);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 26, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }   
        }
    }
    
    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试4: 编译期递归调用
static void test_compile_time_recursion(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Recursion";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time recursive function call", 
              "Testing comptime function factorial");
    
    const TZrChar* source = 
        "module test;\n"
        "comptime fn factorial(n: int): int {\n"
        "    if (n <= 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    return n * factorial(n - 1);\n"
        "}\n"
        "comptime var fact5 = factorial(5);\n"
        "var runtimeVar = fact5;\n"
        "fn __fixture(): int {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_recursion.zr", 32);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 120, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }
        }
    }
    
    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试5: 编译期语句块
static void test_compile_time_statements(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Statement Blocks";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time statement block execution", 
              "Testing comptime { ... }");
    
    const TZrChar* source = 
        "module test;\n"
        "comptime var MAX_SIZE = 100;\n"
        "comptime var MIN_SIZE = 1;\n"
        "comptime {\n"
        "    if (MAX_SIZE < MIN_SIZE) {\n"
        "        FatalError(\"MAX_SIZE must be greater than MIN_SIZE\");\n"
        "    }\n"
        "}\n"
        "var runtimeVar = 42;\n"
        "fn __fixture(): int {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_stmts.zr", 28);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 42, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }
        }
    }
    
    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试6: 编译期数组大小验证
static void test_compile_time_array_validation(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Array Size Validation";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time array size validation", 
              "Testing array size validation using compile-time function");
    
    const TZrChar* source = 
        "module test;\n"
        "comptime var MAX_SIZE = 100;\n"
        "comptime var MIN_SIZE = 1;\n"
        "comptime fn validateArraySize(size: int): bool {\n"
        "    return size >= MIN_SIZE && size <= MAX_SIZE;\n"
        "}\n"
        "var validatedArray: int[validateArraySize(50) ? 50 : 10];\n"
        "fn __fixture(): int {\n"
        "    return validatedArray.length;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_array.zr", 28);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 50, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }
        }
    }
    
    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试7: 编译期函数结果投影到后续运行时代码编译
static void test_compile_time_function_projection_to_runtime(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Function Projection";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time function result projection",
              "Testing runtime initializer uses comptime function call directly");

    const TZrChar* source =
        "module test;\n"
        "comptime fn calculateSum(a: int, b: int): int {\n"
        "    return a + b;\n"
        "}\n"
        "var runtimeVar = calculateSum(10, 20);\n"
        "fn __fixture(): int {\n"
        "    return runtimeVar;\n"
        "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_projection.zr", 31);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }

    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }

    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 30, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }
        }
    }

    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试8: comptime block 内声明持久注册并投影到运行时代码
static void test_compile_time_block_persistent_registration(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Block Persistent Registration";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time block persistent declarations",
              "Testing var/function declared inside comptime block remain available afterwards");

    const TZrChar* source =
        "module test;\n"
        "comptime {\n"
        "    var BLOCK_VALUE = 40;\n"
        "    fn addOffset(base: int): int {\n"
        "        return base + BLOCK_VALUE + 2;\n"
        "    }\n"
        "}\n"
        "var runtimeValue = addOffset(0);\n"
        "fn __fixture(): int {\n"
        "    return runtimeValue;\n"
        "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_block_registration.zr", 40);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }

    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }

    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 42, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }
        }
    }

    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试9: 编译期函数命名参数和默认参数投影
static void test_compile_time_named_and_default_argument_projection(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Named Default Projection";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time function named/default args projection",
              "Testing runtime initializer uses named args and default args from comptime function");

    const TZrChar* source =
        "module test;\n"
        "comptime fn combine(a: int, b: int = 10, c: int = 100): int {\n"
        "    return a + b + c;\n"
        "}\n"
        "var runtimeNamed = combine(c: 3, a: 1);\n"
        "var runtimeDefault = combine(a: 4, c: 6);\n"
        "var runtimeAllDefaults = combine(a: 4);\n"
        "fn __fixture(): int {\n"
        "    return runtimeNamed + runtimeDefault + runtimeAllDefaults;\n"
        "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_named_default_projection.zr", 47);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }

    STestCompileResult compileResult;
    if (!compile_fixture_with_entry(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }

    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 148, testSummary)) {
                timer.endTime = clock();
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
                TEST_FAIL_MESSAGE("Execute test function failed");
            }
        }
    }

    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试10: comptime block 内前向依赖应给出诊断并阻止编译
static void test_compile_time_block_forward_reference_diagnostic(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Block Forward Reference";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time block forward reference diagnostic",
              "Testing comptime block rejects references to declarations defined later in the same block");

    assert_compile_time_compile_failure(
            state,
            "module test;\n"
            "comptime {\n"
            "    var computed = laterValue + 1;\n"
            "    var laterValue = 41;\n"
            "}\n"
            "var runtimeValue = 0;\n"
            "fn __fixture(): int {\n"
            "    return runtimeValue;\n"
            "}\n",
            "test_compile_time_forward_reference.zr");

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试11: 重复声明采用最后一次覆盖策略
static void test_compile_time_duplicate_declaration_override(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Duplicate Override";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time duplicate declaration override",
              "Testing later comptime var/function declarations override earlier ones");

    const TZrChar* source =
            "module test;\n"
            "comptime {\n"
            "    var VALUE = 1;\n"
            "    var VALUE = 2;\n"
            "    fn pick(): int {\n"
            "        return VALUE;\n"
            "    }\n"
            "    fn pick(): int {\n"
            "        return VALUE + 40;\n"
            "    }\n"
            "}\n"
            "var runtimeValue = pick();\n"
            "fn __fixture(): int {\n"
            "    return runtimeValue;\n"
            "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_duplicate_override.zr", 39);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);

    STestCompileResult compileResult;
    TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试12: 编译期对象成员调用投影
static void test_compile_time_member_call_projection(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Member Call Projection";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time object member call projection",
              "Testing compile-time object members can reference compile-time functions and project member calls");

    const TZrChar* source =
            "module test;\n"
            "comptime fn addImpl(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "comptime var helper = { add: addImpl };\n"
            "var runtimeValue = helper.add(19, 23);\n"
            "fn __fixture(): int {\n"
            "    return runtimeValue;\n"
            "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_member_call_projection.zr", 43);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);

    STestCompileResult compileResult;
    TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试13: import + member-call 的编译期投影
static void test_compile_time_import_member_call_projection(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "helper",
                    "module helper;\n"
                    "pub var greet = fn(): int {\n"
                    "    return 42;\n"
                    "};\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };

    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Import Member Call Projection";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    gCompileTimeImportFixtures = fixtures;
    gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
    state->global->sourceLoader = compile_time_import_source_loader;

    TEST_INFO("Compile-time import member call projection",
              "Testing import(\"helper\").greet() is projected during compilation");

    const TZrChar* source =
            "module test;\n"
            "let runtimeValue = import(\"helper\").greet();\n"
            "fn __fixture(): int {\n"
            "    return runtimeValue;\n"
            "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_import_member_call_projection.zr", 50);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);

    STestCompileResult compileResult;
    TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试14: 编译期对象中包含 compile-time function ref 时禁止投影到 runtime
static void test_compile_time_projection_rejects_function_ref_leak(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Reject Function Ref Leak";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time object projection leak guard",
              "Testing runtime projection fails when compile-time object contains compile-time-only function refs");

    assert_compile_time_compile_failure(
            state,
            "module test;\n"
            "comptime fn addImpl(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "comptime fn buildHelper() {\n"
            "    return { add: addImpl };\n"
            "}\n"
            "var leaked = buildHelper();\n"
            "fn __fixture(): int {\n"
            "    return 1;\n"
            "}\n",
            "test_compile_time_function_ref_leak.zr");

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试15: 更深层 import/member-call 组合的编译期投影
static void test_compile_time_import_deep_member_call_projection(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "helper",
                    "module helper;\n"
                    "pub var toolkit = {\n"
                    "    math: {\n"
                    "        greet: fn(): int {\n"
                    "            return 42;\n"
                    "        }\n"
                    "    }\n"
                    "};\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };

    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Deep Import Member Call Projection";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    gCompileTimeImportFixtures = fixtures;
    gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
    state->global->sourceLoader = compile_time_import_source_loader;

    TEST_INFO("Compile-time deep import member-call projection",
              "Testing import(\"helper\").toolkit.math.greet() is fully projected during compilation");

    {
        const TZrChar* source =
                "module test;\n"
                "let runtimeValue = import(\"helper\").toolkit.math.greet();\n"
                "fn __fixture(): int {\n"
                "    return runtimeValue;\n"
                "}\n";

        SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_import_deep_member_call_projection.zr", 55);
        SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);

        reset_loaded_module_registry(state);
        state->global->sourceLoader = ZR_NULL;
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_compile_time_import_runtime_callable_named_default_projection(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "helper",
                    "module helper;\n"
                    "pub fn compute(seed: int, bonus: int = 5, factor: int = 2): int {\n"
                    "    return seed * factor + bonus;\n"
                    "}\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };

    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Import Runtime Callable Named Default Projection";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    ZrParser_ToGlobalState_Register(state);
    gCompileTimeImportFixtures = fixtures;
    gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
    state->global->sourceLoader = compile_time_import_source_loader;

    TEST_INFO("Compile-time import runtime callable named/default projection",
              "Testing import(\"helper\").compute(seed: 10, factor: 3) uses named/default args during compile-time projection");

    const TZrChar* source =
            "module test;\n"
            "let runtimeValue = import(\"helper\").compute(seed: 10, factor: 3) + import(\"helper\").compute(10, bonus: 7);\n"
            "fn __fixture(): int {\n"
            "    return runtimeValue;\n"
            "}\n";

    SZrString* sourceName =
            ZrCore_String_Create(state, "test_compile_time_import_runtime_callable_named_default_projection.zr", 69);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);

    STestCompileResult compileResult;
    TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

    test_compile_result_free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_compile_time_fixed_array_bound_import_named_default_projection(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "helper",
                    "module helper;\n"
                    "pub var sizing = {\n"
                    "    plan: fn(seed: int, factor: int = 2, bonus: int = 2): int {\n"
                    "        return seed * factor + bonus;\n"
                    "    }\n"
                    "};\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };

    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Fixed Array Bound Import Named Default Projection";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "var staged: int[import(\"helper\").sizing.plan(seed: 4, factor: 2)] = [1,2,3,4,5,6,7,8,9,10];\n"
                "fn __fixture(): int {\n"
                "    return staged.length * 100 + staged[0] + staged[9];\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(
                state,
                "test_compile_time_fixed_array_bound_import_named_default_projection.zr",
                strlen("test_compile_time_fixed_array_bound_import_named_default_projection.zr"));
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);

        reset_loaded_module_registry(state);
        state->global->sourceLoader = ZR_NULL;
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 1011, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_fixed_array_bound_import_mismatch_fails(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "helper",
                    "module helper;\n"
                    "pub fn chooseSize(seed: int, extra: int = 2): int {\n"
                    "    return seed + extra;\n"
                    "}\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };

    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Fixed Array Bound Import Mismatch Fails";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        assert_compile_time_compile_failure(
                state,
                "module test;\n"
                "var staged: int[import(\"helper\").chooseSize(seed: 2, extra: 2)] = [1,2,3];\n"
                "fn __fixture(): int {\n"
                "    return staged.length;\n"
                "}\n",
                "test_compile_time_fixed_array_bound_import_mismatch.zr");

        state->global->sourceLoader = ZR_NULL;
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_binary_import_function_alias_projection(void) {
    static const TZrChar* providerSource =
            "module provider;\n"
            "comptime var SCALE = 8;\n"
            "comptime fn buildBias(seed: int): int {\n"
            "    return seed + SCALE;\n"
            "}\n";

    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Binary Import Function Alias Projection";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    const TZrChar* binaryPath = "test_compile_time_import_provider_binary.zro";
    TZrByte* binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrCompileTimeImportFixture fixtures[1];
        SZrState* state = create_test_state();
        const TZrChar* source =
                "module test;\n"
                "let provider = import(\"provider\");\n"
                "var runtimeValue = buildBias(34);\n"
                "fn __fixture(): int {\n"
                "    return runtimeValue;\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        binaryBytes = build_compile_time_import_binary_fixture(state, providerSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);
        {
            SZrCompileTimeImportReader* binaryReader =
                    (SZrCompileTimeImportReader*)malloc(sizeof(SZrCompileTimeImportReader));
            SZrIo binaryIo;
            SZrIoSource* binarySource;

            TEST_ASSERT_NOT_NULL(binaryReader);
            binaryReader->bytes = binaryBytes;
            binaryReader->length = binaryLength;
            binaryReader->consumed = ZR_FALSE;
            ZrCore_Io_Init(state,
                           &binaryIo,
                           compile_time_import_reader_read,
                           compile_time_import_reader_close,
                           binaryReader);
            binaryIo.isBinary = ZR_TRUE;
            binarySource = ZrCore_Io_ReadSourceNew(&binaryIo);
            if (binaryIo.close != ZR_NULL) {
                binaryIo.close(state, binaryIo.customData);
            }

            TEST_ASSERT_NOT_NULL(binarySource);
            TEST_ASSERT_TRUE(binarySource->modulesLength > 0);
            TEST_ASSERT_NOT_NULL(binarySource->modules);
            TEST_ASSERT_NOT_NULL(binarySource->modules[0].entryFunction);
            TEST_ASSERT_TRUE(binarySource->modules[0].entryFunction->compileTimeFunctionInfosLength > 0);
        }

        fixtures[0].path = "provider";
        fixtures[0].source = ZR_NULL;
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;

        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = 1;
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_binary_import_function_alias_projection.zr",
                                          61);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        TEST_ASSERT_NOT_NULL(compileResult.mainFunction);
        TEST_ASSERT_TRUE(compileResult.mainFunction->compileTimeFunctionInfoLength > 0);
        {
            TZrBool foundBuildBias = ZR_FALSE;

            for (TZrUInt32 infoIndex = 0; infoIndex < compileResult.mainFunction->compileTimeFunctionInfoLength;
                 infoIndex++) {
                SZrFunctionCompileTimeFunctionInfo* info =
                        &compileResult.mainFunction->compileTimeFunctionInfos[infoIndex];

                if (info->name == ZR_NULL || strcmp(ZrCore_String_GetNativeString(info->name), "buildBias") != 0) {
                    continue;
                }

                foundBuildBias = ZR_TRUE;
                TEST_ASSERT_EQUAL_UINT32(1u, info->parameterCount);
                TEST_ASSERT_NOT_NULL(info->parameters);
                TEST_ASSERT_NOT_NULL(info->parameters[0].name);
                TEST_ASSERT_EQUAL_STRING("seed", ZrCore_String_GetNativeString(info->parameters[0].name));
                break;
            }

            TEST_ASSERT_TRUE(foundBuildBias);
        }
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
    if (binaryBytes != ZR_NULL) {
        free(binaryBytes);
    }
    remove(binaryPath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_binary_import_named_and_default_argument_projection(void) {
    static const TZrChar* providerSource =
            "module provider;\n"
            "comptime var BASE = 5;\n"
            "comptime fn compute(seed: int, bonus: int = BASE, factor: int = 2): int {\n"
            "    return seed * factor + bonus;\n"
            "}\n";

    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Binary Import Named Default Projection";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    const TZrChar* binaryPath = "test_compile_time_import_provider_named_default_binary.zro";
    TZrByte* binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrCompileTimeImportFixture fixtures[1];
        SZrState* state = create_test_state();
        const TZrChar* source =
                "module test;\n"
                "let provider = import(\"provider\");\n"
                "var runtimeValue = compute(seed: 10, factor: 3) + compute(10, bonus: 7);\n"
                "fn __fixture(): int {\n"
                "    return runtimeValue;\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        binaryBytes = build_compile_time_import_binary_fixture(state, providerSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "provider";
        fixtures[0].source = ZR_NULL;
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;

        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = 1;
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_binary_import_named_default_argument_projection.zr",
                                          67);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
    if (binaryBytes != ZR_NULL) {
        free(binaryBytes);
    }
    remove(binaryPath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_binary_import_runtime_callable_named_default_projection(void) {
    static const TZrChar* providerSource =
            "module provider;\n"
            "pub fn compute(seed: int, bonus: int = 5, factor: int = 2): int {\n"
            "    return seed * factor + bonus;\n"
            "}\n";

    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Binary Import Runtime Callable Named Default Projection";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    const TZrChar* binaryPath = "test_compile_time_import_runtime_callable_named_default_binary.zro";
    TZrByte* binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrCompileTimeImportFixture fixtures[1];
        SZrState* state = create_test_state();
        const TZrChar* source =
                "module test;\n"
                "let provider = import(\"provider\");\n"
                "var runtimeValue = provider.compute(seed: 10, factor: 3) + provider.compute(10, bonus: 7);\n"
                "fn __fixture(): int {\n"
                "    return runtimeValue;\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        binaryBytes = build_compile_time_import_binary_fixture(state, providerSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "provider";
        fixtures[0].source = ZR_NULL;
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;

        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = 1;
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_binary_import_runtime_callable_named_default_projection.zr",
                                          76);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
    if (binaryBytes != ZR_NULL) {
        free(binaryBytes);
    }
    remove(binaryPath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_binary_import_named_default_arguments_inside_function_decorator(void) {
    static const TZrChar* providerSource =
            "module provider;\n"
            "comptime var BASE = 5;\n"
            "comptime fn compute(seed: int, bonus: int = BASE, factor: int = 2): int {\n"
            "    return seed * factor + bonus;\n"
            "}\n";

    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Binary Import Named Default Arguments Inside Function Decorator";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    const TZrChar* binaryPath = "test_compile_time_import_provider_decorator_binary.zro";
    TZrByte* binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrCompileTimeImportFixture fixtures[1];
        SZrState* state = create_test_state();
        const TZrChar* source =
                "module test;\n"
                "let provider = import(\"provider\");\n"
                "comptime fn markFunction(target: typeof Function, bonus: int = 0) {\n"
                "    return { metadata: { instrumented: bonus } };\n"
                "}\n"
                "#markFunction(bonus: compute(seed: 10, factor: 3))#\n"
                "pub fn decoratedBonusDefault(): int {\n"
                "    var meta = typeof(decoratedBonusDefault).metadata;\n"
                "    return meta.instrumented;\n"
                "}\n"
                "#markFunction(bonus: compute(10, bonus: 7))#\n"
                "pub fn decoratedBonusNamed(): int {\n"
                "    var meta = typeof(decoratedBonusNamed).metadata;\n"
                "    return meta.instrumented;\n"
                "}\n"
                "fn __fixture(): int {\n"
                "    return decoratedBonusDefault() + decoratedBonusNamed();\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        binaryBytes = build_compile_time_import_binary_fixture(state, providerSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "provider";
        fixtures[0].source = ZR_NULL;
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;

        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = 1;
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_binary_import_function_decorator_projection.zr",
                                          65);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
    if (binaryBytes != ZR_NULL) {
        free(binaryBytes);
    }
    remove(binaryPath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_binary_import_named_default_arguments_inside_imported_module_decorator(void) {
    static const TZrChar* providerSource =
            "module provider;\n"
            "comptime var BASE = 5;\n"
            "comptime fn compute(seed: int, bonus: int = BASE, factor: int = 2): int {\n"
            "    return seed * factor + bonus;\n"
            "}\n";
    static const TZrChar* decoratedUserSource =
            "module decorated_user;\n"
            "let provider = import(\"provider\");\n"
            "comptime fn markFunction(target: typeof Function, bonus: int = 0) {\n"
            "    return { metadata: { instrumented: bonus } };\n"
            "}\n"
            "#markFunction(bonus: compute(seed: 10, factor: 3))#\n"
            "pub fn decoratedBonusDefault(): int {\n"
            "    var meta = typeof(decoratedBonusDefault).metadata;\n"
            "    return meta.instrumented;\n"
            "}\n"
            "#markFunction(bonus: compute(10, bonus: 7))#\n"
            "pub fn decoratedBonusNamed(): int {\n"
            "    var meta = typeof(decoratedBonusNamed).metadata;\n"
            "    return meta.instrumented;\n"
            "}\n";

    SZrTestTimer timer;
    const TZrChar* testSummary =
            "Compile-Time Execution - Binary Import Named Default Arguments Inside Imported Module Decorator";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    const TZrChar* binaryPath = "test_compile_time_import_provider_imported_decorator_binary.zro";
    TZrByte* binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrCompileTimeImportFixture fixtures[2];
        SZrState* state = create_test_state();
        const TZrChar* source =
                "module main;\n"
                "let decorated = import(\"decorated_user\");\n"
                "fn __fixture(): int {\n"
                "    return decorated.decoratedBonusDefault() + decorated.decoratedBonusNamed();\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        binaryBytes = build_compile_time_import_binary_fixture(state, providerSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "provider";
        fixtures[0].source = ZR_NULL;
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;
        fixtures[1].path = "decorated_user";
        fixtures[1].source = decoratedUserSource;
        fixtures[1].bytes = ZR_NULL;
        fixtures[1].length = 0;
        fixtures[1].isBinary = ZR_FALSE;

        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = 2;
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_binary_import_imported_module_function_decorator_projection.zr",
                                          81);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
    if (binaryBytes != ZR_NULL) {
        free(binaryBytes);
    }
    remove(binaryPath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_binary_import_named_default_arguments_inside_imported_module_decorator_via_file_loader_without_intermediate_sidecar(void) {
    static const TZrChar* providerSource =
            "module provider;\n"
            "comptime var BASE = 5;\n"
            "comptime fn compute(seed: int, bonus: int = BASE, factor: int = 2): int {\n"
            "    return seed * factor + bonus;\n"
            "}\n";
    static const TZrChar* decoratedUserSource =
            "module decorated_user;\n"
            "let provider = import(\"provider\");\n"
            "comptime fn markFunction(target: typeof Function, bonus: int = 0) {\n"
            "    return { metadata: { instrumented: bonus } };\n"
            "}\n"
            "#markFunction(bonus: compute(seed: 10, factor: 3))#\n"
            "pub fn decoratedBonusDefault(): int {\n"
            "    var meta = typeof(decoratedBonusDefault).metadata;\n"
            "    return meta.instrumented;\n"
            "}\n"
            "#markFunction(bonus: compute(10, bonus: 7))#\n"
            "pub fn decoratedBonusNamed(): int {\n"
            "    var meta = typeof(decoratedBonusNamed).metadata;\n"
            "    return meta.instrumented;\n"
            "}\n";

    SZrTestTimer timer;
    const TZrChar* testSummary =
            "Compile-Time Execution - Binary Import Imported Decorator Via File Loader Without Sidecar";
    const SZrCompileTimeImportFixture* previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    const TZrChar* previousBinaryModuleName = gCompileTimeImportBinaryModuleName;
    const TZrChar* previousBinaryPath = gCompileTimeImportBinaryPath;
    const TZrChar* binaryPath = "test_compile_time_import_provider_imported_decorator_file_loader_binary.zro";
    const TZrChar* intermediatePath = "test_compile_time_import_provider_imported_decorator_file_loader_binary.zri";
    TZrByte* binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrCompileTimeImportFixture fixtures[1];
        SZrState* state = create_test_state();
        const TZrChar* source =
                "module main;\n"
                "let decorated = import(\"decorated_user\");\n"
                "fn __fixture(): int {\n"
                "    return decorated.decoratedBonusDefault() + decorated.decoratedBonusNamed();\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        remove(binaryPath);
        remove(intermediatePath);
        binaryBytes = build_compile_time_import_binary_fixture(state, providerSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);
        remove(intermediatePath);

        fixtures[0].path = "decorated_user";
        fixtures[0].source = decoratedUserSource;
        fixtures[0].bytes = ZR_NULL;
        fixtures[0].length = 0;
        fixtures[0].isBinary = ZR_FALSE;

        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = 1;
        gCompileTimeImportBinaryModuleName = "provider";
        gCompileTimeImportBinaryPath = binaryPath;
        state->global->sourceLoader = compile_time_import_mixed_source_loader;

        sourceName = ZrCore_String_Create(
                state,
                "test_compile_time_binary_import_imported_module_function_decorator_file_loader_projection.zr",
                strlen("test_compile_time_binary_import_imported_module_function_decorator_file_loader_projection.zr"));
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
    gCompileTimeImportBinaryModuleName = previousBinaryModuleName;
    gCompileTimeImportBinaryPath = previousBinaryPath;
    if (binaryBytes != ZR_NULL) {
        free(binaryBytes);
    }
    remove(binaryPath);
    remove(intermediatePath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_imported_decorator_member_chain(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "decorators",
                    "module decorators;\n"
                    "comptime class Serializable {\n"
                    "    @decorate(target: typeof Class): zr.DecoratorPatch {\n"
                    "        return { metadata: { serializable: true } };\n"
                    "    }\n"
                    "}\n"
                    "comptime fn markFunction(target: typeof Function, bonus: int = 16) {\n"
                    "    return { metadata: { instrumented: bonus } };\n"
                    "}\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };

    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Imported Decorator Member Chain";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "let decorators = import(\"decorators\");\n"
                "#decorators.markFunction(bonus: 28)#\n"
                "pub fn decorated(): int {\n"
                "    var info = typeof(decorated);\n"
                "    return info.metadata.instrumented;\n"
                "}\n"
                "fn __fixture(): int {\n"
                "    return decorated();\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state, "test_compile_time_imported_decorator_member_chain.zr", 54);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 28, testSummary));

        state->global->sourceLoader = ZR_NULL;
        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_imported_decorator_deep_member_chain(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "decorators",
                    "module decorators;\n"
                    "comptime fn markFunction(target: typeof Function, bonus: int = 16) {\n"
                    "    return { metadata: { instrumented: bonus } };\n"
                    "}\n"
                    "comptime var registry = {\n"
                    "    nested: {\n"
                    "        mark: markFunction\n"
                    "    }\n"
                    "};\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };

    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Imported Decorator Deep Member Chain";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "let decorators = import(\"decorators\");\n"
                "#decorators.registry.nested.mark(bonus: 33)#\n"
                "pub fn decorated(): int {\n"
                "    var info = typeof(decorated);\n"
                "    return info.metadata.instrumented;\n"
                "}\n"
                "fn __fixture(): int {\n"
                "    return decorated();\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName =
                ZrCore_String_Create(state, "test_compile_time_imported_decorator_deep_member_chain.zr", 59);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 33, testSummary));

        state->global->sourceLoader = ZR_NULL;
        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_binary_imported_decorator_deep_member_chain(void) {
    static const TZrChar *decoratorSource =
            "module decorators;\n"
            "comptime fn markFunction(target: typeof Function, bonus: int = 16) {\n"
            "    return { metadata: { instrumented: bonus } };\n"
            "}\n"
            "comptime var registry = {\n"
            "    nested: {\n"
            "        mark: markFunction\n"
            "    }\n"
            "};\n";

    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Binary Imported Decorator Deep Member Chain";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    const TZrChar *binaryPath = "test_compile_time_imported_decorator_deep_member_chain.zro";
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrCompileTimeImportFixture fixtures[1];
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "let decorators = import(\"decorators\");\n"
                "#decorators.registry.nested.mark(bonus: 41)#\n"
                "pub fn decorated(): int {\n"
                "    var info = typeof(decorated);\n"
                "    return info.metadata.instrumented;\n"
                "}\n"
                "fn __fixture(): int {\n"
                "    return decorated();\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);

        binaryBytes = build_compile_time_import_binary_fixture(state, decoratorSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        fixtures[0].path = "decorators";
        fixtures[0].source = ZR_NULL;
        fixtures[0].bytes = binaryBytes;
        fixtures[0].length = binaryLength;
        fixtures[0].isBinary = ZR_TRUE;

        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = 1;
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_binary_imported_decorator_deep_member_chain.zr",
                                          66);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 41, testSummary));

        state->global->sourceLoader = ZR_NULL;
        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
    if (binaryBytes != ZR_NULL) {
        free(binaryBytes);
    }
    remove(binaryPath);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_object_decorator_member_chain(void) {
    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Object Decorator Member Chain";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "comptime fn markFunction(target: typeof Function, bonus: int = 11) {\n"
                "    return { metadata: { instrumented: bonus } };\n"
                "}\n"
                "comptime var decorators = { markFunction: markFunction };\n"
                "#decorators.markFunction(bonus: 17)#\n"
                "pub fn decorated(): int {\n"
                "    var info = typeof(decorated);\n"
                "    return info.metadata.instrumented;\n"
                "}\n"
                "fn __fixture(): int {\n"
                "    return decorated();\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "test_compile_time_object_decorator_member_chain.zr", 52);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 17, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_object_member_assignment_projects_mutation(void) {
    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Object Member Assignment";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "comptime fn mark(target): void {\n"
                "    target.metadata.instrumented = true;\n"
                "}\n"
                "comptime var target = { metadata: {} };\n"
                "comptime {\n"
                "    mark(target);\n"
                "}\n"
                "var runtimeValue = target.metadata.instrumented ? 1 : 0;\n"
                "fn __fixture(): int {\n"
                "    return runtimeValue;\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "test_compile_time_object_member_assignment.zr", 46);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 1, testSummary));

        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// 测试16: 编译期类装饰器将 metadata 投影到运行时反射
static void test_compile_time_class_decorator_projects_metadata_to_runtime_reflection(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "compile_time_class_decorator_reflection_fixture",
                    "module compile_time_class_decorator_reflection_fixture;\n"
                    "comptime class Serializable {\n"
                    "    @decorate(target: typeof Class): zr.DecoratorPatch {\n"
                    "        return { metadata: { serializable: true } };\n"
                    "    }\n"
                    "}\n"
                    "#Serializable#\n"
                    "pub class User {\n"
                    "    pub var id: int = 1;\n"
                    "}\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };
    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Class Decorator Reflection Metadata";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "let decorated = import(\"compile_time_class_decorator_reflection_fixture\");\n"
                "fn __fixture(): int {\n"
                "    var info = typeof(decorated.User);\n"
                "    if (info.metadata == null) {\n"
                "        return 0;\n"
                "    }\n"
                "    return info.metadata.serializable ? 1 : 0;\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_class_decorator_reflection.zr",
                                          strlen("test_compile_time_class_decorator_reflection.zr"));
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 1, testSummary));

        state->global->sourceLoader = ZR_NULL;
        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_function_decorator_projects_metadata_to_runtime_reflection(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "compile_time_function_decorator_reflection_fixture",
                    "module compile_time_function_decorator_reflection_fixture;\n"
                    "comptime fn decorate(target: typeof Class, version: int = 7) {\n"
                    "    return { metadata: { version: version } };\n"
                    "}\n"
                    "#decorate#\n"
                    "pub class User {\n"
                    "    pub var id: int = 1;\n"
                    "}\n"
                    "#decorate(version: 11)#\n"
                    "pub class Admin {\n"
                    "    pub var id: int = 2;\n"
                    "}\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };
    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Function Decorator Reflection Metadata";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "let decorated = import(\"compile_time_function_decorator_reflection_fixture\");\n"
                "fn __fixture(): int {\n"
                "    var userInfo = typeof(decorated.User);\n"
                "    var adminInfo = typeof(decorated.Admin);\n"
                "    return userInfo.metadata.version + adminInfo.metadata.version;\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_function_decorator_reflection.zr",
                                          strlen("test_compile_time_function_decorator_reflection.zr"));
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 18, testSummary));

        state->global->sourceLoader = ZR_NULL;
        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compile_time_struct_decorator_projects_metadata_to_runtime_reflection(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "compile_time_struct_decorator_reflection_fixture",
                    "module compile_time_struct_decorator_reflection_fixture;\n"
                    "comptime struct Packed {\n"
                    "    @decorate(target: typeof Struct): zr.DecoratorPatch {\n"
                    "        return { metadata: { packed: true } };\n"
                    "    }\n"
                    "}\n"
                    "#Packed#\n"
                    "pub struct Packet {\n"
                    "    var id: int = 1;\n"
                    "}\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };
    SZrTestTimer timer;
    const TZrChar *testSummary = "Compile-Time Execution - Struct Decorator Reflection Metadata";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "module test;\n"
                "let decorated = import(\"compile_time_struct_decorator_reflection_fixture\");\n"
                "fn __fixture(): int {\n"
                "    var info = typeof(decorated.Packet);\n"
                "    if (info.metadata == null) {\n"
                "        return 0;\n"
                "    }\n"
                "    return info.metadata.packed ? 1 : 0;\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        STestCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state,
                                          "test_compile_time_struct_decorator_reflection.zr",
                                          strlen("test_compile_time_struct_decorator_reflection.zr"));
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(compile_fixture_with_entry(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 1, testSummary));

        state->global->sourceLoader = ZR_NULL;
        test_compile_result_free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_comptime_fn_and_block_use_current_surface(void) {
    static const TZrChar *source =
            "comptime fn sum(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "comptime {\n"
            "    let checked = sum(20, 22);\n"
            "}\n"
            "return 42;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "comptime_current_surface.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(state, function, 42, "current comptime surface"));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_pub_comptime_fn_uses_current_surface_without_runtime_projection(void) {
    static const TZrChar *source =
            "pub comptime fn answer(): int {\n"
            "    return 42;\n"
            "}\n"
            "comptime {\n"
            "    let checked = answer();\n"
            "}\n"
            "return 42;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "pub_comptime_current_surface.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(state, function, 42, "public current comptime function"));

    ZrCore_Function_Free(state, function);

    state->global->emitCompileTimeRuntimeSupport = ZR_TRUE;
    sourceName = ZrCore_String_CreateFromNative(state, "pub_comptime_no_runtime_projection.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state,
            "pub comptime fn hidden(): int { return 42; }\nreturn hidden();\n",
            strlen("pub comptime fn hidden(): int { return 42; }\nreturn hidden();\n"),
            sourceName);
    TEST_ASSERT_NOT_NULL(function);
    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        SZrFunction *child = &function->childFunctionList[index];
        TEST_ASSERT_TRUE(child->functionName == ZR_NULL ||
                         strcmp(ZrCore_String_GetNativeString(child->functionName), "hidden") != 0);
    }
    TEST_ASSERT_TRUE(execute_test_function(state, function, 42, "no hidden comptime runtime function"));
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_current_comptime_block_runs_after_signature_collection(void) {
    static const TZrChar *source =
            "comptime {\n"
            "    let checked = declaredLater();\n"
            "}\n"
            "comptime fn declaredLater(): int { return 42; }\n"
            "return 42;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "comptime_late_check.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(state, function, 42, "comptime late check"));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_compile_tool_descriptor_is_compile_only_and_contract_stable(void) {
    const SZrParserCompileToolModuleDescriptor *descriptor =
            ZrParser_CompileTool_FindModule(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    SZrState *state = create_test_state();

    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_EQUAL_STRING(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD, descriptor->moduleName);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL, descriptor->providerPhase);
    TEST_ASSERT_EQUAL_STRING(ZR_PARSER_COMPILE_TOOL_BUILD_PUBLIC_CONTRACT_HASH,
                             descriptor->publicContractHash);
    TEST_ASSERT_EQUAL_UINT64(ZrParser_CompileTool_ComputePublicContractHash(descriptor),
                             descriptor->computedPublicContractHash);
    TEST_ASSERT_EQUAL_size_t(4u, descriptor->callableCount);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPILE_TOOL_ROLE_BUILD_FEATURE,
                          descriptor->callables[0].role);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPILE_TOOL_ROLE_ASSERT,
                          descriptor->callables[1].role);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPILE_TOOL_ROLE_ERROR,
                          descriptor->callables[2].role);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPILE_TOOL_ROLE_WARNING,
                          descriptor->callables[3].role);
    TEST_ASSERT_TRUE(ZrParser_CompileTool_IsModuleName(
            ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION));

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NULL(ZrLibrary_NativeRegistry_FindModule(
            state->global,
            ZR_PARSER_COMPILE_TOOL_MODULE_BUILD));
    destroy_test_state(state);
}

static void test_comptime_if_reads_declared_project_feature_and_prunes_branch(void) {
    static const TZrChar *projectJson =
            "{\"name\":\"comptime-test\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main.zr\","
            "\"features\":{\"trace\":true}}";
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime if (compile.build.feature(\"trace\")) {\n"
            "    fn selected(): int { return 42; }\n"
            "} else {\n"
            "    fn selected(): int { return 7; }\n"
            "}\n"
            "return selected();\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    TZrPtr previousUserData;
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, "E:/tmp/comptime-test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    previousUserData = state->global->userData;
    state->global->userData = project;

    sourceName = ZrCore_String_CreateFromNative(state, "comptime_feature.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(state, function, 42, "comptime feature branch"));

    ZrCore_Function_Free(state, function);
    state->global->userData = previousUserData;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_comptime_if_prunes_static_import_summary_before_module_analysis(void) {
    static const TZrChar *projectJson =
            "{\"name\":\"comptime-test\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main.zr\","
            "\"features\":{\"trace\":true}}";
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime if (compile.build.feature(\"trace\")) {\n"
            "    let selected = import(\"comptime_enabled\");\n"
            "} else {\n"
            "    let selected = import(\"comptime_disabled\");\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    TZrPtr previousUserData;
    SZrString *sourceName;
    SZrString *moduleName;
    SZrAstNode *ast;
    const SZrParserModuleInitSummary *summary;
    SZrString *staticImport;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, "E:/tmp/comptime-test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    previousUserData = state->global->userData;
    state->global->userData = project;

    sourceName = ZrCore_String_CreateFromNative(state, "comptime_static_import_summary.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    moduleName = ZrCore_String_CreateFromNative(state, "comptime.summary");
    TEST_ASSERT_NOT_NULL(moduleName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state, moduleName, ast));

    summary = ZrParser_ModuleInitAnalysis_FindSummaryByAst(state->global, ast);
    TEST_ASSERT_NOT_NULL(summary);
    TEST_ASSERT_EQUAL_size_t(1u, summary->staticImports.length);
    staticImport = *(SZrString **)ZrCore_Array_Get((SZrArray *)&summary->staticImports, 0);
    TEST_ASSERT_NOT_NULL(staticImport);
    TEST_ASSERT_EQUAL_STRING("comptime_enabled", ZrCore_String_GetNativeString(staticImport));

    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, ast);
    ZrParser_Ast_Free(state, ast);
    state->global->userData = previousUserData;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_nested_comptime_if_prunes_build_facts_before_module_analysis(void) {
    static const TZrChar *projectJson =
            "{\"name\":\"comptime-test\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main.zr\","
            "\"features\":{\"trace\":true}}";
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime if (true) {\n"
            "    comptime if (compile.build.feature(\"trace\")) {\n"
            "        let selected = import(\"nested_enabled\");\n"
            "    } else {\n"
            "        let selected = import(\"nested_disabled\");\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    TZrPtr previousUserData;
    SZrString *sourceName;
    SZrString *moduleName;
    SZrAstNode *ast;
    const SZrParserModuleInitSummary *summary;
    SZrString *staticImport;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, "E:/tmp/comptime-test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    previousUserData = state->global->userData;
    state->global->userData = project;

    sourceName = ZrCore_String_CreateFromNative(state, "nested_comptime_summary.zr");
    moduleName = ZrCore_String_CreateFromNative(state, "comptime.nested.summary");
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(moduleName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state, moduleName, ast));

    summary = ZrParser_ModuleInitAnalysis_FindSummaryByAst(state->global, ast);
    TEST_ASSERT_NOT_NULL(summary);
    TEST_ASSERT_EQUAL_size_t(1u, summary->staticImports.length);
    staticImport = *(SZrString **)ZrCore_Array_Get((SZrArray *)&summary->staticImports, 0);
    TEST_ASSERT_NOT_NULL(staticImport);
    TEST_ASSERT_EQUAL_STRING("nested_enabled", ZrCore_String_GetNativeString(staticImport));

    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, ast);
    ZrParser_Ast_Free(state, ast);
    state->global->userData = previousUserData;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_statement_comptime_if_is_selected_during_build_facts(void) {
    static const TZrChar *source =
            "fn selected(): int {\n"
            "    comptime if (true) { return 42; } else { return 7; }\n"
            "}\n"
            "return selected();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *functionNode;
    SZrAstNode *comptimeNode;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "statement_comptime_build_facts.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));

    functionNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionNode->type);
    comptimeNode = functionNode->data.functionDeclaration.body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(comptimeNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_COMPILE_TIME_DECLARATION, comptimeNode->type);
    TEST_ASSERT_NOT_NULL(comptimeNode->data.compileTimeDeclaration.selectedBranch);

    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_runtime_scope_rejects_compile_tool_import(void) {
    static const TZrChar *source =
            "fn invalid(): int {\n"
            "    let compile = import(\"zr.compile\");\n"
            "    return 0;\n"
            "}\n"
            "return invalid();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "runtime_compile_tool_import.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, state);
    TEST_ASSERT_FALSE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "compiletool.phase_mismatch"));
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_runtime_scope_rejects_top_level_compile_tool_alias_use(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "fn invalid(): bool {\n"
            "    return compile.build.feature(\"trace\");\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "runtime_compile_tool_alias_use.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, state);
    TEST_ASSERT_FALSE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "compiletool.phase_mismatch"));
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_runtime_top_level_statements_reject_compile_tool_alias_use(void) {
    static const TZrChar *sources[] = {
            "let compile = import(\"zr.compile\");\n"
            "return compile.build.feature(\"trace\");\n",
            "let compile = import(\"zr.compile\");\n"
            "if (true) { return compile.build.feature(\"trace\"); }\n"
            "return false;\n",
            "let compile = import(\"zr.compile\");\n"
            "while (false) { return compile.build.feature(\"trace\"); }\n"
            "return false;\n"};
    static const TZrChar *sourceNames[] = {
            "runtime_top_level_return_compile_tool_alias.zr",
            "runtime_top_level_if_compile_tool_alias.zr",
            "runtime_top_level_while_compile_tool_alias.zr"};

    for (TZrSize index = 0; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompilerState cs;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)sourceNames[index]);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        ZrParser_CompilerState_Init(&cs, state);
        TEST_ASSERT_FALSE(
                ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
        TEST_ASSERT_NOT_NULL(cs.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "compiletool.phase_mismatch"));
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }
}

static void test_runtime_local_binding_shadows_compile_tool_alias(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "fn valid(): int {\n"
            "    let compile = 42;\n"
            "    return compile;\n"
            "}\n"
            "return valid();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "runtime_compile_tool_shadow.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_expression_nested_comptime_if_is_selected_during_build_facts(void) {
    static const TZrChar *lambdaSource =
            "fn selected(): int {\n"
            "    let run = fn(): int {\n"
            "        comptime if (true) { return 42; } else { return 7; }\n"
            "    };\n"
            "    return run();\n"
            "}\n"
            "return selected();\n";
    static const TZrChar *generatorSource =
            "fn selected(): int {\n"
            "    fn values(): Iterator<int> {\n"
            "        comptime if (true) { yield 42; } else { yield 7; }\n"
            "    }\n"
            "    return 42;\n"
            "}\n"
            "return selected();\n";
    const TZrChar *sources[] = {lambdaSource, generatorSource};
    const TZrChar *names[] = {"lambda_comptime_build_facts.zr", "generator_comptime_build_facts.zr"};

    for (TZrSize index = 0; index < 2; index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString)names[index]);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }
}

static void test_comptime_feature_supports_disabled_no_else_and_typed_string(void) {
    static const TZrChar *projectJson =
            "{\"name\":\"comptime-test\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main.zr\","
            "\"features\":{\"trace\":false}}";
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime fn configuredFeature(): string { return \"trace\"; }\n"
            "comptime if (compile.build.feature(configuredFeature())) {\n"
            "    fn selected(): int { return 7; }\n"
            "} else {\n"
            "    fn selected(): int { return 42; }\n"
            "}\n"
            "comptime if (false) {\n"
            "    let missing = import(\"inactive.no.else\");\n"
            "}\n"
            "return selected();\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    TZrPtr previousUserData;
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, "E:/tmp/comptime-test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    previousUserData = state->global->userData;
    state->global->userData = project;
    sourceName = ZrCore_String_CreateFromNative(state, "comptime_disabled_typed_feature.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(state, function, 42, "disabled typed feature"));

    ZrCore_Function_Free(state, function);
    state->global->userData = previousUserData;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_comptime_feature_rejects_wrong_arity_and_type(void) {
    static const TZrChar *wrongArity =
            "let compile = import(\"zr.compile\");\n"
            "comptime if (compile.build.feature()) { return 1; }\n"
            "return 0;\n";
    static const TZrChar *wrongType =
            "let compile = import(\"zr.compile\");\n"
            "comptime if (compile.build.feature(42)) { return 1; }\n"
            "return 0;\n";
    SZrState *state = create_test_state();

    TEST_ASSERT_NOT_NULL(state);
    assert_compile_time_compile_failure(state, wrongArity, "comptime_feature_wrong_arity.zr");
    assert_compile_time_compile_failure(state, wrongType, "comptime_feature_wrong_type.zr");
    destroy_test_state(state);
}

static void test_comptime_if_selected_declaration_enters_module_summary(void) {
    static const TZrChar *source =
            "comptime if (true) {\n"
            "    pub fn selected(): int { return 42; }\n"
            "} else {\n"
            "    pub fn inactive(): int { return 7; }\n"
            "}\n"
            "return selected();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrString *moduleName;
    SZrAstNode *ast;
    const SZrParserModuleInitSummary *summary;
    const SZrModuleInitExportInfo *exportInfo;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "comptime_selected_export.zr");
    moduleName = ZrCore_String_CreateFromNative(state, "comptime.selected.export");
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(moduleName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state, moduleName, ast));

    summary = ZrParser_ModuleInitAnalysis_FindSummaryByAst(state->global, ast);
    TEST_ASSERT_NOT_NULL(summary);
    TEST_ASSERT_EQUAL_size_t(1u, summary->exports.length);
    exportInfo = (const SZrModuleInitExportInfo *)ZrCore_Array_Get(
            (SZrArray *)&summary->exports,
            0);
    TEST_ASSERT_NOT_NULL(exportInfo);
    TEST_ASSERT_EQUAL_STRING("selected", ZrCore_String_GetNativeString(exportInfo->name));

    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, ast);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_comptime_if_inactive_import_is_not_canonicalized(void) {
    static const TZrChar *source =
            "comptime if (false) {\n"
            "    let missing = import(\"inactive.module.must.not.resolve\");\n"
            "} else {\n"
            "    fn selected(): int { return 42; }\n"
            "}\n"
            "return selected();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "comptime_inactive_import.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(state, function, 42, "inactive import pruning"));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_comptime_if_rejects_unknown_project_feature(void) {
    static const TZrChar *projectJson =
            "{\"name\":\"comptime-test\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main.zr\","
            "\"features\":{\"trace\":true}}";
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime if (compile.build.feature(\"missing\")) {\n"
            "    fn selected(): int { return 1; }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    TZrPtr previousUserData;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, "E:/tmp/comptime-test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    previousUserData = state->global->userData;
    state->global->userData = project;

    assert_compile_time_compile_failure(state, source, "comptime_unknown_feature.zr");

    state->global->userData = previousUserData;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_compile_tool_alias_shadowed_by_runtime_binding_forms(void) {
    static const TZrChar *sources[] = {
            "let compile = import(\"zr.compile\");\n"
            "fn valid(): int { let {value: compile} = {value: 42}; return compile; }\n"
            "return 0;\n",
            "let compile = import(\"zr.compile\");\n"
            "fn valid(): int { for (let compile in [42]) { let value = compile; } return 0; }\n"
            "return valid();\n",
            "let compile = import(\"zr.compile\");\n"
            "fn valid(): int { try { throw 1; } catch (compile) { let value = compile; } return 0; }\n"
            "return valid();\n",
            "let compile = import(\"zr.compile\");\n"
            "fn valid(): int { using (var compile = 42) { let value = compile; } return 0; }\n"
            "return valid();\n",
            "let compile = import(\"zr.compile\");\n"
            "fn valid(...compile: int[]): int { return 0; }\n"
            "return 0;\n"};
    static const TZrChar *names[] = {
            "compile_tool_destructuring_shadow.zr",
            "compile_tool_foreach_shadow.zr",
            "compile_tool_catch_shadow.zr",
            "compile_tool_using_shadow.zr",
            "compile_tool_vararg_shadow.zr"};

    for (TZrSize index = 0; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)names[index]);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }
}

static void test_for_initializer_shadow_does_not_escape_loop(void) {
    static const TZrChar *projectJson =
            "{\"name\":\"comptime-test\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main.zr\","
            "\"features\":{\"trace\":true}}";
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "for (var compile = 0; compile < 1; compile = compile + 1) {}\n"
            "comptime if (compile.build.feature(\"trace\")) { fn selected(): int { return 42; } }\n"
            "return selected();\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    SZrString *sourceName;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(
            state, (TZrNativeString)projectJson, "E:/tmp/comptime-test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    sourceName = ZrCore_String_CreateFromNative(
            state, "compile_tool_for_scope.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[2]
                                 ->data.compileTimeDeclaration.selectedBranch);

    ZrParser_Ast_Free(state, ast);
    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_default_parameter_expression_enters_build_facts(void) {
    static const TZrChar *source =
            "fn use(factory = fn(): int {\n"
            "    comptime if (true) { return 42; } else { return 7; }\n"
            "}): void {}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *functionNode;
    SZrAstNode *parameterNode;
    SZrAstNode *lambdaNode;
    SZrAstNode *comptimeNode;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "comptime_default_parameter_build_facts.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
    functionNode = ast->data.script.statements->nodes[0];
    parameterNode = functionNode->data.functionDeclaration.params->nodes[0];
    lambdaNode = parameterNode->data.parameter.defaultValue;
    comptimeNode = lambdaNode->data.lambdaExpression.block->data.block.body->nodes[0];
    TEST_ASSERT_TRUE(comptimeNode->data.compileTimeDeclaration.buildFactsEvaluated);
    TEST_ASSERT_NOT_NULL(comptimeNode->data.compileTimeDeclaration.selectedBranch);

    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_selected_branch_late_comptime_block_is_not_skipped(void) {
    static const TZrChar *source =
            "comptime if (true) {\n"
            "    comptime { let value = missingComptimeFunction(); }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();

    TEST_ASSERT_NOT_NULL(state);
    assert_compile_time_compile_failure(
            state, source, "selected_branch_late_comptime.zr");
    destroy_test_state(state);
}

static void test_function_local_comptime_block_is_rejected(void) {
    static const TZrChar *source =
            "fn invalid(): int {\n"
            "    comptime { let value = 42; }\n"
            "    return 0;\n"
            "}\n"
            "return invalid();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "function_local_comptime_block.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, state);
    TEST_ASSERT_FALSE(
            ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "comptime.module_scope_only"));

    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_runtime_local_function_hoists_over_compile_tool_alias(void) {
    static const TZrChar *sources[] = {
            "let compile = import(\"zr.compile\");\n"
            "fn outer(): int {\n"
            "    let result = compile();\n"
            "    fn compile(): int { return 42; }\n"
            "    return result;\n"
            "}\n"
            "return outer();\n",
            "let compile = import(\"zr.compile\");\n"
            "fn outer(): int {\n"
            "    fn compile(): int { return 42; }\n"
            "    let result = compile();\n"
            "    return result;\n"
            "}\n"
            "return outer();\n"};
    static const TZrChar *sourceNames[] = {
            "compile_tool_local_function_hoist_after_use.zr",
            "compile_tool_local_function_hoist_before_use.zr"};

    for (TZrSize index = 0; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)sourceNames[index]);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }
}

static void test_module_function_hoists_over_compile_tool_alias(void) {
    static const TZrChar *sources[] = {
            "let compile = import(\"zr.compile\");\n"
            "fn compile(): int { return 42; }\n"
            "fn use(): int { return compile(); }\n"
            "return use();\n",
            "let compile = import(\"zr.compile\");\n"
            "comptime if (true) {\n"
            "    fn compile(): int { return 42; }\n"
            "}\n"
            "fn use(): int { return compile(); }\n"
            "return use();\n",
            "let compile = import(\"zr.compile\");\n"
            "fn use(): int { return compile(); }\n"
            "comptime if (true) {\n"
            "    fn compile(): int { return 42; }\n"
            "}\n"
            "return use();\n"};
    static const TZrChar *sourceNames[] = {
            "compile_tool_module_function_hoist.zr",
            "compile_tool_selected_module_function_hoist.zr",
            "compile_tool_selected_module_function_hoist_after_use.zr"};

    for (TZrSize index = 0; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)sourceNames[index]);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }
}

static void test_selected_runtime_function_shadow_does_not_escape_branch(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "fn selected(): int {\n"
            "    comptime if (true) {\n"
            "        fn compile(): int { return 42; }\n"
            "        let value = compile();\n"
            "    }\n"
            "    comptime if (compile.build.feature(\"trace\")) {\n"
            "        return 42;\n"
            "    } else {\n"
            "        return 7;\n"
            "    }\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    SZrString *sourceName;
    SZrAstNode *ast;
    TZrPtr previousUserData;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(
            state,
            "{\"name\":\"scope-test\",\"source\":\"src\",\"binary\":\"bin\","
            "\"entry\":\"main.zr\",\"features\":{\"trace\":true}}",
            "E:/tmp/compile-tool-scope-test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    previousUserData = state->global->userData;
    state->global->userData = project;
    sourceName = ZrCore_String_CreateFromNative(
            state, "compile_tool_selected_runtime_shadow_scope.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));

    ZrParser_Ast_Free(state, ast);
    state->global->userData = previousUserData;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_runtime_top_level_containers_reject_comptime_block(void) {
    static const TZrChar *sources[] = {
            "if (true) { comptime { let value = 42; } }\nreturn 0;\n",
            "while (false) { comptime { let value = 42; } }\nreturn 0;\n"};
    static const TZrChar *sourceNames[] = {
            "top_level_runtime_if_comptime.zr",
            "top_level_runtime_while_comptime.zr"};

    for (TZrSize index = 0; index < sizeof(sources) / sizeof(sources[0]); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompilerState cs;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, (TZrNativeString)sourceNames[index]);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        ZrParser_CompilerState_Init(&cs, state);
        TEST_ASSERT_FALSE(
                ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
        TEST_ASSERT_NOT_NULL(cs.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "comptime.module_scope_only"));
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }
}

static void test_interface_and_extern_defaults_enter_build_facts(void) {
    static const TZrChar *source =
            "interface Service {\n"
            "    fn run(factory = fn(): int {\n"
            "        comptime if (true) { return 42; } else { return 7; }\n"
            "    }): void;\n"
            "}\n"
            "native extern(\"fixture\") {\n"
            "    fn read(factory = fn(): int {\n"
            "        comptime if (false) { return 7; }\n"
            "    }): int;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *interfaceNode;
    SZrAstNode *interfaceMethod;
    SZrAstNode *interfaceParameter;
    SZrAstNode *interfaceLambda;
    SZrAstNode *activeComptime;
    SZrAstNode *externBlock;
    SZrAstNode *externFunction;
    SZrAstNode *externParameter;
    SZrAstNode *externLambda;
    SZrAstNode *inactiveComptime;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "interface_extern_default_build_facts.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));

    interfaceNode = ast->data.script.statements->nodes[0];
    interfaceMethod = interfaceNode->data.interfaceDeclaration.members->nodes[0];
    interfaceParameter = interfaceMethod->data.interfaceMethodSignature.params->nodes[0];
    interfaceLambda = interfaceParameter->data.parameter.defaultValue;
    activeComptime = interfaceLambda->data.lambdaExpression.block->data.block.body->nodes[0];
    TEST_ASSERT_TRUE(activeComptime->data.compileTimeDeclaration.buildFactsEvaluated);
    TEST_ASSERT_NOT_NULL(activeComptime->data.compileTimeDeclaration.selectedBranch);

    externBlock = ast->data.script.statements->nodes[1];
    externFunction = externBlock->data.externBlock.declarations->nodes[0];
    externParameter = externFunction->data.externFunctionDeclaration.params->nodes[0];
    externLambda = externParameter->data.parameter.defaultValue;
    inactiveComptime = externLambda->data.lambdaExpression.block->data.block.body->nodes[0];
    TEST_ASSERT_TRUE(inactiveComptime->data.compileTimeDeclaration.buildFactsEvaluated);
    TEST_ASSERT_NULL(inactiveComptime->data.compileTimeDeclaration.selectedBranch);

    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static SZrAstNode *build_fact_comptime_from_decorator(SZrAstNode *decorator) {
    SZrAstNode *expression;
    SZrAstNode *call = ZR_NULL;
    SZrAstNode *lambda;

    TEST_ASSERT_NOT_NULL(decorator);
    TEST_ASSERT_EQUAL_INT(ZR_AST_DECORATOR_EXPRESSION, decorator->type);
    expression = decorator->data.decoratorExpression.expr;
    TEST_ASSERT_NOT_NULL(expression);
    if (expression->type == ZR_AST_FUNCTION_CALL) {
        call = expression;
    } else {
        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
        for (TZrSize index = 0;
             expression->data.primaryExpression.members != ZR_NULL &&
             index < expression->data.primaryExpression.members->count;
             index++) {
            SZrAstNode *member =
                    expression->data.primaryExpression.members->nodes[index];
            if (member != ZR_NULL && member->type == ZR_AST_FUNCTION_CALL) {
                call = member;
                break;
            }
        }
    }
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
    TEST_ASSERT_TRUE(call->data.functionCall.args->count > 0u);
    lambda = call->data.functionCall.args->nodes[0];
    TEST_ASSERT_NOT_NULL(lambda);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambda->type);
    TEST_ASSERT_NOT_NULL(lambda->data.lambdaExpression.block);
    TEST_ASSERT_NOT_NULL(lambda->data.lambdaExpression.block->data.block.body);
    TEST_ASSERT_TRUE(lambda->data.lambdaExpression.block->data.block.body->count > 0u);
    return lambda->data.lambdaExpression.block->data.block.body->nodes[0];
}

static void test_signature_variants_and_decorators_enter_build_facts(void) {
    static const TZrChar *source =
            "interface Service {\n"
            "    @create(factory = fn(): int {\n"
            "        comptime if (true) { return 42; } else { return 7; }\n"
            "    }): void;\n"
            "    @variadic(...args: int[]): void;\n"
            "}\n"
            "native extern(\"fixture\") {\n"
            "    #decorate(fn(): int { comptime if (true) { return 1; } })#\n"
            "    delegate Callback(\n"
            "        #decorate(fn(): int { comptime if (false) { return 2; } })#\n"
            "        factory = fn(): int { comptime if (true) { return 3; } }\n"
            "    ): int;\n"
            "    delegate VariadicCallback(...args: int[]): int;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *interfaceNode;
    SZrAstNode *interfaceMeta;
    SZrAstNode *interfaceVariadicMeta;
    SZrAstNode *interfaceDefault;
    SZrAstNode *interfaceComptime;
    SZrAstNode *externBlock;
    SZrAstNode *externDelegate;
    SZrAstNode *externVariadicDelegate;
    SZrAstNode *declarationComptime;
    SZrAstNode *parameter;
    SZrAstNode *parameterComptime;
    SZrAstNode *delegateDefault;
    SZrAstNode *delegateDefaultComptime;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "signature_variant_build_facts.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFacts(state, ast));

    interfaceNode = ast->data.script.statements->nodes[0];
    interfaceMeta = interfaceNode->data.interfaceDeclaration.members->nodes[0];
    interfaceVariadicMeta =
            interfaceNode->data.interfaceDeclaration.members->nodes[1];
    TEST_ASSERT_NOT_NULL(interfaceVariadicMeta->data.interfaceMetaSignature.args);
    interfaceDefault =
            interfaceMeta->data.interfaceMetaSignature.params->nodes[0]
                    ->data.parameter.defaultValue;
    interfaceComptime =
            interfaceDefault->data.lambdaExpression.block->data.block.body->nodes[0];
    TEST_ASSERT_TRUE(interfaceComptime->data.compileTimeDeclaration.buildFactsEvaluated);
    TEST_ASSERT_NOT_NULL(interfaceComptime->data.compileTimeDeclaration.selectedBranch);

    externBlock = ast->data.script.statements->nodes[1];
    externDelegate = externBlock->data.externBlock.declarations->nodes[0];
    externVariadicDelegate = externBlock->data.externBlock.declarations->nodes[1];
    TEST_ASSERT_NOT_NULL(
            externVariadicDelegate->data.externDelegateDeclaration.args);
    TEST_ASSERT_NOT_NULL(externDelegate->data.externDelegateDeclaration.decorators);
    declarationComptime = build_fact_comptime_from_decorator(
            externDelegate->data.externDelegateDeclaration.decorators->nodes[0]);
    TEST_ASSERT_TRUE(declarationComptime->data.compileTimeDeclaration.buildFactsEvaluated);
    TEST_ASSERT_NOT_NULL(declarationComptime->data.compileTimeDeclaration.selectedBranch);

    parameter = externDelegate->data.externDelegateDeclaration.params->nodes[0];
    TEST_ASSERT_NOT_NULL(parameter->data.parameter.decorators);
    parameterComptime = build_fact_comptime_from_decorator(
            parameter->data.parameter.decorators->nodes[0]);
    TEST_ASSERT_TRUE(parameterComptime->data.compileTimeDeclaration.buildFactsEvaluated);
    TEST_ASSERT_NULL(parameterComptime->data.compileTimeDeclaration.selectedBranch);
    delegateDefault = parameter->data.parameter.defaultValue;
    delegateDefaultComptime =
            delegateDefault->data.lambdaExpression.block->data.block.body->nodes[0];
    TEST_ASSERT_TRUE(delegateDefaultComptime->data.compileTimeDeclaration.buildFactsEvaluated);
    TEST_ASSERT_NOT_NULL(delegateDefaultComptime->data.compileTimeDeclaration.selectedBranch);

    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

// 主函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    printf("Compile-Time Execution Tests\n");
    TEST_MODULE_DIVIDER();
    
    RUN_TEST(test_compile_tool_descriptor_is_compile_only_and_contract_stable);
    RUN_TEST(test_comptime_fn_and_block_use_current_surface);
    RUN_TEST(test_pub_comptime_fn_uses_current_surface_without_runtime_projection);
    RUN_TEST(test_current_comptime_block_runs_after_signature_collection);
    RUN_TEST(test_comptime_if_reads_declared_project_feature_and_prunes_branch);
    RUN_TEST(test_comptime_if_prunes_static_import_summary_before_module_analysis);
    RUN_TEST(test_nested_comptime_if_prunes_build_facts_before_module_analysis);
    RUN_TEST(test_statement_comptime_if_is_selected_during_build_facts);
    RUN_TEST(test_runtime_scope_rejects_compile_tool_import);
    RUN_TEST(test_runtime_scope_rejects_top_level_compile_tool_alias_use);
    RUN_TEST(test_runtime_top_level_statements_reject_compile_tool_alias_use);
    RUN_TEST(test_runtime_local_binding_shadows_compile_tool_alias);
    RUN_TEST(test_expression_nested_comptime_if_is_selected_during_build_facts);
    RUN_TEST(test_comptime_feature_supports_disabled_no_else_and_typed_string);
    RUN_TEST(test_comptime_feature_rejects_wrong_arity_and_type);
    RUN_TEST(test_comptime_if_selected_declaration_enters_module_summary);
    RUN_TEST(test_comptime_if_inactive_import_is_not_canonicalized);
    RUN_TEST(test_comptime_if_rejects_unknown_project_feature);
    RUN_TEST(test_compile_tool_alias_shadowed_by_runtime_binding_forms);
    RUN_TEST(test_for_initializer_shadow_does_not_escape_loop);
    RUN_TEST(test_default_parameter_expression_enters_build_facts);
    RUN_TEST(test_selected_branch_late_comptime_block_is_not_skipped);
    RUN_TEST(test_function_local_comptime_block_is_rejected);
    RUN_TEST(test_runtime_local_function_hoists_over_compile_tool_alias);
    RUN_TEST(test_module_function_hoists_over_compile_tool_alias);
    RUN_TEST(test_selected_runtime_function_shadow_does_not_escape_branch);
    RUN_TEST(test_runtime_top_level_containers_reject_comptime_block);
    RUN_TEST(test_interface_and_extern_defaults_enter_build_facts);
    RUN_TEST(test_signature_variants_and_decorators_enter_build_facts);
    
    return UNITY_END();
}

