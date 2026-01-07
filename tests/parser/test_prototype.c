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
#include "zr_vm_core/object.h"
#include "zr_vm_core/module.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_object_conf.h"

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

// 读取文件内容
static char* read_file_content(const char* filename, TZrSize* size) {
    FILE* file = fopen(filename, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = (char*)malloc(fileSize + 1);
    if (content == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }
    
    TZrSize bytesRead = fread(content, 1, fileSize, file);
    content[bytesRead] = '\0';
    fclose(file);
    
    if (size != ZR_NULL) {
        *size = bytesRead;
    }
    
    return content;
}

// 查找测试文件路径
static char* find_test_file(const char* filename) {
    char* paths[] = {
        filename,  // 当前目录
        "",  // 将在下面填充
        "",  // 将在下面填充
        ZR_NULL
    };
    
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != ZR_NULL) {
        TZrSize cwdLen = strlen(cwd);
        TZrSize filenameLen = strlen(filename);
        paths[1] = (char*)malloc(cwdLen + filenameLen + 20);
        if (paths[1] != ZR_NULL) {
            sprintf(paths[1], "%s/%s", cwd, filename);
        }
        
        paths[2] = (char*)malloc(cwdLen + filenameLen + 30);
        if (paths[2] != ZR_NULL) {
            sprintf(paths[2], "%s/tests/parser/%s", cwd, filename);
        }
    }
    
    for (int i = 0; paths[i] != ZR_NULL; i++) {
        FILE* testFile = fopen(paths[i], "r");
        if (testFile != ZR_NULL) {
            fclose(testFile);
            if (i > 0) {
                // 释放其他路径
                for (int j = 0; j < 3; j++) {
                    if (j != i && paths[j] != ZR_NULL && j > 0) {
                        free(paths[j]);
                    }
                }
            }
            return paths[i];
        }
        if (i > 0 && paths[i] != ZR_NULL) {
            free(paths[i]);
        }
    }
    
    return ZR_NULL;
}

// 测试 struct prototype 编译时收集
void test_struct_prototype_compilation(void) {
    TEST_START("Struct Prototype Compilation");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    TEST_INFO("Struct prototype compilation", 
              "Testing that struct declarations are collected during compilation");
    
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, ZR_NULL);
    if (global == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Failed to create global state");
        return;
    }
    
    SZrState *state = global->mainThreadState;
    
    // 读取测试文件
    char* testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Test file not found");
        return;
    }
    
    TZrSize fileSize = 0;
    char* source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Failed to read test file");
        return;
    }
    
    // 创建源文件名
    SZrString *sourceName = ZrStringCreateFromNative(state, testFile);
    
    // 编译源代码
    SZrFunction *func = ZrParserCompileSource(state, source, fileSize, sourceName);
    
    if (func == ZR_NULL) {
        free(source);
        free(testFile);
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Compilation failed");
        return;
    }
    
    // 检查常量池中是否有 prototype 信息
    // 注意：prototype 信息存储在常量池中
    TBool hasPrototypeInfo = ZR_FALSE;
    if (func->constantValueList != ZR_NULL && func->constantValueLength > 0) {
        // 查找字符串常量（类型名称）
        for (TUInt32 i = 0; i < func->constantValueLength; i++) {
            SZrTypeValue *constant = &func->constantValueList[i];
            if (constant->type == ZR_VALUE_TYPE_STRING) {
                SZrString *str = ZR_CAST_STRING(state, constant->value.object);
                if (str != ZR_NULL) {
                    TNativeString nativeStr = ZrStringGetNativeStringShort(str);
                    if (nativeStr != ZR_NULL && strcmp(nativeStr, "Vector3") == 0) {
                        hasPrototypeInfo = ZR_TRUE;
                        break;
                    }
                }
            }
        }
    }
    
    free(source);
    free(testFile);
    ZrGlobalStateFree(global);
    
    timer.endTime = clock();
    
    if (hasPrototypeInfo) {
        TEST_PASS_CUSTOM(timer, "Struct Prototype Compilation");
    } else {
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Prototype information not found in constants");
    }
}

// 测试 class prototype 编译时收集
void test_class_prototype_compilation(void) {
    TEST_START("Class Prototype Compilation");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    TEST_INFO("Class prototype compilation", 
              "Testing that class declarations are collected during compilation");
    
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12346, ZR_NULL);
    if (global == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Failed to create global state");
        return;
    }
    
    SZrState *state = global->mainThreadState;
    
    // 读取测试文件
    char* testFile = find_test_file("test_prototype_class.zr");
    if (testFile == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Test file not found");
        return;
    }
    
    TZrSize fileSize = 0;
    char* source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Failed to read test file");
        return;
    }
    
    // 创建源文件名
    SZrString *sourceName = ZrStringCreateFromNative(state, testFile);
    
    // 编译源代码
    SZrFunction *func = ZrParserCompileSource(state, source, fileSize, sourceName);
    
    if (func == ZR_NULL) {
        free(source);
        free(testFile);
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Compilation failed");
        return;
    }
    
    // 检查常量池中是否有 prototype 信息
    TBool hasPrototypeInfo = ZR_FALSE;
    if (func->constantValueList != ZR_NULL && func->constantValueLength > 0) {
        // 查找字符串常量（类型名称）
        for (TUInt32 i = 0; i < func->constantValueLength; i++) {
            SZrTypeValue *constant = &func->constantValueList[i];
            if (constant->type == ZR_VALUE_TYPE_STRING) {
                SZrString *str = ZR_CAST_STRING(state, constant->value.object);
                if (str != ZR_NULL) {
                    TNativeString nativeStr = ZrStringGetNativeStringShort(str);
                    if (nativeStr != ZR_NULL && strcmp(nativeStr, "Animal") == 0) {
                        hasPrototypeInfo = ZR_TRUE;
                        break;
                    }
                }
            }
        }
    }
    
    free(source);
    free(testFile);
    ZrGlobalStateFree(global);
    
    timer.endTime = clock();
    
    if (hasPrototypeInfo) {
        TEST_PASS_CUSTOM(timer, "Class Prototype Compilation");
    } else {
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Prototype information not found in constants");
    }
}

