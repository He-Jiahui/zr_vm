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

/** 仅记录单个场景的 clock() 起止值用于日志；耗时不参与通过条件。 */
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

/** TEST_FAIL 写入，main 最终将累计值折算为 CTest 退出状态。 */
static int g_failures = 0;

/** 每个测试函数自带 timer；这些局部宏统一日志和失败计数。 */
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

/** 独立符号表测试用此回调建立 VM 全局状态；测试对象都通过同一分配器生命周期管理。 */
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

/** 由 CTest 入口检查构造后全局作用域存在，以及符号表无需额外场景即可安全释放。 */
static void test_symbol_table_create_and_free(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Symbol Table Creation and Free");
    
    TEST_INFO("Symbol Table Creation", "Creating and freeing symbol table");
    
    SZrSymbolTable *table = ZrLanguageServer_SymbolTable_New(state);
    if (table == ZR_NULL) {
        TEST_FAIL(timer, "Symbol Table Creation and Free", "Failed to create symbol table");
        return;
    }
    
    if (table->globalScope == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Table Creation and Free", "Global scope is NULL");
        return;
    }
    
    ZrLanguageServer_SymbolTable_Free(state, table);
    TEST_PASS(timer, "Symbol Table Creation and Free");
}

/** 登记变量后按名称查找并检查种类，覆盖语义分析器建立符号索引所依赖的基础路径。 */
static void test_symbol_table_add_and_lookup(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Symbol Table Add and Lookup");
    
    TEST_INFO("Add Symbol", "Adding variable symbol to symbol table");
    
    SZrSymbolTable *table = ZrLanguageServer_SymbolTable_New(state);
    if (table == ZR_NULL) {
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Failed to create symbol table");
        return;
    }
    
    // 创建测试符号名称
    SZrString *name = ZrCore_String_Create(state, "testVar", 7);
    if (name == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Failed to create string");
        return;
    }
    
    // 创建文件范围
    SZrFileRange location = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(7, 1, 7),
        ZR_NULL
    );
    
    // 添加符号
    TZrBool success = ZrLanguageServer_SymbolTable_AddSymbolEx(state, table, ZR_SYMBOL_VARIABLE, name,
                                              location, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL, ZR_NULL);
    if (!success) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Failed to add symbol");
        return;
    }
    
    // 查找符号
    SZrSymbol *found = ZrLanguageServer_SymbolTable_Lookup(table, name, ZR_NULL);
    if (found == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Failed to lookup symbol");
        return;
    }
    
    // 验证符号信息
    if (found->type != ZR_SYMBOL_VARIABLE) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Table Add and Lookup", "Symbol type mismatch");
        return;
    }
    
    ZrLanguageServer_SymbolTable_Free(state, table);
    TEST_PASS(timer, "Symbol Table Add and Lookup");
}

/** 进入局部作用域再退出，验证当前作用域回到全局节点，防止后续符号解析遗留在子作用域。 */
static void test_symbol_table_scope_management(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Symbol Table Scope Management");
    
    TEST_INFO("Scope Management", "Testing scope enter and exit");
    
    SZrSymbolTable *table = ZrLanguageServer_SymbolTable_New(state);
    if (table == ZR_NULL) {
        TEST_FAIL(timer, "Symbol Table Scope Management", "Failed to create symbol table");
        return;
    }
    
    // 进入新作用域
    SZrFileRange scopeRange = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(100, 10, 0),
        ZR_NULL
    );
    
    ZrLanguageServer_SymbolTable_EnterScope(state, table, scopeRange, ZR_FALSE, ZR_FALSE, ZR_FALSE);
    
    SZrSymbolScope *currentScope = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
    if (currentScope == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Table Scope Management", "Current scope is NULL after enter");
        return;
    }
    
    // 退出作用域
    ZrLanguageServer_SymbolTable_ExitScope(table);
    
    // 验证回到全局作用域
    SZrSymbolScope *globalScope = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
    if (globalScope != table->globalScope) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Table Scope Management", "Failed to return to global scope");
        return;
    }
    
    ZrLanguageServer_SymbolTable_Free(state, table);
    TEST_PASS(timer, "Symbol Table Scope Management");
}

/** 通过符号表取得符号并登记引用，验证位置范围保留在符号自身的引用数组中供导航使用。 */
static void test_symbol_reference_storage(SZrState *state) {
    SZrTestTimer timer;
    TEST_START("Symbol Reference Storage");
    
    TEST_INFO("Reference Storage", "Testing the single retained symbol reference array");
    
    SZrSymbolTable *table = ZrLanguageServer_SymbolTable_New(state);
    if (table == ZR_NULL) {
        TEST_FAIL(timer, "Symbol Reference Storage", "Failed to create symbol table");
        return;
    }
    
    SZrString *name = ZrCore_String_Create(state, "testVar", 7);
    SZrFileRange location = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 1, 0),
        ZrParser_FilePosition_Create(7, 1, 7),
        ZR_NULL
    );
    
    ZrLanguageServer_SymbolTable_AddSymbolEx(state, table, ZR_SYMBOL_VARIABLE, name,
                            location, ZR_NULL, ZR_ACCESS_PUBLIC, ZR_NULL, ZR_NULL);
    
    SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(table, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Reference Storage", "Failed to lookup symbol");
        return;
    }
    
    // 添加引用
    SZrFileRange refLocation = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(10, 2, 0),
        ZrParser_FilePosition_Create(17, 2, 7),
        ZR_NULL
    );
    
    ZrLanguageServer_Symbol_AddReference(state, symbol, refLocation);
    
    SZrFileRange *storedReference = symbol->references.length == 1
        ? (SZrFileRange *)ZrCore_Array_Get(&symbol->references, 0)
        : ZR_NULL;
    if (storedReference == ZR_NULL ||
        storedReference->start.offset != refLocation.start.offset ||
        storedReference->end.offset != refLocation.end.offset) {
        ZrLanguageServer_SymbolTable_Free(state, table);
        TEST_FAIL(timer, "Symbol Reference Storage", "Stored reference range mismatch");
        return;
    }
    
    ZrLanguageServer_SymbolTable_Free(state, table);
    TEST_PASS(timer, "Symbol Reference Storage");
}

/** CTest 启动此独立目标；建立 VM 状态，顺序执行符号表生命周期、查找、作用域与引用场景。 */
int main(void) {
    printf("==========\n");
    printf("Language Server - Symbol Table Tests\n");
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
    test_symbol_table_create_and_free(state);
    TEST_DIVIDER();
    
    test_symbol_table_add_and_lookup(state);
    TEST_DIVIDER();
    
    test_symbol_table_scope_management(state);
    TEST_DIVIDER();
    
    test_symbol_reference_storage(state);
    TEST_DIVIDER();
    
    // 清理
    ZrCore_GlobalState_Free(global);
    
    printf("\n==========\n");
    if (g_failures == 0) {
        printf("All Symbol Table Tests Completed\n");
    } else {
        printf("%d Symbol Table Test(s) Failed\n", g_failures);
    }
    printf("==========\n");
    
    return g_failures == 0 ? 0 : 1;
}

