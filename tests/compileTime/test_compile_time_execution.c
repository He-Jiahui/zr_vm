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
#include "zr_vm_core/gc.h"
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
#include "zr_vm_parser/declaration_transform_contract.h"
#include "zr_vm_core/reflection.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_attributes.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_interfaces.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_transaction.h"
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

static void test_pub_comptime_fn_projects_through_direct_source_import(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "sizing",
                    "module sizing;\n"
                    "pub comptime fn staticPlanSize(seed: int, factor: int): int {\n"
                    "    return seed * factor + 3;\n"
                    "}\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };
    static const TZrChar *source =
            "var staged: int[import(\"sizing\").staticPlanSize(seed: 4, factor: 2)] = "
            "[1,2,3,4,5,6,7,8,9,10,11];\n"
            "return staged.length;\n";
    const SZrCompileTimeImportFixture *previousFixtures = gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    gCompileTimeImportFixtures = fixtures;
    gCompileTimeImportFixtureCount = sizeof(fixtures) / sizeof(fixtures[0]);
    state->global->sourceLoader = compile_time_import_source_loader;

    sourceName = ZrCore_String_CreateFromNative(
            state, "pub_comptime_direct_source_import.zr");
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    state->global->sourceLoader = ZR_NULL;
    TEST_ASSERT_TRUE(execute_test_function(
            state,
            function,
            11,
            "public comptime direct source import"));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
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

static TZrSize count_call_instructions(const SZrFunction *function) {
    TZrSize count = 0;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0;
    }
    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        EZrInstructionCode opcode =
                (EZrInstructionCode)ZR_INSTRUCTION_OPCODE(
                        function->instructionsList[index]);
        if (opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(DYN_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(META_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(META_TAIL_CALL)) {
            count++;
        }
    }
    return count;
}

static void test_conditional_disabled_direct_void_call_elides_argument_lowering(void) {
    static const TZrChar *projectJson =
            "{\"name\":\"conditional-test\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main.zr\","
            "\"features\":{\"trace\":false}}";
    static const TZrChar *source =
            "fn expensive(): int { return 99; }\n"
            "#zr.compile.conditional(\"trace\")#\n"
            "fn trace(value: int): void { }\n"
            "fn __fixture(): int { trace(expensive()); return 42; }\n"
            "return __fixture();\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    TZrPtr previousUserData;
    SZrString *sourceName;
    SZrFunction *function;
    SZrFunction *fixture;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(
            state, (TZrNativeString)projectJson, "E:/tmp/conditional-test.zrp");
    TEST_ASSERT_NOT_NULL(project);
    previousUserData = state->global->userData;
    state->global->userData = project;
    sourceName = ZrCore_String_CreateFromNative(state, "conditional_disabled.zr");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    fixture = find_fixture_function(function);
    TEST_ASSERT_NOT_NULL(fixture);
    TEST_ASSERT_EQUAL_UINT64(0, count_call_instructions(fixture));
    TEST_ASSERT_TRUE(execute_test_function(
            state, function, 42, "conditional disabled call"));

    ZrCore_Function_Free(state, function);
    state->global->userData = previousUserData;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_conditional_disabled_call_still_type_checks_arguments(void) {
    static const TZrChar *projectJson =
            "{\"name\":\"conditional-negative\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main.zr\","
            "\"features\":{\"trace\":false}}";
    static const TZrChar *source =
            "#zr.compile.conditional(\"trace\")#\n"
            "fn trace(value: int): void { }\n"
            "trace(\"not-an-int\");\n";
    SZrState *state = create_test_state();
    SZrLibrary_Project *project;
    TZrPtr previousUserData;
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(
            state, (TZrNativeString)projectJson, "E:/tmp/conditional-negative.zrp");
    TEST_ASSERT_NOT_NULL(project);
    previousUserData = state->global->userData;
    state->global->userData = project;
    sourceName = ZrCore_String_CreateFromNative(state, "conditional_type_error.zr");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NULL(function);

    state->global->userData = previousUserData;
    ZrLibrary_Project_Free(state, project);
    destroy_test_state(state);
}

static void test_readonly_struct_attribute_schema_binds_typed_field_metadata(void) {
    static const TZrChar *source =
            "#zr.reflection.attributeUsage(\n"
            "    targets: zr.reflection.AttributeTargets.field,\n"
            "    retention: zr.reflection.AttributeRetention.runtime,\n"
            "    repeatable: false, inherited: false)#\n"
            "pub readonly struct Range { pub let min: int; pub let max: int; }\n"
            "class Meter {\n"
            "    #Range(min: 0, max: 100)#\n"
            "    pub var value: int;\n"
            "}\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "typed_attribute_schema.zr");
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_attribute_schema_rejects_invalid_shape_and_application(void) {
    static const TZrChar *sources[] = {
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.field, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: false, inherited: false)#\n"
            "pub struct MutableSchema { pub let value: int; }\n"
            "return 0;\n",
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.field, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: false, inherited: false)#\n"
            "pub readonly struct PrivateSchema { pri let value: int; }\n"
            "return 0;\n",
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.field, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: false, inherited: false)#\n"
            "pub readonly struct Count { pub let value: int; }\n"
            "class InvalidUse { #Count(value: \"wrong\")# pub var value: int; }\n"
            "return 0;\n",
    };

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(state, "invalid_attribute_schema.zr");
        function = ZrParser_Source_Compile(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NULL(function);
        destroy_test_state(state);
    }
}

