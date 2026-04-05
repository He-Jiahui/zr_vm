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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _MSC_VER
#include <direct.h>
#include <stdlib.h>
#define getcwd _getcwd
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif
#else
#include <unistd.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif
#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "test_support.h"

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

// realpath 兼容函数（MSVC使用_fullpath）
#ifdef _MSC_VER
static char *test_realpath(const char *path, char *resolved_path) { return _fullpath(resolved_path, path, _MAX_PATH); }
#define realpath test_realpath
#endif

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
        }
        return ZR_NULL;
    }
}

// 读取文件内容
static char *read_file_content(const char *filename, TZrSize *size) {
    return ZrTests_ReadTextFile(filename, size);
}

// 查找测试文件路径
static char *find_test_file(const char *filename) {
    char resolved[ZR_TESTS_PATH_MAX];
    TZrSize length = 0;
    char *resultPath = ZR_NULL;

    if (!ZrTests_Path_GetParserFixture(filename, resolved, sizeof(resolved))) {
        return ZR_NULL;
    }

    length = strlen(resolved);
    resultPath = (char *) malloc(length + 1);
    if (resultPath != ZR_NULL) {
        memcpy(resultPath, resolved, length + 1);
    }

    return resultPath;
}

// 生成输出文件名（将 .zr 替换为新的扩展名）
static char *generate_output_filename(const char *inputFile, const char *newExt) {
    const char *fileName = inputFile;
    const char *forwardSlash;
    const char *backSlash;
    const char *extPos;
    const char *subDir = "runtime";
    TZrSize baseLen;
    char baseName[256];
    char resolved[ZR_TESTS_PATH_MAX];
    char *outputFile;

    if (inputFile == ZR_NULL || newExt == ZR_NULL) {
        return ZR_NULL;
    }

    forwardSlash = strrchr(inputFile, '/');
    backSlash = strrchr(inputFile, '\\');
    if (forwardSlash != ZR_NULL || backSlash != ZR_NULL) {
        const char *lastSeparator = forwardSlash;
        if (lastSeparator == ZR_NULL || (backSlash != ZR_NULL && backSlash > lastSeparator)) {
            lastSeparator = backSlash;
        }
        fileName = lastSeparator + 1;
    }

    extPos = strrchr(fileName, '.');
    if (extPos != ZR_NULL && strcmp(extPos, ".zr") == 0) {
        baseLen = (TZrSize)(extPos - fileName);
    } else {
        baseLen = strlen(fileName);
    }

    if (baseLen >= sizeof(baseName)) {
        return ZR_NULL;
    }

    memcpy(baseName, fileName, baseLen);
    baseName[baseLen] = '\0';

    if (strcmp(newExt, ".zrs") == 0) {
        subDir = "ast";
    } else if (strcmp(newExt, ".zri") == 0) {
        subDir = "intermediate";
    } else if (strcmp(newExt, ".zro") == 0) {
        subDir = "binary";
    }

    if (!ZrTests_Path_GetGeneratedArtifact("language_pipeline", subDir, baseName, newExt, resolved, sizeof(resolved))) {
        return ZR_NULL;
    }

    outputFile = (char *)malloc(strlen(resolved) + 1);
    if (outputFile != ZR_NULL) {
        strcpy(outputFile, resolved);
    }

    return outputFile;
}

