//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

// Unity 测试宏扩展 - 添加缺失的 UINT64 不等断言
#ifndef TEST_ASSERT_NOT_EQUAL_UINT64
#define TEST_ASSERT_NOT_EQUAL_UINT64(expected, actual)                                             TEST_ASSERT_NOT_EQUAL((expected), (actual))
#endif

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

// 测试日志宏（符合测试规范）
#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_INFO(summary, details)                                                                                    \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                       \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                    \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason)                                                                       \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                       \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason);                                      \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_MODULE_DIVIDER()                                                                                          \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

// 简单的测试分配器
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr) pointer >= (TZrPtr) 0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    } else {
        if ((TZrPtr) pointer >= (TZrPtr) 0x1000) {
            return realloc(pointer, newSize);
        } else {
            return malloc(newSize);
        }
    }
}

// 创建测试用的SZrState
static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, &callbacks);
    if (!global)
        return ZR_NULL;

    SZrState *mainState = global->mainThreadState;
    if (mainState) {
        ZrCore_GlobalState_InitRegistry(mainState, global);
        // 注册 parser 模块
        ZrParser_ToGlobalState_Register(mainState);
    }

    return mainState;
}

// 销毁测试用的SZrState
static void destroy_test_state(SZrState *state) {
    if (!state)
        return;

    SZrGlobalState *global = state->global;
    if (global) {
        ZrCore_GlobalState_Free(global);
    }
}

static SZrObjectPrototype *get_module_exported_prototype(SZrState *state,
                                                         SZrObjectModule *module,
                                                         const TZrChar *typeName) {
    SZrString *name;
    const SZrTypeValue *exportedValue;
    SZrObject *object;

    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    name = ZrCore_String_Create(state, (TZrNativeString)typeName, strlen(typeName));
    if (name == ZR_NULL) {
        return ZR_NULL;
    }

    exportedValue = ZrCore_Module_GetPubExport(state, module, name);
    if (exportedValue == ZR_NULL || exportedValue->type != ZR_VALUE_TYPE_OBJECT || exportedValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, exportedValue->value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)object;
}

