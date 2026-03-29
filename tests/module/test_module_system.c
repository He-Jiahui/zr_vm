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
#include "runtime_support.h"
#include "zr_vm_library.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

// Unity 测试宏扩展 - 添加缺失的 UINT64 不等断言
#ifndef TEST_ASSERT_NOT_EQUAL_UINT64
#define TEST_ASSERT_NOT_EQUAL_UINT64(expected, actual)                                             TEST_ASSERT_NOT_EQUAL((expected), (actual))
#endif

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const ZrLibMethodDescriptor kProbeReadableMethods[] = {
        {"read", 0, 0, ZR_NULL, "int", "Read the current value.", ZR_FALSE},
};

static const ZrLibMethodDescriptor kProbeStreamReadableMethods[] = {
        {"available", 0, 0, ZR_NULL, "int", "Return the available item count.", ZR_FALSE},
};

static const TZrChar *kProbeDeviceImplements[] = {
        "NativeStreamReadable",
};

static const ZrLibFieldDescriptor kProbeDeviceFields[] = {
        {"mode", "NativeMode", "The current device mode."},
};

static const ZrLibEnumMemberDescriptor kProbeModeMembers[] = {
        {"Off", ZR_LIB_CONSTANT_KIND_INT, 0, 0.0, ZR_NULL, ZR_FALSE, "Disabled mode."},
        {"On", ZR_LIB_CONSTANT_KIND_INT, 1, 0.0, ZR_NULL, ZR_FALSE, "Enabled mode."},
};

static const ZrLibTypeDescriptor kProbeNativeTypes[] = {
        {"NativeReadable",
         ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
         ZR_NULL,
         0,
         kProbeReadableMethods,
         ZR_ARRAY_COUNT(kProbeReadableMethods),
         ZR_NULL,
         0,
         "Readable interface.",
         ZR_NULL,
         ZR_NULL,
         0,
         ZR_NULL,
         0,
         ZR_NULL,
         ZR_FALSE,
         ZR_FALSE,
         "NativeReadable()"},
        {"NativeStreamReadable",
         ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
         ZR_NULL,
         0,
         kProbeStreamReadableMethods,
         ZR_ARRAY_COUNT(kProbeStreamReadableMethods),
         ZR_NULL,
         0,
         "Stream-readable interface.",
         "NativeReadable",
         ZR_NULL,
         0,
         ZR_NULL,
         0,
         ZR_NULL,
         ZR_FALSE,
         ZR_FALSE,
         "NativeStreamReadable()"},
        {"NativeMode",
         ZR_OBJECT_PROTOTYPE_TYPE_ENUM,
         ZR_NULL,
         0,
         ZR_NULL,
         0,
         ZR_NULL,
         0,
         "Device mode enum.",
         ZR_NULL,
         ZR_NULL,
         0,
         kProbeModeMembers,
         ZR_ARRAY_COUNT(kProbeModeMembers),
         "int",
         ZR_TRUE,
         ZR_TRUE,
         "NativeMode(value: int)"},
        {"NativeDevice",
         ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         kProbeDeviceFields,
         ZR_ARRAY_COUNT(kProbeDeviceFields),
         ZR_NULL,
         0,
         ZR_NULL,
         0,
         "Concrete device type.",
         ZR_NULL,
         kProbeDeviceImplements,
         ZR_ARRAY_COUNT(kProbeDeviceImplements),
         ZR_NULL,
         0,
         ZR_NULL,
         ZR_TRUE,
         ZR_TRUE,
         "NativeDevice()"},
};

static const ZrLibModuleDescriptor kProbeNativeModuleDescriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "probe.native_shapes",
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        kProbeNativeTypes,
        ZR_ARRAY_COUNT(kProbeNativeTypes),
        ZR_NULL,
        0,
        ZR_NULL,
        "Native test module containing interface, enum and implements metadata.",
        ZR_NULL,
        0,
};

static const ZrLibModuleDescriptor kProbeFutureAbiModuleDescriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "probe.future_abi",
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        "Native module that requires a future runtime ABI.",
        ZR_NULL,
        0,
        "9.9.9",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION + 1,
        0,
};

static const ZrLibModuleDescriptor kProbeUnsupportedCapabilityModuleDescriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "probe.unsupported_capability",
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        ZR_NULL,
        "Native module that requires unsupported runtime capabilities.",
        ZR_NULL,
        0,
        "1.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        ((TZrUInt64)1u << 40),
};

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
        ZrVmLibSystem_Register(global);
    }

    return mainState;
}