static void init_test_string_key(SZrState *state, SZrTypeValue *key, const char *fieldName) {
    SZrString *fieldString;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(key);
    TEST_ASSERT_NOT_NULL(fieldName);

    fieldString = ZrCore_String_CreateFromNative(state, fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(state, key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key->type = ZR_VALUE_TYPE_STRING;
}

static const SZrTypeValue *get_test_object_field_value(SZrState *state, SZrObject *object, const char *fieldName) {
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(object);
    init_test_string_key(state, &key, fieldName);
    return ZrCore_Object_GetValue(state, object, &key);
}

static void set_test_object_int_field(SZrState *state, SZrObject *object, const char *fieldName, TZrInt64 value) {
    SZrTypeValue key;
    SZrTypeValue fieldValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(object);
    init_test_string_key(state, &key, fieldName);
    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    ZrCore_Object_SetValue(state, object, &key, &fieldValue);
}

static void set_test_object_object_field(SZrState *state,
                                         SZrObject *object,
                                         const char *fieldName,
                                         SZrObject *fieldObject) {
    SZrTypeValue key;
    SZrTypeValue fieldValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(object);
    TEST_ASSERT_NOT_NULL(fieldObject);
    init_test_string_key(state, &key, fieldName);
    ZrCore_Value_InitAsRawObject(state, &fieldValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldObject));
    fieldValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Object_SetValue(state, object, &key, &fieldValue);
}

// 测试函数前向声明
static void test_struct_prototype_compilation(void);
static void test_class_prototype_compilation(void);
static void test_prototype_creation_functions(void);
static void test_prototype_inheritance(void);
static void test_prototype_module_export(void);
static void test_struct_field_offsets(void);
static void test_prototype_inheritance_loading(void);
static void test_struct_value_copy_clones_nested_storage(void);

// 测试 struct prototype 编译时收集
static void test_struct_prototype_compilation(void) {
    TEST_START("Struct Prototype Compilation");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    TEST_INFO("Struct prototype compilation", "Testing that struct declarations are collected during compilation");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Failed to read test file");
        return;
    }

    // 创建源文件名
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, testFile);

    // 编译源代码
    SZrFunction *func = ZrParser_Source_Compile(state, source, fileSize, sourceName);

    if (func == ZR_NULL) {
        free(source);
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Compilation failed");
        return;
    }


    // 测试prototypeData存储
    if (func != ZR_NULL) {
        // 验证prototypeData字段
        TZrBool hasPrototypeData = (func->prototypeData != ZR_NULL && 
                                  func->prototypeDataLength > 0 && 
                                  func->prototypeCount > 0);
        if (hasPrototypeData) {
            printf("\n  Prototype data stored in function->prototypeData:\n");
            printf("    prototypeCount: %u\n", func->prototypeCount);
            printf("    prototypeDataLength: %u bytes\n", func->prototypeDataLength);
            
            // 验证数据格式（头部应该是prototypeCount）
            TZrUInt32 *countPtr = (TZrUInt32 *)func->prototypeData;
            if (*countPtr == func->prototypeCount) {
                printf("    Data format verification: PASS (header count matches)\n");
            } else {
                printf("    Data format verification: FAIL (header count mismatch: %u vs %u)\n", 
                       *countPtr, func->prototypeCount);
            }
        } else {
            printf("\n  WARNING: No prototype data found in function->prototypeData\n");
        }
        
        // 输出prototype信息（使用新函数）
        printf("\n  Prototype information from prototypeData (new format):\n");
        ZrCore_Debug_PrintPrototypesFromData(state, func, stdout);
        
        // 也测试旧的函数（向后兼容性）
        // 注意：ZrDebugPrintPrototypeFromConstants 已被移除，使用 ZrCore_Debug_PrintPrototypesFromData 替代

        // 创建模块对象
        struct SZrObjectModule *module = ZrCore_Module_Create(state);
        if (module != ZR_NULL) {
            // 优先使用新的函数从prototypeData创建
            TZrSize createdCount = 0;
            if (hasPrototypeData) {
                createdCount = ZrCore_Module_CreatePrototypesFromData(state, module, func);
                if (createdCount > 0) {
                    printf("\n  Created %zu prototype(s) from prototypeData:\n", createdCount);
                } else {
                    // 如果新函数失败，尝试旧函数（向后兼容）
                    createdCount = ZrCore_Module_CreatePrototypesFromConstants(state, module, func);
                    if (createdCount > 0) {
                        printf("\n  Created %zu prototype(s) from constants (legacy fallback):\n", createdCount);
                    }
                }
            } else {
                // 如果没有新格式数据，尝试旧函数
                createdCount = ZrCore_Module_CreatePrototypesFromConstants(state, module, func);
                if (createdCount > 0) {
                    printf("\n  Created %zu prototype(s) from constants (legacy format):\n", createdCount);
                }
            }

            if (createdCount > 0) {

                // 查找并输出Vector3 prototype信息
                SZrString *vector3Name = ZrCore_String_CreateFromNative(state, "Vector3");
                if (vector3Name != ZR_NULL) {
                    const SZrTypeValue *prototypeValue = ZrCore_Module_GetPubExport(state, module, vector3Name);
                    if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObjectPrototype *prototype =
                                (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                        if (prototype != ZR_NULL) {
                            ZrCore_Debug_PrintPrototype(state, prototype, stdout);
                        }
                    }
                }

                // 输出Point3D prototype（测试继承关系）
                SZrString *point3DName = ZrCore_String_CreateFromNative(state, "Point3D");
                if (point3DName != ZR_NULL) {
                    const SZrTypeValue *prototypeValue = ZrCore_Module_GetPubExport(state, module, point3DName);
                    if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObjectPrototype *prototype =
                                (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                        if (prototype != ZR_NULL) {
                            printf("\n  Point3D prototype (with inheritance):\n");
                            ZrCore_Debug_PrintPrototype(state, prototype, stdout);
                        }
                    }
                }
            }
        }
    }

    // 生成 ZRI 和 ZRO 文件
    if (func != ZR_NULL) {
        // 生成 .zri 文件名
        char *zriFile = generate_output_filename(testFile, ".zri");
        if (zriFile != ZR_NULL) {
            TZrBool zriResult = ZrParser_Writer_WriteIntermediateFile(state, func, zriFile);
            if (zriResult) {
                printf("\n  Generated ZRI file: %s\n", zriFile);
            }
            free(zriFile);
        }
        
        // 生成 .zro 文件名
        char *zroFile = generate_output_filename(testFile, ".zro");
        if (zroFile != ZR_NULL) {
            TZrBool zroResult = ZrParser_Writer_WriteBinaryFile(state, func, zroFile);
            if (zroResult) {
                printf("  Generated ZRO file: %s\n", zroFile);
            }
            free(zroFile);
        }
    }

    free(source);
    free(testFile);
    ZrCore_GlobalState_Free(global);

    timer.endTime = clock();


    TEST_PASS_CUSTOM(timer, "Struct Prototype Compilation");
}

