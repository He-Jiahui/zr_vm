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

// 测试引用追踪器创建和释放
static void test_reference_tracker_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Reference Tracker Creation and Free");
    
    TEST_INFO("Reference Tracker Creation", "Creating and freeing reference tracker");
    
    SZrSymbolTable *symbolTable = ZrLanguageServer_SymbolTable_New(state);
    if (symbolTable == ZR_NULL) {
        TEST_FAIL(timer, "Reference Tracker Creation and Free", "Failed to create symbol table");
        return;
    }
    
    SZrReferenceTracker *tracker = ZrLanguageServer_ReferenceTracker_New(state, symbolTable);
    if (tracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Creation and Free", "Failed to create reference tracker");
        return;
    }
    
    ZrLanguageServer_ReferenceTracker_Free(state, tracker);
    ZrLanguageServer_SymbolTable_Free(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Creation and Free");
}

// 测试添加和查找引用
static void test_reference_tracker_add_and_find(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Reference Tracker Add and Find");
    
    TEST_INFO("Add Reference", "Adding reference to symbol");
    
    SZrSymbolTable *symbolTable = ZrLanguageServer_SymbolTable_New(state);
    if (symbolTable == ZR_NULL) {
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to create symbol table");
        return;
    }
    
    SZrReferenceTracker *tracker = ZrLanguageServer_ReferenceTracker_New(state, symbolTable);
    if (tracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to create reference tracker");
        return;
    }
    
    // 创建符号
    SZrString *name = ZrCore_String_Create(state, "testVar", 7);
    SZrFileRange defLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(7, 1, 7),
        ZR_NULL
    );
    
    ZrLanguageServer_SymbolTable_AddSymbol(state, symbolTable, ZR_SYMBOL_VARIABLE, name,
                          defLocation, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL);
    
    SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(symbolTable, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to lookup symbol");
        return;
    }
    
    // 添加引用
    SZrFileRange refLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(10, 2, 0),
        ZrParser_FilePosition_Create(17, 2, 7),
        ZR_NULL
    );
    
    TZrBool success = ZrLanguageServer_ReferenceTracker_AddReference(state, tracker, symbol, refLocation, ZR_REFERENCE_READ);
    if (!success) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to add reference");
        return;
    }
    
    // 查找所有引用
    SZrArray references;
    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), 4);
    success = ZrLanguageServer_ReferenceTracker_FindReferences(state, tracker, symbol, &references);
    if (!success || references.length == 0) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to find references");
        return;
    }
    
    // 验证引用计数
    TZrSize refCount = ZrLanguageServer_ReferenceTracker_GetReferenceCount(tracker, symbol);
    if (refCount == 0) {
        ZrCore_Array_Free(state, &references);
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Reference count is zero");
        return;
    }
    
    ZrCore_Array_Free(state, &references);
    ZrLanguageServer_ReferenceTracker_Free(state, tracker);
    ZrLanguageServer_SymbolTable_Free(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Add and Find");
}

// 测试获取引用位置
static void test_reference_tracker_get_locations(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Reference Tracker Get Locations");
    
    TEST_INFO("Get Reference Locations", "Getting all reference locations for a symbol");
    
    SZrSymbolTable *symbolTable = ZrLanguageServer_SymbolTable_New(state);
    SZrReferenceTracker *tracker = ZrLanguageServer_ReferenceTracker_New(state, symbolTable);
    
    if (symbolTable == ZR_NULL || tracker == ZR_NULL) {
        if (tracker != ZR_NULL) ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        if (symbolTable != ZR_NULL) ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Get Locations", "Failed to create tracker");
        return;
    }
    
    // 创建符号并添加多个引用
    SZrString *name = ZrCore_String_Create(state, "testVar", 7);
    SZrFileRange defLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(7, 1, 7),
        ZR_NULL
    );
    
    ZrLanguageServer_SymbolTable_AddSymbol(state, symbolTable, ZR_SYMBOL_VARIABLE, name,
                          defLocation, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL);
    
    SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(symbolTable, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Get Locations", "Failed to lookup symbol");
        return;
    }
    
    // 添加多个引用
    for (int i = 0; i < 3; i++) {
        SZrFileRange refLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10 + i * 20, 2 + i, 0),
            ZrParser_FilePosition_Create(17 + i * 20, 2 + i, 7),
            ZR_NULL
        );
        ZrLanguageServer_ReferenceTracker_AddReference(state, tracker, symbol, refLocation, ZR_REFERENCE_READ);
    }
    
    // 获取所有引用位置
    SZrArray locations;
    ZrCore_Array_Init(state, &locations, sizeof(SZrFileRange), 4);
    TZrBool success = ZrLanguageServer_ReferenceTracker_GetReferenceLocations(state, tracker, symbol, &locations);
    
    if (!success || locations.length != 3) {
        ZrCore_Array_Free(state, &locations);
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Get Locations", "Failed to get all reference locations");
        return;
    }
    
    ZrCore_Array_Free(state, &locations);
    ZrLanguageServer_ReferenceTracker_Free(state, tracker);
    ZrLanguageServer_SymbolTable_Free(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Get Locations");
}

// 主测试函数
int main(void) {
    printf("==========\n");
    printf("Language Server - Reference Tracker Tests\n");
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
    test_reference_tracker_create_and_free(state);
    TEST_DIVIDER();
    
    test_reference_tracker_add_and_find(state);
    TEST_DIVIDER();
    
    test_reference_tracker_get_locations(state);
    TEST_DIVIDER();
    
    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    printf("All Reference Tracker Tests Completed\n");
    printf("==========\n");
    
    return 0;
}

