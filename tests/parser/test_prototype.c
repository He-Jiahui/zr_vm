// 覆盖 struct/class 原型的编译、继承、字段布局及值复制；由 literal_surface_main 注册。
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
#include "zr_test_log_macros.h"

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
/* 根据扩展名选择测试产物目录，返回需要调用方 free 的绝对输出路径。 */
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
void test_struct_prototype_compilation(void);
void test_class_prototype_compilation(void);
void test_prototype_creation_functions(void);
void test_prototype_inheritance(void);
void test_prototype_module_export(void);
void test_struct_field_offsets(void);
void test_prototype_inheritance_loading(void);
void test_struct_value_copy_clones_nested_storage(void);

// 测试 struct prototype 编译时收集
/**
 * ## Summary
 * 从 parser 的 struct fixture 编译到原型元数据，再提供装载、调试输出及生成产物的诊断入口。
 *
 * ## Returns
 * 找不到文件、读取或编译失败时标记 Unity 失败并返回；编译后的诊断路径最后打印 PASS，不能据此认定原型数量、字段或产物写入正确。
 *
 * ## Constraints and lifecycle
 * 路径及源码由本例 free；原型和模块对象依附本例 Global。正常返回分支释放 Global 后记录结束时刻，因此日志耗时包含该释放阶段。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；ZrCore_GlobalState_New/Free 界定本例 VM 对象的生存期；ZR_TEST_PASS 提供已采样片段的耗时日志。
 */
void test_struct_prototype_compilation(void) {
    ZR_TEST_START("Struct Prototype Compilation");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    ZR_TEST_INFO("Struct prototype compilation", "Testing that struct declarations are collected during compilation");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Struct Prototype Compilation", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Struct Prototype Compilation", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Struct Prototype Compilation", "Failed to read test file");
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
        ZR_TEST_FAIL(timer, "Struct Prototype Compilation", "Compilation failed");
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
        /* TODO: 原型数不符这里只打印 FAIL，后续仍可 PASS；先确认本例是编译诊断还是元数据验收，再核对字节格式及 Unity 失败判据。 */
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


    ZR_TEST_PASS(timer, "Struct Prototype Compilation");
}

// 测试 class prototype 编译时收集
/**
 * ## Summary
 * 编译 class fixture，检查是否留下原型数据或 Animal 名称常量，并在具备数据时显示装载结果。
 *
 * ## Returns
 * 存在原型数据或相应字符串即可打印 PASS；两者都缺失时标记 Unity 失败。这一判据不证明 class 装载或 Dog 的继承关系已经成立。
 *
 * ## Constraints and lifecycle
 * 输入路径及源码由本例拥有；VM 对象限制在本例 Global 内。可见普通错误分支和正常分支均手工释放，结束采样包含 Global 的释放。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；ZrCore_GlobalState_New/Free 界定本例 VM 对象的生存期；ZR_TEST_PASS 提供已采样片段的耗时日志。
 */
void test_class_prototype_compilation(void) {
    ZR_TEST_START("Class Prototype Compilation");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    ZR_TEST_INFO("Class prototype compilation", "Testing that class declarations are collected during compilation");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12346, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Class Prototype Compilation", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_class.zr");
    if (testFile == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Class Prototype Compilation", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Class Prototype Compilation", "Failed to read test file");
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
        ZR_TEST_FAIL(timer, "Class Prototype Compilation", "Compilation failed");
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

    /* TODO: Animal 字符串即可满足通过判据，装载失败仍可能 PASS；需确认 class 原型验收是否要求成功重建实际导出。 */
    if (hasPrototypeData || hasPrototypeInfo) {
        ZR_TEST_PASS(timer, "Class Prototype Compilation");
    } else {
        ZR_TEST_FAIL(timer, "Class Prototype Compilation", "Prototype information not found");
    }
}

// 测试 prototype 运行时创建函数
/**
 * ## Summary
 * 直接覆盖 struct/class 原型工厂的基本身份合同，防止类型名称或原型种类在构造过程中改变。
 *
 * ## Returns
 * 原型创建、名称或种类检查失败时标记 Unity 失败；末尾对象构造只提供可选调试输出，其 NULL 结果不会改变当前通过判定。
 *
 * ## Constraints and lifecycle
 * 所有 VM 名称、原型和对象在本例 Global 内使用；普通失败分支由本例释放，正常耗时包含 Global 释放。日志宏不承担对象清理。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；ZrCore_GlobalState_New/Free 界定本例 VM 对象的生存期；ZR_TEST_PASS 提供已采样片段的耗时日志。
 */
