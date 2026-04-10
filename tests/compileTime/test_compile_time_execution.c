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

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

#define TEST_START(summary) ZR_TEST_START(summary)
#define TEST_INFO(summary, details) ZR_TEST_INFO(summary, details)
#define TEST_PASS_CUSTOM(timer, summary) ZR_TEST_PASS(timer, summary)
#define TEST_FAIL_CUSTOM(timer, summary, reason) do { \
    (timer).endTime = clock(); \
    ZR_TEST_FAIL(timer, summary, reason); \
} while (0)
#define TEST_DIVIDER() ZR_TEST_DIVIDER()
#define TEST_MODULE_DIVIDER() ZR_TEST_MODULE_DIVIDER()

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
    SZrCompileResult compileResult;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));

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
              "Testing %compileTime var MAX_SIZE = 100");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "%compileTime var MAX_SIZE = 100;\n"
        "%compileTime var MIN_SIZE = 1;\n"
        "%compileTime var DEFAULT_VALUE = 42;\n"
        "var runtimeVar = DEFAULT_VALUE;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_vars.zr", 26);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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
    
    ZrParser_CompileResult_Free(state, &compileResult);
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
              "Testing %compileTime function calculateSum");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "%compileTime calculateSum(a: int, b: int): int {\n"
        "    return a + b;\n"
        "}\n"
        "%compileTime var computedValue = calculateSum(10, 20);\n"
        "var runtimeVar = computedValue;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_funcs.zr", 27);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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
    
    ZrParser_CompileResult_Free(state, &compileResult);
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
              "Testing %compileTime var complexExpr = (1+2)*3*4 - 10");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "%compileTime calculateSum(a: int, b: int): int {\n"
        "    return a + b;\n"
        "}\n"
        "%compileTime multiply(x: int, y: int): int {\n"
        "    return x * y;\n"
        "}\n"
        "%compileTime var complexExpr = (calculateSum(1, 2) * multiply(3, 4)) - 10;\n"
        "var runtimeVar = complexExpr;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_expr.zr", 26);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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
    
    ZrParser_CompileResult_Free(state, &compileResult);
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
              "Testing %compileTime function factorial");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "%compileTime factorial(n: int): int {\n"
        "    if (n <= 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    return n * factorial(n - 1);\n"
        "}\n"
        "%compileTime var fact5 = factorial(5);\n"
        "var runtimeVar = fact5;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_recursion.zr", 32);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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
    
    ZrParser_CompileResult_Free(state, &compileResult);
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
              "Testing %compileTime { ... }");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "%compileTime var MAX_SIZE = 100;\n"
        "%compileTime var MIN_SIZE = 1;\n"
        "%compileTime {\n"
        "    if (MAX_SIZE < MIN_SIZE) {\n"
        "        FatalError(\"MAX_SIZE must be greater than MIN_SIZE\");\n"
        "    }\n"
        "}\n"
        "var runtimeVar = 42;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_stmts.zr", 28);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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
    
    ZrParser_CompileResult_Free(state, &compileResult);
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
        "%module \"test\";\n"
        "%compileTime var MAX_SIZE = 100;\n"
        "%compileTime var MIN_SIZE = 1;\n"
        "%compileTime validateArraySize(size: int): bool {\n"
        "    return size >= MIN_SIZE && size <= MAX_SIZE;\n"
        "}\n"
        "var validatedArray: int[validateArraySize(50) ? 50 : 10];\n"
        "%test(\"test\") {\n"
        "    return validatedArray.length;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_array.zr", 28);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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
    
    ZrParser_CompileResult_Free(state, &compileResult);
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
              "Testing runtime initializer uses %compileTime function call directly");

    const TZrChar* source =
        "%module \"test\";\n"
        "%compileTime calculateSum(a: int, b: int): int {\n"
        "    return a + b;\n"
        "}\n"
        "var runtimeVar = calculateSum(10, 20);\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_projection.zr", 31);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }

    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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

    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试8: %compileTime block 内声明持久注册并投影到运行时代码
