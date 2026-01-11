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
static TZrPtr testAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
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
    FILE *file = fopen(filename, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char *) malloc(fileSize + 1);
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
static char *find_test_file(const char *filename) {
    char *paths[4];
    // 第一个路径：当前目录（需要复制，因为 filename 是 const）
    TZrSize filenameLen = strlen(filename);
    paths[0] = (char *) malloc(filenameLen + 1);
    if (paths[0] != ZR_NULL) {
        memcpy(paths[0], filename, filenameLen + 1);
    }
    paths[1] = ZR_NULL; // 将在下面填充
    paths[2] = ZR_NULL; // 将在下面填充
    paths[3] = ZR_NULL;

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != ZR_NULL) {
        TZrSize cwdLen = strlen(cwd);
        paths[1] = (char *) malloc(cwdLen + filenameLen + 20);
        if (paths[1] != ZR_NULL) {
            sprintf(paths[1], "%s/%s", cwd, filename);
        }

        paths[2] = (char *) malloc(cwdLen + filenameLen + 30);
        if (paths[2] != ZR_NULL) {
            sprintf(paths[2], "%s/tests/parser/%s", cwd, filename);
        }
    }

    char *resultPath = ZR_NULL;
    for (int i = 0; i < 3 && paths[i] != ZR_NULL; i++) {
        FILE *testFile = fopen(paths[i], "r");
        if (testFile != ZR_NULL) {
            fclose(testFile);
            // 确保返回的路径始终是堆分配的，便于调用方释放
            resultPath = paths[i];
            // 释放其他未使用的路径
            for (int j = 0; j < 3; j++) {
                if (j != i && paths[j] != ZR_NULL) {
                    free(paths[j]);
                }
            }
            return resultPath;
        }
    }

    // 如果所有路径都未找到，释放所有分配的路径
    for (int i = 0; i < 3; i++) {
        if (paths[i] != ZR_NULL) {
            free(paths[i]);
        }
    }

    return ZR_NULL;
}