static TZrBool register_probe_native_module(SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_RegisterModule(state->global, &kProbeNativeModuleDescriptor);
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

static TZrBool string_equals_cstring(SZrString *value, const TZrChar *expected) {
    const TZrChar *nativeString;

    if (value == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeString = ZrCore_String_GetNativeString(value);
    return nativeString != ZR_NULL && strcmp(nativeString, expected) == 0;
}

static const SZrTypeValue *get_object_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldNameString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static const SZrTypeValue *get_module_export_value(SZrState *state, SZrObjectModule *module, const TZrChar *exportName) {
    SZrString *exportNameString;

    if (state == ZR_NULL || module == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    exportNameString = ZrCore_String_Create(state, (TZrNativeString)exportName, strlen(exportName));
    if (exportNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportNameString);
}

static TZrSize get_array_length(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }

    return array->nodeMap.elementCount;
}

static SZrObject *get_array_entry_object(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    const SZrTypeValue *entryValue;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    entryValue = ZrCore_Object_GetValue(state, array, &key);
    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

static const SZrTypeValue *get_array_entry_value(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

static const SZrTypeValue *get_zr_global_value(SZrState *state, const TZrChar *memberName) {
    SZrGlobalState *global;
    SZrObject *zrObject;

    if (state == ZR_NULL || state->global == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    if (global->zrObject.type != ZR_VALUE_TYPE_OBJECT || global->zrObject.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    zrObject = ZR_CAST_OBJECT(state, global->zrObject.value.object);
    return get_object_field_value(state, zrObject, memberName);
}

static SZrObjectModule *import_native_module(SZrState *state, const TZrChar *moduleName) {
    const SZrTypeValue *importValue;
    SZrTypeValue argument;
    SZrTypeValue result;
    SZrObject *object;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    importValue = get_zr_global_value(state, "import");
    if (importValue == ZR_NULL) {
        return ZR_NULL;
    }

    ZrLib_Value_SetString(state, &argument, moduleName);
    if (!ZrLib_CallValue(state, importValue, ZR_NULL, &argument, 1, &result)) {
        return ZR_NULL;
    }

    if (result.type != ZR_VALUE_TYPE_OBJECT || result.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, result.value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        return ZR_NULL;
    }

    return (SZrObjectModule *)object;
}

typedef struct {
    const TZrChar *path;
    const TZrChar *source;
} SZrModuleFixtureSource;

typedef struct {
    const TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrModuleFixtureReader;

static const SZrModuleFixtureSource *g_module_fixture_sources = ZR_NULL;
static TZrSize g_module_fixture_source_count = 0;

static TZrBytePtr module_fixture_reader_read(SZrState *state, TZrPtr customData, TZrSize *size) {
    SZrModuleFixtureReader *reader = (SZrModuleFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed) {
        if (size != ZR_NULL) {
            *size = 0;
        }
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return (TZrBytePtr)reader->bytes;
}

static void module_fixture_reader_close(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);

    if (customData != ZR_NULL) {
        free(customData);
    }
}

static TZrBool module_fixture_source_loader(SZrState *state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo *io) {
    TZrSize index;

    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || sourcePath == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < g_module_fixture_source_count; index++) {
        const SZrModuleFixtureSource *fixture = &g_module_fixture_sources[index];
        if (fixture->path != ZR_NULL &&
            fixture->source != ZR_NULL &&
            strcmp(fixture->path, sourcePath) == 0) {
            SZrModuleFixtureReader *reader =
                    (SZrModuleFixtureReader *)malloc(sizeof(SZrModuleFixtureReader));
            if (reader == ZR_NULL) {
                return ZR_FALSE;
            }

            reader->bytes = (const TZrByte *)fixture->source;
            reader->length = strlen(fixture->source);
            reader->consumed = ZR_FALSE;
            ZrCore_Io_Init(state, io, module_fixture_reader_read, module_fixture_reader_close, reader);
            io->isBinary = ZR_FALSE;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrObject *find_named_entry_in_array(SZrState *state,
                                            SZrObject *array,
                                            const TZrChar *fieldName,
                                            const TZrChar *expectedValue) {
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || fieldName == ZR_NULL || expectedValue == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < get_array_length(array); index++) {
        SZrObject *entry = get_array_entry_object(state, array, index);
        const SZrTypeValue *fieldValue = get_object_field_value(state, entry, fieldName);
        if (fieldValue != ZR_NULL &&
            fieldValue->type == ZR_VALUE_TYPE_STRING &&
            string_equals_cstring(ZR_CAST_STRING(state, fieldValue->value.object), expectedValue)) {
            return entry;
        }
    }

    return ZR_NULL;
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

void test_source_module_exports_complex_function_graph_without_null_call_targets(void) {
    static const SZrModuleFixtureSource kFixtures[] = {
            {
                    "lin_alg",
                    "projectVectorsImpl(seed) {\n"
                    "    return seed + 2;\n"
                    "}\n"
                    "pub var projectVectors = projectVectorsImpl;\n",
            },
            {
                    "signal",
                    "mixSignalImpl(seed) {\n"
                    "    return seed + 3;\n"
                    "}\n"
                    "pub var mixSignal = mixSignalImpl;\n",
            },
            {
                    "tensor_pipeline",
                    "runTensorPassImpl() {\n"
                    "    return 11;\n"
                    "}\n"
                    "pub var runTensorPass = runTensorPassImpl;\n",
            },
            {
                    "probe_callbacks",
                    "var lin = import(\"lin_alg\");\n"
                    "var signal = import(\"signal\");\n"
                    "var tensor = import(\"tensor_pipeline\");\n"
                    "\n"
                    "scaleValue(input) {\n"
                    "    return input + 1;\n"
                    "}\n"
                    "\n"
                    "runProbeImpl() {\n"
                    "    var vectorValue = lin.projectVectors(2);\n"
                    "    var signalValue = signal.mixSignal(5);\n"
                    "    var tensorValue = tensor.runTensorPass();\n"
                    "    return scaleValue(vectorValue + signalValue + tensorValue);\n"
                    "}\n"
                    "\n"
                    "summarizeProbeImpl(value) {\n"
                    "    return value + 7;\n"
                    "}\n"
                    "\n"
                    "pub var runProbe = runProbeImpl;\n"
                    "pub var summarizeProbe = summarizeProbeImpl;\n",
            },
    };
    SZrTestTimer timer;
    const char *testSummary = "Source Module Exports Complex Function Graph";
    const SZrModuleFixtureSource *previousFixtures = g_module_fixture_sources;
    TZrSize previousFixtureCount = g_module_fixture_source_count;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *runExport;
        const SZrTypeValue *summarizeExport;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);

        g_module_fixture_sources = kFixtures;
        g_module_fixture_source_count = ZR_ARRAY_COUNT(kFixtures);
        state->global->sourceLoader = module_fixture_source_loader;

        module = import_native_module(state, "probe_callbacks");
        TEST_ASSERT_NOT_NULL(module);

        runExport = get_module_export_value(state, module, "runProbe");
        summarizeExport = get_module_export_value(state, module, "summarizeProbe");
        TEST_ASSERT_NOT_NULL(runExport);
        TEST_ASSERT_NOT_NULL(summarizeExport);
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(runExport->type));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(summarizeExport->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FUNCTION(runExport->type) || ZR_VALUE_IS_TYPE_CLOSURE(runExport->type));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_FUNCTION(summarizeExport->type) || ZR_VALUE_IS_TYPE_CLOSURE(summarizeExport->type));

        TEST_ASSERT_TRUE(ZrLib_CallValue(state, runExport, ZR_NULL, ZR_NULL, 0, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
        TEST_ASSERT_EQUAL_INT64(24, result.value.nativeObject.nativeInt64);

        state->global->sourceLoader = ZR_NULL;
        g_module_fixture_sources = previousFixtures;
        g_module_fixture_source_count = previousFixtureCount;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_system_vm_call_module_export_executes_nested_native_export(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Vm CallModuleExport Executes Nested Native Export";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrTypeValue directArgument;
        SZrTypeValue directResult;
        const TZrChar *source =
                "var system = import(\"zr.system\");\n"
                "return system.vm.callModuleExport(\"zr.math\", \"sqrt\", [4.0]);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibMath_Register(state->global));

        ZrLib_Value_SetFloat(state, &directArgument, 4.0);
        TEST_ASSERT_TRUE(ZrLib_CallModuleExport(state, "zr.math", "sqrt", &directArgument, 1, &directResult));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(directResult.type));

        sourceName = ZrCore_String_Create(state, "system_vm_call_module_export_test.zr", 37);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_FALSE(ZR_VALUE_IS_TYPE_NULL(result.type));
        TEST_ASSERT_EQUAL_INT(directResult.type, result.type);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_system_root_aggregates_leaf_modules_and_reuses_cached_instances(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Root Aggregates Leaf Modules";
    static const struct {
        const TZrChar *exportName;
        const TZrChar *moduleName;
    } kExpectedModules[] = {
            {"console", "zr.system.console"},
            {"fs", "zr.system.fs"},
            {"env", "zr.system.env"},
            {"process", "zr.system.process"},
            {"gc", "zr.system.gc"},
            {"exception", "zr.system.exception"},
            {"vm", "zr.system.vm"},
    };

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *rootModule;
        TZrSize index;

        TEST_ASSERT_NOT_NULL(state);

        rootModule = import_native_module(state, "zr.system");
        TEST_ASSERT_NOT_NULL(rootModule);

        for (index = 0; index < ZR_ARRAY_COUNT(kExpectedModules); index++) {
            SZrObjectModule *leafModule = import_native_module(state, kExpectedModules[index].moduleName);
            const SZrTypeValue *exportValue = get_module_export_value(state, rootModule, kExpectedModules[index].exportName);
            SZrObject *exportObject;

            TEST_ASSERT_NOT_NULL(leafModule);
            TEST_ASSERT_NOT_NULL(exportValue);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, exportValue->type);

            exportObject = ZR_CAST_OBJECT(state, exportValue->value.object);
            TEST_ASSERT_NOT_NULL(exportObject);
            TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_MODULE, exportObject->internalType);
            TEST_ASSERT_EQUAL_PTR(leafModule, exportObject);
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_system_root_exports_only_submodules(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Root Exports Only Submodules";
    static const TZrChar *kAbsentExports[] = {
            "printLine",
            "println",
            "writeText",
            "vmState",
            "gcDisable",
            "callModuleExport",
            "SystemFileInfo",
            "SystemVmState",
            "SystemLoadedModuleInfo",
    };

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *rootModule;
        TZrSize index;

        TEST_ASSERT_NOT_NULL(state);

        rootModule = import_native_module(state, "zr.system");
        TEST_ASSERT_NOT_NULL(rootModule);

        for (index = 0; index < ZR_ARRAY_COUNT(kAbsentExports); index++) {
            TEST_ASSERT_NULL(get_module_export_value(state, rootModule, kAbsentExports[index]));
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_system_leaf_modules_expose_new_api_and_owned_types(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Leaf Modules Expose New API";
    static const TZrChar *kConsoleExports[] = {"print", "printLine", "printError", "printErrorLine"};
    static const TZrChar *kFsExports[] = {
            "currentDirectory",
            "changeCurrentDirectory",
            "pathExists",
            "isFile",
            "isDirectory",
            "createDirectory",
            "createDirectories",
            "removePath",
            "readText",
            "writeText",
            "appendText",
            "getInfo",
            "SystemFileInfo",
    };
    static const TZrChar *kEnvExports[] = {"getVariable"};
    static const TZrChar *kGcExports[] = {"start", "stop", "step", "collect"};
    static const TZrChar *kExceptionExports[] = {
            "registerUnhandledException",
            "Error",
            "StackFrame",
            "RuntimeError",
            "TypeError",
            "MemoryError",
            "ExceptionError",
    };
    static const TZrChar *kVmExports[] = {"loadedModules", "state", "callModuleExport", "SystemVmState", "SystemLoadedModuleInfo"};
    static const TZrChar *kConsoleAbsentExports[] = {"println", "eprint", "eprintln"};
    static const TZrChar *kProcessAbsentExports[] = {"args", "sleepMs"};

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *consoleModule;
        SZrObjectModule *fsModule;
        SZrObjectModule *envModule;
        SZrObjectModule *processModule;
        SZrObjectModule *gcModule;
        SZrObjectModule *exceptionModule;
        SZrObjectModule *vmModule;
        const SZrTypeValue *argumentsValue;
        TZrSize index;

        TEST_ASSERT_NOT_NULL(state);

        consoleModule = import_native_module(state, "zr.system.console");
        fsModule = import_native_module(state, "zr.system.fs");
        envModule = import_native_module(state, "zr.system.env");
        processModule = import_native_module(state, "zr.system.process");
        gcModule = import_native_module(state, "zr.system.gc");
        exceptionModule = import_native_module(state, "zr.system.exception");
        vmModule = import_native_module(state, "zr.system.vm");

        TEST_ASSERT_NOT_NULL(consoleModule);
        TEST_ASSERT_NOT_NULL(fsModule);
        TEST_ASSERT_NOT_NULL(envModule);
        TEST_ASSERT_NOT_NULL(processModule);
        TEST_ASSERT_NOT_NULL(gcModule);
        TEST_ASSERT_NOT_NULL(exceptionModule);
        TEST_ASSERT_NOT_NULL(vmModule);

        for (index = 0; index < ZR_ARRAY_COUNT(kConsoleExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, consoleModule, kConsoleExports[index]));
        }
        for (index = 0; index < ZR_ARRAY_COUNT(kConsoleAbsentExports); index++) {
            TEST_ASSERT_NULL(get_module_export_value(state, consoleModule, kConsoleAbsentExports[index]));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kFsExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, fsModule, kFsExports[index]));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kEnvExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, envModule, kEnvExports[index]));
        }

        argumentsValue = get_module_export_value(state, processModule, "arguments");
        TEST_ASSERT_NOT_NULL(argumentsValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, argumentsValue->type);
        TEST_ASSERT_NOT_NULL(get_module_export_value(state, processModule, "sleepMilliseconds"));
        TEST_ASSERT_NOT_NULL(get_module_export_value(state, processModule, "exit"));
        for (index = 0; index < ZR_ARRAY_COUNT(kProcessAbsentExports); index++) {
            TEST_ASSERT_NULL(get_module_export_value(state, processModule, kProcessAbsentExports[index]));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kGcExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, gcModule, kGcExports[index]));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kExceptionExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, exceptionModule, kExceptionExports[index]));
        }

        for (index = 0; index < ZR_ARRAY_COUNT(kVmExports); index++) {
            TEST_ASSERT_NOT_NULL(get_module_export_value(state, vmModule, kVmExports[index]));
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_system_native_types_register_complete_struct_fields(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Native Types Register Complete Struct Fields";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *fsModule;
        SZrObjectModule *vmModule;
        SZrObjectPrototype *fileInfoPrototype;
        SZrObjectPrototype *vmStatePrototype;
        SZrObjectPrototype *loadedModuleInfoPrototype;
        TZrUInt64 offset = 0;

        TEST_ASSERT_NOT_NULL(state);

        fsModule = import_native_module(state, "zr.system.fs");
        vmModule = import_native_module(state, "zr.system.vm");
        TEST_ASSERT_NOT_NULL(fsModule);
        TEST_ASSERT_NOT_NULL(vmModule);

        fileInfoPrototype = get_module_exported_prototype(state, fsModule, "SystemFileInfo");
        vmStatePrototype = get_module_exported_prototype(state, vmModule, "SystemVmState");
        loadedModuleInfoPrototype = get_module_exported_prototype(state, vmModule, "SystemLoadedModuleInfo");

        TEST_ASSERT_NOT_NULL(fileInfoPrototype);
        TEST_ASSERT_NOT_NULL(vmStatePrototype);
        TEST_ASSERT_NOT_NULL(loadedModuleInfoPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, fileInfoPrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, vmStatePrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, loadedModuleInfoPrototype->type);

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "path", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "size", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "isFile", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "isDirectory", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)fileInfoPrototype, "modifiedMilliseconds", &offset));

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "loadedModuleCount", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "garbageCollectionMode", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "garbageCollectionDebt", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "garbageCollectionThreshold", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "stackDepth", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)vmStatePrototype, "frameDepth", &offset));

        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "name", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "sourceKind", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "sourcePath", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "registrationKind", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "hasTypeHints", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "moduleVersion", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "runtimeAbiVersion", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "requiredCapabilities", &offset));
        TEST_ASSERT_TRUE(lookup_struct_field_offset(state, (SZrStructPrototype *)loadedModuleInfoPrototype, "isDescriptorPlugin", &offset));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_system_root_native_module_info_exposes_module_links(void) {
    SZrTestTimer timer;
    const char *testSummary = "System Root Native Module Info Exposes Module Links";
    static const struct {
        const TZrChar *name;
        const TZrChar *moduleName;
    } kExpectedModules[] = {
            {"console", "zr.system.console"},
            {"fs", "zr.system.fs"},
            {"env", "zr.system.env"},
            {"process", "zr.system.process"},
            {"gc", "zr.system.gc"},
            {"exception", "zr.system.exception"},
            {"vm", "zr.system.vm"},
    };

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *rootModule;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *functionsValue;
        const SZrTypeValue *constantsValue;
        const SZrTypeValue *typesValue;
        const SZrTypeValue *modulesValue;
        SZrObject *moduleInfo;
        TZrSize index;

        TEST_ASSERT_NOT_NULL(state);

        rootModule = import_native_module(state, "zr.system");
        TEST_ASSERT_NOT_NULL(rootModule);

        moduleInfoValue = get_module_export_value(state, rootModule, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        functionsValue = get_object_field_value(state, moduleInfo, "functions");
        constantsValue = get_object_field_value(state, moduleInfo, "constants");
        typesValue = get_object_field_value(state, moduleInfo, "types");
        modulesValue = get_object_field_value(state, moduleInfo, "modules");

        TEST_ASSERT_NOT_NULL(functionsValue);
        TEST_ASSERT_NOT_NULL(constantsValue);
        TEST_ASSERT_NOT_NULL(typesValue);
        TEST_ASSERT_NOT_NULL(modulesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, functionsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, constantsValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, modulesValue->type);
        TEST_ASSERT_EQUAL_UINT64(0, get_array_length(ZR_CAST_OBJECT(state, functionsValue->value.object)));
        TEST_ASSERT_EQUAL_UINT64(0, get_array_length(ZR_CAST_OBJECT(state, constantsValue->value.object)));
        TEST_ASSERT_EQUAL_UINT64(0, get_array_length(ZR_CAST_OBJECT(state, typesValue->value.object)));
        TEST_ASSERT_EQUAL_UINT64(ZR_ARRAY_COUNT(kExpectedModules), get_array_length(ZR_CAST_OBJECT(state, modulesValue->value.object)));

        for (index = 0; index < ZR_ARRAY_COUNT(kExpectedModules); index++) {
            SZrObject *entry = find_named_entry_in_array(state,
                                                         ZR_CAST_OBJECT(state, modulesValue->value.object),
                                                         "name",
                                                         kExpectedModules[index].name);
            const SZrTypeValue *moduleNameValue;

            TEST_ASSERT_NOT_NULL(entry);
            moduleNameValue = get_object_field_value(state, entry, "moduleName");
            TEST_ASSERT_NOT_NULL(moduleNameValue);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, moduleNameValue->type);
            TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, moduleNameValue->value.object),
                                                   kExpectedModules[index].moduleName));
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_native_module_info_exposes_enum_and_interface_descriptors(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Module Info Exposes Enum And Interface Descriptors";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        const SZrTypeValue *moduleInfoValue;
        const SZrTypeValue *typesValue;
        SZrObject *moduleInfo;
        SZrObject *typesArray;
        SZrObject *readableEntry;
        SZrObject *streamReadableEntry;
        SZrObject *enumEntry;
        SZrObject *deviceEntry;
        const SZrTypeValue *prototypeTypeValue;
        const SZrTypeValue *allowValueValue;
        const SZrTypeValue *allowBoxedValue;
        const SZrTypeValue *extendsValue;
        const SZrTypeValue *constructorSignatureValue;
        const SZrTypeValue *implementsValue;
        const SZrTypeValue *enumMembersValue;
        const SZrTypeValue *enumValueTypeValue;
        const SZrTypeValue *firstImplementValue;
        SZrObject *enumMembersArray;
        SZrObject *onMemberEntry;
        const SZrTypeValue *onMemberIntValue;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(register_probe_native_module(state));

        module = import_native_module(state, "probe.native_shapes");
        TEST_ASSERT_NOT_NULL(module);

        moduleInfoValue = get_module_export_value(state, module, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
        TEST_ASSERT_NOT_NULL(moduleInfoValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);

        moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
        TEST_ASSERT_NOT_NULL(moduleInfo);

        typesValue = get_object_field_value(state, moduleInfo, "types");
        TEST_ASSERT_NOT_NULL(typesValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);

        typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
        TEST_ASSERT_NOT_NULL(typesArray);
        TEST_ASSERT_EQUAL_UINT64(4, get_array_length(typesArray));

        readableEntry = find_named_entry_in_array(state, typesArray, "name", "NativeReadable");
        streamReadableEntry = find_named_entry_in_array(state, typesArray, "name", "NativeStreamReadable");
        enumEntry = find_named_entry_in_array(state, typesArray, "name", "NativeMode");
        deviceEntry = find_named_entry_in_array(state, typesArray, "name", "NativeDevice");

        TEST_ASSERT_NOT_NULL(readableEntry);
        TEST_ASSERT_NOT_NULL(streamReadableEntry);
        TEST_ASSERT_NOT_NULL(enumEntry);
        TEST_ASSERT_NOT_NULL(deviceEntry);

        prototypeTypeValue = get_object_field_value(state, readableEntry, "prototypeType");
        allowValueValue = get_object_field_value(state, readableEntry, "allowValueConstruction");
        allowBoxedValue = get_object_field_value(state, readableEntry, "allowBoxedConstruction");
        constructorSignatureValue = get_object_field_value(state, readableEntry, "constructorSignature");
        TEST_ASSERT_NOT_NULL(prototypeTypeValue);
        TEST_ASSERT_NOT_NULL(allowValueValue);
        TEST_ASSERT_NOT_NULL(allowBoxedValue);
        TEST_ASSERT_NOT_NULL(constructorSignatureValue);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, prototypeTypeValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, allowValueValue->type);
        TEST_ASSERT_FALSE(allowValueValue->value.nativeObject.nativeBool);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, allowBoxedValue->type);
        TEST_ASSERT_FALSE(allowBoxedValue->value.nativeObject.nativeBool);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, constructorSignatureValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, constructorSignatureValue->value.object),
                                               "NativeReadable()"));

        prototypeTypeValue = get_object_field_value(state, streamReadableEntry, "prototypeType");
        extendsValue = get_object_field_value(state, streamReadableEntry, "extendsTypeName");
        TEST_ASSERT_NOT_NULL(prototypeTypeValue);
        TEST_ASSERT_NOT_NULL(extendsValue);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, prototypeTypeValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, extendsValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, extendsValue->value.object), "NativeReadable"));

        prototypeTypeValue = get_object_field_value(state, enumEntry, "prototypeType");
        allowValueValue = get_object_field_value(state, enumEntry, "allowValueConstruction");
        allowBoxedValue = get_object_field_value(state, enumEntry, "allowBoxedConstruction");
        constructorSignatureValue = get_object_field_value(state, enumEntry, "constructorSignature");
        enumMembersValue = get_object_field_value(state, enumEntry, "enumMembers");
        enumValueTypeValue = get_object_field_value(state, enumEntry, "enumValueTypeName");
        TEST_ASSERT_NOT_NULL(prototypeTypeValue);
        TEST_ASSERT_NOT_NULL(allowValueValue);
        TEST_ASSERT_NOT_NULL(allowBoxedValue);
        TEST_ASSERT_NOT_NULL(constructorSignatureValue);
        TEST_ASSERT_NOT_NULL(enumMembersValue);
        TEST_ASSERT_NOT_NULL(enumValueTypeValue);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_ENUM, prototypeTypeValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_TRUE(allowValueValue->value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(allowBoxedValue->value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, constructorSignatureValue->value.object),
                                               "NativeMode(value: int)"));
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, enumValueTypeValue->value.object), "int"));

        enumMembersArray = ZR_CAST_OBJECT(state, enumMembersValue->value.object);
        TEST_ASSERT_NOT_NULL(enumMembersArray);
        TEST_ASSERT_EQUAL_UINT64(2, get_array_length(enumMembersArray));
        onMemberEntry = find_named_entry_in_array(state, enumMembersArray, "name", "On");
        TEST_ASSERT_NOT_NULL(onMemberEntry);
        onMemberIntValue = get_object_field_value(state, onMemberEntry, "intValue");
        TEST_ASSERT_NOT_NULL(onMemberIntValue);
        TEST_ASSERT_EQUAL_INT64(1, onMemberIntValue->value.nativeObject.nativeInt64);

        prototypeTypeValue = get_object_field_value(state, deviceEntry, "prototypeType");
        implementsValue = get_object_field_value(state, deviceEntry, "implements");
        TEST_ASSERT_NOT_NULL(prototypeTypeValue);
        TEST_ASSERT_NOT_NULL(implementsValue);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_CLASS, prototypeTypeValue->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, implementsValue->type);
        TEST_ASSERT_EQUAL_UINT64(1, get_array_length(ZR_CAST_OBJECT(state, implementsValue->value.object)));
        firstImplementValue = get_array_entry_value(state, ZR_CAST_OBJECT(state, implementsValue->value.object), 0);
        TEST_ASSERT_NOT_NULL(firstImplementValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, firstImplementValue->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, firstImplementValue->value.object),
                                               "NativeStreamReadable"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_native_module_runtime_registers_enum_members_and_interface_inheritance(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Module Runtime Registers Enum Members And Interface Inheritance";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrObjectModule *module;
        SZrObjectPrototype *readablePrototype;
        SZrObjectPrototype *streamReadablePrototype;
        SZrObjectPrototype *enumPrototype;
        const SZrTypeValue *onValue;
        SZrObject *onObject;
        const SZrTypeValue *enumValueField;
        const SZrTypeValue *enumNameField;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(register_probe_native_module(state));

        module = import_native_module(state, "probe.native_shapes");
        TEST_ASSERT_NOT_NULL(module);

        readablePrototype = get_module_exported_prototype(state, module, "NativeReadable");
        streamReadablePrototype = get_module_exported_prototype(state, module, "NativeStreamReadable");
        enumPrototype = get_module_exported_prototype(state, module, "NativeMode");

        TEST_ASSERT_NOT_NULL(readablePrototype);
        TEST_ASSERT_NOT_NULL(streamReadablePrototype);
        TEST_ASSERT_NOT_NULL(enumPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, readablePrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, streamReadablePrototype->type);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_ENUM, enumPrototype->type);
        TEST_ASSERT_EQUAL_PTR(readablePrototype, streamReadablePrototype->superPrototype);

        onValue = get_object_field_value(state, &enumPrototype->super, "On");
        TEST_ASSERT_NOT_NULL(onValue);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, onValue->type);

        onObject = ZR_CAST_OBJECT(state, onValue->value.object);
        TEST_ASSERT_NOT_NULL(onObject);
        TEST_ASSERT_EQUAL_PTR(enumPrototype, onObject->prototype);

        enumValueField = get_object_field_value(state, onObject, "__zr_enumValue");
        enumNameField = get_object_field_value(state, onObject, "__zr_enumName");
        TEST_ASSERT_NOT_NULL(enumValueField);
        TEST_ASSERT_NOT_NULL(enumNameField);
        TEST_ASSERT_EQUAL_INT64(1, enumValueField->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, enumNameField->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, enumNameField->value.object), "On"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_native_enum_construction_returns_runtime_enum_instance(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Enum Construction Returns Runtime Enum Instance";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *source =
                "var probe = import(\"probe.native_shapes\");\n"
                "return $probe.NativeMode(1);\n";
        SZrString *sourceName;
        SZrFunction *entryFunction;
        SZrTypeValue result;
        SZrObjectModule *module;
        SZrObjectPrototype *enumPrototype;
        SZrObject *resultObject;
        const SZrTypeValue *enumValueField;
        const SZrTypeValue *enumNameField;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(register_probe_native_module(state));

        sourceName = ZrCore_String_Create(state, "probe_native_enum_runtime_test.zr", 33);
        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, entryFunction, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);

        resultObject = ZR_CAST_OBJECT(state, result.value.object);
        TEST_ASSERT_NOT_NULL(resultObject);

        module = import_native_module(state, "probe.native_shapes");
        TEST_ASSERT_NOT_NULL(module);
        enumPrototype = get_module_exported_prototype(state, module, "NativeMode");
        TEST_ASSERT_NOT_NULL(enumPrototype);
        TEST_ASSERT_EQUAL_PTR(enumPrototype, resultObject->prototype);

        enumValueField = get_object_field_value(state, resultObject, "__zr_enumValue");
        enumNameField = get_object_field_value(state, resultObject, "__zr_enumName");
        TEST_ASSERT_NOT_NULL(enumValueField);
        TEST_ASSERT_NOT_NULL(enumNameField);
        TEST_ASSERT_EQUAL_INT64(1, enumValueField->value.nativeObject.nativeInt64);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, enumNameField->type);
        TEST_ASSERT_TRUE(string_equals_cstring(ZR_CAST_STRING(state, enumNameField->value.object), "On"));

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_native_registry_rejects_future_runtime_abi(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Registry Rejects Future Runtime ABI";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *errorMessage;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_RegisterModule(state->global, &kProbeFutureAbiModuleDescriptor));
        TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_VERSION_MISMATCH,
                              ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
        errorMessage = ZrLibrary_NativeRegistry_GetLastErrorMessage(state->global);
        TEST_ASSERT_NOT_NULL(errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(errorMessage, "probe.future_abi"));
        TEST_ASSERT_NOT_NULL(strstr(errorMessage, "requires runtime ABI"));
        TEST_ASSERT_NULL(ZrLibrary_NativeRegistry_FindModule(state->global, "probe.future_abi"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_native_registry_rejects_unsupported_capabilities(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Registry Rejects Unsupported Capabilities";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const TZrChar *errorMessage;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_FALSE(
                ZrLibrary_NativeRegistry_RegisterModule(state->global, &kProbeUnsupportedCapabilityModuleDescriptor));
        TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_CAPABILITY_MISMATCH,
                              ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
        errorMessage = ZrLibrary_NativeRegistry_GetLastErrorMessage(state->global);
        TEST_ASSERT_NOT_NULL(errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(errorMessage, "probe.unsupported_capability"));
        TEST_ASSERT_NOT_NULL(strstr(errorMessage, "unsupported capabilities"));
        TEST_ASSERT_NULL(ZrLibrary_NativeRegistry_FindModule(state->global, "probe.unsupported_capability"));

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
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

    // 12. 复杂 source module 导出函数图不应出现 null call target
    RUN_TEST(test_source_module_exports_complex_function_graph_without_null_call_targets);

    // 13. zr.system.vm.callModuleExport 可执行嵌套 native 导出
    RUN_TEST(test_system_vm_call_module_export_executes_nested_native_export);

    // 14. zr.system 聚合根导出叶子模块
    RUN_TEST(test_system_root_aggregates_leaf_modules_and_reuses_cached_instances);

    // 15. zr.system 根模块仅导出子模块
    RUN_TEST(test_system_root_exports_only_submodules);

    // 16. 叶子模块导出新的 system API
    RUN_TEST(test_system_leaf_modules_expose_new_api_and_owned_types);

    // 17. system 原生类型字段元信息完整
    RUN_TEST(test_system_native_types_register_complete_struct_fields);

    // 18. 根模块原生元信息包含 modules 数组
    RUN_TEST(test_system_root_native_module_info_exposes_module_links);

    // 19. native enum/interface descriptor 元信息完整暴露
    RUN_TEST(test_native_module_info_exposes_enum_and_interface_descriptors);

    // 20. native runtime 注册 enum 静态成员和 interface 继承链
    RUN_TEST(test_native_module_runtime_registers_enum_members_and_interface_inheritance);

    // 21. native enum 构造在 runtime 返回正确实例
    RUN_TEST(test_native_enum_construction_returns_runtime_enum_instance);

    // 22. native registry 拒绝未来 ABI 版本
    RUN_TEST(test_native_registry_rejects_future_runtime_abi);

    // 23. native registry 拒绝未支持 capability
    RUN_TEST(test_native_registry_rejects_unsupported_capabilities);

    return UNITY_END();
}