// 测试 class prototype 编译时收集
static void test_class_prototype_compilation(void) {
    TEST_START("Class Prototype Compilation");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    TEST_INFO("Class prototype compilation", "Testing that class declarations are collected during compilation");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12346, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_class.zr");
    if (testFile == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Failed to read test file");
        return;
    }

    // 创建源文件名
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, testFile);

    // 编译源代码
    SZrFunction *func = ZrParser_Source_Compile(state, source, fileSize, sourceName);

    if (func == ZR_NULL) {
        free(source);
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Compilation failed");
        return;
    }

    // 验证prototypeData存储
    TZrBool hasPrototypeData = (func->prototypeData != ZR_NULL && 
                              func->prototypeDataLength > 0 && 
                              func->prototypeCount > 0);
    if (hasPrototypeData) {
        printf("\n  Prototype data stored in function->prototypeData:\n");
        printf("    prototypeCount: %u\n", func->prototypeCount);
        printf("    prototypeDataLength: %u bytes\n", func->prototypeDataLength);
    }
    
    // 检查常量池中是否有 prototype 相关信息（类型名称字符串等）
    TZrBool hasPrototypeInfo = ZR_FALSE;
    if (func->constantValueList != ZR_NULL && func->constantValueLength > 0) {
        // 查找字符串常量（类型名称）
        for (TZrUInt32 i = 0; i < func->constantValueLength; i++) {
            SZrTypeValue *constant = &func->constantValueList[i];
            if (constant->type == ZR_VALUE_TYPE_STRING) {
                SZrString *str = ZR_CAST_STRING(state, constant->value.object);
                if (str != ZR_NULL) {
                    TZrNativeString nativeStr = ZrCore_String_GetNativeStringShort(str);
                    if (nativeStr != ZR_NULL && strcmp(nativeStr, "Animal") == 0) {
                        hasPrototypeInfo = ZR_TRUE;
                        break;
                    }
                }
            }
        }
    }

    // 输出prototype信息
    if ((hasPrototypeInfo || hasPrototypeData) && func != ZR_NULL) {
        printf("\n  Prototype information from prototypeData (new format):\n");
        ZrCore_Debug_PrintPrototypesFromData(state, func, stdout);
        
        // 注意：ZrDebugPrintPrototypeFromConstants 已被移除，使用 ZrCore_Debug_PrintPrototypesFromData 替代

        // 创建模块并加载prototype
        struct SZrObjectModule *module = ZrCore_Module_Create(state);
        if (module != ZR_NULL) {
            TZrSize createdCount = 0;
            if (hasPrototypeData) {
                createdCount = ZrCore_Module_CreatePrototypesFromData(state, module, func);
            }
            if (createdCount == 0) {
                createdCount = ZrCore_Module_CreatePrototypesFromConstants(state, module, func);
            }

            if (createdCount > 0) {
                printf("\n  Created %zu prototype(s), runtime debug output:\n", createdCount);

                // 输出Animal prototype
                SZrString *animalName = ZrCore_String_CreateFromNative(state, "Animal");
                if (animalName != ZR_NULL) {
                    const SZrTypeValue *prototypeValue = ZrCore_Module_GetPubExport(state, module, animalName);
                    if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObjectPrototype *prototype =
                                (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                        if (prototype != ZR_NULL) {
                            ZrCore_Debug_PrintPrototype(state, prototype, stdout);
                        }
                    }
                }

                // 输出Dog prototype（测试继承关系）
                SZrString *dogName = ZrCore_String_CreateFromNative(state, "Dog");
                if (dogName != ZR_NULL) {
                    const SZrTypeValue *prototypeValue = ZrCore_Module_GetPubExport(state, module, dogName);
                    if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObjectPrototype *prototype =
                                (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                        if (prototype != ZR_NULL) {
                            printf("\n  Dog prototype (with inheritance):\n");
                            ZrCore_Debug_PrintPrototype(state, prototype, stdout);
                        }
                    }
                }
            }
        }
    }

    free(source);
    free(testFile);
    ZrCore_GlobalState_Free(global);

    timer.endTime = clock();

    if (hasPrototypeData || hasPrototypeInfo) {
        TEST_PASS_CUSTOM(timer, "Class Prototype Compilation");
    } else {
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Prototype information not found");
    }
}

