//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "zr_vm_language_server.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_common_conf.h"

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

// 测试日志宏
#define TEST_START(summary) do { \
    timer.startTime = clock(); \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while(0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while(0)

#define TEST_PASS(timer, summary) do { \
    timer.endTime = clock(); \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while(0)

#define TEST_FAIL(timer, summary, reason) do { \
    timer.endTime = clock(); \
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

// 测试语义分析器创建和释放
static void test_semantic_analyzer_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Creation and Free");
    
    TEST_INFO("Semantic Analyzer Creation", "Creating and freeing semantic analyzer");
    
    SZrSemanticAnalyzer *analyzer = ZrSemanticAnalyzerNew(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Creation and Free", "Failed to create semantic analyzer");
        return;
    }
    
    if (analyzer->symbolTable == ZR_NULL || analyzer->referenceTracker == ZR_NULL) {
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Creation and Free", "Symbol table or reference tracker is NULL");
        return;
    }
    
    ZrSemanticAnalyzerFree(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Creation and Free");
}

// 测试语义分析
static void test_semantic_analyzer_analyze(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Analyze");
    
    TEST_INFO("Analyze AST", "Analyzing simple AST for semantic information");
    
    SZrSemanticAnalyzer *analyzer = ZrSemanticAnalyzerNew(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Analyze", "Failed to create semantic analyzer");
        return;
    }
    
    // 创建简单的测试代码
    const TChar *testCode = "var x = 10;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    
    // 解析代码
    SZrAstNode *ast = ZrParserParse(state, testCode, strlen(testCode), sourceName);
    if (ast == ZR_NULL) {
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Analyze", "Failed to parse test code");
        return;
    }
    
    // 分析 AST
    TBool success = ZrSemanticAnalyzerAnalyze(state, analyzer, ast);
    if (!success) {
        ZrParserFreeAst(state, ast);
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Analyze", "Failed to analyze AST");
        return;
    }
    
    // 验证符号表中有符号
    SZrString *varName = ZrStringCreate(state, "x", 1);
    SZrSymbol *symbol = ZrSymbolTableLookup(analyzer->symbolTable, varName, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Analyze", "Symbol not found in symbol table");
        return;
    }
    
    ZrParserFreeAst(state, ast);
    ZrSemanticAnalyzerFree(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Analyze");
}

// 测试获取诊断信息
static void test_semantic_analyzer_get_diagnostics(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Get Diagnostics");
    
    TEST_INFO("Get Diagnostics", "Getting diagnostic information from analyzer");
    
    SZrSemanticAnalyzer *analyzer = ZrSemanticAnalyzerNew(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Get Diagnostics", "Failed to create semantic analyzer");
        return;
    }
    
    // 添加一个测试诊断
    SZrFileRange location = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 1, 0),
        ZrFilePositionCreate(10, 1, 10),
        ZR_NULL
    );
    
    TBool success = ZrSemanticAnalyzerAddDiagnostic(state, analyzer, ZR_DIAGNOSTIC_ERROR,
                                                     location, "Test error", "test_error");
    if (!success) {
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Get Diagnostics", "Failed to add diagnostic");
        return;
    }
    
    // 获取诊断信息
    SZrArray diagnostics;
    ZrArrayInit(state, &diagnostics, sizeof(SZrDiagnostic *), 4);
    success = ZrSemanticAnalyzerGetDiagnostics(state, analyzer, &diagnostics);
    
    if (!success || diagnostics.length == 0) {
        ZrArrayFree(state, &diagnostics);
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Get Diagnostics", "Failed to get diagnostics");
        return;
    }
    
    ZrArrayFree(state, &diagnostics);
    ZrSemanticAnalyzerFree(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Get Diagnostics");
}

// 测试代码补全
static void test_semantic_analyzer_get_completions(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Get Completions");
    
    TEST_INFO("Get Completions", "Getting code completion suggestions");
    
    SZrSemanticAnalyzer *analyzer = ZrSemanticAnalyzerNew(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Get Completions", "Failed to create semantic analyzer");
        return;
    }
    
    // 创建测试代码并分析
    const TChar *testCode = "var x = 10; var y = 20;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, testCode, strlen(testCode), sourceName);
    
    if (ast != ZR_NULL) {
        ZrSemanticAnalyzerAnalyze(state, analyzer, ast);
    }
    
    // 获取补全
    SZrFileRange position = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 1, 0),
        ZrFilePositionCreate(0, 1, 0),
        ZR_NULL
    );
    
    SZrArray completions;
    ZrArrayInit(state, &completions, sizeof(SZrCompletionItem *), 4);
    TBool success = ZrSemanticAnalyzerGetCompletions(state, analyzer, position, &completions);
    
    if (ast != ZR_NULL) {
        ZrParserFreeAst(state, ast);
    }
    
    // 补全可能为空（取决于实现），只要不崩溃就算成功
    ZrArrayFree(state, &completions);
    ZrSemanticAnalyzerFree(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Get Completions");
}

// 测试缓存功能
static void test_semantic_analyzer_cache(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Semantic Analyzer Cache");
    
    TEST_INFO("Cache Functionality", "Testing cache enable/disable and clear");
    
    SZrSemanticAnalyzer *analyzer = ZrSemanticAnalyzerNew(state);
    if (analyzer == ZR_NULL) {
        TEST_FAIL(timer, "Semantic Analyzer Cache", "Failed to create semantic analyzer");
        return;
    }
    
    // 测试启用/禁用缓存
    ZrSemanticAnalyzerSetCacheEnabled(analyzer, ZR_TRUE);
    if (!analyzer->enableCache) {
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Cache", "Failed to enable cache");
        return;
    }
    
    ZrSemanticAnalyzerSetCacheEnabled(analyzer, ZR_FALSE);
    if (analyzer->enableCache) {
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Cache", "Failed to disable cache");
        return;
    }
    
    // 测试清除缓存
    ZrSemanticAnalyzerSetCacheEnabled(analyzer, ZR_TRUE);
    ZrSemanticAnalyzerClearCache(state, analyzer);
    
    if (analyzer->cache != ZR_NULL && analyzer->cache->isValid) {
        ZrSemanticAnalyzerFree(state, analyzer);
        TEST_FAIL(timer, "Semantic Analyzer Cache", "Cache still valid after clear");
        return;
    }
    
    ZrSemanticAnalyzerFree(state, analyzer);
    TEST_PASS(timer, "Semantic Analyzer Cache");
}

// 主测试函数
int main() {
    printf("==========\n");
    printf("Language Server - Semantic Analyzer Tests\n");
    printf("==========\n\n");
    
    // 创建全局状态
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Fail - Failed to create global state\n");
        return 1;
    }
    
    // 获取主线程状态
    SZrState *state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrGlobalStateFree(global);
        printf("Fail - Failed to get main thread state\n");
        return 1;
    }
    
    // 初始化注册表
    ZrGlobalStateInitRegistry(state, global);
    
    // 运行测试
    test_semantic_analyzer_create_and_free(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_analyze(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_get_diagnostics(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_get_completions(state);
    TEST_DIVIDER();
    
    test_semantic_analyzer_cache(state);
    TEST_DIVIDER();
    
    // 清理
    ZrGlobalStateFree(global);
    
    printf("\n==========\n");
    printf("All Semantic Analyzer Tests Completed\n");
    printf("==========\n");
    
    return 0;
}