void test_prototype_creation_functions(void) {
    ZR_TEST_START("Prototype Creation Functions");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    ZR_TEST_INFO("Prototype creation functions", "Testing ZrCore_ObjectPrototype_New and ZrCore_StructPrototype_New functions");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12347, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建类型名称
    SZrString *typeName = ZrCore_String_CreateFromNative(state, "TestStruct");
    if (typeName == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "Failed to create type name string");
        return;
    }

    // 测试创建 StructPrototype
    SZrStructPrototype *structPrototype = ZrCore_StructPrototype_New(state, typeName);
    if (structPrototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "Failed to create StructPrototype");
        return;
    }

    // 验证 prototype 属性
    if (structPrototype->super.name != typeName) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "StructPrototype name mismatch");
        return;
    }

    if (structPrototype->super.type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "StructPrototype type mismatch");
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
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "Failed to create class name string");
        return;
    }

    SZrObjectPrototype *classPrototype = ZrCore_ObjectPrototype_New(state, className, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (classPrototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "Failed to create ObjectPrototype");
        return;
    }

    // 验证 prototype 属性
    if (classPrototype->name != className) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "ObjectPrototype name mismatch");
        return;
    }

    if (classPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Creation Functions", "ObjectPrototype type mismatch");
        return;
    }

    // 使用debug工具输出prototype信息
    printf("\n  Debug output for created ClassPrototype:\n");
    ZrCore_Debug_PrintPrototype(state, classPrototype, stdout);

    // 测试创建并输出object信息
    /* TODO: 对象工厂返回 NULL 只跳过调试输出；确认本例是否也承诺对象构造成功，再决定此结果的验收条件。 */
    SZrObject *testObject = ZrCore_Object_New(state, classPrototype);
    if (testObject != ZR_NULL) {
        printf("\n  Debug output for created Object:\n");
        ZrCore_Debug_PrintObject(state, testObject, stdout);
    }

    ZrCore_GlobalState_Free(global);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "Prototype Creation Functions");
}

// 测试 prototype 继承关系设置
/**
 * ## Summary
 * 绕过源码编译层，直接检查 class 原型 SetSuper 是否保留本例指定的父原型身份。
 *
 * ## Returns
 * 构造或父指针身份检查失败时标记 Unity 失败；正常通过只证明这一直接设置合同，不证明继承的源码解析或序列化装载。
 *
 * ## Constraints and lifecycle
 * 父子原型同属本例 Global，普通退出路径手工释放；PASS 的 timer 结束采样发生在 Global 释放之后。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；ZrCore_GlobalState_New/Free 界定本例 VM 对象的生存期；ZR_TEST_PASS 提供已采样片段的耗时日志。
 */
void test_prototype_inheritance(void) {
    ZR_TEST_START("Prototype Inheritance");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    ZR_TEST_INFO("Prototype inheritance", "Testing ZrCore_ObjectPrototype_SetSuper function");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12348, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Inheritance", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建父类 prototype
    SZrString *parentName = ZrCore_String_CreateFromNative(state, "Parent");
    SZrObjectPrototype *parentPrototype = ZrCore_ObjectPrototype_New(state, parentName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (parentPrototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Inheritance", "Failed to create parent prototype");
        return;
    }

    // 创建子类 prototype
    SZrString *childName = ZrCore_String_CreateFromNative(state, "Child");
    SZrObjectPrototype *childPrototype = ZrCore_ObjectPrototype_New(state, childName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (childPrototype == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Inheritance", "Failed to create child prototype");
        return;
    }

    // 设置继承关系
    ZrCore_ObjectPrototype_SetSuper(state, childPrototype, parentPrototype);

    // 验证继承关系
    if (childPrototype->superPrototype != parentPrototype) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Inheritance", "Inheritance relationship not set correctly");
        return;
    }

    ZrCore_GlobalState_Free(global);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "Prototype Inheritance");
}

// 测试 prototype 模块导出
/**
 * ## Summary
 * 把本例创建的 struct 原型发布为模块公共导出，并核对按名取得的值仍指向同一原型。
 *
 * ## Returns
 * 导出缺失、值种类或对象身份错误均标记 Unity 失败；本例验证模块内导出查询，不覆盖文件装载后的导出重建。
 *
 * ## Constraints and lifecycle
 * 模块、原型及查询结果在同一 Global 中借用，不能在本例释放后交给 runner 使用。普通失败分支和正常分支由本例清理，结束采样包含释放。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；ZrCore_GlobalState_New/Free 界定本例 VM 对象的生存期；ZR_TEST_PASS 提供已采样片段的耗时日志。
 */