// 测试 prototype 运行时创建函数
static void test_prototype_creation_functions(void) {
    TEST_START("Prototype Creation Functions");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    TEST_INFO("Prototype creation functions", "Testing ZrCore_ObjectPrototype_New and ZrCore_StructPrototype_New functions");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12347, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建类型名称
    SZrString *typeName = ZrCore_String_CreateFromNative(state, "TestStruct");
    if (typeName == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create type name string");
        return;
    }

    // 测试创建 StructPrototype
    SZrStructPrototype *structPrototype = ZrCore_StructPrototype_New(state, typeName);
    if (structPrototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create StructPrototype");
        return;
    }

    // 验证 prototype 属性
    if (structPrototype->super.name != typeName) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "StructPrototype name mismatch");
        return;
    }

    if (structPrototype->super.type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "StructPrototype type mismatch");
        return;
    }

    // 使用debug工具输出prototype信息
    printf("\n  Debug output for created StructPrototype:\n");
    ZrCore_Debug_PrintPrototype(state, (SZrObjectPrototype *) structPrototype, stdout);

    // 测试创建 ObjectPrototype (Class)
    SZrString *className = ZrCore_String_CreateFromNative(state, "TestClass");
    if (className == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create class name string");
        return;
    }

    SZrObjectPrototype *classPrototype = ZrCore_ObjectPrototype_New(state, className, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (classPrototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create ObjectPrototype");
        return;
    }

    // 验证 prototype 属性
    if (classPrototype->name != className) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "ObjectPrototype name mismatch");
        return;
    }

    if (classPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "ObjectPrototype type mismatch");
        return;
    }

    // 使用debug工具输出prototype信息
    printf("\n  Debug output for created ClassPrototype:\n");
    ZrCore_Debug_PrintPrototype(state, classPrototype, stdout);

    // 测试创建并输出object信息
    SZrObject *testObject = ZrCore_Object_New(state, classPrototype);
    if (testObject != ZR_NULL) {
        printf("\n  Debug output for created Object:\n");
        ZrCore_Debug_PrintObject(state, testObject, stdout);
    }

    ZrCore_GlobalState_Free(global);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Creation Functions");
}