static const SZrManagedFieldInfo *find_managed_field_info(const SZrObjectPrototype *prototype,
                                                          SZrState *state,
                                                          const TZrChar *fieldName) {
    SZrString *expectedName;

    if (prototype == ZR_NULL || state == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    expectedName = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (expectedName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 i = 0; i < prototype->managedFieldCount; i++) {
        const SZrManagedFieldInfo *info = &prototype->managedFields[i];
        if (info->name != ZR_NULL && ZrCore_String_Equal(info->name, expectedName)) {
            return info;
        }
    }

    return ZR_NULL;
}

static TZrBool lookup_struct_field_offset(SZrState *state,
                                        SZrStructPrototype *prototype,
                                        const TZrChar *fieldName,
                                        TZrUInt64 *outOffset) {
    SZrString *fieldNameString;
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL || outOffset == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldNameString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    pair = ZrCore_HashSet_Find(state, &prototype->keyOffsetMap, &key);
    if (pair == ZR_NULL) {
        return ZR_FALSE;
    }

    *outOffset = pair->value.value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

// 测试初始化和清理
void setUp(void) {}

void tearDown(void) {}

// ==================== 1. 脚本级变量作为 __entry 局部变量 ====================

void test_script_level_variables_as_entry_locals(void) {
    SZrTestTimer timer;
    const char *testSummary = "Script Level Variables as __entry Locals";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Script level variables compilation",
              "Testing that script-level variables are compiled as __entry function local variables");

    const char *source = "var x = 1;\nvar y = 2;\nvar z = x + y;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func != ZR_NULL) {
        // 检查函数是否有局部变量
        TEST_ASSERT_TRUE(func->localVariableLength >= 3); // 至少应该有 x, y, z 三个局部变量

        // 检查局部变量名
        TZrBool foundX = ZR_FALSE;
        TZrBool foundY = ZR_FALSE;
        TZrBool foundZ = ZR_FALSE;

        for (TZrUInt32 i = 0; i < func->localVariableLength; i++) {
            SZrString *varName = func->localVariableList[i].name;
            if (varName != ZR_NULL) {
                SZrString *xName = ZrCore_String_Create(state, "x", 1);
                SZrString *yName = ZrCore_String_Create(state, "y", 1);
                SZrString *zName = ZrCore_String_Create(state, "z", 1);
                if (ZrCore_String_Equal(varName, xName))
                    foundX = ZR_TRUE;
                if (ZrCore_String_Equal(varName, yName))
                    foundY = ZR_TRUE;
                if (ZrCore_String_Equal(varName, zName))
                    foundZ = ZR_TRUE;
            }
        }

        TEST_ASSERT_TRUE(foundX);
        TEST_ASSERT_TRUE(foundY);
        TEST_ASSERT_TRUE(foundZ);

        ZrCore_Function_Free(state, func);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 2. pub/pri/pro 可见性修饰符解析 ====================

void test_visibility_modifiers_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Visibility Modifiers Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Visibility modifiers parsing", "Testing parsing of pub/pri/pro visibility modifiers for variables");

    // 测试 pub var
    const char *source1 = "pub var x = 1;";
    SZrString *sourceName1 = ZrCore_String_Create(state, "test1.zr", 8);
    SZrAstNode *ast1 = ZrParser_Parse(state, source1, strlen(source1), sourceName1);
    TEST_ASSERT_NOT_NULL(ast1);
    if (ast1 != ZR_NULL) {
        if (ast1->type == ZR_AST_SCRIPT && ast1->data.script.statements != ZR_NULL) {
            if (ast1->data.script.statements->count > 0) {
                SZrAstNode *stmt = ast1->data.script.statements->nodes[0];
                if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
                    EZrAccessModifier access = stmt->data.variableDeclaration.accessModifier;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, access);
                }
            }
        }
        ZrParser_Ast_Free(state, ast1);
    }

    // 测试 pri var（默认）
    const char *source2 = "var y = 2;";
    SZrString *sourceName2 = ZrCore_String_Create(state, "test2.zr", 8);
    SZrAstNode *ast2 = ZrParser_Parse(state, source2, strlen(source2), sourceName2);
    TEST_ASSERT_NOT_NULL(ast2);
    if (ast2 != ZR_NULL) {
        if (ast2->type == ZR_AST_SCRIPT && ast2->data.script.statements != ZR_NULL) {
            if (ast2->data.script.statements->count > 0) {
                SZrAstNode *stmt = ast2->data.script.statements->nodes[0];
                if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
                    EZrAccessModifier access = stmt->data.variableDeclaration.accessModifier;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PRIVATE, access);
                }
            }
        }
        ZrParser_Ast_Free(state, ast2);
    }

    // 测试 pro var
    const char *source3 = "pro var z = 3;";
    SZrString *sourceName3 = ZrCore_String_Create(state, "test3.zr", 8);
    SZrAstNode *ast3 = ZrParser_Parse(state, source3, strlen(source3), sourceName3);
    TEST_ASSERT_NOT_NULL(ast3);
    if (ast3 != ZR_NULL) {
        if (ast3->type == ZR_AST_SCRIPT && ast3->data.script.statements != ZR_NULL) {
            if (ast3->data.script.statements->count > 0) {
                SZrAstNode *stmt = ast3->data.script.statements->nodes[0];
                if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
                    EZrAccessModifier access = stmt->data.variableDeclaration.accessModifier;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PROTECTED, access);
                }
            }
        }
        ZrParser_Ast_Free(state, ast3);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 3. 模块导出收集（pub 和 pro 分开存储）====================

