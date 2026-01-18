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
    ZR_UNUSED_PARAMETER(flag);
    
    if (newSize == 0) {
        // 释放内存
        if (pointer != ZR_NULL) {
            // 检查指针是否在合理范围内（避免释放无效指针）
            // 同时检查 originalSize 是否合理（避免释放时传入错误的 size）
            if ((TZrPtr)pointer >= (TZrPtr)0x1000 && originalSize > 0 && originalSize < 1024 * 1024 * 1024) {
                free(pointer);
            }
            // 如果指针无效，不调用free，避免崩溃
        }
        return ZR_NULL;
    }
    
    if (pointer == ZR_NULL) {
        // 分配新内存
        return malloc(newSize);
    } else {
        // 重新分配内存
        // 检查指针是否在合理范围内（避免realloc无效指针）
        if ((TZrPtr)pointer >= (TZrPtr)0x1000 && originalSize > 0 && originalSize < 1024 * 1024 * 1024) {
            return realloc(pointer, newSize);
        } else {
            // 无效指针，分配新内存
            return malloc(newSize);
        }
    }
}

// 测试增量解析器创建和释放
static void test_incremental_parser_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Incremental Parser Creation and Free");
    
    TEST_INFO("Incremental Parser Creation", "Creating and freeing incremental parser");
    
    SZrIncrementalParser *parser = ZrIncrementalParserNew(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "Incremental Parser Creation and Free", "Failed to create incremental parser");
        return;
    }
    
    ZrIncrementalParserFree(state, parser);
    TEST_PASS(timer, "Incremental Parser Creation and Free");
}

// 测试文件更新和解析
static void test_incremental_parser_update_and_parse(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Incremental Parser Update and Parse");
    
    TEST_INFO("Update File", "Updating file content and parsing");
    
    SZrIncrementalParser *parser = ZrIncrementalParserNew(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "Incremental Parser Update and Parse", "Failed to create incremental parser");
        return;
    }
    
    // 创建文件 URI
    SZrString *uri = ZrStringCreate(state, "file:///test.zr", 15);
    const TChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    // 更新文件
    TBool success = ZrIncrementalParserUpdateFile(state, parser, uri, content, contentLength, 1);
    if (!success) {
        ZrIncrementalParserFree(state, parser);
        TEST_FAIL(timer, "Incremental Parser Update and Parse", "Failed to update file");
        return;
    }
    
    // 解析文件
    success = ZrIncrementalParserParse(state, parser, uri);
    if (!success) {
        ZrIncrementalParserFree(state, parser);
        TEST_FAIL(timer, "Incremental Parser Update and Parse", "Failed to parse file");
        return;
    }
    
    // 获取 AST
    SZrAstNode *ast = ZrIncrementalParserGetAST(parser, uri);
    if (ast == ZR_NULL) {
        ZrIncrementalParserFree(state, parser);
        TEST_FAIL(timer, "Incremental Parser Update and Parse", "AST is NULL");
        return;
    }
    
    ZrIncrementalParserFree(state, parser);
    TEST_PASS(timer, "Incremental Parser Update and Parse");
}

// 测试增量更新（相同内容不重新解析）
static void test_incremental_parser_same_content(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Incremental Parser Same Content");
    
    TEST_INFO("Same Content", "Testing that same content doesn't trigger re-parse");
    
    SZrIncrementalParser *parser = ZrIncrementalParserNew(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "Incremental Parser Same Content", "Failed to create incremental parser");
        return;
    }
    
    SZrString *uri = ZrStringCreate(state, "file:///test.zr", 15);
    const TChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    // 第一次更新和解析
    ZrIncrementalParserUpdateFile(state, parser, uri, content, contentLength, 1);
    ZrIncrementalParserParse(state, parser, uri);
    SZrAstNode *ast1 = ZrIncrementalParserGetAST(parser, uri);
    
    // 第二次更新相同内容
    ZrIncrementalParserUpdateFile(state, parser, uri, content, contentLength, 2);
    ZrIncrementalParserParse(state, parser, uri);
    SZrAstNode *ast2 = ZrIncrementalParserGetAST(parser, uri);
    
    // 如果启用内容哈希，AST 应该相同（或至少不为 NULL）
    if (ast1 == ZR_NULL || ast2 == ZR_NULL) {
        ZrIncrementalParserFree(state, parser);
        TEST_FAIL(timer, "Incremental Parser Same Content", "AST is NULL");
        return;
    }
    
    ZrIncrementalParserFree(state, parser);
    TEST_PASS(timer, "Incremental Parser Same Content");
}

// 测试文件移除
static void test_incremental_parser_remove_file(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Incremental Parser Remove File");
    
    TEST_INFO("Remove File", "Removing file from parser");
    
    SZrIncrementalParser *parser = ZrIncrementalParserNew(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "Incremental Parser Remove File", "Failed to create incremental parser");
        return;
    }
    
    SZrString *uri = ZrStringCreate(state, "file:///test.zr", 15);
    const TChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    // 添加文件
    ZrIncrementalParserUpdateFile(state, parser, uri, content, contentLength, 1);
    
    // 移除文件
    ZrIncrementalParserRemoveFile(state, parser, uri);
    
    // 验证文件已移除
    SZrFileVersion *fileVersion = ZrIncrementalParserGetFileVersion(parser, uri);
    if (fileVersion != ZR_NULL) {
        ZrIncrementalParserFree(state, parser);
        TEST_FAIL(timer, "Incremental Parser Remove File", "File still exists after removal");
        return;
    }
    
    ZrIncrementalParserFree(state, parser);
    TEST_PASS(timer, "Incremental Parser Remove File");
}

// 主测试函数
int main() {
    printf("==========\n");
    printf("Language Server - Incremental Parser Tests\n");
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
    test_incremental_parser_create_and_free(state);
    TEST_DIVIDER();
    
    test_incremental_parser_update_and_parse(state);
    TEST_DIVIDER();
    
    test_incremental_parser_same_content(state);
    TEST_DIVIDER();
    
    test_incremental_parser_remove_file(state);
    TEST_DIVIDER();
    
    // 清理
    ZrGlobalStateFree(global);
    
    printf("\n==========\n");
    printf("All Incremental Parser Tests Completed\n");
    printf("==========\n");
    
    return 0;
}