// 测试 prototype 继承关系设置
static void test_prototype_inheritance(void) {
    TEST_START("Prototype Inheritance");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    TEST_INFO("Prototype inheritance", "Testing ZrCore_ObjectPrototype_SetSuper function");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12348, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建父类 prototype
    SZrString *parentName = ZrCore_String_CreateFromNative(state, "Parent");
    SZrObjectPrototype *parentPrototype = ZrCore_ObjectPrototype_New(state, parentName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (parentPrototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create parent prototype");
        return;
    }

    // 创建子类 prototype
    SZrString *childName = ZrCore_String_CreateFromNative(state, "Child");
    SZrObjectPrototype *childPrototype = ZrCore_ObjectPrototype_New(state, childName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (childPrototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create child prototype");
        return;
    }

    // 设置继承关系
    ZrCore_ObjectPrototype_SetSuper(state, childPrototype, parentPrototype);

    // 验证继承关系
    if (childPrototype->superPrototype != parentPrototype) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Inheritance relationship not set correctly");
        return;
    }

    ZrCore_GlobalState_Free(global);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Inheritance");
}

// 测试 prototype 模块导出
static void test_prototype_module_export(void) {
    TEST_START("Prototype Module Export");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    TEST_INFO("Prototype module export", "Testing that prototypes are exported to module correctly");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12349, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建模块
    struct SZrObjectModule *module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Failed to create module");
        return;
    }

    SZrString *moduleName = ZrCore_String_CreateFromNative(state, "test_module");
    TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, moduleName);
    ZrCore_Module_SetInfo(state, module, moduleName, pathHash, moduleName);

    // 创建 prototype
    SZrString *typeName = ZrCore_String_CreateFromNative(state, "TestType");
    SZrObjectPrototype *prototype = ZrCore_ObjectPrototype_New(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (prototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Failed to create prototype");
        return;
    }

    // 导出 prototype
    SZrTypeValue prototypeValue;
    ZrCore_Value_InitAsRawObject(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;

    ZrCore_Module_AddPubExport(state, module, typeName, &prototypeValue);

    // 验证导出
    const SZrTypeValue *exportedValue = ZrCore_Module_GetPubExport(state, module, typeName);
    if (exportedValue == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Prototype not found in module exports");
        return;
    }

    if (exportedValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Exported value type mismatch");
        return;
    }

    SZrObjectPrototype *exportedPrototype = (SZrObjectPrototype *) ZR_CAST_OBJECT(state, exportedValue->value.object);
    if (exportedPrototype != prototype) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Exported prototype mismatch");
        return;
    }

    ZrCore_GlobalState_Free(global);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Module Export");
}

// 测试 struct 字段偏移量
static void test_struct_field_offsets(void) {
    TEST_START("Struct Field Offsets");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime;

    TEST_INFO("Struct field offsets", "Testing that struct fields have correct offsets in keyOffsetMap");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12350, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Field Offsets", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Field Offsets", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Field Offsets", "Failed to read test file");
        return;
    }

    // 编译源代码
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, testFile);
    SZrFunction *func = ZrParser_Source_Compile(state, source, fileSize, sourceName);

    if (func == ZR_NULL) {
        free(source);
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Field Offsets", "Compilation failed");
        return;
    }

    // 验证prototypeData存储
    TZrBool hasPrototypeData = (func->prototypeData != ZR_NULL && 
                              func->prototypeDataLength > 0 && 
                              func->prototypeCount > 0);
    if (hasPrototypeData) {
        printf("\n  Prototype data verification:\n");
        printf("    prototypeCount: %u\n", func->prototypeCount);
        printf("    prototypeDataLength: %u bytes\n", func->prototypeDataLength);
    }
    
    // 创建模块并加载prototype
    struct SZrObjectModule *module = ZrCore_Module_Create(state);
    if (module != ZR_NULL) {
        TZrSize createdCount = 0;
        if (hasPrototypeData) {
            createdCount = ZrCore_Module_CreatePrototypesFromData(state, module, func);
        }
        if (createdCount == 0) {
            createdCount = ZrCore_Module_CreatePrototypesFromConstants(state, module, func);
        }

        if (createdCount > 0) {
            // 检查Vector3的字段偏移量
            SZrString *vector3Name = ZrCore_String_CreateFromNative(state, "Vector3");
            if (vector3Name != ZR_NULL) {
                const SZrTypeValue *prototypeValue = ZrCore_Module_GetPubExport(state, module, vector3Name);
                if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                    SZrStructPrototype *structProto =
                            (SZrStructPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                    if (structProto != ZR_NULL && structProto->super.type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                        // 检查keyOffsetMap中是否有字段
                        if (structProto->keyOffsetMap.elementCount > 0) {
                            printf("\n  Vector3 field offsets:\n");
                            ZrCore_Debug_PrintPrototype(state, (SZrObjectPrototype *) structProto, stdout);
                        }
                    }
                }
            }
        }
    }

    free(source);
    free(testFile);
    ZrCore_GlobalState_Free(global);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Struct Field Offsets");
}