static void test_compile_time_block_persistent_registration(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Block Persistent Registration";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time block persistent declarations",
              "Testing var/function declared inside %compileTime block remain available afterwards");

    const TZrChar* source =
        "%module \"test\";\n"
        "%compileTime {\n"
        "    var BLOCK_VALUE = 40;\n"
        "    addOffset(base: int): int {\n"
        "        return base + BLOCK_VALUE + 2;\n"
        "    }\n"
        "}\n"
        "var runtimeValue = addOffset(0);\n"
        "%test(\"test\") {\n"
        "    return runtimeValue;\n"
        "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_block_registration.zr", 40);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }

    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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

    ZrParser_CompileResult_Free(state, &compileResult);
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
              "Testing runtime initializer uses named args and default args from %compileTime function");

    const TZrChar* source =
        "%module \"test\";\n"
        "%compileTime combine(a: int, b: int = 10, c: int = 100): int {\n"
        "    return a + b + c;\n"
        "}\n"
        "var runtimeNamed = combine(c: 3, a: 1);\n"
        "var runtimeDefault = combine(a: 4, c: 6);\n"
        "var runtimeAllDefaults = combine(a: 4);\n"
        "%test(\"test\") {\n"
        "    return runtimeNamed + runtimeDefault + runtimeAllDefaults;\n"
        "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_named_default_projection.zr", 47);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }

    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
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

    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试10: %compileTime block 内前向依赖应给出诊断并阻止编译