void test_prototype_module_export(void) {
    ZR_TEST_START("Prototype Module Export");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    ZR_TEST_INFO("Prototype module export", "Testing that prototypes are exported to module correctly");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12349, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Module Export", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建模块
    struct SZrObjectModule *module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Module Export", "Failed to create module");
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
        ZR_TEST_FAIL(timer, "Prototype Module Export", "Failed to create prototype");
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
        ZR_TEST_FAIL(timer, "Prototype Module Export", "Prototype not found in module exports");
        return;
    }

    if (exportedValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Module Export", "Exported value type mismatch");
        return;
    }

    SZrObjectPrototype *exportedPrototype = (SZrObjectPrototype *) ZR_CAST_OBJECT(state, exportedValue->value.object);
    if (exportedPrototype != prototype) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Module Export", "Exported prototype mismatch");
        return;
    }

    ZrCore_GlobalState_Free(global);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "Prototype Module Export");
}

// 测试 struct 字段偏移量
/**
 * ## Summary
 * 编译 struct fixture 后尝试装载 Vector3，并提供字段偏移布局的调试观察入口。
 *
 * ## Returns
 * 文件或编译失败会标记 Unity 失败；装载后只在字段表存在且非空时输出布局，随后统一打印 PASS，当前没有数值偏移断言。
 *
 * ## Constraints and lifecycle
 * 本例拥有路径及源码；模块和原型只在本例 Global 存活期间使用。普通退出分支手工清理，结束采样包含 Global 的释放。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；ZrCore_GlobalState_New/Free 界定本例 VM 对象的生存期；ZR_TEST_PASS 提供已采样片段的耗时日志。
 */
void test_struct_field_offsets(void) {
    ZR_TEST_START("Struct Field Offsets");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime;

    ZR_TEST_INFO("Struct field offsets", "Testing that struct fields have correct offsets in keyOffsetMap");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12350, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Struct Field Offsets", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Struct Field Offsets", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Struct Field Offsets", "Failed to read test file");
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
        ZR_TEST_FAIL(timer, "Struct Field Offsets", "Compilation failed");
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
            /* TODO: 当前只输出非空字段表，未核对字段偏移或缺失导出；需以 Vector3 fixture 的预期布局建立可失败的验收。 */
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
    ZR_TEST_PASS(timer, "Struct Field Offsets");
}

// 测试继承关系的完整加载
/**
 * ## Summary
 * 尝试从 struct fixture 的编译产物取得 Point2D/Point3D，在父指针匹配时显示装载后的继承信息。
 *
 * ## Returns
 * 输入或编译失败会标记 Unity 失败；缺失导出、零装载数量或父指针不匹配都可能跳过诊断后打印 PASS，不能把此日志当作继承验收。
 *
 * ## Constraints and lifecycle
 * 原型查询值依附本例 Global；路径、源码与 Global 在普通返回路径手工释放后结束计时。夹具父关系仍需按 TODO 核实。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；ZrCore_GlobalState_New/Free 界定本例 VM 对象的生存期；ZR_TEST_PASS 提供已采样片段的耗时日志。
 */
void test_prototype_inheritance_loading(void) {
    ZR_TEST_START("Prototype Inheritance Loading");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime;

    ZR_TEST_INFO("Prototype inheritance loading",
              "Testing that inheritance relationships are correctly loaded from constants");

    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12351, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Inheritance Loading", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Inheritance Loading", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrCore_GlobalState_Free(global);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, "Prototype Inheritance Loading", "Failed to read test file");
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
        ZR_TEST_FAIL(timer, "Prototype Inheritance Loading", "Compilation failed");
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
                /* TODO: fixture 的 Point3D 未声明父结构，且父指针不匹配不会失败；先核实合法继承夹具，再明确装载关系断言。 */
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
    ZR_TEST_PASS(timer, "Prototype Inheritance Loading");
}

/* 值类型复制后修改嵌套字段，防止原对象与副本共享可写内部存储。 */
/**
 * ## Summary
 * 构造嵌套 struct 值并复制，再修改副本内层字段，防止值复制退化为共享可变对象存储。
 *
 * ## Returns
 * 外层及内层对象身份、源值 1 与副本值 9 由 Unity 硬断言检查；正常路径释放 Global 后打印耗时，失败可在该日志前直接跳回 runner。
 *
 * ## Constraints and lifecycle
 * 所有对象属于本例 Global；正常释放不涵盖硬断言的非局部退出。当前目标链接空 tearDown，失败后 Global 的释放缺口见创建位置的 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；ZrCore_GlobalState_New/Free 界定本例 VM 对象的生存期；ZR_TEST_PASS 提供已采样片段的耗时日志。
 */
void test_struct_value_copy_clones_nested_storage(void) {
    ZR_TEST_START("Struct Value Copy Clones Nested Storage");
    ZR_TEST_INFO("Struct copy semantics",
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

        /* BUG: Global 成功创建后，后续 Unity 硬断言失败会 longjmp 跳过尾部 GlobalState_Free；本目标 tearDown 为空，VM 分配持续到进程退出。需将所有权交给可达的失败清理入口。 */
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
        ZR_TEST_PASS(timer, "Struct Value Copy Clones Nested Storage");
    }

    ZR_TEST_DIVIDER();
}