static void test_declaration_transform_accepts_typed_empty_patch_once(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn audit(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#audit#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_empty_patch.zr");
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_declaration_transform_generated_field_enters_normal_layout(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn deriveMarker(target: declaration.Struct): declaration.Patch {\n"
            "    let marker = init declaration.GeneratedField(\n"
            "        name: \"generatedEquality\",\n"
            "        type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let\n"
            "    );\n"
            "    return init declaration.Patch(target: target.symbolId, additions: [marker]);\n"
            "}\n"
            "#deriveMarker#\n"
            "pub struct Meter { pub let value: int; }\n"
            "fn __fixture(): int {\n"
            "    let meter = init Meter();\n"
            "    return meter.generatedEquality ? 7 : 0;\n"
            "}\n"
            "return __fixture();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_generated_field.zr");
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(
            state, function, 0, "declaration transform generated field"));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_declaration_transform_interface_add_enters_normal_contract(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Class): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "class Device { pub fn read(): int { return 7; } }\n"
            "fn __fixture(): int { let device = new Device(); return device.read(); }\n"
            "return __fixture();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_interface_add.zr");
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(
            state, function, 7, "declaration transform interface add"));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_declaration_transform_interface_add_validates_value_type_contract(void) {
    static const TZrChar *sources[] = {
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "struct Reader { pub fn read(): int { return 3; } }\n"
            "return 0;\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface BaseReadable { fn baseRead(): int; }\n"
            "interface Readable : BaseReadable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "struct Reader {\n"
            "    pub fn baseRead(): int { return 2; }\n"
            "    pub fn read(): int { return 3; }\n"
            "}\n"
            "return 0;\n",
    };

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "declaration_transform_struct_interface.zr");
        function = ZrParser_Source_Compile(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NOT_NULL(function);

        ZrCore_Function_Free(state, function);
        destroy_test_state(state);
    }
}

static void test_declaration_transform_interface_add_validates_imported_parent_after_growth(void) {
    static const SZrCompileTimeImportFixture fixtures[] = {
            {
                    "contracts",
                    "module contracts;\n"
                    "pub interface Filler0 { }\n"
                    "pub interface Filler1 { }\n"
                    "pub interface Filler2 { }\n"
                    "pub interface Filler3 { }\n"
                    "pub interface Filler4 { }\n"
                    "pub interface Filler5 { }\n"
                    "pub interface Filler6 { }\n"
                    "pub interface Filler7 { }\n"
                    "pub interface ParentReadable { fn baseRead(): int; }\n",
                    ZR_NULL,
                    0,
                    ZR_FALSE,
            },
    };
    static const TZrChar *source =
            "let contracts = import(\"contracts\");\n"
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable : contracts.ParentReadable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "struct Reader {\n"
            "    pub fn baseRead(): int { return 2; }\n"
            "    pub fn read(): int { return 3; }\n"
            "}\n"
            "return 0;\n";
    const SZrCompileTimeImportFixture *previousFixtures =
            gCompileTimeImportFixtures;
    TZrSize previousFixtureCount = gCompileTimeImportFixtureCount;
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);
    gCompileTimeImportFixtures = fixtures;
    gCompileTimeImportFixtureCount = ZR_ARRAY_COUNT(fixtures);
    state->global->sourceLoader = compile_time_import_source_loader;
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_imported_parent_interface.zr");
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    reset_loaded_module_registry(state);
    state->global->sourceLoader = ZR_NULL;
    destroy_test_state(state);
    gCompileTimeImportFixtures = previousFixtures;
    gCompileTimeImportFixtureCount = previousFixtureCount;
}

static void test_declaration_transform_interface_add_resolves_later_signature(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Class): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "class Device { pub fn read(): int { return 7; } }\n"
            "interface Readable { fn read(): int; }\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_later_interface.zr");
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static SZrTypePrototypeInfo *compile_time_test_find_prototype(
        SZrCompilerState *cs,
        const TZrChar *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &cs->typePrototypes, index);
        const TZrChar *prototypeName =
                prototype != ZR_NULL && prototype->name != ZR_NULL
                        ? ZrCore_String_GetNativeString(prototype->name)
                        : ZR_NULL;
        if (prototypeName != ZR_NULL && strcmp(prototypeName, name) == 0) {
            return prototype;
        }
    }
    return ZR_NULL;
}