void test_module_export_collection(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Export Collection";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Module export collection",
              "Testing that pub and pro variables are collected and stored separately in module object");

    const char *source = "pub var pubVar = 10;\npro var proVar = 20;\npri var priVar = 30;";
    SZrString *sourceName = ZrCore_String_Create(state, "test_module.zr", 14);
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);

    if (func != ZR_NULL) {
        // 检查导出变量信息
        TEST_ASSERT_TRUE(func->exportedVariableLength >= 2); // 至少应该有 pubVar 和 proVar

        TZrBool foundPubVar = ZR_FALSE;
        TZrBool foundProVar = ZR_FALSE;
        TZrBool foundPriVar = ZR_FALSE;

        for (TZrUInt32 i = 0; i < func->exportedVariableLength; i++) {
            SZrString *varName = func->exportedVariables[i].name;
            TZrUInt8 accessModifier = func->exportedVariables[i].accessModifier;

            if (varName != ZR_NULL) {
                SZrString *pubVarName = ZrCore_String_Create(state, "pubVar", 6);
                SZrString *proVarName = ZrCore_String_Create(state, "proVar", 6);
                SZrString *priVarName = ZrCore_String_Create(state, "priVar", 6);

                if (ZrCore_String_Equal(varName, pubVarName)) {
                    foundPubVar = ZR_TRUE;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, accessModifier);
                } else if (ZrCore_String_Equal(varName, proVarName)) {
                    foundProVar = ZR_TRUE;
                    TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PROTECTED, accessModifier);
                } else if (ZrCore_String_Equal(varName, priVarName)) {
                    foundPriVar = ZR_TRUE;
                }
            }
        }

        TEST_ASSERT_TRUE(foundPubVar);
        TEST_ASSERT_TRUE(foundProVar);
        TEST_ASSERT_TRUE(!foundPriVar); // pri 变量不应该被导出

        ZrCore_Function_Free(state, func);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 4. 模块缓存机制 ====================

void test_module_cache_operations(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Cache Operations";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Module cache operations", "Testing module cache lookup and add operations with path as key");

    // 创建测试模块
    SZrObjectModule *module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);

    SZrString *moduleName = ZrCore_String_Create(state, "test_module", 11);
    SZrString *fullPath = ZrCore_String_Create(state, "test/path/module.zr", 19);
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, fullPath);

    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, fullPath);

    // 添加 pub 导出
    SZrString *pubVarName = ZrCore_String_Create(state, "pubVar", 6);
    SZrTypeValue pubValue;
    ZrCore_Value_InitAsInt(state, &pubValue, 100);
    ZrCore_Module_AddPubExport(state, module, pubVarName, &pubValue);

    // 添加到缓存
    SZrString *cacheKey = ZrCore_String_Create(state, "test/path/module.zr", 19);
    ZrCore_Module_AddToCache(state, cacheKey, module);

    // 从缓存获取
    SZrObjectModule *cachedModule = ZrCore_Module_GetFromCache(state, cacheKey);
    TEST_ASSERT_NOT_NULL(cachedModule);
    TEST_ASSERT_EQUAL_PTR(module, cachedModule);

    // 验证模块信息
    TEST_ASSERT_EQUAL_UINT64(pathHash, cachedModule->pathHash);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(moduleName, cachedModule->moduleName));

    // 验证导出
    const SZrTypeValue *retrievedPub = ZrCore_Module_GetPubExport(state, cachedModule, pubVarName);
    TEST_ASSERT_NOT_NULL(retrievedPub);
    TEST_ASSERT_EQUAL_INT64(100, retrievedPub->value.nativeObject.nativeInt64);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 5. zr.import 函数调用 ====================

