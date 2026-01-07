//
// Created by Auto on 2025/01/XX.
//

// 定义GNU源以支持realpath函数（Linux系统）
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
// 定义POSIX源以支持realpath函数（备用）
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#ifdef _MSC_VER
    #include <direct.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
#endif
#include "unity.h"
#include "zr_vm_parser.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

// 测试日志宏（符合测试规范）
#define TEST_START(summary) do { \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while(0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while(0)

#define TEST_PASS_CUSTOM(timer, summary) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while(0)

#define TEST_FAIL_CUSTOM(timer, summary, reason) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
} while(0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while(0)

#define TEST_MODULE_DIVIDER() do { \
    printf("==========\n"); \
    fflush(stdout); \
} while(0)

// realpath 兼容函数（MSVC使用_fullpath）
#ifdef _MSC_VER
static char* test_realpath(const char* path, char* resolved_path) {
    return _fullpath(resolved_path, path, _MAX_PATH);
}
#define realpath test_realpath
#endif

// 简单的测试分配器
static TZrPtr testAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    
    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }
    
    if (pointer == ZR_NULL) {
        return malloc(newSize);
    } else {
        if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
            return realloc(pointer, newSize);
        }
        return ZR_NULL;
    }
}

// 创建测试状态
static SZrState* createTestState(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState* global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, &callbacks);
    if (!global) return ZR_NULL;
    
    SZrState* mainState = global->mainThreadState;
    if (mainState) {
        ZrGlobalStateInitRegistry(mainState, global);
    }
    
    return mainState;
}

// 销毁测试状态
static void destroyTestState(SZrState* state) {
    if (!state) return;
    
    SZrGlobalState* global = state->global;
    if (global) {
        ZrGlobalStateFree(global);
    }
}

// 读取测试文件内容
static char* readTestFile(const char* filename, size_t* outSize) {
    FILE* file = fopen(filename, "r");
    if (file == ZR_NULL) {
        // 尝试其他路径
        char path1[512];
        snprintf(path1, sizeof(path1), "tests/parser/%s", filename);
        file = fopen(path1, "r");
    }
    if (file == ZR_NULL) {
        char path2[512];
        snprintf(path2, sizeof(path2), "../../tests/parser/%s", filename);
        file = fopen(path2, "r");
    }
    if (file == ZR_NULL) {
        return ZR_NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* source = (char*)malloc(fileSize + 1);
    if (source == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }
    
    size_t readSize = fread(source, 1, fileSize, file);
    fclose(file);
    source[readSize] = '\0';
    
    if (outSize != ZR_NULL) {
        *outSize = readSize;
    }
    
    return source;
}

// 测试字符字面量解析
void test_char_literals_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Character Literals Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Character literals parsing", 
              "Testing parsing of character literals with various escape sequences");
    
    size_t fileSize;
    char* source = readTestFile("test_char_literals.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Cannot find test_char_literals.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrStringCreate(state, "test_char_literals.zr", 22);
    SZrAstNode* ast = ZrParserParse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse test_char_literals.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    // 验证 AST 结构
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试字符字面量编译
void test_char_literals_compilation(void) {
    SZrTestTimer timer;
    const char* testSummary = "Character Literals Compilation";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Character literals compilation", 
              "Testing compilation of character literals to VM instructions");
    
    size_t fileSize;
    char* source = readTestFile("test_char_literals.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Cannot find test_char_literals.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrStringCreate(state, "test_char_literals.zr", 22);
    SZrAstNode* ast = ZrParserParse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse test_char_literals.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    // 编译 AST 为指令码
    SZrFunction* function = ZrCompilerCompile(state, ast);
    
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile AST to instructions");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试基本类型转换解析
void test_type_cast_basic_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Basic Type Cast Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Basic type cast parsing", 
              "Testing parsing of basic type cast expressions: <int>, <float>, <string>, <bool>");
    
    size_t fileSize;
    char* source = readTestFile("test_type_cast_basic.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Cannot find test_type_cast_basic.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrStringCreate(state, "test_type_cast_basic.zr", 24);
    SZrAstNode* ast = ZrParserParse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse test_type_cast_basic.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    // 验证 AST 结构
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试基本类型转换编译
void test_type_cast_basic_compilation(void) {
    SZrTestTimer timer;
    const char* testSummary = "Basic Type Cast Compilation";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Basic type cast compilation", 
              "Testing compilation of basic type cast expressions to conversion instructions");
    
    size_t fileSize;
    char* source = readTestFile("test_type_cast_basic.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Cannot find test_type_cast_basic.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrStringCreate(state, "test_type_cast_basic.zr", 24);
    SZrAstNode* ast = ZrParserParse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse test_type_cast_basic.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    // 编译 AST 为指令码
    SZrFunction* function = ZrCompilerCompile(state, ast);
    
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile AST to instructions");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试结构体类型转换解析
void test_type_cast_struct_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Struct Type Cast Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Struct type cast parsing", 
              "Testing parsing of struct type cast expressions: <StructType>");
    
    size_t fileSize;
    char* source = readTestFile("test_type_cast_struct.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Cannot find test_type_cast_struct.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrStringCreate(state, "test_type_cast_struct.zr", 25);
    SZrAstNode* ast = ZrParserParse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse test_type_cast_struct.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    // 验证 AST 结构
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试类类型转换解析
void test_type_cast_class_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Class Type Cast Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Class type cast parsing", 
              "Testing parsing of class type cast expressions: <ClassName>");
    
    size_t fileSize;
    char* source = readTestFile("test_type_cast_class.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Cannot find test_type_cast_class.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrStringCreate(state, "test_type_cast_class.zr", 24);
    SZrAstNode* ast = ZrParserParse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse test_type_cast_class.zr file");
        destroyTestState(state);
        TEST_DIVIDER();
        return;
    }
    
    // 验证 AST 结构
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 主测试函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    
    // 字符字面量测试
    RUN_TEST(test_char_literals_parsing);
    RUN_TEST(test_char_literals_compilation);
    
    TEST_MODULE_DIVIDER();
    
    // 类型转换测试
    RUN_TEST(test_type_cast_basic_parsing);
    RUN_TEST(test_type_cast_basic_compilation);
    RUN_TEST(test_type_cast_struct_parsing);
    RUN_TEST(test_type_cast_class_parsing);
    
    TEST_MODULE_DIVIDER();
    
    return UNITY_END();
}