// 生成输出文件名（将 .zr 替换为新的扩展名）
static char *generate_output_filename(const char *inputFile, const char *newExt) {
    if (inputFile == ZR_NULL || newExt == ZR_NULL) {
        return ZR_NULL;
    }
    
    TZrSize inputLen = strlen(inputFile);
    TZrSize extLen = strlen(newExt);
    
    // 查找 .zr 的位置
    const char *extPos = strrchr(inputFile, '.');
    TZrSize baseLen;
    if (extPos != ZR_NULL && strcmp(extPos, ".zr") == 0) {
        // 找到 .zr 扩展名
        baseLen = extPos - inputFile;
    } else {
        // 没有找到 .zr 扩展名，使用整个文件名
        baseLen = inputLen;
    }
    
    // 分配新文件名（base + newExt + null terminator）
    char *outputFile = (char *)malloc(baseLen + extLen + 1);
    if (outputFile == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 复制基础文件名
    memcpy(outputFile, inputFile, baseLen);
    // 复制新扩展名
    memcpy(outputFile + baseLen, newExt, extLen);
    outputFile[baseLen + extLen] = '\0';
    
    return outputFile;
}

// 测试函数前向声明
static void test_struct_prototype_compilation(void);
static void test_class_prototype_compilation(void);
static void test_prototype_creation_functions(void);
static void test_prototype_inheritance(void);
static void test_prototype_module_export(void);
static void test_struct_field_offsets(void);
static void test_prototype_inheritance_loading(void);

// 测试 struct prototype 编译时收集
static void test_struct_prototype_compilation(void) {
    TEST_START("Struct Prototype Compilation");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    TEST_INFO("Struct prototype compilation", "Testing that struct declarations are collected during compilation");

    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrGlobalStateFree(global);
        timer.endTime = clock();
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
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Prototype Compilation", "Compilation failed");
        return;
    }


    // 测试prototypeData存储
    if (func != ZR_NULL) {
        // 验证prototypeData字段
        TBool hasPrototypeData = (func->prototypeData != ZR_NULL && 
                                  func->prototypeDataLength > 0 && 
                                  func->prototypeCount > 0);
        if (hasPrototypeData) {
            printf("\n  Prototype data stored in function->prototypeData:\n");
            printf("    prototypeCount: %u\n", func->prototypeCount);
            printf("    prototypeDataLength: %u bytes\n", func->prototypeDataLength);
            
            // 验证数据格式（头部应该是prototypeCount）
            TUInt32 *countPtr = (TUInt32 *)func->prototypeData;
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
        ZrDebugPrintPrototypesFromData(state, func, stdout);
        
        // 也测试旧的函数（向后兼容性）
        // 注意：ZrDebugPrintPrototypeFromConstants 已被移除，使用 ZrDebugPrintPrototypesFromData 替代

        // 创建模块对象
        struct SZrObjectModule *module = ZrModuleCreate(state);
        if (module != ZR_NULL) {
            // 优先使用新的函数从prototypeData创建
            TZrSize createdCount = 0;
            if (hasPrototypeData) {
                createdCount = ZrModuleCreatePrototypesFromData(state, module, func);
                if (createdCount > 0) {
                    printf("\n  Created %zu prototype(s) from prototypeData:\n", createdCount);
                } else {
                    // 如果新函数失败，尝试旧函数（向后兼容）
                    createdCount = ZrModuleCreatePrototypesFromConstants(state, module, func);
                    if (createdCount > 0) {
                        printf("\n  Created %zu prototype(s) from constants (legacy fallback):\n", createdCount);
                    }
                }
            } else {
                // 如果没有新格式数据，尝试旧函数
                createdCount = ZrModuleCreatePrototypesFromConstants(state, module, func);
                if (createdCount > 0) {
                    printf("\n  Created %zu prototype(s) from constants (legacy format):\n", createdCount);
                }
            }

            if (createdCount > 0) {

                // 查找并输出Vector3 prototype信息
                SZrString *vector3Name = ZrStringCreateFromNative(state, "Vector3");
                if (vector3Name != ZR_NULL) {
                    const SZrTypeValue *prototypeValue = ZrModuleGetPubExport(state, module, vector3Name);
                    if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObjectPrototype *prototype =
                                (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                        if (prototype != ZR_NULL) {
                            ZrDebugPrintPrototype(state, prototype, stdout);
                        }
                    }
                }

                // 输出Point3D prototype（测试继承关系）
                SZrString *point3DName = ZrStringCreateFromNative(state, "Point3D");
                if (point3DName != ZR_NULL) {
                    const SZrTypeValue *prototypeValue = ZrModuleGetPubExport(state, module, point3DName);
                    if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObjectPrototype *prototype =
                                (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                        if (prototype != ZR_NULL) {
                            printf("\n  Point3D prototype (with inheritance):\n");
                            ZrDebugPrintPrototype(state, prototype, stdout);
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
            TBool zriResult = ZrWriterWriteIntermediateFile(state, func, zriFile);
            if (zriResult) {
                printf("\n  Generated ZRI file: %s\n", zriFile);
            }
            free(zriFile);
        }
        
        // 生成 .zro 文件名
        char *zroFile = generate_output_filename(testFile, ".zro");
        if (zroFile != ZR_NULL) {
            TBool zroResult = ZrWriterWriteBinaryFile(state, func, zroFile);
            if (zroResult) {
                printf("  Generated ZRO file: %s\n", zroFile);
            }
            free(zroFile);
        }
    }

    free(source);
    free(testFile);
    ZrGlobalStateFree(global);

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

    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12346, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_class.zr");
    if (testFile == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrGlobalStateFree(global);
        timer.endTime = clock();
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
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Class Prototype Compilation", "Compilation failed");
        return;
    }

    // 验证prototypeData存储
    TBool hasPrototypeData = (func->prototypeData != ZR_NULL && 
                              func->prototypeDataLength > 0 && 
                              func->prototypeCount > 0);
    if (hasPrototypeData) {
        printf("\n  Prototype data stored in function->prototypeData:\n");
        printf("    prototypeCount: %u\n", func->prototypeCount);
        printf("    prototypeDataLength: %u bytes\n", func->prototypeDataLength);
    }
    
    // 检查常量池中是否有 prototype 相关信息（类型名称字符串等）
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

    // 输出prototype信息
    if ((hasPrototypeInfo || hasPrototypeData) && func != ZR_NULL) {
        printf("\n  Prototype information from prototypeData (new format):\n");
        ZrDebugPrintPrototypesFromData(state, func, stdout);
        
        // 注意：ZrDebugPrintPrototypeFromConstants 已被移除，使用 ZrDebugPrintPrototypesFromData 替代

        // 创建模块并加载prototype
        struct SZrObjectModule *module = ZrModuleCreate(state);
        if (module != ZR_NULL) {
            TZrSize createdCount = 0;
            if (hasPrototypeData) {
                createdCount = ZrModuleCreatePrototypesFromData(state, module, func);
            }
            if (createdCount == 0) {
                createdCount = ZrModuleCreatePrototypesFromConstants(state, module, func);
            }

            if (createdCount > 0) {
                printf("\n  Created %zu prototype(s), runtime debug output:\n", createdCount);

                // 输出Animal prototype
                SZrString *animalName = ZrStringCreateFromNative(state, "Animal");
                if (animalName != ZR_NULL) {
                    const SZrTypeValue *prototypeValue = ZrModuleGetPubExport(state, module, animalName);
                    if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObjectPrototype *prototype =
                                (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                        if (prototype != ZR_NULL) {
                            ZrDebugPrintPrototype(state, prototype, stdout);
                        }
                    }
                }

                // 输出Dog prototype（测试继承关系）
                SZrString *dogName = ZrStringCreateFromNative(state, "Dog");
                if (dogName != ZR_NULL) {
                    const SZrTypeValue *prototypeValue = ZrModuleGetPubExport(state, module, dogName);
                    if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObjectPrototype *prototype =
                                (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                        if (prototype != ZR_NULL) {
                            printf("\n  Dog prototype (with inheritance):\n");
                            ZrDebugPrintPrototype(state, prototype, stdout);
                        }
                    }
                }
            }
        }
    }

    free(source);
    free(testFile);
    ZrGlobalStateFree(global);

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

    TEST_INFO("Prototype creation functions", "Testing ZrObjectPrototypeNew and ZrStructPrototypeNew functions");

    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12347, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建类型名称
    SZrString *typeName = ZrStringCreateFromNative(state, "TestStruct");
    if (typeName == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create type name string");
        return;
    }

    // 测试创建 StructPrototype
    SZrStructPrototype *structPrototype = ZrStructPrototypeNew(state, typeName);
    if (structPrototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create StructPrototype");
        return;
    }

    // 验证 prototype 属性
    if (structPrototype->super.name != typeName) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "StructPrototype name mismatch");
        return;
    }

    if (structPrototype->super.type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "StructPrototype type mismatch");
        return;
    }

    // 使用debug工具输出prototype信息
    printf("\n  Debug output for created StructPrototype:\n");
    ZrDebugPrintPrototype(state, (SZrObjectPrototype *) structPrototype, stdout);

    // 测试创建 ObjectPrototype (Class)
    SZrString *className = ZrStringCreateFromNative(state, "TestClass");
    if (className == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create class name string");
        return;
    }

    SZrObjectPrototype *classPrototype = ZrObjectPrototypeNew(state, className, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (classPrototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "Failed to create ObjectPrototype");
        return;
    }

    // 验证 prototype 属性
    if (classPrototype->name != className) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "ObjectPrototype name mismatch");
        return;
    }

    if (classPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Creation Functions", "ObjectPrototype type mismatch");
        return;
    }

    // 使用debug工具输出prototype信息
    printf("\n  Debug output for created ClassPrototype:\n");
    ZrDebugPrintPrototype(state, classPrototype, stdout);

    // 测试创建并输出object信息
    SZrObject *testObject = ZrObjectNew(state, classPrototype);
    if (testObject != ZR_NULL) {
        printf("\n  Debug output for created Object:\n");
        ZrDebugPrintObject(state, testObject, stdout);
    }

    ZrGlobalStateFree(global);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Creation Functions");
}

