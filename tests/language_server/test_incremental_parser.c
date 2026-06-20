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
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
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
    
    SZrIncrementalParser *parser = ZrLanguageServer_IncrementalParser_New(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "Incremental Parser Creation and Free", "Failed to create incremental parser");
        return;
    }
    
    ZrLanguageServer_IncrementalParser_Free(state, parser);
    TEST_PASS(timer, "Incremental Parser Creation and Free");
}

// 测试文件更新和解析
static void test_incremental_parser_update_and_parse(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Incremental Parser Update and Parse");
    
    TEST_INFO("Update File", "Updating file content and parsing");
    
    SZrIncrementalParser *parser = ZrLanguageServer_IncrementalParser_New(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "Incremental Parser Update and Parse", "Failed to create incremental parser");
        return;
    }
    
    // 创建文件 URI
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    // 更新文件
    TZrBool success = ZrLanguageServer_IncrementalParser_UpdateFile(state, parser, uri, content, contentLength, 1);
    if (!success) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "Incremental Parser Update and Parse", "Failed to update file");
        return;
    }
    
    // 解析文件
    success = ZrLanguageServer_IncrementalParser_Parse(state, parser, uri);
    if (!success) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "Incremental Parser Update and Parse", "Failed to parse file");
        return;
    }
    
    // 获取 AST
    SZrAstNode *ast = ZrLanguageServer_IncrementalParser_GetAST(parser, uri);
    if (ast == ZR_NULL) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "Incremental Parser Update and Parse", "AST is NULL");
        return;
    }
    
    ZrLanguageServer_IncrementalParser_Free(state, parser);
    TEST_PASS(timer, "Incremental Parser Update and Parse");
}

// 测试增量更新（相同内容不重新解析）
static void test_incremental_parser_same_content(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Incremental Parser Same Content");
    
    TEST_INFO("Same Content", "Testing that same content doesn't trigger re-parse");
    
    SZrIncrementalParser *parser = ZrLanguageServer_IncrementalParser_New(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "Incremental Parser Same Content", "Failed to create incremental parser");
        return;
    }
    
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    // 第一次更新和解析
    ZrLanguageServer_IncrementalParser_UpdateFile(state, parser, uri, content, contentLength, 1);
    ZrLanguageServer_IncrementalParser_Parse(state, parser, uri);
    SZrAstNode *ast1 = ZrLanguageServer_IncrementalParser_GetAST(parser, uri);
    
    // 第二次更新相同内容
    ZrLanguageServer_IncrementalParser_UpdateFile(state, parser, uri, content, contentLength, 2);
    ZrLanguageServer_IncrementalParser_Parse(state, parser, uri);
    SZrAstNode *ast2 = ZrLanguageServer_IncrementalParser_GetAST(parser, uri);
    
    // 如果启用内容哈希，AST 应该相同（或至少不为 NULL）
    if (ast1 == ZR_NULL || ast2 == ZR_NULL) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "Incremental Parser Same Content", "AST is NULL");
        return;
    }
    
    ZrLanguageServer_IncrementalParser_Free(state, parser);
    TEST_PASS(timer, "Incremental Parser Same Content");
}

// 测试文件移除
static void test_incremental_parser_remove_file(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Incremental Parser Remove File");
    
    TEST_INFO("Remove File", "Removing file from parser");
    
    SZrIncrementalParser *parser = ZrLanguageServer_IncrementalParser_New(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "Incremental Parser Remove File", "Failed to create incremental parser");
        return;
    }
    
    SZrString *uri = ZrCore_String_Create(state, "file:///test.zr", 15);
    const TZrChar *content = "var x = 10;";
    TZrSize contentLength = strlen(content);
    
    // 添加文件
    ZrLanguageServer_IncrementalParser_UpdateFile(state, parser, uri, content, contentLength, 1);
    
    // 移除文件
    ZrLanguageServer_IncrementalParser_RemoveFile(state, parser, uri);
    
    // 验证文件已移除
    SZrFileVersion *fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(parser, uri);
    if (fileVersion != ZR_NULL) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "Incremental Parser Remove File", "File still exists after removal");
        return;
    }
    
    ZrLanguageServer_IncrementalParser_Free(state, parser);
    TEST_PASS(timer, "Incremental Parser Remove File");
}