void test_zr_import_function_call(void) {
    SZrTestTimer timer;
    const char *testSummary = "zr.import Function Call";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("zr.import function call", "Testing zr.import native function to load and compile modules");

    // 获取全局 zr 对象
    SZrGlobalState *global = state->global;
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_TRUE(global->zrObject.type == ZR_VALUE_TYPE_OBJECT);

    // 获取 zr.import 函数
    SZrObject *zrObject = ZR_CAST_OBJECT(state, global->zrObject.value.object);
    TEST_ASSERT_NOT_NULL(zrObject);

    SZrString *importName = ZrCore_String_Create(state, "import", 6);
    SZrTypeValue importKey;
    ZrCore_Value_InitAsRawObject(state, &importKey, ZR_CAST_RAW_OBJECT_AS_SUPER(importName));
    importKey.type = ZR_VALUE_TYPE_STRING;

    const SZrTypeValue *importValue = ZrCore_Object_GetValue(state, zrObject, &importKey);
    TEST_ASSERT_NOT_NULL(importValue);
    TEST_ASSERT_TRUE(importValue->type == ZR_VALUE_TYPE_CLOSURE);
    TEST_ASSERT_TRUE(importValue->isNative);
    TEST_ASSERT_NOT_NULL(importValue->value.object);
    {
        SZrClosureNative *importClosure = (SZrClosureNative *) importValue->value.object;
        TEST_ASSERT_NOT_NULL(importClosure);
        TEST_ASSERT_NOT_NULL(importClosure->nativeFunction);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 6. 缓存命中/未命中场景 ====================

void test_module_cache_hit_miss(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Cache Hit/Miss Scenarios";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Module cache hit/miss scenarios", "Testing cache hit and miss scenarios");

    // 测试缓存未命中
    SZrString *nonExistentPath = ZrCore_String_Create(state, "non/existent/path.zr", 21);
    SZrObjectModule *cached1 = ZrCore_Module_GetFromCache(state, nonExistentPath);
    TEST_ASSERT_NULL(cached1);

    // 创建并添加模块到缓存
    SZrObjectModule *module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);

    SZrString *moduleName = ZrCore_String_Create(state, "test", 4);
    SZrString *fullPath = ZrCore_String_Create(state, "test/path.zr", 12);
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, fullPath);
    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, fullPath);

    SZrString *cacheKey = ZrCore_String_Create(state, "test/path.zr", 12);
    ZrCore_Module_AddToCache(state, cacheKey, module);

    // 测试缓存命中
    SZrObjectModule *cached2 = ZrCore_Module_GetFromCache(state, cacheKey);
    TEST_ASSERT_NOT_NULL(cached2);
    TEST_ASSERT_EQUAL_PTR(module, cached2);

    // 测试不同路径的缓存未命中
    SZrString *differentPath = ZrCore_String_Create(state, "different/path.zr", 18);
    SZrObjectModule *cached3 = ZrCore_Module_GetFromCache(state, differentPath);
    TEST_ASSERT_NULL(cached3);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 7. 模块可见性访问控制 ====================

void test_module_visibility_access_control(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Visibility Access Control";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Module visibility access control", "Testing that pub exports are accessible across modules, pro exports "
                                                  "are accessible within same module library");

    // 创建测试模块
    SZrObjectModule *module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);

    SZrString *moduleName = ZrCore_String_Create(state, "test_module", 11);
    SZrString *fullPath = ZrCore_String_Create(state, "test/path.zr", 12);
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, fullPath);
    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, fullPath);

    // 添加 pub 导出
    SZrString *pubVarName = ZrCore_String_Create(state, "pubVar", 6);
    SZrTypeValue pubValue;
    ZrCore_Value_InitAsInt(state, &pubValue, 100);
    ZrCore_Module_AddPubExport(state, module, pubVarName, &pubValue);

    // 添加 pro 导出
    SZrString *proVarName = ZrCore_String_Create(state, "proVar", 6);
    SZrTypeValue proValue;
    ZrCore_Value_InitAsInt(state, &proValue, 200);
    ZrCore_Module_AddProExport(state, module, proVarName, &proValue);

    // 测试 pub 导出访问（跨模块）
    const SZrTypeValue *retrievedPub = ZrCore_Module_GetPubExport(state, module, pubVarName);
    TEST_ASSERT_NOT_NULL(retrievedPub);
    TEST_ASSERT_EQUAL_INT64(100, retrievedPub->value.nativeObject.nativeInt64);

    // 测试 pro 导出访问（同模块库）
    const SZrTypeValue *retrievedPro = ZrCore_Module_GetProExport(state, module, proVarName);
    TEST_ASSERT_NOT_NULL(retrievedPro);
    TEST_ASSERT_EQUAL_INT64(200, retrievedPro->value.nativeObject.nativeInt64);

    // 验证 pro 区域包含 pub（pro 应该包含所有 pub）
    const SZrTypeValue *pubInPro = ZrCore_Module_GetProExport(state, module, pubVarName);
    TEST_ASSERT_NOT_NULL(pubInPro);
    TEST_ASSERT_EQUAL_INT64(100, pubInPro->value.nativeObject.nativeInt64);

    // 验证 pro 变量在 pub 区域不可访问
    const SZrTypeValue *proInPub = ZrCore_Module_GetPubExport(state, module, proVarName);
    TEST_ASSERT_NULL(proInPub); // pro 变量不应该在 pub 区域

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 8. zr 标识符的全局访问和作用域覆盖 ====================