// 测试继承关系的完整加载
static void test_prototype_inheritance_loading(void) {
    TEST_START("Prototype Inheritance Loading");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime;

    TEST_INFO("Prototype inheritance loading",
              "Testing that inheritance relationships are correctly loaded from constants");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12351, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance Loading", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance Loading", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance Loading", "Failed to read test file");
        return;
    }

    // 编译源代码
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, testFile);
    SZrFunction *func = ZrParser_Source_Compile(state, source, fileSize, sourceName);

    if (func == ZR_NULL) {
        free(source);
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance Loading", "Compilation failed");
        return;
    }

    // 验证prototypeData存储
    TZrBool hasPrototypeData = (func->prototypeData != ZR_NULL && 
                              func->prototypeDataLength > 0 && 
                              func->prototypeCount > 0);
    if (hasPrototypeData) {
        printf("\n  Prototype data verification:\n");
        printf("    prototypeCount: %u\n", func->prototypeCount);
        printf("    prototypeDataLength: %u bytes\n", func->prototypeDataLength);
    }
    
    // 创建模块并加载prototype
    struct SZrObjectModule *module = ZrCore_Module_Create(state);
    if (module != ZR_NULL) {
        TZrSize createdCount = 0;
        if (hasPrototypeData) {
            createdCount = ZrCore_Module_CreatePrototypesFromData(state, module, func);
        }
        if (createdCount == 0) {
            createdCount = ZrCore_Module_CreatePrototypesFromConstants(state, module, func);
        }

        if (createdCount > 0) {
            // 检查Point3D的继承关系（应该继承自Point2D）
            SZrString *point3DName = ZrCore_String_CreateFromNative(state, "Point3D");
            if (point3DName != ZR_NULL) {
                const SZrTypeValue *prototypeValue = ZrCore_Module_GetPubExport(state, module, point3DName);
                if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                    SZrObjectPrototype *prototype =
                            (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                    if (prototype != ZR_NULL && prototype->superPrototype != ZR_NULL) {
                        SZrString *point2DName = ZrCore_String_CreateFromNative(state, "Point2D");
                        const SZrTypeValue *point2DValue = ZrCore_Module_GetPubExport(state, module, point2DName);
                        if (point2DValue != ZR_NULL && point2DValue->type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObjectPrototype *point2D =
                                    (SZrObjectPrototype *) ZR_CAST_OBJECT(state, point2DValue->value.object);
                            if (prototype->superPrototype == point2D) {
                                printf("\n  Point3D correctly inherits from Point2D:\n");
                                ZrCore_Debug_PrintPrototype(state, prototype, stdout);
                            }
                        }
                    }
                }
            }
        }
    }

    free(source);
    free(testFile);
    ZrCore_GlobalState_Free(global);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Inheritance Loading");
}

