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

// 测试日志宏（符合测试规范）
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

// 测试符号表创建和释放
static void test_symbol_table_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Symbol Table Creation and Free");
    
    TEST_INFO("Symbol Table Creation", "Creating and freeing symbol table");
    
    SZrSymbolTable *table = ZrSymbolTableNew(state);
    if (table == ZR_NULL) {
        TEST_FAIL(timer, "Symbol Table Creation and Free", "Failed to create symbol table");
        return;
    }
    
    if (table->globalScope == ZR_NULL) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Table Creation and Free", "Global scope is NULL");
        return;
    }
    
    ZrSymbolTableFree(state, table);
    TEST_PASS(timer, "Symbol Table Creation and Free");
}

// 测试添加和查找符号
static void test_symbol_table_add_and_lookup(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Symbol Table Add and Lookup");
    
    TEST_INFO("Add Symbol", "Adding variable symbol to symbol table");
    
    SZrSymbolTable *table = ZrSymbolTableNew(state);
    if (table == ZR_NULL) {
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Failed to create symbol table");
        return;
    }
    
    // 创建测试符号名称
    SZrString *name = ZrStringCreate(state, "testVar", 7);
    if (name == ZR_NULL) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Failed to create string");
        return;
    }
    
    // 创建文件范围
    SZrFileRange location = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 1, 0),
        ZrFilePositionCreate(7, 1, 7),
        ZR_NULL
    );
    
    // 添加符号
    TBool success = ZrSymbolTableAddSymbol(state, table, ZR_SYMBOL_VARIABLE, name,
                                            location, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL);
    if (!success) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Failed to add symbol");
        return;
    }
    
    // 查找符号
    SZrSymbol *found = ZrSymbolTableLookup(table, name, ZR_NULL);
    if (found == ZR_NULL) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Failed to lookup symbol");
        return;
    }
    
    // 验证符号信息
    if (found->type != ZR_SYMBOL_VARIABLE) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Symbol type mismatch");
        return;
    }
    
    ZrSymbolTableFree(state, table);
    TEST_PASS(timer, "Symbol Table Add and Lookup");
}

// 测试作用域管理
static void test_symbol_table_scope_management(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Symbol Table Scope Management");
    
    TEST_INFO("Scope Management", "Testing scope enter and exit");
    
    SZrSymbolTable *table = ZrSymbolTableNew(state);
    if (table == ZR_NULL) {
        TEST_FAIL(timer, "Symbol Table Scope Management", "Failed to create symbol table");
        return;
    }
    
    // 进入新作用域
    SZrFileRange scopeRange = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 1, 0),
        ZrFilePositionCreate(100, 10, 0),
        ZR_NULL
    );
    
    ZrSymbolTableEnterScope(state, table, scopeRange, ZR_FALSE, ZR_FALSE, ZR_FALSE);
    
    SZrSymbolScope *currentScope = ZrSymbolTableGetCurrentScope(table);
    if (currentScope == ZR_NULL) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Table Scope Management", "Current scope is NULL after enter");
        return;
    }
    
    // 退出作用域
    ZrSymbolTableExitScope(table);
    
    // 验证回到全局作用域
    SZrSymbolScope *globalScope = ZrSymbolTableGetCurrentScope(table);
    if (globalScope != table->globalScope) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Table Scope Management", "Failed to return to global scope");
        return;
    }
    
    ZrSymbolTableFree(state, table);
    TEST_PASS(timer, "Symbol Table Scope Management");
}

// 测试符号引用计数
static void test_symbol_reference_count(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Symbol Reference Count");
    
    TEST_INFO("Reference Count", "Testing symbol reference counting");
    
    SZrSymbolTable *table = ZrSymbolTableNew(state);
    if (table == ZR_NULL) {
        TEST_FAIL(timer, "Symbol Reference Count", "Failed to create symbol table");
        return;
    }
    
    SZrString *name = ZrStringCreate(state, "testVar", 7);
    SZrFileRange location = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 1, 0),
        ZrFilePositionCreate(7, 1, 7),
        ZR_NULL
    );
    
    ZrSymbolTableAddSymbol(state, table, ZR_SYMBOL_VARIABLE, name,
                          location, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL);
    
    SZrSymbol *symbol = ZrSymbolTableLookup(table, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Reference Count", "Failed to lookup symbol");
        return;
    }
    
    // 添加引用
    SZrFileRange refLocation = ZrFileRangeCreate(
        ZrFilePositionCreate(10, 2, 0),
        ZrFilePositionCreate(17, 2, 7),
        ZR_NULL
    );
    
    ZrSymbolAddReference(state, symbol, refLocation);
    
    TZrSize refCount = ZrSymbolGetReferenceCount(symbol);
    if (refCount != 1) {
        ZrSymbolTableFree(state, table);
        TEST_FAIL(timer, "Symbol Reference Count", "Reference count mismatch");
        return;
    }
    
    ZrSymbolTableFree(state, table);
    TEST_PASS(timer, "Symbol Reference Count");
}

// 主测试函数
int main() {
    printf("==========\n");
    printf("Language Server - Symbol Table Tests\n");
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
    test_symbol_table_create_and_free(state);
    TEST_DIVIDER();
    
    test_symbol_table_add_and_lookup(state);
    TEST_DIVIDER();
    
    test_symbol_table_scope_management(state);
    TEST_DIVIDER();
    
    test_symbol_reference_count(state);
    TEST_DIVIDER();
    
    // 清理
    ZrGlobalStateFree(global);
    
    printf("\n==========\n");
    printf("All Symbol Table Tests Completed\n");
    printf("==========\n");
    
    return 0;
}
