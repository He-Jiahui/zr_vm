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

static int g_failures = 0;

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
    g_failures++; \
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
    SZrString *source = ZrCore_String_Create(
        state, "file:///workspace/reference.zr", strlen("file:///workspace/reference.zr"));
    SZrFileRange defLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(7, 1, 7),
        source
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
    symbol->semanticId = 101U;
    
    // 添加引用
    SZrFileRange refLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(10, 2, 0),
        ZrParser_FilePosition_Create(17, 2, 7),
        source
    );
    
    TZrBool success = ZrLanguageServer_ReferenceTracker_AddReference(state, tracker, symbol, refLocation, ZR_REFERENCE_READ);
    if (!success) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to add reference");
        return;
    }
    
    SZrFileRange queryPosition = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(12, 2, 2),
        ZrParser_FilePosition_Create(12, 2, 2),
        source
    );
    SZrReference *reference =
        ZrLanguageServer_ReferenceTracker_FindReferenceAt(tracker, queryPosition);
    if (reference == ZR_NULL || reference->symbol != symbol ||
        reference->symbolId != symbol->semanticId ||
        reference->type != ZR_REFERENCE_READ) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Add and Find", "Failed to find the exact-position reference");
        return;
    }

    ZrLanguageServer_ReferenceTracker_Free(state, tracker);
    ZrLanguageServer_SymbolTable_Free(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Add and Find");
}

static void test_reference_tracker_finds_each_exact_location(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Reference Tracker Finds Each Exact Location");
    
    TEST_INFO("Exact Reference Locations", "Finding each tracked reference by its exact source position");
    
    SZrSymbolTable *symbolTable = ZrLanguageServer_SymbolTable_New(state);
    SZrReferenceTracker *tracker = ZrLanguageServer_ReferenceTracker_New(state, symbolTable);
    
    if (symbolTable == ZR_NULL || tracker == ZR_NULL) {
        if (tracker != ZR_NULL) ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        if (symbolTable != ZR_NULL) ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Finds Each Exact Location", "Failed to create tracker");
        return;
    }
    
    // 创建符号并添加多个引用
    SZrString *name = ZrCore_String_Create(state, "testVar", 7);
    SZrString *source = ZrCore_String_Create(
        state, "file:///workspace/locations.zr", strlen("file:///workspace/locations.zr"));
    SZrFileRange defLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(7, 1, 7),
        source
    );
    
    ZrLanguageServer_SymbolTable_AddSymbol(state, symbolTable, ZR_SYMBOL_VARIABLE, name,
                          defLocation, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL);
    
    SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(symbolTable, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(timer, "Reference Tracker Finds Each Exact Location", "Failed to lookup symbol");
        return;
    }
    
    // 添加多个引用
    for (int i = 0; i < 3; i++) {
        SZrFileRange refLocation = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(10 + i * 20, 2 + i, 0),
            ZrParser_FilePosition_Create(17 + i * 20, 2 + i, 7),
            source
        );
        ZrLanguageServer_ReferenceTracker_AddReference(state, tracker, symbol, refLocation, ZR_REFERENCE_READ);
    }

    for (int i = 0; i < 3; i++) {
        SZrFileRange position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(12 + i * 20, 2 + i, 2),
            ZrParser_FilePosition_Create(12 + i * 20, 2 + i, 2),
            source
        );
        SZrReference *reference =
            ZrLanguageServer_ReferenceTracker_FindReferenceAt(tracker, position);
        if (reference == ZR_NULL || reference->symbol != symbol) {
            ZrLanguageServer_ReferenceTracker_Free(state, tracker);
            ZrLanguageServer_SymbolTable_Free(state, symbolTable);
            TEST_FAIL(timer, "Reference Tracker Finds Each Exact Location", "Failed to find an exact-position reference");
            return;
        }
    }

    ZrLanguageServer_ReferenceTracker_Free(state, tracker);
    ZrLanguageServer_SymbolTable_Free(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Finds Each Exact Location");
}