static TZrBool compile_time_test_name_array_contains(
        const SZrArray *array,
        const TZrChar *name) {
    if (array == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < array->length; index++) {
        SZrString **candidate =
                (SZrString **)ZrCore_Array_Get((SZrArray *)array, index);
        const TZrChar *candidateName =
                candidate != ZR_NULL && *candidate != ZR_NULL
                        ? ZrCore_String_GetNativeString(*candidate)
                        : ZR_NULL;
        if (candidateName != ZR_NULL && strcmp(candidateName, name) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void test_declaration_transform_interface_add_binds_prototype_and_contract_slot(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Class): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "class Device { pub fn read(): int { return 7; } }\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;
    SZrTypePrototypeInfo *device;
    SZrTypeMemberInfo *readMember = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_interface_binding.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, state);
    cs.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(
            ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
    for (TZrSize index = 0; index < ast->data.script.statements->count; index++) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement != ZR_NULL && statement->type == ZR_AST_INTERFACE_DECLARATION) {
            ZrParser_Compiler_CompileInterfaceDeclaration(&cs, statement);
        } else if (statement != ZR_NULL &&
                   statement->type == ZR_AST_CLASS_DECLARATION) {
            ZrParser_Compiler_CompileClassDeclaration(&cs, statement);
        }
    }
    TEST_ASSERT_FALSE(cs.hasError);
    device = compile_time_test_find_prototype(&cs, "Device");
    TEST_ASSERT_NOT_NULL(device);
    TEST_ASSERT_TRUE(compile_time_test_name_array_contains(
            &device->inherits, "Readable"));
    TEST_ASSERT_TRUE(compile_time_test_name_array_contains(
            &device->implements, "Readable"));
    for (TZrSize index = 0; index < device->members.length; index++) {
        SZrTypeMemberInfo *member =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(&device->members, index);
        const TZrChar *memberName =
                member != ZR_NULL && member->name != ZR_NULL
                        ? ZrCore_String_GetNativeString(member->name)
                        : ZR_NULL;
        if (memberName != ZR_NULL && strcmp(memberName, "read") == 0) {
            readMember = member;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(readMember);
    TEST_ASSERT_NOT_EQUAL_UINT32(UINT32_MAX, readMember->interfaceContractSlot);

    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_declaration_transform_interface_add_binds_value_type_contract_slot(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "struct Reader { pub fn read(): int { return 3; } }\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;
    SZrTypePrototypeInfo *reader;
    SZrTypeMemberInfo *readMember = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_struct_interface_binding.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, state);
    cs.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(
            ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
    for (TZrSize index = 0; index < ast->data.script.statements->count; index++) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement != ZR_NULL &&
            statement->type == ZR_AST_INTERFACE_DECLARATION) {
            ZrParser_Compiler_CompileInterfaceDeclaration(&cs, statement);
        } else if (statement != ZR_NULL &&
                   statement->type == ZR_AST_STRUCT_DECLARATION) {
            ZrParser_Compiler_CompileStructDeclaration(&cs, statement);
        }
    }
    TEST_ASSERT_FALSE(cs.hasError);
    reader = compile_time_test_find_prototype(&cs, "Reader");
    TEST_ASSERT_NOT_NULL(reader);
    TEST_ASSERT_TRUE(compile_time_test_name_array_contains(
            &reader->inherits, "Readable"));
    TEST_ASSERT_TRUE(compile_time_test_name_array_contains(
            &reader->implements, "Readable"));
    for (TZrSize index = 0; index < reader->members.length; index++) {
        SZrTypeMemberInfo *member =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(&reader->members, index);
        const TZrChar *memberName =
                member != ZR_NULL && member->name != ZR_NULL
                        ? ZrCore_String_GetNativeString(member->name)
                        : ZR_NULL;
        if (memberName != ZR_NULL && strcmp(memberName, "read") == 0) {
            readMember = member;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(readMember);
    TEST_ASSERT_NOT_EQUAL_UINT32(
            UINT32_MAX, readMember->interfaceContractSlot);

    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_declaration_transform_interface_add_runs_normal_requirement_validation(void) {
    static const TZrChar *sources[] = {
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Class): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "class Missing { }\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Versioned { pub const version: int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addVersioned(target: declaration.Class): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Versioned)]\n"
            "    );\n"
            "}\n"
            "#addVersioned#\n"
            "class MutableVersion { pub var version: int; }\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "struct MissingValue { }\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface BaseReadable { fn baseRead(): int; }\n"
            "interface Readable : BaseReadable { fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "struct MissingBase { pub fn read(): int { return 3; } }\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable { pub const fn read(): int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addReadable(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Readable)]\n"
            "    );\n"
            "}\n"
            "#addReadable#\n"
            "struct WritableReader { pub fn read(): int { return 3; } }\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Versioned { pub const version: int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addVersioned(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Versioned)]\n"
            "    );\n"
            "}\n"
            "#addVersioned#\n"
            "struct MutableVersion { pub var version: int; }\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "class NotInterface { }\n"
            "interface Broken : NotInterface { }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addBroken(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        interfaceAdds: [typeid(Broken)]\n"
            "    );\n"
            "}\n"
            "#addBroken#\n"
            "struct InvalidParent { }\n",
    };

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "declaration_transform_interface_requirement.zr");
        function = ZrParser_Source_Compile(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NULL(function);

        destroy_test_state(state);
    }
}

static void test_declaration_transform_interface_add_rejects_invalid_entries(void) {
    static const TZrChar *sources[] = {
            "let declaration = import(\"zr.compile.declaration\");\n"
            "class NotInterface { }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Class): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId, interfaceAdds: [typeid(NotInterface)]);\n"
            "}\n#invalid#\nclass Device { }\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "interface Readable { }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Class): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId, interfaceAdds: [typeid(Readable)]);\n"
            "}\n#invalid#\nclass Device : Readable { }\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Class): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId, interfaceAdds: [1]);\n"
            "}\n#invalid#\nclass Device { }\n",
    };

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "declaration_transform_invalid_interface_add.zr");
        function = ZrParser_Source_Compile(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NULL(function);
        destroy_test_state(state);
    }
}