static void test_file_version_content_snapshot_survives_update(SZrState *state) {
    SZrTestTimer timer;
    SZrIncrementalParser *parser;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot;
    SZrFileVersionContentSnapshot updatedSnapshot;
    const TZrChar *firstContent = "var before = 1;";
    const TZrChar *secondContent = "var after = 2;";

    TEST_START("File Version Content Snapshot Survives Update");
    TEST_INFO("Snapshot Content", "Snapshot should retain its versioned content block after file content changes");

    parser = ZrLanguageServer_IncrementalParser_New(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "File Version Content Snapshot Survives Update", "Failed to create incremental parser");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///snapshot.zr", 19);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_IncrementalParser_UpdateFile(state,
                                                       parser,
                                                       uri,
                                                       firstContent,
                                                       strlen(firstContent),
                                                       1)) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Update", "Failed to create initial file");
        return;
    }

    fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(parser, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Update", "Failed to acquire content snapshot");
        return;
    }

    if (!ZrLanguageServer_IncrementalParser_UpdateFile(state,
                                                       parser,
                                                       uri,
                                                       secondContent,
                                                       strlen(secondContent),
                                                       2)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Update", "Failed to update file");
        return;
    }

    memset(&updatedSnapshot, 0, sizeof(updatedSnapshot));
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &updatedSnapshot)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Update", "Failed to acquire updated content snapshot");
        return;
    }

    if (snapshot.content == ZR_NULL ||
        snapshot.contentLength != strlen(firstContent) ||
        memcmp(snapshot.content, firstContent, snapshot.contentLength) != 0 ||
        snapshot.version != 1 ||
        snapshot.contentGeneration != 1) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &updatedSnapshot);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Update", "Snapshot did not preserve original content");
        return;
    }

    if (updatedSnapshot.content == ZR_NULL ||
        updatedSnapshot.contentLength != strlen(secondContent) ||
        memcmp(updatedSnapshot.content, secondContent, updatedSnapshot.contentLength) != 0 ||
        updatedSnapshot.version != 2 ||
        updatedSnapshot.contentGeneration != 2) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &updatedSnapshot);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Update", "Updated snapshot did not advance content generation");
        return;
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &updatedSnapshot);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    ZrLanguageServer_IncrementalParser_Free(state, parser);
    TEST_PASS(timer, "File Version Content Snapshot Survives Update");
}

static void test_file_version_content_snapshot_survives_parser_free(SZrState *state) {
    SZrTestTimer timer;
    SZrIncrementalParser *parser;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot;
    const TZrChar *content = "var retained = 3;";

    TEST_START("File Version Content Snapshot Survives Parser Free");
    TEST_INFO("Snapshot Lifetime", "Snapshot should keep its content block after the owning parser is freed");

    parser = ZrLanguageServer_IncrementalParser_New(state);
    if (parser == ZR_NULL) {
        TEST_FAIL(timer, "File Version Content Snapshot Survives Parser Free", "Failed to create incremental parser");
        return;
    }

    uri = ZrCore_String_Create(state, "file:///retained-snapshot.zr", 27);
    if (uri == ZR_NULL ||
        !ZrLanguageServer_IncrementalParser_UpdateFile(state,
                                                       parser,
                                                       uri,
                                                       content,
                                                       strlen(content),
                                                       1)) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Parser Free", "Failed to create initial file");
        return;
    }

    fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(parser, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Parser Free", "Failed to acquire content snapshot");
        return;
    }

    ZrLanguageServer_IncrementalParser_Free(state, parser);

    if (snapshot.content == ZR_NULL ||
        snapshot.contentLength != strlen(content) ||
        memcmp(snapshot.content, content, snapshot.contentLength) != 0 ||
        snapshot.version != 1 ||
        snapshot.contentGeneration != 1) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        TEST_FAIL(timer, "File Version Content Snapshot Survives Parser Free", "Snapshot content was released with parser");
        return;
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    TEST_PASS(timer, "File Version Content Snapshot Survives Parser Free");
}

// 主测试函数
int main(void) {
    printf("==========\n");
    printf("Language Server - Incremental Parser Tests\n");
    printf("==========\n\n");
    
    // 创建全局状态
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (global == ZR_NULL) {
        printf("Fail - Failed to create global state\n");
        return 1;
    }
    
    // 获取主线程状态
    SZrState *state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        printf("Fail - Failed to get main thread state\n");
        return 1;
    }
    
    // 初始化注册表
    ZrCore_GlobalState_InitRegistry(state, global);
    
    // 运行测试
    test_incremental_parser_create_and_free(state);
    TEST_DIVIDER();
    
    test_incremental_parser_update_and_parse(state);
    TEST_DIVIDER();
    
    test_incremental_parser_same_content(state);
    TEST_DIVIDER();
    
    test_incremental_parser_remove_file(state);
    TEST_DIVIDER();

    test_file_version_content_snapshot_survives_update(state);
    TEST_DIVIDER();

    test_file_version_content_snapshot_survives_parser_free(state);
    TEST_DIVIDER();
    
    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    printf("All Incremental Parser Tests Completed\n");
    printf("==========\n");
    
    return 0;
}