// 测试 prototype 运行时创建函数
void test_prototype_creation_functions(void) {
    TEST_START("Prototype Creation Functions");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    TEST_INFO("Prototype creation functions", 
              "Testing ZrObjectPrototypeNew and ZrStructPrototypeNew functions");
    
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12347, ZR_NULL);
    if (global == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create global state");
        return;
    }
    
    SZrState *state = global->mainThreadState;
    
    // 创建类型名称
    SZrString *typeName = ZrStringCreateFromNative(state, "TestStruct");
    if (typeName == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create type name string");
        return;
    }
    
    // 测试创建 StructPrototype
    SZrStructPrototype *structPrototype = ZrStructPrototypeNew(state, typeName);
    if (structPrototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create StructPrototype");
        return;
    }
    
    // 验证 prototype 属性
    if (structPrototype->super.name != typeName) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "StructPrototype name mismatch");
        return;
    }
    
    if (structPrototype->super.type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "StructPrototype type mismatch");
        return;
    }
    
    // 测试创建 ObjectPrototype (Class)
    SZrString *className = ZrStringCreateFromNative(state, "TestClass");
    if (className == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create class name string");
        return;
    }
    
    SZrObjectPrototype *classPrototype = ZrObjectPrototypeNew(state, className, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (classPrototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create ObjectPrototype");
        return;
    }
    
    // 验证 prototype 属性
    if (classPrototype->name != className) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "ObjectPrototype name mismatch");
        return;
    }
    
    if (classPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "ObjectPrototype type mismatch");
        return;
    }
    
    ZrGlobalStateFree(global);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Creation Functions");
}

// 测试 prototype 继承关系设置
void test_prototype_inheritance(void) {
    TEST_START("Prototype Inheritance");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    TEST_INFO("Prototype inheritance", 
              "Testing ZrObjectPrototypeSetSuper function");
    
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12348, ZR_NULL);
    if (global == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create global state");
        return;
    }
    
    SZrState *state = global->mainThreadState;
    
    // 创建父类 prototype
    SZrString *parentName = ZrStringCreateFromNative(state, "Parent");
    SZrObjectPrototype *parentPrototype = ZrObjectPrototypeNew(state, parentName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (parentPrototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create parent prototype");
        return;
    }
    
    // 创建子类 prototype
    SZrString *childName = ZrStringCreateFromNative(state, "Child");
    SZrObjectPrototype *childPrototype = ZrObjectPrototypeNew(state, childName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (childPrototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create child prototype");
        return;
    }
    
    // 设置继承关系
    ZrObjectPrototypeSetSuper(state, childPrototype, parentPrototype);
    
    // 验证继承关系
    if (childPrototype->superPrototype != parentPrototype) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Inheritance relationship not set correctly");
        return;
    }
    
    ZrGlobalStateFree(global);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Inheritance");
}

// 测试 prototype 模块导出
void test_prototype_module_export(void) {
    TEST_START("Prototype Module Export");
    SZrTestTimer timer;
    timer.startTime = clock();
    
    TEST_INFO("Prototype module export", 
              "Testing that prototypes are exported to module correctly");
    
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12349, ZR_NULL);
    if (global == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Failed to create global state");
        return;
    }
    
    SZrState *state = global->mainThreadState;
    
    // 创建模块
    struct SZrObjectModule *module = ZrModuleCreate(state);
    if (module == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Failed to create module");
        return;
    }
    
    SZrString *moduleName = ZrStringCreateFromNative(state, "test_module");
    TUInt64 pathHash = ZrModuleCalculatePathHash(state, moduleName);
    ZrModuleSetInfo(state, module, moduleName, pathHash, moduleName);
    
    // 创建 prototype
    SZrString *typeName = ZrStringCreateFromNative(state, "TestType");
    SZrObjectPrototype *prototype = ZrObjectPrototypeNew(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (prototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Failed to create prototype");
        return;
    }
    
    // 导出 prototype
    SZrTypeValue prototypeValue;
    ZrValueInitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    
    ZrModuleAddPubExport(state, module, typeName, &prototypeValue);
    
    // 验证导出
    const SZrTypeValue *exportedValue = ZrModuleGetPubExport(state, module, typeName);
    if (exportedValue == ZR_NULL) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Prototype not found in module exports");
        return;
    }
    
    if (exportedValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Exported value type mismatch");
        return;
    }
    
    SZrObjectPrototype *exportedPrototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exportedValue->value.object);
    if (exportedPrototype != prototype) {
        ZrGlobalStateFree(global);
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Exported prototype mismatch");
        return;
    }
    
    ZrGlobalStateFree(global);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Module Export");
}

// 主测试函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    RUN_TEST(test_struct_prototype_compilation);
    TEST_DIVIDER();
    
    RUN_TEST(test_class_prototype_compilation);
    TEST_DIVIDER();
    
    RUN_TEST(test_prototype_creation_functions);
    TEST_DIVIDER();
    
    RUN_TEST(test_prototype_inheritance);
    TEST_DIVIDER();
    
    RUN_TEST(test_prototype_module_export);
    TEST_MODULE_DIVIDER();
    
    return UNITY_END();
}