static void test_declaration_transform_interface_add_rejects_alias_identity_duplicate(void) {
    static const TZrChar *source = "interface Readable { }\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrString *readableName;
    SZrString *aliasName;
    SZrAstNode *ast;
    SZrCompilerState cs;
    SZrTypeBinding aliasBinding;
    SZrTypePrototypeInfo targetInfo;
    SZrReflectionTypeIdentity identity = {0};
    SZrObject *typeIdObject;
    SZrObject *arrayObject;
    SZrTypeValue typeIdValue;
    SZrTypeValue arrayValue;
    SZrTypeValue key;
    SZrParserCompileTimePatchInterfaceAdds prepared;
    TZrBool prepareResult;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_interface_alias_duplicate.zr");
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, state);
    cs.suppressErrorOutput = ZR_TRUE;
    ZrParser_Compiler_CompileInterfaceDeclaration(
            &cs, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs.hasError);

    readableName = ZrCore_String_CreateFromNative(state, "Readable");
    aliasName = ZrCore_String_CreateFromNative(state, "ReadableAlias");
    TEST_ASSERT_NOT_NULL(readableName);
    TEST_ASSERT_NOT_NULL(aliasName);
    aliasBinding.name = aliasName;
    ZrParser_InferredType_InitFull(
            state,
            &aliasBinding.type,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            readableName);
    ZrCore_Array_Push(state, &cs.typeValueAliases, &aliasBinding);

    ZrCore_Memory_RawSet(&targetInfo, 0, sizeof(targetInfo));
    ZrCore_Array_Init(
            state, &targetInfo.inherits, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.implements, sizeof(SZrString *), 1U);
    ZrCore_Array_Push(state, &targetInfo.inherits, &aliasName);

    identity.canonicalTypeId = 71U;
    identity.typeToken = 17U;
    identity.metadataGeneration = 1U;
    identity.category = ZR_REFLECTION_TYPE_CATEGORY_INTERFACE;
    typeIdObject = ZrCore_Reflection_BuildTypeIdObject(
            state, readableName, &identity);
    TEST_ASSERT_NOT_NULL(typeIdObject);
    ZrCore_Value_InitAsRawObject(
            state, &typeIdValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeIdObject));
    typeIdValue.type = ZR_VALUE_TYPE_OBJECT;
    arrayObject = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Object_Init(state, arrayObject);
    ZrCore_Value_InitAsInt(state, &key, 0);
    ZrCore_Object_SetValue(state, arrayObject, &key, &typeIdValue);
    ZrCore_Value_InitAsRawObject(
            state, &arrayValue, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    arrayValue.type = ZR_VALUE_TYPE_ARRAY;

    ZrCore_Memory_RawSet(&prepared, 0, sizeof(prepared));
    prepareResult = ZrParser_CompileTime_PreparePatchInterfaceAdds(
            &cs, &targetInfo, &arrayValue, ast->location, &prepared);
    ZrParser_CompileTime_FreePatchInterfaceAdds(&cs, &prepared);
    TEST_ASSERT_FALSE(prepareResult);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_EQUAL_STRING(
            "declaration_transform.interface_add: duplicate interface",
            cs.errorMessage);

    ZrCore_Array_Free(state, &targetInfo.inherits);
    ZrCore_Array_Free(state, &targetInfo.implements);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_declaration_transform_typed_warning_is_nonfatal(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn deriveMarker(target: declaration.Struct): declaration.Patch {\n"
            "    let marker = init declaration.GeneratedField(\n"
            "        name: \"generatedEquality\",\n"
            "        type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let\n"
            "    );\n"
            "    let warning = init declaration.CompileDiagnostic(\n"
            "        isError: false,\n"
            "        message: \"generated marker\",\n"
            "        target: target.symbolId\n"
            "    );\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        additions: [marker],\n"
            "        diagnostics: [warning]\n"
            "    );\n"
            "}\n"
            "#deriveMarker#\n"
            "pub struct Meter { pub let value: int; }\n"
            "fn __fixture(): int {\n"
            "    let meter = init Meter();\n"
            "    return meter.generatedEquality ? 7 : 0;\n"
            "}\n"
            "return __fixture();\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_typed_warning.zr");
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(execute_test_function(
            state, function, 0, "declaration transform typed warning"));

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_declaration_transform_typed_error_rejects_patch(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn rejectMarker(target: declaration.Struct): declaration.Patch {\n"
            "    let marker = init declaration.GeneratedField(\n"
            "        name: \"mustNotBind\",\n"
            "        type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let\n"
            "    );\n"
            "    let error = init declaration.CompileDiagnostic(\n"
            "        isError: true,\n"
            "        message: \"reject generated marker\",\n"
            "        target: target.symbolId\n"
            "    );\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        additions: [marker],\n"
            "        diagnostics: [error]\n"
            "    );\n"
            "}\n"
            "#rejectMarker#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_typed_error.zr");
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NULL(function);

    destroy_test_state(state);
}

static void test_declaration_transform_error_prevents_generated_member_registration(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn rejectMarker(target: declaration.Struct): declaration.Patch {\n"
            "    let marker = init declaration.GeneratedField(\n"
            "        name: \"mustNotBind\",\n"
            "        type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let\n"
            "    );\n"
            "    let error = init declaration.CompileDiagnostic(\n"
            "        isError: true,\n"
            "        message: \"reject before append\",\n"
            "        target: target.symbolId\n"
            "    );\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        additions: [marker],\n"
            "        diagnostics: [error]\n"
            "    );\n"
            "}\n"
            "#rejectMarker#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrString *generatedMemberName;
    SZrAstNode *ast;
    SZrAstNode *structNode = ZR_NULL;
    SZrCompilerState cs;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_error_before_append.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, state);
    cs.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(
            ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
    for (TZrSize index = 0;
         index < ast->data.script.statements->count;
         index++) {
        if (ast->data.script.statements->nodes[index] != ZR_NULL &&
            ast->data.script.statements->nodes[index]->type ==
                    ZR_AST_STRUCT_DECLARATION) {
            structNode = ast->data.script.statements->nodes[index];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(structNode);

    ZrParser_Compiler_CompileStructDeclaration(&cs, structNode);
    TEST_ASSERT_TRUE(cs.hasCompileTimeError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_EQUAL_STRING("reject before append", cs.errorMessage);
    generatedMemberName = ZrCore_String_CreateFromNative(state, "mustNotBind");
    TEST_ASSERT_NOT_NULL(generatedMemberName);
    TEST_ASSERT_NULL(ZrParser_Semantic_FindSymbolByNameAndKind(
            cs.semanticContext,
            generatedMemberName,
            ZR_SEMANTIC_SYMBOL_KIND_FIELD));

    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_declaration_transform_invalid_multi_add_commits_nothing(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn duplicate(target: declaration.Struct): declaration.Patch {\n"
            "    let first = init declaration.GeneratedField(\n"
            "        name: \"mustNotBind\", type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let);\n"
            "    let second = init declaration.GeneratedField(\n"
            "        name: \"mustNotBind\", type: typeid(int),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let);\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId, additions: [first, second]);\n"
            "}\n"
            "#duplicate#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrString *generatedMemberName;
    SZrAstNode *ast;
    SZrAstNode *structNode = ZR_NULL;
    SZrCompilerState cs;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_invalid_multi_add.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    ZrParser_CompilerState_Init(&cs, state);
    cs.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(
            ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast));
    for (TZrSize index = 0;
         index < ast->data.script.statements->count;
         index++) {
        if (ast->data.script.statements->nodes[index] != ZR_NULL &&
            ast->data.script.statements->nodes[index]->type ==
                    ZR_AST_STRUCT_DECLARATION) {
            structNode = ast->data.script.statements->nodes[index];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(structNode);

    ZrParser_Compiler_CompileStructDeclaration(&cs, structNode);
    TEST_ASSERT_TRUE(cs.hasCompileTimeError);
    generatedMemberName = ZrCore_String_CreateFromNative(state, "mustNotBind");
    TEST_ASSERT_NOT_NULL(generatedMemberName);
    TEST_ASSERT_NULL(ZrParser_Semantic_FindSymbolByNameAndKind(
            cs.semanticContext,
            generatedMemberName,
            ZR_SEMANTIC_SYMBOL_KIND_FIELD));

    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

#include "test_compile_time_declaration_patch_transaction_cases.h"
#include "test_compile_time_declaration_patch_transaction_hash_cases.h"

static void test_declaration_transform_patch_target_rejects_uint32_wraparound(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId + 4294967296\n"
            "    );\n"
            "}\n"
            "#invalid#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_target_wraparound.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
    TEST_ASSERT_NULL(function);

    destroy_test_state(state);
}

static void test_declaration_transform_diagnostic_constructor_rejects_invalid_fields(void) {
    static const TZrChar *sources[] = {
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Struct): declaration.Patch {\n"
            "    let bad = init declaration.CompileDiagnostic(isError: 1, message: \"x\", target: target.symbolId);\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n#invalid#\npub struct Meter { pub let value: int; }\nreturn 0;\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Struct): declaration.Patch {\n"
            "    let bad = init declaration.CompileDiagnostic(isError: false, message: false, target: target.symbolId);\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n#invalid#\npub struct Meter { pub let value: int; }\nreturn 0;\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Struct): declaration.Patch {\n"
            "    let bad = init declaration.CompileDiagnostic(isError: false, message: \"x\", target: \"wrong\");\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n#invalid#\npub struct Meter { pub let value: int; }\nreturn 0;\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Struct): declaration.Patch {\n"
            "    let bad = init declaration.CompileDiagnostic(isError: false, message: \"\", target: target.symbolId);\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n#invalid#\npub struct Meter { pub let value: int; }\nreturn 0;\n",
    };

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "declaration_transform_invalid_diagnostic.zr");
        function = ZrParser_Source_Compile(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NULL(function);
        destroy_test_state(state);
    }
}

static void test_declaration_transform_rejects_invalid_signature_and_patch_shape(void) {
    static const TZrChar *sources[] = {
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: int): declaration.Patch {\n"
            "    return init declaration.Patch(target: 1);\n"
            "}\nreturn 0;\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn invalid(target: declaration.Struct): declaration.Patch {\n"
            "    return { metadata: {} };\n"
            "}\n#invalid#\npub struct Meter { pub let value: int; }\nreturn 0;\n",
    };

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "declaration_transform_invalid.zr");
        function = ZrParser_Source_Compile(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NULL(function);
        destroy_test_state(state);
    }
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

static void test_declaration_transform_attribute_adds_use_registered_schema(void) {
    static const TZrChar *validSource =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.type, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: false, inherited: false)#\n"
            "pub readonly struct GeneratedLabel { pub let value: int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addLabel(target: declaration.Struct): declaration.Patch {\n"
            "    let label = init declaration.AttributeData(\n"
            "        typeId: typeid(GeneratedLabel),\n"
            "        fieldValues: [7]\n"
            "    );\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId,\n"
            "        attributeAdds: [label]\n"
            "    );\n"
            "}\n"
            "#addLabel#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    static const TZrChar *invalidSources[] = {
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.type, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: false, inherited: false)#\n"
            "pub readonly struct GeneratedLabel { pub let value: int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addLabel(target: declaration.Struct): declaration.Patch {\n"
            "    let label = init declaration.AttributeData("
            "typeId: typeid(GeneratedLabel), fieldValues: [\"wrong\"]);\n"
            "    return init declaration.Patch(target: target.symbolId, attributeAdds: [label]);\n"
            "}\n#addLabel#\npub struct Meter { pub let value: int; }\nreturn 0;\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.type, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: false, inherited: false)#\n"
            "pub readonly struct GeneratedLabel { pub let value: int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addLabel(target: declaration.Struct): declaration.Patch {\n"
            "    let label = init declaration.AttributeData("
            "typeId: typeid(GeneratedLabel), fieldValues: [7]);\n"
            "    return init declaration.Patch("
            "target: target.symbolId, attributeAdds: [label, label]);\n"
            "}\n#addLabel#\npub struct Meter { pub let value: int; }\nreturn 0;\n",
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.reflection.attributeUsage("
            "targets: zr.reflection.AttributeTargets.field, "
            "retention: zr.reflection.AttributeRetention.artifact, "
            "repeatable: true, inherited: false)#\n"
            "pub readonly struct FieldOnly { pub let value: int; }\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn addLabel(target: declaration.Struct): declaration.Patch {\n"
            "    let label = init declaration.AttributeData("
            "typeId: typeid(FieldOnly), fieldValues: [7]);\n"
            "    return init declaration.Patch(target: target.symbolId, attributeAdds: [label]);\n"
            "}\n#addLabel#\npub struct Meter { pub let value: int; }\nreturn 0;\n",
    };
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(
            state, "declaration_transform_attribute_add.zr");
    function = ZrParser_Source_Compile(
            state, validSource, strlen(validSource), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Function_Free(state, function);
    destroy_test_state(state);

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(invalidSources); index++) {
        state = create_test_state();
        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "declaration_transform_invalid_attribute_add.zr");
        function = ZrParser_Source_Compile(
                state,
                invalidSources[index],
                strlen(invalidSources[index]),
                sourceName);
        TEST_ASSERT_NULL(function);
        destroy_test_state(state);
    }
}

static void test_static_decorator_gate_covers_all_declaration_shapes(void) {
    static const TZrChar *invalidSources[] = {
            "fn legacy(target: object): object { return target; }\n"
            "#legacy#\nunion Choice { None; }\nreturn 0;\n",
            "fn legacy(target: object): object { return target; }\n"
            "union Choice { #legacy# None; }\nreturn 0;\n",
            "fn legacy(target: object): object { return target; }\n"
            "union Choice { Some(#legacy# value: int); }\nreturn 0;\n",
            "fn legacy(target: object): object { return target; }\n"
            "interface Service { fn run(#legacy# value: int): int; }\nreturn 0;\n",
            "fn legacy(target: object): object { return target; }\n"
            "interface Service { @constructor(#legacy# value: int): Service; }\nreturn 0;\n",
            "fn legacy(target: object): object { return target; }\n"
            "enum Mode { #legacy# Active; }\nreturn 0;\n",
            "#zr.ffi.underlying(\"i32\")#\n"
            "enum Mode { Active; }\nreturn 0;\n",
            "enum Mode { #zr.ffi.value(7)# Active; }\nreturn 0;\n",
            "native extern(\"fixture\") {\n"
            "  #legacy# fn read(): i32;\n}\nreturn 0;\n",
            "native extern(\"fixture\") {\n"
            "  #legacy# delegate Callback(value: i32): i32;\n}\nreturn 0;\n",
            "native extern(\"fixture\") {\n"
            "  #legacy# struct Pair { var value: i32; }\n}\nreturn 0;\n",
            "native extern(\"fixture\") {\n"
            "  struct Pair { #legacy# var value: i32; }\n}\nreturn 0;\n",
            "native extern(\"fixture\") {\n"
            "  #legacy# enum Mode { Active; }\n}\nreturn 0;\n",
            "native extern(\"fixture\") {\n"
            "  enum Mode { #legacy# Active; }\n}\nreturn 0;\n",
            "native extern(\"fixture\") {\n"
            "  fn read(#legacy# value: i32): i32;\n}\nreturn 0;\n",
    };
    static const TZrChar *knownStaticFfiSource =
            "native extern(\"fixture\") {\n"
            "  #zr.ffi.entry(\"read\")#\n"
            "  #zr.ffi.callconv(\"system\")#\n"
            "  fn read(#zr.ffi.in# value: i32): i32;\n"
            "  #zr.ffi.pack(1)#\n"
            "  #zr.ffi.align(1)#\n"
            "  struct Packed { #zr.ffi.offset(0)# var value: i32; }\n"
            "  #zr.ffi.underlying(\"i32\")#\n"
            "  enum Mode { #zr.ffi.value(7)# Active; }\n"
            "  delegate Callback(#zr.ffi.out# value: pointer<i32>): i32;\n"
            "  delegate Both(#zr.ffi.inout# value: pointer<i32>): i32;\n"
            "  delegate Text(#zr.ffi.charset(\"utf8\")# value: string): i32;\n"
            "}\nreturn 0;\n";

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(invalidSources); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "runtime_decorator_shape_rejected.zr");
        function = ZrParser_Source_Compile(
                state,
                invalidSources[index],
                strlen(invalidSources[index]),
                sourceName);
        TEST_ASSERT_NULL(function);
        destroy_test_state(state);
    }

    {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "known_static_ffi_decorators.zr");
        function = ZrParser_Source_Compile(
                state,
                knownStaticFfiSource,
                strlen(knownStaticFfiSource),
                sourceName);
        TEST_ASSERT_NOT_NULL(function);
        ZrCore_Function_Free(state, function);
        destroy_test_state(state);
    }
}

static void test_legacy_runtime_decorators_are_rejected_without_codegen(void) {
    static const TZrChar *sources[] = {
            "fn legacy(target: object): object { return target; }\n"
            "#legacy#\n"
            "class Device { }\n",
            "fn legacy(target: object): object { return target; }\n"
            "class Device { #legacy# pub var value: int = 1; }\n",
            "fn legacy(target: object): object { return target; }\n"
            "#legacy#\n"
            "fn run(): int { return 1; }\n",
            "fn legacy(target: object): object { return target; }\n"
            "fn run(#legacy# value: int): int { return value; }\n",
    };

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *function;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_CreateFromNative(
                state, "legacy_runtime_decorator_rejected.zr");
        function = ZrParser_Source_Compile(
                state, sources[index], strlen(sources[index]), sourceName);
        TEST_ASSERT_NULL(function);
        destroy_test_state(state);
    }

    TEST_ASSERT_NULL(ZrCore_Io_GetSerializableNativeHelperFunction(
            ZR_IO_NATIVE_HELPER_RESERVED_LEGACY_RUNTIME_DECORATOR_APPLY));
    TEST_ASSERT_NULL(ZrCore_Io_GetSerializableNativeHelperFunction(
            ZR_IO_NATIVE_HELPER_RESERVED_LEGACY_RUNTIME_MEMBER_DECORATOR_APPLY));
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

#include "test_compile_time_decorator_shape_retention_cases.h"

// 主函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    printf("Compile-Time Execution Tests\n");
    TEST_MODULE_DIVIDER();
    
    RUN_TEST(test_compile_tool_descriptor_is_compile_only_and_contract_stable);
    RUN_TEST(test_comptime_fn_and_block_use_current_surface);
    RUN_TEST(test_pub_comptime_fn_uses_current_surface_without_runtime_projection);
    RUN_TEST(test_pub_comptime_fn_projects_through_direct_source_import);
    RUN_TEST(test_current_comptime_block_runs_after_signature_collection);
    RUN_TEST(test_comptime_if_reads_declared_project_feature_and_prunes_branch);
    RUN_TEST(test_conditional_disabled_direct_void_call_elides_argument_lowering);
    RUN_TEST(test_conditional_disabled_call_still_type_checks_arguments);
    RUN_TEST(test_readonly_struct_attribute_schema_binds_typed_field_metadata);
    RUN_TEST(test_attribute_schema_rejects_invalid_shape_and_application);
    RUN_TEST(test_declaration_transform_accepts_typed_empty_patch_once);
    RUN_TEST(test_declaration_transform_generated_field_enters_normal_layout);
    RUN_TEST(test_declaration_transform_interface_add_enters_normal_contract);
    RUN_TEST(test_declaration_transform_interface_add_validates_value_type_contract);
    RUN_TEST(test_declaration_transform_interface_add_validates_imported_parent_after_growth);
    RUN_TEST(test_declaration_transform_interface_add_resolves_later_signature);
    RUN_TEST(test_declaration_transform_interface_add_binds_prototype_and_contract_slot);
    RUN_TEST(test_declaration_transform_interface_add_binds_value_type_contract_slot);
    RUN_TEST(test_declaration_transform_interface_add_runs_normal_requirement_validation);
    RUN_TEST(test_declaration_transform_interface_add_rejects_invalid_entries);
    RUN_TEST(test_declaration_transform_interface_add_rejects_alias_identity_duplicate);
    RUN_TEST(test_declaration_transform_typed_warning_is_nonfatal);
    RUN_TEST(test_declaration_transform_typed_error_rejects_patch);
    RUN_TEST(test_declaration_transform_error_prevents_generated_member_registration);
    RUN_TEST(test_declaration_transform_invalid_multi_add_commits_nothing);
    RUN_TEST(test_declaration_transform_generated_multi_add_failure_rolls_back);
    RUN_TEST(test_declaration_transform_cross_kind_failure_rolls_back);
    RUN_TEST(test_declaration_transform_attribute_prepare_oom_frees_only_initialized_entries);
    RUN_TEST(test_declaration_transform_hash_pair_retry_preserves_metadata);
    RUN_TEST(test_declaration_transform_attribute_hash_pair_retry_preserves_metadata);
    RUN_TEST(test_declaration_transform_failed_stage_preserves_array_identity);
    RUN_TEST(test_declaration_transform_generated_metadata_survives_commit_gc);
    RUN_TEST(test_declaration_transform_patch_target_rejects_uint32_wraparound);
    RUN_TEST(test_declaration_transform_diagnostic_constructor_rejects_invalid_fields);
    RUN_TEST(test_declaration_transform_rejects_invalid_signature_and_patch_shape);
    RUN_TEST(test_comptime_if_prunes_static_import_summary_before_module_analysis);
    RUN_TEST(test_nested_comptime_if_prunes_build_facts_before_module_analysis);
    RUN_TEST(test_statement_comptime_if_is_selected_during_build_facts);
    RUN_TEST(test_runtime_scope_rejects_compile_tool_import);
    RUN_TEST(test_runtime_scope_rejects_top_level_compile_tool_alias_use);
    RUN_TEST(test_runtime_top_level_statements_reject_compile_tool_alias_use);
    RUN_TEST(test_declaration_transform_attribute_adds_use_registered_schema);
    RUN_TEST(test_static_decorator_gate_covers_all_declaration_shapes);
    RUN_TEST(test_union_interface_parameter_decorator_metadata_is_retained);
    RUN_TEST(test_ordinary_enum_static_and_dynamic_decorators_compose);
    RUN_TEST(test_generated_field_retains_transform_source_provenance);
    RUN_TEST(test_generated_field_metadata_roundtrips_to_artifact_and_reflection);
    RUN_TEST(test_intermediate_omits_empty_generated_source_map_section);
    RUN_TEST(test_generated_source_maps_are_ordered_and_byte_stable);
    RUN_TEST(test_intermediate_rejects_malformed_prototype_data_before_write);
    RUN_TEST(test_legacy_runtime_decorators_are_rejected_without_codegen);
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