static void test_compile_time_block_forward_reference_diagnostic(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Compile-Time Execution - Block Forward Reference";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Compile-time block forward reference diagnostic",
              "Testing %compileTime block rejects references to declarations defined later in the same block");

    assert_compile_time_compile_failure(
            state,
            "%module \"test\";\n"
            "%compileTime {\n"
            "    var computed = laterValue + 1;\n"
            "    var laterValue = 41;\n"
            "}\n"
            "var runtimeValue = 0;\n"
            "%test(\"test\") {\n"
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
              "Testing later %compileTime var/function declarations override earlier ones");

    const TZrChar* source =
            "%module \"test\";\n"
            "%compileTime {\n"
            "    var VALUE = 1;\n"
            "    var VALUE = 2;\n"
            "    pick(): int {\n"
            "        return VALUE;\n"
            "    }\n"
            "    pick(): int {\n"
            "        return VALUE + 40;\n"
            "    }\n"
            "}\n"
            "var runtimeValue = pick();\n"
            "%test(\"test\") {\n"
            "    return runtimeValue;\n"
            "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_duplicate_override.zr", 39);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);

    SZrCompileResult compileResult;
    TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

    ZrParser_CompileResult_Free(state, &compileResult);
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
            "%module \"test\";\n"
            "%compileTime addImpl(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "%compileTime var helper = { add: addImpl };\n"
            "var runtimeValue = helper.add(19, 23);\n"
            "%test(\"test\") {\n"
            "    return runtimeValue;\n"
            "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_member_call_projection.zr", 43);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);

    SZrCompileResult compileResult;
    TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

    ZrParser_CompileResult_Free(state, &compileResult);
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
                    "%module \"helper\";\n"
                    "pub var greet = () => {\n"
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
              "Testing %import(\"helper\").greet() is projected during compilation");

    const TZrChar* source =
            "%module \"test\";\n"
            "var runtimeValue = %import(\"helper\").greet();\n"
            "%test(\"test\") {\n"
            "    return runtimeValue;\n"
            "}\n";

    SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_import_member_call_projection.zr", 50);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);

    SZrCompileResult compileResult;
    TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

    ZrParser_CompileResult_Free(state, &compileResult);
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
            "%module \"test\";\n"
            "%compileTime addImpl(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "%compileTime buildHelper() {\n"
            "    return { add: addImpl };\n"
            "}\n"
            "var leaked = buildHelper();\n"
            "%test(\"test\") {\n"
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
                    "%module \"helper\";\n"
                    "pub var toolkit = {\n"
                    "    math: {\n"
                    "        greet: () => {\n"
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
              "Testing %import(\"helper\").toolkit.math.greet() is fully projected during compilation");

    {
        const TZrChar* source =
                "%module \"test\";\n"
                "var runtimeValue = %import(\"helper\").toolkit.math.greet();\n"
                "%test(\"test\") {\n"
                "    return runtimeValue;\n"
                "}\n";

        SZrString* sourceName = ZrCore_String_Create(state, "test_compile_time_import_deep_member_call_projection.zr", 55);
        SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);

        reset_loaded_module_registry(state);
        state->global->sourceLoader = ZR_NULL;
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 42, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
                    "%module \"helper\";\n"
                    "pub compute(seed: int, bonus: int = 5, factor: int = 2): int {\n"
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
              "Testing %import(\"helper\").compute(seed: 10, factor: 3) uses named/default args during compile-time projection");

    const TZrChar* source =
            "%module \"test\";\n"
            "var runtimeValue = %import(\"helper\").compute(seed: 10, factor: 3) + %import(\"helper\").compute(10, bonus: 7);\n"
            "%test(\"test\") {\n"
            "    return runtimeValue;\n"
            "}\n";

    SZrString* sourceName =
            ZrCore_String_Create(state, "test_compile_time_import_runtime_callable_named_default_projection.zr", 69);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);

    SZrCompileResult compileResult;
    TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_compile_time_binary_import_function_alias_projection(void) {
    static const TZrChar* providerSource =
            "%module \"provider\";\n"
            "%compileTime var SCALE = 8;\n"
            "%compileTime buildBias(seed: int): int {\n"
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
                "%module \"test\";\n"
                "var provider = %import(\"provider\");\n"
                "var runtimeValue = buildBias(34);\n"
                "%test(\"test\") {\n"
                "    return runtimeValue;\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
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

        ZrParser_CompileResult_Free(state, &compileResult);
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
            "%module \"provider\";\n"
            "%compileTime var BASE = 5;\n"
            "%compileTime compute(seed: int, bonus: int = BASE, factor: int = 2): int {\n"
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
                "%module \"test\";\n"
                "var provider = %import(\"provider\");\n"
                "var runtimeValue = compute(seed: 10, factor: 3) + compute(10, bonus: 7);\n"
                "%test(\"test\") {\n"
                "    return runtimeValue;\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
            "%module \"provider\";\n"
            "pub compute(seed: int, bonus: int = 5, factor: int = 2): int {\n"
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
                "%module \"test\";\n"
                "var provider = %import(\"provider\");\n"
                "var runtimeValue = provider.compute(seed: 10, factor: 3) + provider.compute(10, bonus: 7);\n"
                "%test(\"test\") {\n"
                "    return runtimeValue;\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
            "%module \"provider\";\n"
            "%compileTime var BASE = 5;\n"
            "%compileTime compute(seed: int, bonus: int = BASE, factor: int = 2): int {\n"
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
                "%module \"test\";\n"
                "var provider = %import(\"provider\");\n"
                "%compileTime markFunction(target: %type Function, bonus: int = 0) {\n"
                "    return { metadata: { instrumented: bonus } };\n"
                "}\n"
                "#markFunction(bonus: compute(seed: 10, factor: 3))#\n"
                "pub decoratedBonusDefault(): int {\n"
                "    var meta = %type(decoratedBonusDefault).metadata;\n"
                "    return meta.instrumented;\n"
                "}\n"
                "#markFunction(bonus: compute(10, bonus: 7))#\n"
                "pub decoratedBonusNamed(): int {\n"
                "    var meta = %type(decoratedBonusNamed).metadata;\n"
                "    return meta.instrumented;\n"
                "}\n"
                "%test(\"test\") {\n"
                "    return decoratedBonusDefault() + decoratedBonusNamed();\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
            "%module \"provider\";\n"
            "%compileTime var BASE = 5;\n"
            "%compileTime compute(seed: int, bonus: int = BASE, factor: int = 2): int {\n"
            "    return seed * factor + bonus;\n"
            "}\n";
    static const TZrChar* decoratedUserSource =
            "%module \"decorated_user\";\n"
            "var provider = %import(\"provider\");\n"
            "%compileTime markFunction(target: %type Function, bonus: int = 0) {\n"
            "    return { metadata: { instrumented: bonus } };\n"
            "}\n"
            "#markFunction(bonus: compute(seed: 10, factor: 3))#\n"
            "pub decoratedBonusDefault(): int {\n"
            "    var meta = %type(decoratedBonusDefault).metadata;\n"
            "    return meta.instrumented;\n"
            "}\n"
            "#markFunction(bonus: compute(10, bonus: 7))#\n"
            "pub decoratedBonusNamed(): int {\n"
            "    var meta = %type(decoratedBonusNamed).metadata;\n"
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
                "%module \"main\";\n"
                "var decorated = %import(\"decorated_user\");\n"
                "%test(\"test\") {\n"
                "    return decorated.decoratedBonusDefault() + decorated.decoratedBonusNamed();\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
            "%module \"provider\";\n"
            "%compileTime var BASE = 5;\n"
            "%compileTime compute(seed: int, bonus: int = BASE, factor: int = 2): int {\n"
            "    return seed * factor + bonus;\n"
            "}\n";
    static const TZrChar* decoratedUserSource =
            "%module \"decorated_user\";\n"
            "var provider = %import(\"provider\");\n"
            "%compileTime markFunction(target: %type Function, bonus: int = 0) {\n"
            "    return { metadata: { instrumented: bonus } };\n"
            "}\n"
            "#markFunction(bonus: compute(seed: 10, factor: 3))#\n"
            "pub decoratedBonusDefault(): int {\n"
            "    var meta = %type(decoratedBonusDefault).metadata;\n"
            "    return meta.instrumented;\n"
            "}\n"
            "#markFunction(bonus: compute(10, bonus: 7))#\n"
            "pub decoratedBonusNamed(): int {\n"
            "    var meta = %type(decoratedBonusNamed).metadata;\n"
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
                "%module \"main\";\n"
                "var decorated = %import(\"decorated_user\");\n"
                "%test(\"test\") {\n"
                "    return decorated.decoratedBonusDefault() + decorated.decoratedBonusNamed();\n"
                "}\n";
        SZrString* sourceName;
        SZrAstNode* ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 62, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
                    "%module \"decorators\";\n"
                    "%compileTime class Serializable {\n"
                    "    @decorate(target: %type Class): DecoratorPatch {\n"
                    "        return { metadata: { serializable: true } };\n"
                    "    }\n"
                    "}\n"
                    "%compileTime markFunction(target: %type Function, bonus: int = 16) {\n"
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
                "%module \"test\";\n"
                "var decorators = %import(\"decorators\");\n"
                "#decorators.markFunction(bonus: 28)#\n"
                "pub decorated(): int {\n"
                "    var info = %type(decorated);\n"
                "    return info.metadata.instrumented;\n"
                "}\n"
                "%test(\"test\") {\n"
                "    return decorated();\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName = ZrCore_String_Create(state, "test_compile_time_imported_decorator_member_chain.zr", 54);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        state->global->sourceLoader = ZR_NULL;
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 28, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
                    "%module \"decorators\";\n"
                    "%compileTime markFunction(target: %type Function, bonus: int = 16) {\n"
                    "    return { metadata: { instrumented: bonus } };\n"
                    "}\n"
                    "%compileTime var registry = {\n"
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
                "%module \"test\";\n"
                "var decorators = %import(\"decorators\");\n"
                "#decorators.registry.nested.mark(bonus: 33)#\n"
                "pub decorated(): int {\n"
                "    var info = %type(decorated);\n"
                "    return info.metadata.instrumented;\n"
                "}\n"
                "%test(\"test\") {\n"
                "    return decorated();\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        ZrParser_ToGlobalState_Register(state);
        gCompileTimeImportFixtures = fixtures;
        gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
        state->global->sourceLoader = compile_time_import_source_loader;

        sourceName =
                ZrCore_String_Create(state, "test_compile_time_imported_decorator_deep_member_chain.zr", 59);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        state->global->sourceLoader = ZR_NULL;
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 33, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
            "%module \"decorators\";\n"
            "%compileTime markFunction(target: %type Function, bonus: int = 16) {\n"
            "    return { metadata: { instrumented: bonus } };\n"
            "}\n"
            "%compileTime var registry = {\n"
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
                "%module \"test\";\n"
                "var decorators = %import(\"decorators\");\n"
                "#decorators.registry.nested.mark(bonus: 41)#\n"
                "pub decorated(): int {\n"
                "    var info = %type(decorated);\n"
                "    return info.metadata.instrumented;\n"
                "}\n"
                "%test(\"test\") {\n"
                "    return decorated();\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        state->global->sourceLoader = ZR_NULL;
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 41, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
                "%module \"test\";\n"
                "%compileTime markFunction(target: %type Function, bonus: int = 11) {\n"
                "    return { metadata: { instrumented: bonus } };\n"
                "}\n"
                "%compileTime var decorators = { markFunction: markFunction };\n"
                "#decorators.markFunction(bonus: 17)#\n"
                "pub decorated(): int {\n"
                "    var info = %type(decorated);\n"
                "    return info.metadata.instrumented;\n"
                "}\n"
                "%test(\"test\") {\n"
                "    return decorated();\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "test_compile_time_object_decorator_member_chain.zr", 52);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 17, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
                "%module \"test\";\n"
                "%compileTime mark(target): void {\n"
                "    target.metadata.instrumented = true;\n"
                "}\n"
                "%compileTime var target = { metadata: {} };\n"
                "%compileTime {\n"
                "    mark(target);\n"
                "}\n"
                "var runtimeValue = target.metadata.instrumented ? 1 : 0;\n"
                "%test(\"test\") {\n"
                "    return runtimeValue;\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompileResult compileResult;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZrCore_String_Create(state, "test_compile_time_object_member_assignment.zr", 46);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 1, testSummary));

        ZrParser_CompileResult_Free(state, &compileResult);
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
                    "%module \"compile_time_class_decorator_reflection_fixture\";\n"
                    "%compileTime class Serializable {\n"
                    "    @decorate(target: %type Class): DecoratorPatch {\n"
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
                "%module \"test\";\n"
                "var decorated = %import(\"compile_time_class_decorator_reflection_fixture\");\n"
                "%test(\"test\") {\n"
                "    var info = %type(decorated.User);\n"
                "    if (info.metadata == null) {\n"
                "        return 0;\n"
                "    }\n"
                "    return info.metadata.serializable ? 1 : 0;\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 1, testSummary));

        state->global->sourceLoader = ZR_NULL;
        ZrParser_CompileResult_Free(state, &compileResult);
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
                    "%module \"compile_time_function_decorator_reflection_fixture\";\n"
                    "%compileTime decorate(target: %type Class, version: int = 7) {\n"
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
                "%module \"test\";\n"
                "var decorated = %import(\"compile_time_function_decorator_reflection_fixture\");\n"
                "%test(\"test\") {\n"
                "    var userInfo = %type(decorated.User);\n"
                "    var adminInfo = %type(decorated.Admin);\n"
                "    return userInfo.metadata.version + adminInfo.metadata.version;\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 18, testSummary));

        state->global->sourceLoader = ZR_NULL;
        ZrParser_CompileResult_Free(state, &compileResult);
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
                    "%module \"compile_time_struct_decorator_reflection_fixture\";\n"
                    "%compileTime struct Packed {\n"
                    "    @decorate(target: %type Struct): DecoratorPatch {\n"
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
                "%module \"test\";\n"
                "var decorated = %import(\"compile_time_struct_decorator_reflection_fixture\");\n"
                "%test(\"test\") {\n"
                "    var info = %type(decorated.Packet);\n"
                "    if (info.metadata == null) {\n"
                "        return 0;\n"
                "    }\n"
                "    return info.metadata.packed ? 1 : 0;\n"
                "}\n";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrCompileResult compileResult;

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
        TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
        TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
        reset_loaded_module_registry(state);
        TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 1, testSummary));

        state->global->sourceLoader = ZR_NULL;
        ZrParser_CompileResult_Free(state, &compileResult);
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// 主函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    printf("Compile-Time Execution Tests\n");
    TEST_MODULE_DIVIDER();
    
    RUN_TEST(test_compile_time_variables);
    RUN_TEST(test_compile_time_functions);
    RUN_TEST(test_compile_time_expressions);
    RUN_TEST(test_compile_time_recursion);
    RUN_TEST(test_compile_time_statements);
    RUN_TEST(test_compile_time_array_validation);
    RUN_TEST(test_compile_time_function_projection_to_runtime);
    RUN_TEST(test_compile_time_block_persistent_registration);
    RUN_TEST(test_compile_time_named_and_default_argument_projection);
    RUN_TEST(test_compile_time_block_forward_reference_diagnostic);
    RUN_TEST(test_compile_time_duplicate_declaration_override);
    RUN_TEST(test_compile_time_member_call_projection);
    RUN_TEST(test_compile_time_import_member_call_projection);
    RUN_TEST(test_compile_time_import_runtime_callable_named_default_projection);
    RUN_TEST(test_compile_time_projection_rejects_function_ref_leak);
    RUN_TEST(test_compile_time_import_deep_member_call_projection);
    RUN_TEST(test_compile_time_binary_import_function_alias_projection);
    RUN_TEST(test_compile_time_binary_import_named_and_default_argument_projection);
    RUN_TEST(test_compile_time_binary_import_runtime_callable_named_default_projection);
    RUN_TEST(test_compile_time_binary_import_named_default_arguments_inside_function_decorator);
    RUN_TEST(test_compile_time_binary_import_named_default_arguments_inside_imported_module_decorator);
    RUN_TEST(test_compile_time_binary_import_named_default_arguments_inside_imported_module_decorator_via_file_loader_without_intermediate_sidecar);
    RUN_TEST(test_compile_time_imported_decorator_member_chain);
    RUN_TEST(test_compile_time_imported_decorator_deep_member_chain);
    RUN_TEST(test_compile_time_binary_imported_decorator_deep_member_chain);
    RUN_TEST(test_compile_time_object_decorator_member_chain);
    RUN_TEST(test_compile_time_object_member_assignment_projects_mutation);
    RUN_TEST(test_compile_time_class_decorator_projects_metadata_to_runtime_reflection);
    RUN_TEST(test_compile_time_function_decorator_projects_metadata_to_runtime_reflection);
    RUN_TEST(test_compile_time_struct_decorator_projects_metadata_to_runtime_reflection);
    
    return UNITY_END();
}