void test_zr_identifier_global_access_and_scope_override(void) {
    SZrTestTimer timer;
    const char *testSummary = "zr Identifier Global Access and Scope Override";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("zr identifier access",
              "Testing that zr identifier is globally accessible but can be overridden by local variables");

    // 测试全局 zr 对象访问
    SZrGlobalState *global = state->global;
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_TRUE(global->zrObject.type == ZR_VALUE_TYPE_OBJECT);

    // 编译一个访问 zr 的脚本
    const char *source1 = "var result = zr;";
    SZrString *sourceName1 = ZrCore_String_Create(state, "test1.zr", 8);
    SZrFunction *func1 = ZrParser_Source_Compile(state, source1, strlen(source1), sourceName1);
    TEST_ASSERT_NOT_NULL(func1);

    // 检查是否生成了 GET_GLOBAL 指令
    TZrBool foundGetGlobal = ZR_FALSE;
    for (TZrUInt32 i = 0; i < func1->instructionsLength; i++) {
        EZrInstructionCode opcode = (EZrInstructionCode) func1->instructionsList[i].instruction.operationCode;
        if (opcode == ZR_INSTRUCTION_ENUM(GET_GLOBAL)) {
            foundGetGlobal = ZR_TRUE;
            break;
        }
    }
    TEST_ASSERT_TRUE(foundGetGlobal);

    ZrCore_Function_Free(state, func1);

    // 测试局部变量覆盖 zr
    const char *source2 = "var zr = 123;\nvar result = zr;";
    SZrString *sourceName2 = ZrCore_String_Create(state, "test2.zr", 8);
    SZrFunction *func2 = ZrParser_Source_Compile(state, source2, strlen(source2), sourceName2);
    TEST_ASSERT_NOT_NULL(func2);

    // 检查是否使用了 GET_STACK 而不是 GET_GLOBAL（因为局部变量覆盖了全局 zr）
    TZrBool foundGetStack = ZR_FALSE;
    TZrBool foundGetGlobal2 = ZR_FALSE;
    for (TZrUInt32 i = 0; i < func2->instructionsLength; i++) {
        EZrInstructionCode opcode = (EZrInstructionCode) func2->instructionsList[i].instruction.operationCode;
        if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK)) {
            foundGetStack = ZR_TRUE;
        }
        if (opcode == ZR_INSTRUCTION_ENUM(GET_GLOBAL)) {
            foundGetGlobal2 = ZR_TRUE;
        }
    }
    // 应该使用 GET_STACK 访问局部变量，而不是 GET_GLOBAL
    TEST_ASSERT_TRUE(foundGetStack);
    // 不应该使用 GET_GLOBAL（因为局部变量覆盖了）
    TEST_ASSERT_TRUE(!foundGetGlobal2);

    ZrCore_Function_Free(state, func2);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 9. 路径哈希计算 ====================