static void test_reference_tracker_requires_exact_source_identity(
        SZrState *state) {
    SZrTestTimer timer;
    SZrSymbolTable *symbolTable;
    SZrReferenceTracker *tracker;
    SZrString *name;
    SZrString *sourceA;
    SZrString *sourceACopy;
    SZrFileRange location;
    SZrFileRange position;
    SZrSymbol *symbol;

    TEST_START("Reference Tracker Exact Source Identity");
    TEST_INFO(
            "Source Identity",
            "Rejecting missing source URIs while preserving exact URI text equality");

    symbolTable = ZrLanguageServer_SymbolTable_New(state);
    tracker = ZrLanguageServer_ReferenceTracker_New(state, symbolTable);
    if (symbolTable == ZR_NULL || tracker == ZR_NULL) {
        if (tracker != ZR_NULL) {
            ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        }
        if (symbolTable != ZR_NULL) {
            ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        }
        TEST_FAIL(
                timer,
                "Reference Tracker Exact Source Identity",
                "Failed to create tracker");
        return;
    }

    name = ZrCore_String_Create(state, "sourceBound", 11);
    sourceA = ZrCore_String_Create(
            state, "file:///workspace/a.zr", strlen("file:///workspace/a.zr"));
    sourceACopy = ZrCore_String_Create(
            state, "file:///workspace/a.zr", strlen("file:///workspace/a.zr"));
    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, 1, 0),
            ZrParser_FilePosition_Create(11, 1, 11),
            sourceA);
    ZrLanguageServer_SymbolTable_AddSymbol(
            state,
            symbolTable,
            ZR_SYMBOL_VARIABLE,
            name,
            location,
            ZR_NULL,
            ZR_ACCESS_PUBLIC,
            ZR_NULL);
    symbol = ZrLanguageServer_SymbolTable_Lookup(symbolTable, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(
                timer,
                "Reference Tracker Exact Source Identity",
                "Failed to create source-bound symbol");
        return;
    }

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(20, 2, 0),
            ZrParser_FilePosition_Create(31, 2, 11),
            sourceA);
    ZrLanguageServer_ReferenceTracker_AddReference(
            state, tracker, symbol, location, ZR_REFERENCE_READ);
    position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(24, 2, 4),
            ZrParser_FilePosition_Create(24, 2, 4),
            ZR_NULL);
    if (ZrLanguageServer_ReferenceTracker_FindReferenceAt(
                tracker, position) != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(
                timer,
                "Reference Tracker Exact Source Identity",
                "A source-less query matched a source-bound reference");
        return;
    }

    location = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(40, 3, 0),
            ZrParser_FilePosition_Create(51, 3, 11),
            ZR_NULL);
    ZrLanguageServer_ReferenceTracker_AddReference(
            state, tracker, symbol, location, ZR_REFERENCE_READ);
    position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(44, 3, 4),
            ZrParser_FilePosition_Create(44, 3, 4),
            sourceA);
    if (ZrLanguageServer_ReferenceTracker_FindReferenceAt(
                tracker, position) != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(
                timer,
                "Reference Tracker Exact Source Identity",
                "A source-less reference matched a source-bound query");
        return;
    }

    position.source = ZR_NULL;
    if (ZrLanguageServer_ReferenceTracker_FindReferenceAt(
                tracker, position) != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(
                timer,
                "Reference Tracker Exact Source Identity",
                "Two missing source URIs were treated as one source");
        return;
    }

    position = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(24, 2, 4),
            ZrParser_FilePosition_Create(24, 2, 4),
            sourceACopy);
    if (ZrLanguageServer_ReferenceTracker_FindReferenceAt(
                tracker, position) == ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(
                timer,
                "Reference Tracker Exact Source Identity",
                "Equal URI text from distinct strings did not match");
        return;
    }

    ZrLanguageServer_ReferenceTracker_Free(state, tracker);
    ZrLanguageServer_SymbolTable_Free(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Exact Source Identity");
}