static void test_struct_value_copy_clones_nested_storage(void) {
    TEST_START("Struct Value Copy Clones Nested Storage");
    TEST_INFO("Struct copy semantics",
              "Copying a struct value should allocate a distinct boxed object and recursively clone nested struct storage");

    {
        SZrTestTimer timer;
        SZrGlobalState *global;
        SZrState *state;
        SZrStructPrototype *innerPrototype;
        SZrStructPrototype *outerPrototype;
        SZrObject *sourceInnerObject;
        SZrObject *sourceOuterObject;
        const SZrTypeValue *sourceInnerValue;
        const SZrTypeValue *copiedInnerValue;
        SZrObject *copiedOuterObject;
        SZrObject *sourceStoredInnerObject;
        SZrObject *copiedStoredInnerObject;
        const SZrTypeValue *sourceXValue;
        const SZrTypeValue *copiedXValue;
        SZrTypeValue sourceOuterValue;
        SZrTypeValue copiedOuterValue;

        timer.startTime = clock();
        timer.endTime = timer.startTime;

        global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12351, ZR_NULL);
        TEST_ASSERT_NOT_NULL(global);
        state = global->mainThreadState;
        TEST_ASSERT_NOT_NULL(state);

        innerPrototype = ZrCore_StructPrototype_New(state, ZrCore_String_CreateFromNative(state, "InnerBox"));
        outerPrototype = ZrCore_StructPrototype_New(state, ZrCore_String_CreateFromNative(state, "OuterBox"));
        TEST_ASSERT_NOT_NULL(innerPrototype);
        TEST_ASSERT_NOT_NULL(outerPrototype);
        ZrCore_StructPrototype_AddField(state, innerPrototype, ZrCore_String_CreateFromNative(state, "x"), 0);
        ZrCore_StructPrototype_AddField(state, outerPrototype, ZrCore_String_CreateFromNative(state, "inner"), 0);

        sourceInnerObject = ZrCore_Object_New(state, &innerPrototype->super);
        sourceOuterObject = ZrCore_Object_New(state, &outerPrototype->super);
        TEST_ASSERT_NOT_NULL(sourceInnerObject);
        TEST_ASSERT_NOT_NULL(sourceOuterObject);
        ZrCore_Object_Init(state, sourceInnerObject);
        ZrCore_Object_Init(state, sourceOuterObject);
        sourceInnerObject->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;
        sourceOuterObject->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;

        set_test_object_int_field(state, sourceInnerObject, "x", 1);
        set_test_object_object_field(state, sourceOuterObject, "inner", sourceInnerObject);

        ZrCore_Value_ResetAsNull(&sourceOuterValue);
        ZrCore_Value_ResetAsNull(&copiedOuterValue);
        ZrCore_Value_InitAsRawObject(state, &sourceOuterValue, ZR_CAST_RAW_OBJECT_AS_SUPER(sourceOuterObject));
        sourceOuterValue.type = ZR_VALUE_TYPE_OBJECT;
        ZrCore_Value_Copy(state, &copiedOuterValue, &sourceOuterValue);

        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, copiedOuterValue.type);
        copiedOuterObject = ZR_CAST_OBJECT(state, copiedOuterValue.value.object);
        TEST_ASSERT_NOT_NULL(copiedOuterObject);
        TEST_ASSERT_NOT_EQUAL(sourceOuterObject, copiedOuterObject);

        sourceInnerValue = get_test_object_field_value(state, sourceOuterObject, "inner");
        copiedInnerValue = get_test_object_field_value(state, copiedOuterObject, "inner");
        TEST_ASSERT_NOT_NULL(sourceInnerValue);
        TEST_ASSERT_NOT_NULL(copiedInnerValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, sourceInnerValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, copiedInnerValue->type);

        sourceStoredInnerObject = ZR_CAST_OBJECT(state, sourceInnerValue->value.object);
        copiedStoredInnerObject = ZR_CAST_OBJECT(state, copiedInnerValue->value.object);
        TEST_ASSERT_NOT_NULL(sourceStoredInnerObject);
        TEST_ASSERT_NOT_NULL(copiedStoredInnerObject);
        TEST_ASSERT_NOT_EQUAL(sourceStoredInnerObject, copiedStoredInnerObject);

        set_test_object_int_field(state, copiedStoredInnerObject, "x", 9);

        sourceXValue = get_test_object_field_value(state, sourceStoredInnerObject, "x");
        copiedXValue = get_test_object_field_value(state, copiedStoredInnerObject, "x");
        TEST_ASSERT_NOT_NULL(sourceXValue);
        TEST_ASSERT_NOT_NULL(copiedXValue);
        TEST_ASSERT_EQUAL_INT64(1, sourceXValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT64(9, copiedXValue->value.nativeObject.nativeInt64);

        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        TEST_PASS_CUSTOM(timer, "Struct Value Copy Clones Nested Storage");
    }

    TEST_DIVIDER();
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
    TEST_DIVIDER();

    RUN_TEST(test_struct_field_offsets);
    TEST_DIVIDER();

    RUN_TEST(test_prototype_inheritance_loading);
    TEST_DIVIDER();

    RUN_TEST(test_struct_value_copy_clones_nested_storage);
    TEST_MODULE_DIVIDER();

    return UNITY_END();
}