void test_path_hash_calculation(void) {
    SZrTestTimer timer;
    const char *testSummary = "Path Hash Calculation";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Path hash calculation", "Testing path hash calculation using xxhash");

    SZrString *path1 = ZrCore_String_Create(state, "test/path.zr", 12);
    TZrUInt64 hash1 = ZrCore_Module_CalculatePathHash(state, path1);
    TEST_ASSERT_NOT_EQUAL_UINT64(0, hash1);

    // 相同路径应该产生相同哈希
    SZrString *path2 = ZrCore_String_Create(state, "test/path.zr", 12);
    TZrUInt64 hash2 = ZrCore_Module_CalculatePathHash(state, path2);
    TEST_ASSERT_EQUAL_UINT64(hash1, hash2);

    // 不同路径应该产生不同哈希
    SZrString *path3 = ZrCore_String_Create(state, "different/path.zr", 18);
    TZrUInt64 hash3 = ZrCore_Module_CalculatePathHash(state, path3);
    TEST_ASSERT_NOT_EQUAL_UINT64(hash1, hash3);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 10. 完整模块加载流程 ====================

void test_complete_module_loading_flow(void) {
    SZrTestTimer timer;
    const char *testSummary = "Complete Module Loading Flow";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Complete module loading flow",
              "Testing complete flow: compile source -> collect exports -> create module -> cache");

    // 编译包含导出的源代码
    const char *source = "module \"test_module\";\npub var pubVar = 100;\npro var proVar = 200;";
    SZrString *sourceName = ZrCore_String_Create(state, "test_module.zr", 14);
    SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(func);

    // 检查导出信息
    TEST_ASSERT_TRUE(func->exportedVariableLength >= 2);

    // 创建模块对象
    SZrObjectModule *module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);

    SZrString *moduleName = ZrCore_String_Create(state, "test_module", 11);
    SZrString *fullPath = ZrCore_String_Create(state, "test_module.zr", 14);
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, fullPath);
    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, fullPath);

    // 模拟导出收集（从函数的导出信息中收集）
    for (TZrUInt32 i = 0; i < func->exportedVariableLength; i++) {
        SZrString *varName = func->exportedVariables[i].name;
        TZrUInt8 accessModifier = func->exportedVariables[i].accessModifier;

        // 创建测试值
        SZrTypeValue value;
        ZrCore_Value_InitAsInt(state, &value, (TZrInt64) (i + 1) * 100);

        if (accessModifier == ZR_ACCESS_PUBLIC) {
            ZrCore_Module_AddPubExport(state, module, varName, &value);
        } else if (accessModifier == ZR_ACCESS_PROTECTED) {
            ZrCore_Module_AddProExport(state, module, varName, &value);
        }
    }

    // 添加到缓存
    SZrString *cacheKey = ZrCore_String_Create(state, "test_module.zr", 14);
    ZrCore_Module_AddToCache(state, cacheKey, module);

    // 从缓存获取并验证
    SZrObjectModule *cachedModule = ZrCore_Module_GetFromCache(state, cacheKey);
    TEST_ASSERT_NOT_NULL(cachedModule);
    TEST_ASSERT_EQUAL_PTR(module, cachedModule);

    // 验证导出
    SZrString *pubVarName = ZrCore_String_Create(state, "pubVar", 6);
    const SZrTypeValue *pubValue = ZrCore_Module_GetPubExport(state, cachedModule, pubVarName);
    TEST_ASSERT_NOT_NULL(pubValue);

    SZrString *proVarName = ZrCore_String_Create(state, "proVar", 6);
    const SZrTypeValue *proValue = ZrCore_Module_GetProExport(state, cachedModule, proVarName);
    TEST_ASSERT_NOT_NULL(proValue);

    ZrCore_Function_Free(state, func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

void test_module_restores_field_scoped_using_prototype_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Module Restores Field-Scoped Using Metadata";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Field-scoped using runtime metadata",
              "Testing that prototypeData loading restores struct field offsets and managed-field metadata for `using var` fields");

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    {
        const char *source =
            "module \"field_meta\";\n"
            "pub struct HandleBox { using var handle: unique<Resource>; var count: int; }\n"
            "pub class Holder { using var resource: shared<Resource>; var version: int; }";
        SZrString *sourceName = ZrCore_String_Create(state, "field_meta.zr", 13);
        SZrFunction *entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        SZrObjectModule *module;
        SZrObjectPrototype *handleBoxPrototype;
        SZrObjectPrototype *holderPrototype;
        const SZrManagedFieldInfo *handleInfo;
        const SZrManagedFieldInfo *resourceInfo;
        TZrUInt64 handleOffset = 0;
        TZrUInt64 countOffset = 0;
        TZrSize createdCount;

        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_NOT_NULL(entryFunction->prototypeData);
        TEST_ASSERT_EQUAL_UINT32(2, entryFunction->prototypeCount);

        module = ZrCore_Module_Create(state);
        TEST_ASSERT_NOT_NULL(module);
        ZrCore_Module_SetInfo(state,
                        module,
                        ZrCore_String_Create(state, "field_meta", 10),
                        ZrCore_Module_CalculatePathHash(state, ZrCore_String_Create(state, "field_meta.zr", 13)),
                        ZrCore_String_Create(state, "field_meta.zr", 13));

        createdCount = ZrCore_Module_CreatePrototypesFromData(state, module, entryFunction);
        TEST_ASSERT_EQUAL_UINT64(2, createdCount);

        handleBoxPrototype = get_module_exported_prototype(state, module, "HandleBox");
        holderPrototype = get_module_exported_prototype(state, module, "Holder");
        TEST_ASSERT_NOT_NULL(handleBoxPrototype);
        TEST_ASSERT_NOT_NULL(holderPrototype);

        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, handleBoxPrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_CLASS, holderPrototype->type);

        TEST_ASSERT_EQUAL_UINT32(1, handleBoxPrototype->managedFieldCount);
        TEST_ASSERT_EQUAL_UINT32(1, holderPrototype->managedFieldCount);

        handleInfo = find_managed_field_info(handleBoxPrototype, state, "handle");
        resourceInfo = find_managed_field_info(holderPrototype, state, "resource");
        TEST_ASSERT_NOT_NULL(handleInfo);
        TEST_ASSERT_NOT_NULL(resourceInfo);

        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_UNIQUE, handleInfo->ownershipQualifier);
        TEST_ASSERT_TRUE(handleInfo->callsClose);
        TEST_ASSERT_TRUE(handleInfo->callsDestructor);
        TEST_ASSERT_EQUAL_UINT32(0, handleInfo->declarationOrder);

        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_SHARED, resourceInfo->ownershipQualifier);
        TEST_ASSERT_TRUE(resourceInfo->callsClose);
        TEST_ASSERT_TRUE(resourceInfo->callsDestructor);
        TEST_ASSERT_EQUAL_UINT32(0, resourceInfo->declarationOrder);

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)handleBoxPrototype, "handle", &handleOffset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)handleBoxPrototype, "count", &countOffset));
        TEST_ASSERT_EQUAL_UINT64(0, handleOffset);
        TEST_ASSERT_TRUE(countOffset > handleOffset);

        ZrCore_Function_Free(state, entryFunction);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 主函数 ====================