static void test_reference_tracker_preserves_canonical_symbol_identity(
        SZrState *state) {
    SZrTestTimer timer;
    SZrSymbolTable *symbolTable;
    SZrReferenceTracker *tracker;
    SZrString *name;
    SZrString *source;
    SZrFileRange declaration;
    SZrFileRange firstUse;
    SZrFileRange secondUse;
    SZrFileRange firstPosition;
    SZrFileRange secondPosition;
    SZrSymbol *firstSymbol;
    SZrSymbol *differentIdentitySymbol;
    SZrReference *firstReference;
    SZrReference *secondReference;

    TEST_START("Reference Tracker Canonical Symbol Identity");
    TEST_INFO(
            "Symbol Identity",
            "Preserving each canonical SymbolId on exact-position reference hits");

    symbolTable = ZrLanguageServer_SymbolTable_New(state);
    tracker = ZrLanguageServer_ReferenceTracker_New(state, symbolTable);
    name = ZrCore_String_Create(state, "duplicate", 9);
    source = ZrCore_String_Create(
            state,
            "file:///workspace/identity.zr",
            strlen("file:///workspace/identity.zr"));
    declaration = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, 1, 0),
            ZrParser_FilePosition_Create(9, 1, 9),
            source);
    firstSymbol = ZrLanguageServer_Symbol_New(
            state,
            ZR_SYMBOL_VARIABLE,
            name,
            declaration,
            ZR_NULL,
            ZR_ACCESS_PUBLIC,
            ZR_NULL);
    differentIdentitySymbol = ZrLanguageServer_Symbol_New(
            state,
            ZR_SYMBOL_VARIABLE,
            name,
            declaration,
            ZR_NULL,
            ZR_ACCESS_PUBLIC,
            ZR_NULL);
    if (symbolTable == ZR_NULL || tracker == ZR_NULL ||
        firstSymbol == ZR_NULL || differentIdentitySymbol == ZR_NULL) {
        TEST_FAIL(
                timer,
                "Reference Tracker Canonical Symbol Identity",
                "Failed to create tracker or symbols");
        return;
    }

    firstSymbol->semanticId = 101U;
    differentIdentitySymbol->semanticId = 202U;
    firstUse = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(20, 2, 0),
            ZrParser_FilePosition_Create(29, 2, 9),
            source);
    secondUse = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(40, 3, 0),
            ZrParser_FilePosition_Create(49, 3, 9),
            source);
    ZrLanguageServer_ReferenceTracker_AddReference(
            state, tracker, firstSymbol, firstUse, ZR_REFERENCE_READ);
    ZrLanguageServer_ReferenceTracker_AddReference(
            state,
            tracker,
            differentIdentitySymbol,
            secondUse,
            ZR_REFERENCE_WRITE);

    firstPosition = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(24, 2, 4),
            ZrParser_FilePosition_Create(24, 2, 4),
            source);
    secondPosition = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(44, 3, 4),
            ZrParser_FilePosition_Create(44, 3, 4),
            source);
    firstReference = ZrLanguageServer_ReferenceTracker_FindReferenceAt(
            tracker, firstPosition);
    secondReference = ZrLanguageServer_ReferenceTracker_FindReferenceAt(
            tracker, secondPosition);
    if (firstReference == ZR_NULL || secondReference == ZR_NULL ||
        firstReference->symbol != firstSymbol ||
        firstReference->symbolId != 101U ||
        secondReference->symbol != differentIdentitySymbol ||
        secondReference->symbolId != 202U) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_Symbol_Free(state, firstSymbol);
        ZrLanguageServer_Symbol_Free(state, differentIdentitySymbol);
        ZrLanguageServer_SymbolTable_Free(state, symbolTable);
        TEST_FAIL(
                timer,
                "Reference Tracker Canonical Symbol Identity",
                "Exact-position references did not preserve canonical SymbolIds");
        return;
    }

    ZrLanguageServer_ReferenceTracker_Free(state, tracker);
    ZrLanguageServer_Symbol_Free(state, firstSymbol);
    ZrLanguageServer_Symbol_Free(state, differentIdentitySymbol);
    ZrLanguageServer_SymbolTable_Free(state, symbolTable);
    TEST_PASS(timer, "Reference Tracker Canonical Symbol Identity");
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
    
    test_reference_tracker_finds_each_exact_location(state);
    TEST_DIVIDER();

    test_reference_tracker_requires_exact_source_identity(state);
    TEST_DIVIDER();

    test_reference_tracker_preserves_canonical_symbol_identity(state);
    TEST_DIVIDER();
    
    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    if (g_failures == 0) {
        printf("All Reference Tracker Tests Completed\n");
    } else {
        printf("%d Reference Tracker Test(s) Failed\n", g_failures);
    }
    printf("==========\n");
    
    return g_failures == 0 ? 0 : 1;
}

