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

// 测试引用追踪器创建和释放
static void test_reference_tracker_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Reference Tracker Creation and Free");
    
    TEST_INFO("Reference Tracker Creation", "Creating and freeing reference tracker");
    
    SZrSymbolTable *symbolTable = ZrSymbolTableNew(state);
    if (symbolTable == ZR_NULL) {
        TEST_FAIL(timer, "Reference Tracker Creation and Free", "Failed to create symbol table");
        return;
    }
    
    SZrReferenceTracker *tracker = ZrReferenceTrackerNew(state, symbolTable);
    if (tracker == ZR_NULL) {
        ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Creation and Free", "Failed to create reference tracker");
        return;
    }
    
    ZrReferenceTrackerFree(state, tracker);
    ZrSymbolTableFree(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Creation and Free");
}

// 测试添加和查找引用
static void test_reference_tracker_add_and_find(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Reference Tracker Add and Find");
    
    TEST_INFO("Add Reference", "Adding reference to symbol");
    
    SZrSymbolTable *symbolTable = ZrSymbolTableNew(state);
    if (symbolTable == ZR_NULL) {
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to create symbol table");
        return;
    }
    
    SZrReferenceTracker *tracker = ZrReferenceTrackerNew(state, symbolTable);
    if (tracker == ZR_NULL) {
        ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to create reference tracker");
        return;
    }
    
    // 创建符号
    SZrString *name = ZrStringCreate(state, "testVar", 7);
    SZrFileRange defLocation = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 1, 0),
        ZrFilePositionCreate(7, 1, 7),
        ZR_NULL
    );
    
    ZrSymbolTableAddSymbol(state, symbolTable, ZR_SYMBOL_VARIABLE, name,
                          defLocation, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL);
    
    SZrSymbol *symbol = ZrSymbolTableLookup(symbolTable, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrReferenceTrackerFree(state, tracker);
        ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to lookup symbol");
        return;
    }
    
    // 添加引用
    SZrFileRange refLocation = ZrFileRangeCreate(
        ZrFilePositionCreate(10, 2, 0),
        ZrFilePositionCreate(17, 2, 7),
        ZR_NULL
    );
    
    TBool success = ZrReferenceTrackerAddReference(state, tracker, symbol, refLocation, ZR_REFERENCE_READ);
    if (!success) {
        ZrReferenceTrackerFree(state, tracker);
        ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to add reference");
        return;
    }
    
    // 查找所有引用
    SZrArray references;
    ZrArrayInit(state, &references, sizeof(SZrReference *), 4);
    success = ZrReferenceTrackerFindReferences(state, tracker, symbol, &references);
    if (!success || references.length == 0) {
        ZrArrayFree(state, &references);
        ZrReferenceTrackerFree(state, tracker);
        ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to find references");
        return;
    }
    
    // 验证引用计数
    TZrSize refCount = ZrReferenceTrackerGetReferenceCount(tracker, symbol);
    if (refCount == 0) {
        ZrArrayFree(state, &references);
        ZrReferenceTrackerFree(state, tracker);
        ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Reference count is zero");
        return;
    }
    
    ZrArrayFree(state, &references);
    ZrReferenceTrackerFree(state, tracker);
    ZrSymbolTableFree(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Add and Find");
}

// 测试获取引用位置
static void test_reference_tracker_get_locations(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Reference Tracker Get Locations");
    
    TEST_INFO("Get Reference Locations", "Getting all reference locations for a symbol");
    
    SZrSymbolTable *symbolTable = ZrSymbolTableNew(state);
    SZrReferenceTracker *tracker = ZrReferenceTrackerNew(state, symbolTable);
    
    if (symbolTable == ZR_NULL || tracker == ZR_NULL) {
        if (tracker != ZR_NULL) ZrReferenceTrackerFree(state, tracker);
        if (symbolTable != ZR_NULL) ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Get Locations", "Failed to create tracker");
        return;
    }
    
    // 创建符号并添加多个引用
    SZrString *name = ZrStringCreate(state, "testVar", 7);
    SZrFileRange defLocation = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 1, 0),
        ZrFilePositionCreate(7, 1, 7),
        ZR_NULL
    );
    
    ZrSymbolTableAddSymbol(state, symbolTable, ZR_SYMBOL_VARIABLE, name,
                          defLocation, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL);
    
    SZrSymbol *symbol = ZrSymbolTableLookup(symbolTable, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrReferenceTrackerFree(state, tracker);
        ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Get Locations", "Failed to lookup symbol");
        return;
    }
    
    // 添加多个引用
    for (int i = 0; i < 3; i++) {
        SZrFileRange refLocation = ZrFileRangeCreate(
            ZrFilePositionCreate(10 + i * 20, 2 + i, 0),
            ZrFilePositionCreate(17 + i * 20, 2 + i, 7),
            ZR_NULL
        );
        ZrReferenceTrackerAddReference(state, tracker, symbol, refLocation, ZR_REFERENCE_READ);
    }
    
    // 获取所有引用位置
    SZrArray locations;
    ZrArrayInit(state, &locations, sizeof(SZrFileRange), 4);
    TBool success = ZrReferenceTrackerGetReferenceLocations(state, tracker, symbol, &locations);
    
    if (!success || locations.length != 3) {
        ZrArrayFree(state, &locations);
        ZrReferenceTrackerFree(state, tracker);
        ZrSymbolTableFree(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Get Locations", "Failed to get all reference locations");
        return;
    }
    
    ZrArrayFree(state, &locations);
    ZrReferenceTrackerFree(state, tracker);
    ZrSymbolTableFree(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Get Locations");
}

// 主测试函数
int main() {
    printf("==========\n");
    printf("Language Server - Reference Tracker Tests\n");
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
    test_reference_tracker_create_and_free(state);
    TEST_DIVIDER();
    
    test_reference_tracker_add_and_find(state);
    TEST_DIVIDER();
    
    test_reference_tracker_get_locations(state);
    TEST_DIVIDER();
    
    // 清理
    ZrGlobalStateFree(global);
    
    printf("\n==========\n");
    printf("All Reference Tracker Tests Completed\n");
    printf("==========\n");
    
    return 0;
}