int main(void) {
    UNITY_BEGIN();

    TEST_MODULE_DIVIDER();
    printf("Module System Tests\n");
    TEST_MODULE_DIVIDER();

    // 1. 脚本级变量作为 __entry 局部变量
    RUN_TEST(test_script_level_variables_as_entry_locals);

    // 2. pub/pri/pro 可见性修饰符解析
    RUN_TEST(test_visibility_modifiers_parsing);

    // 3. 模块导出收集
    RUN_TEST(test_module_export_collection);

    // 4. 模块缓存机制
    RUN_TEST(test_module_cache_operations);

    // 5. zr.import 函数调用
    RUN_TEST(test_zr_import_function_call);

    // 6. 缓存命中/未命中场景
    RUN_TEST(test_module_cache_hit_miss);

    // 7. 模块可见性访问控制
    RUN_TEST(test_module_visibility_access_control);

    // 8. zr 标识符的全局访问和作用域覆盖
    RUN_TEST(test_zr_identifier_global_access_and_scope_override);

    // 9. 路径哈希计算
    RUN_TEST(test_path_hash_calculation);

    // 10. 完整模块加载流程
    RUN_TEST(test_complete_module_loading_flow);

    // 11. prototypeData 中 using 字段元数据恢复
    RUN_TEST(test_module_restores_field_scoped_using_prototype_metadata);

    return UNITY_END();
}