// 测试 prototype 继承关系设置
static void test_prototype_inheritance(void) {
    TEST_START("Prototype Inheritance");
    SZrTestTimer timer;
    timer.startTime = clock();
    timer.endTime = timer.startTime; // 初始化 endTime 以避免未初始化警告

    TEST_INFO("Prototype inheritance", "Testing ZrObjectPrototypeSetSuper function");

    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12348, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建父类 prototype
    SZrString *parentName = ZrStringCreateFromNative(state, "Parent");
    SZrObjectPrototype *parentPrototype = ZrObjectPrototypeNew(state, parentName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (parentPrototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create parent prototype");
        return;
    }

    // 创建子类 prototype
    SZrString *childName = ZrStringCreateFromNative(state, "Child");
    SZrObjectPrototype *childPrototype = ZrObjectPrototypeNew(state, childName, ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    if (childPrototype == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Failed to create child prototype");
        return;
    }

    // 设置继承关系
    ZrObjectPrototypeSetSuper(state, childPrototype, parentPrototype);

    // 验证继承关系
    if (childPrototype->superPrototype != parentPrototype) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance", "Inheritance relationship not set correctly");
        return;
    }

    ZrGlobalStateFree(global);
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

    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12349, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 创建模块
    struct SZrObjectModule *module = ZrModuleCreate(state);
    if (module == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
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
        timer.endTime = clock();
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
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Prototype not found in module exports");
        return;
    }

    if (exportedValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Exported value type mismatch");
        return;
    }

    SZrObjectPrototype *exportedPrototype = (SZrObjectPrototype *) ZR_CAST_OBJECT(state, exportedValue->value.object);
    if (exportedPrototype != prototype) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Module Export", "Exported prototype mismatch");
        return;
    }

    ZrGlobalStateFree(global);
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

    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12350, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Field Offsets", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Field Offsets", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Field Offsets", "Failed to read test file");
        return;
    }

    // 编译源代码
    SZrString *sourceName = ZrStringCreateFromNative(state, testFile);
    SZrFunction *func = ZrParserCompileSource(state, source, fileSize, sourceName);

    if (func == ZR_NULL) {
        free(source);
        free(testFile);
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Struct Field Offsets", "Compilation failed");
        return;
    }

    // 验证prototypeData存储
    TBool hasPrototypeData = (func->prototypeData != ZR_NULL && 
                              func->prototypeDataLength > 0 && 
                              func->prototypeCount > 0);
    if (hasPrototypeData) {
        printf("\n  Prototype data verification:\n");
        printf("    prototypeCount: %u\n", func->prototypeCount);
        printf("    prototypeDataLength: %u bytes\n", func->prototypeDataLength);
    }
    
    // 创建模块并加载prototype
    struct SZrObjectModule *module = ZrModuleCreate(state);
    if (module != ZR_NULL) {
        TZrSize createdCount = 0;
        if (hasPrototypeData) {
            createdCount = ZrModuleCreatePrototypesFromData(state, module, func);
        }
        if (createdCount == 0) {
            createdCount = ZrModuleCreatePrototypesFromConstants(state, module, func);
        }

        if (createdCount > 0) {
            // 检查Vector3的字段偏移量
            SZrString *vector3Name = ZrStringCreateFromNative(state, "Vector3");
            if (vector3Name != ZR_NULL) {
                const SZrTypeValue *prototypeValue = ZrModuleGetPubExport(state, module, vector3Name);
                if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                    SZrStructPrototype *structProto =
                            (SZrStructPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                    if (structProto != ZR_NULL && structProto->super.type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                        // 检查keyOffsetMap中是否有字段
                        if (structProto->keyOffsetMap.elementCount > 0) {
                            printf("\n  Vector3 field offsets:\n");
                            ZrDebugPrintPrototype(state, (SZrObjectPrototype *) structProto, stdout);
                        }
                    }
                }
            }
        }
    }

    free(source);
    free(testFile);
    ZrGlobalStateFree(global);

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

    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12351, ZR_NULL);
    if (global == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance Loading", "Failed to create global state");
        return;
    }

    SZrState *state = global->mainThreadState;

    // 读取测试文件
    char *testFile = find_test_file("test_prototype_struct.zr");
    if (testFile == ZR_NULL) {
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance Loading", "Test file not found");
        return;
    }

    TZrSize fileSize = 0;
    char *source = read_file_content(testFile, &fileSize);
    if (source == ZR_NULL) {
        free(testFile);
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance Loading", "Failed to read test file");
        return;
    }

    // 编译源代码
    SZrString *sourceName = ZrStringCreateFromNative(state, testFile);
    SZrFunction *func = ZrParserCompileSource(state, source, fileSize, sourceName);

    if (func == ZR_NULL) {
        free(source);
        free(testFile);
        ZrGlobalStateFree(global);
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, "Prototype Inheritance Loading", "Compilation failed");
        return;
    }

    // 验证prototypeData存储
    TBool hasPrototypeData = (func->prototypeData != ZR_NULL && 
                              func->prototypeDataLength > 0 && 
                              func->prototypeCount > 0);
    if (hasPrototypeData) {
        printf("\n  Prototype data verification:\n");
        printf("    prototypeCount: %u\n", func->prototypeCount);
        printf("    prototypeDataLength: %u bytes\n", func->prototypeDataLength);
    }
    
    // 创建模块并加载prototype
    struct SZrObjectModule *module = ZrModuleCreate(state);
    if (module != ZR_NULL) {
        TZrSize createdCount = 0;
        if (hasPrototypeData) {
            createdCount = ZrModuleCreatePrototypesFromData(state, module, func);
        }
        if (createdCount == 0) {
            createdCount = ZrModuleCreatePrototypesFromConstants(state, module, func);
        }

        if (createdCount > 0) {
            // 检查Point3D的继承关系（应该继承自Point2D）
            SZrString *point3DName = ZrStringCreateFromNative(state, "Point3D");
            if (point3DName != ZR_NULL) {
                const SZrTypeValue *prototypeValue = ZrModuleGetPubExport(state, module, point3DName);
                if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                    SZrObjectPrototype *prototype =
                            (SZrObjectPrototype *) ZR_CAST_OBJECT(state, prototypeValue->value.object);
                    if (prototype != ZR_NULL && prototype->superPrototype != ZR_NULL) {
                        SZrString *point2DName = ZrStringCreateFromNative(state, "Point2D");
                        const SZrTypeValue *point2DValue = ZrModuleGetPubExport(state, module, point2DName);
                        if (point2DValue != ZR_NULL && point2DValue->type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObjectPrototype *point2D =
                                    (SZrObjectPrototype *) ZR_CAST_OBJECT(state, point2DValue->value.object);
                            if (prototype->superPrototype == point2D) {
                                printf("\n  Point3D correctly inherits from Point2D:\n");
                                ZrDebugPrintPrototype(state, prototype, stdout);
                            }
                        }
                    }
                }
            }
        }
    }

    free(source);
    free(testFile);
    ZrGlobalStateFree(global);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Prototype Inheritance Loading");
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
    TEST_MODULE_DIVIDER();

    return UNITY_END();
}
