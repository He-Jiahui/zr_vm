//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_library/native_registry.h"
#include "test_support.h"
#include "../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h"

extern void ZrParser_Expression_Compile(SZrCompilerState *cs, SZrAstNode *node);
extern void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node);

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

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const ZrLibMethodDescriptor kProbeReadableMethods[] = {
        {
                .name = "read",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = ZR_NULL,
                .returnTypeName = "int",
                .documentation = "Read the current value.",
                .isStatic = ZR_FALSE,
                .parameters = ZR_NULL,
                .parameterCount = 0,
        },
};

static const ZrLibMethodDescriptor kProbeStreamReadableMethods[] = {
        {
                .name = "available",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = ZR_NULL,
                .returnTypeName = "int",
                .documentation = "Return the available item count.",
                .isStatic = ZR_FALSE,
                .parameters = ZR_NULL,
                .parameterCount = 0,
        },
};

static const ZrLibParameterDescriptor kProbeConfigureParameters[] = {
        {"mode", "NativeMode", "The mode to apply."},
};

static const ZrLibMethodDescriptor kProbeDeviceMethods[] = {
        {
                .name = "configure",
                .minArgumentCount = 1,
                .maxArgumentCount = 1,
                .callback = ZR_NULL,
                .returnTypeName = "NativeMode",
                .documentation = "Apply a new device mode.",
                .isStatic = ZR_FALSE,
                .parameters = kProbeConfigureParameters,
                .parameterCount = ZR_ARRAY_COUNT(kProbeConfigureParameters),
        },
};

static const ZrLibParameterDescriptor kProbeLookupParameters[] = {
        {"key", "K", "The lookup key."},
};

static const ZrLibMethodDescriptor kProbeLookupMethods[] = {
        {
                .name = "lookup",
                .minArgumentCount = 1,
                .maxArgumentCount = 1,
                .callback = ZR_NULL,
                .returnTypeName = "V",
                .documentation = "Resolve a value from the lookup table.",
                .isStatic = ZR_FALSE,
                .parameters = kProbeLookupParameters,
                .parameterCount = ZR_ARRAY_COUNT(kProbeLookupParameters),
        },
};

static const ZrLibParameterDescriptor kProbeCreateDeviceParameters[] = {
        {"mode", "NativeMode", "The initial device mode."},
};

static const ZrLibFunctionDescriptor kProbeNativeFunctions[] = {
        {
                .name = "createDevice",
                .minArgumentCount = 1,
                .maxArgumentCount = 1,
                .callback = ZR_NULL,
                .returnTypeName = "NativeDevice",
                .documentation = "Create a native device.",
                .parameters = kProbeCreateDeviceParameters,
                .parameterCount = ZR_ARRAY_COUNT(kProbeCreateDeviceParameters),
        },
};

static const TZrChar *kProbeDeviceImplements[] = {
        "NativeStreamReadable",
};

static const ZrLibFieldDescriptor kProbeDeviceFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("mode", "NativeMode", "The current device mode."),
};

static const ZrLibEnumMemberDescriptor kProbeModeMembers[] = {
        {"Off", ZR_LIB_CONSTANT_KIND_INT, 0, 0.0, ZR_NULL, ZR_FALSE, "Disabled mode."},
        {"On", ZR_LIB_CONSTANT_KIND_INT, 1, 0.0, ZR_NULL, ZR_FALSE, "Enabled mode."},
};

static const ZrLibFieldDescriptor kProbeBoxFields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("value", "T", "The boxed value."),
};

static const ZrLibMethodDescriptor kProbeBoxMethods[] = {
        {
                .name = "getValue",
                .minArgumentCount = 0,
                .maxArgumentCount = 0,
                .callback = ZR_NULL,
                .returnTypeName = "T",
                .documentation = "Return the boxed value.",
                .isStatic = ZR_FALSE,
        },
};

static const ZrLibGenericParameterDescriptor kProbeBoxGenericParameters[] = {
        {
                .name = "T",
                .documentation = "The boxed element type.",
        },
};

static const TZrChar *kProbeLookupKeyConstraints[] = {
        "NativeReadable",
        "NativeStreamReadable",
};

static const ZrLibGenericParameterDescriptor kProbeLookupGenericParameters[] = {
        {
                .name = "K",
                .documentation = "The lookup key type.",
                .constraintTypeNames = kProbeLookupKeyConstraints,
                .constraintTypeCount = ZR_ARRAY_COUNT(kProbeLookupKeyConstraints),
        },
        {
                .name = "V",
                .documentation = "The lookup value type.",
        },
};

static const ZrLibTypeDescriptor kProbeNativeTypes[] = {
        {
                .name = "NativeReadable",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                .methods = kProbeReadableMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeReadableMethods),
                .documentation = "Readable interface.",
                .allowValueConstruction = ZR_FALSE,
                .allowBoxedConstruction = ZR_FALSE,
                .constructorSignature = "NativeReadable()",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
        },
        {
                .name = "NativeStreamReadable",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                .methods = kProbeStreamReadableMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeStreamReadableMethods),
                .documentation = "Stream-readable interface.",
                .extendsTypeName = "NativeReadable",
                .allowValueConstruction = ZR_FALSE,
                .allowBoxedConstruction = ZR_FALSE,
                .constructorSignature = "NativeStreamReadable()",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
        },
        {
                .name = "NativeMode",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_ENUM,
                .documentation = "Device mode enum.",
                .enumMembers = kProbeModeMembers,
                .enumMemberCount = ZR_ARRAY_COUNT(kProbeModeMembers),
                .enumValueTypeName = "int",
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeMode(value: int)",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
        },
        {
                .name = "NativeDevice",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .fields = kProbeDeviceFields,
                .fieldCount = ZR_ARRAY_COUNT(kProbeDeviceFields),
                .methods = kProbeDeviceMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeDeviceMethods),
                .documentation = "Concrete device type.",
                .implementsTypeNames = kProbeDeviceImplements,
                .implementsTypeCount = ZR_ARRAY_COUNT(kProbeDeviceImplements),
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeDevice()",
                .genericParameters = ZR_NULL,
                .genericParameterCount = 0,
        },
        {
                .name = "NativeBox",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .fields = kProbeBoxFields,
                .fieldCount = ZR_ARRAY_COUNT(kProbeBoxFields),
                .methods = kProbeBoxMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeBoxMethods),
                .documentation = "Open generic native box type.",
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeBox(value: T)",
                .genericParameters = kProbeBoxGenericParameters,
                .genericParameterCount = ZR_ARRAY_COUNT(kProbeBoxGenericParameters),
        },
        {
                .name = "NativeLookup",
                .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                .methods = kProbeLookupMethods,
                .methodCount = ZR_ARRAY_COUNT(kProbeLookupMethods),
                .documentation = "Open generic native lookup type.",
                .allowValueConstruction = ZR_TRUE,
                .allowBoxedConstruction = ZR_TRUE,
                .constructorSignature = "NativeLookup()",
                .genericParameters = kProbeLookupGenericParameters,
                .genericParameterCount = ZR_ARRAY_COUNT(kProbeLookupGenericParameters),
        },
};

static const ZrLibModuleDescriptor kProbeNativeModuleDescriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "probe.native_shapes",
        .constants = ZR_NULL,
        .constantCount = 0,
        .functions = kProbeNativeFunctions,
        .functionCount = ZR_ARRAY_COUNT(kProbeNativeFunctions),
        .types = kProbeNativeTypes,
        .typeCount = ZR_ARRAY_COUNT(kProbeNativeTypes),
        .typeHints = ZR_NULL,
        .typeHintCount = 0,
        .typeHintsJson = ZR_NULL,
        .documentation = "Native test module containing interface, enum and implements metadata.",
        .moduleLinks = ZR_NULL,
        .moduleLinkCount = 0,
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities = 0,
};

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
    SZrState *mainState = ZrTests_State_Create(ZR_NULL);

    if (mainState != ZR_NULL && mainState->global != ZR_NULL) {
        ZrVmLibContainer_Register(mainState->global);
        ZrVmLibMath_Register(mainState->global);
        ZrVmLibSystem_Register(mainState->global);
        ZrVmLibFfi_Register(mainState->global);
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
    if (state != ZR_NULL) {
        ZrTests_State_Destroy(state);
    }
}

// 创建测试用的编译器状态
static SZrCompilerState *create_test_compiler_state(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    SZrCompilerState *cs = (SZrCompilerState *) malloc(sizeof(SZrCompilerState));
    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_CompilerState_Init(cs, state);
    return cs;
}

// 销毁测试用的编译器状态
static void destroy_test_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    if (cs->topLevelFunction != ZR_NULL && cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(cs->state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }

    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(cs->state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static void ensure_test_root_scope(SZrCompilerState *cs) {
    SZrScope scope;

    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->state);

    if (cs->scopeStack.length != 0) {
        return;
    }

    memset(&scope, 0, sizeof(scope));
    scope.startVarIndex = cs->localVarCount;
    scope.parentCompiler = cs->currentFunction != ZR_NULL ? cs : ZR_NULL;
    ZrCore_Array_Push(cs->state, &cs->scopeStack, &scope);
}

static void compile_test_top_level_statement(SZrCompilerState *cs, SZrAstNode *node) {
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(node);

    if (cs->currentFunction == ZR_NULL) {
        cs->currentFunction = ZrCore_Function_New(cs->state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
    }

    cs->isScriptLevel = ZR_TRUE;
    if (cs->scopeStack.length == 0) {
        ensure_test_root_scope(cs);
    }

    switch (node->type) {
        case ZR_AST_INTERFACE_DECLARATION:
            ZrParser_Compiler_CompileInterfaceDeclaration(cs, node);
            break;
        case ZR_AST_CLASS_DECLARATION:
            ZrParser_Compiler_CompileClassDeclaration(cs, node);
            break;
        case ZR_AST_STRUCT_DECLARATION:
            ZrParser_Compiler_CompileStructDeclaration(cs, node);
            break;
        default:
            ZrParser_Statement_Compile(cs, node);
            break;
    }
}

static SZrString *create_test_string(SZrState *state, const char *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_CreateFromNative(state, (TZrNativeString)value);
}

typedef struct SZrImportTestFixtureSource {
    const TZrChar *moduleName;
    const TZrByte *bytes;
    TZrSize length;
    TZrBool isBinary;
} SZrImportTestFixtureSource;

typedef struct SZrImportTestFixtureReader {
    const TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrImportTestFixtureReader;

static SZrImportTestFixtureSource g_single_import_test_fixture = {0};
static const SZrImportTestFixtureSource *g_import_test_fixtures = ZR_NULL;
static TZrSize g_import_test_fixture_count = 0;

static TZrBytePtr import_test_fixture_read(SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrImportTestFixtureReader *fixture = (SZrImportTestFixtureReader *)customData;
    ZR_UNUSED_PARAMETER(state);

    if (size == ZR_NULL || fixture == ZR_NULL || fixture->consumed || fixture->bytes == ZR_NULL || fixture->length == 0) {
        if (size != ZR_NULL) {
            *size = 0;
        }
        return ZR_NULL;
    }

    fixture->consumed = ZR_TRUE;
    *size = fixture->length;
    return (TZrBytePtr)fixture->bytes;
}

static void import_test_fixture_close(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);

    if (customData != ZR_NULL) {
        free(customData);
    }
}

static TZrBool import_test_fixture_source_loader(SZrState *state,
                                                 TZrNativeString sourcePath,
                                                 TZrNativeString md5,
                                                 SZrIo *io) {
    TZrSize index;

    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || sourcePath == ZR_NULL || io == ZR_NULL || g_import_test_fixtures == ZR_NULL ||
        g_import_test_fixture_count == 0) {
        return ZR_FALSE;
    }

    for (index = 0; index < g_import_test_fixture_count; ++index) {
        const SZrImportTestFixtureSource *fixture = &g_import_test_fixtures[index];
        SZrImportTestFixtureReader *reader;

        if (fixture->moduleName == ZR_NULL || strcmp(sourcePath, fixture->moduleName) != 0) {
            continue;
        }

        reader = (SZrImportTestFixtureReader *)malloc(sizeof(*reader));
        if (reader == ZR_NULL) {
            return ZR_FALSE;
        }

        reader->bytes = fixture->bytes;
        reader->length = fixture->length;
        reader->consumed = ZR_FALSE;
        ZrCore_Io_Init(state, io, import_test_fixture_read, import_test_fixture_close, reader);
        io->isBinary = fixture->isBinary;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void install_import_test_fixtures(SZrState *state,
                                         const SZrImportTestFixtureSource *fixtures,
                                         TZrSize fixtureCount) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(fixtures);
    TEST_ASSERT_TRUE(fixtureCount > 0);

    ZrParser_ToGlobalState_Register(state);
    g_import_test_fixtures = fixtures;
    g_import_test_fixture_count = fixtureCount;
    state->global->sourceLoader = import_test_fixture_source_loader;
}

static void install_import_test_fixture(SZrState *state,
                                        const TZrChar *moduleName,
                                        const TZrByte *bytes,
                                        TZrSize length,
                                        TZrBool isBinary) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_TRUE(length > 0);

    g_single_import_test_fixture.moduleName = moduleName;
    g_single_import_test_fixture.bytes = bytes;
    g_single_import_test_fixture.length = length;
    g_single_import_test_fixture.isBinary = isBinary;
    install_import_test_fixtures(state, &g_single_import_test_fixture, 1);
}

static TZrByte *read_test_file_bytes(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (path == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    *outLength = (TZrSize)fileSize;
    return buffer;
}

static TZrByte *build_binary_import_fixture(SZrState *state,
                                            const TZrChar *moduleSource,
                                            const TZrChar *binaryPath,
                                            TZrSize *outLength) {
    SZrString *sourceName;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(moduleSource);
    TEST_ASSERT_NOT_NULL(binaryPath);
    TEST_ASSERT_NOT_NULL(outLength);

    sourceName = ZrCore_String_Create(state, (TZrNativeString) binaryPath, strlen(binaryPath));
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, moduleSource, strlen(moduleSource), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

    return read_test_file_bytes(binaryPath, outLength);
}

static void init_test_object_type(SZrState *state,
                                  SZrInferredType *type,
                                  const char *typeName,
                                  EZrOwnershipQualifier ownershipQualifier) {
    SZrString *zrTypeName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_NOT_NULL(typeName);

    zrTypeName = create_test_string(state, typeName);
    TEST_ASSERT_NOT_NULL(zrTypeName);

    ZrParser_InferredType_InitFull(state, type, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, zrTypeName);
    type->ownershipQualifier = ownershipQualifier;
}

static void init_test_type_prototype(SZrState *state,
                                     SZrTypePrototypeInfo *info,
                                     const char *typeName,
                                     EZrObjectPrototypeType prototypeType) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_NOT_NULL(typeName);

    memset(info, 0, sizeof(*info));
    info->name = create_test_string(state, typeName);
    TEST_ASSERT_NOT_NULL(info->name);
    info->type = prototypeType;
    info->accessModifier = ZR_ACCESS_PUBLIC;
    info->isImportedNative = ZR_FALSE;
    info->allowValueConstruction = ZR_TRUE;
    info->allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(state, &info->inherits, sizeof(SZrString *), 2);
    ZrCore_Array_Init(state, &info->implements, sizeof(SZrString *), 2);
    ZrCore_Array_Init(state, &info->genericParameters, sizeof(SZrTypeGenericParameterInfo), 2);
    ZrCore_Array_Init(state, &info->decorators, sizeof(SZrTypeDecoratorInfo), 1);
    ZrCore_Array_Init(state, &info->members, sizeof(SZrTypeMemberInfo), 4);
}

static void register_test_type_prototype(SZrState *state,
                                         SZrCompilerState *cs,
                                         SZrTypePrototypeInfo *info) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_NOT_NULL(info->name);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(state, cs->typeEnv, info->name));
    ZrCore_Array_Push(state, &cs->typePrototypes, info);
}

static const SZrTypePrototypeInfo *find_test_type_prototype(const SZrCompilerState *cs,
                                                            const char *typeName) {
    TZrSize index;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < cs->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *info =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get((SZrArray *)&cs->typePrototypes, index);
        if (info != ZR_NULL &&
            info->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(info->name), typeName) == 0) {
            return info;
        }
    }

    return ZR_NULL;
}

static void add_test_method_member(SZrState *state,
                                   SZrTypePrototypeInfo *info,
                                   const char *name,
                                   const char *returnTypeName,
                                   EZrOwnershipQualifier receiverQualifier) {
    SZrTypeMemberInfo memberInfo;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_NOT_NULL(name);

    memset(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.memberType =
            info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ? ZR_AST_STRUCT_METHOD : ZR_AST_CLASS_METHOD;
    memberInfo.name = create_test_string(state, name);
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.receiverQualifier = receiverQualifier;
    if (returnTypeName != ZR_NULL) {
        memberInfo.returnTypeName = create_test_string(state, returnTypeName);
        TEST_ASSERT_NOT_NULL(memberInfo.returnTypeName);
    }

    TEST_ASSERT_NOT_NULL(memberInfo.name);
    ZrCore_Array_Push(state, &info->members, &memberInfo);
}

static void add_test_field_member(SZrState *state,
                                  SZrTypePrototypeInfo *info,
                                  const char *name,
                                  const char *fieldTypeName,
                                  EZrOwnershipQualifier ownershipQualifier) {
    SZrTypeMemberInfo memberInfo;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_NOT_NULL(name);

    memset(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.memberType =
            info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ? ZR_AST_STRUCT_FIELD : ZR_AST_CLASS_FIELD;
    memberInfo.name = create_test_string(state, name);
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.ownershipQualifier = ownershipQualifier;
    memberInfo.fieldOffset = 0;
    memberInfo.fieldSize = sizeof(TZrUInt64);
    memberInfo.declarationOrder = (TZrUInt32)info->members.length;
    if (fieldTypeName != ZR_NULL) {
        memberInfo.fieldTypeName = create_test_string(state, fieldTypeName);
        TEST_ASSERT_NOT_NULL(memberInfo.fieldTypeName);
    }

    TEST_ASSERT_NOT_NULL(memberInfo.name);
    ZrCore_Array_Push(state, &info->members, &memberInfo);
}

static void register_test_function_with_one_param(SZrState *state,
                                                  SZrCompilerState *cs,
                                                  const char *functionName,
                                                  EZrValueType returnBaseType,
                                                  const char *returnTypeName,
                                                  EZrValueType paramBaseType,
                                                  const char *paramTypeName,
                                                  EZrOwnershipQualifier paramOwnershipQualifier) {
    SZrInferredType returnType;
    SZrInferredType paramType;
    SZrArray paramTypes;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(functionName);

    if (returnTypeName != ZR_NULL) {
        init_test_object_type(state, &returnType, returnTypeName, ZR_OWNERSHIP_QUALIFIER_NONE);
    } else {
        ZrParser_InferredType_Init(state, &returnType, returnBaseType);
    }

    if (paramTypeName != ZR_NULL) {
        init_test_object_type(state, &paramType, paramTypeName, paramOwnershipQualifier);
    } else {
        ZrParser_InferredType_Init(state, &paramType, paramBaseType);
        paramType.ownershipQualifier = paramOwnershipQualifier;
    }

    ZrCore_Array_Init(state, &paramTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(state, &paramTypes, &paramType);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
            state, cs->typeEnv, create_test_string(state, functionName), &returnType, &paramTypes));

    ZrCore_Array_Free(state, &paramTypes);
    ZrParser_InferredType_Free(state, &paramType);
    ZrParser_InferredType_Free(state, &returnType);
}

static const SZrSemanticSymbolRecord *find_semantic_symbol_record(SZrSemanticContext *context,
                                                               const char *name,
                                                               EZrSemanticSymbolKind kind) {
    TZrSize i;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < context->symbols.length; i++) {
        SZrSemanticSymbolRecord *record =
            (SZrSemanticSymbolRecord *)ZrCore_Array_Get(&context->symbols, i);
        TZrNativeString nativeName;
        if (record != ZR_NULL && record->name != ZR_NULL &&
            record->kind == kind) {
            nativeName = ZrCore_String_GetNativeStringShort(record->name);
            if (nativeName != ZR_NULL && strcmp(nativeName, name) == 0) {
            return record;
            }
        }
    }

    return ZR_NULL;
}

static const SZrSemanticOverloadSetRecord *find_semantic_overload_set_record(SZrSemanticContext *context,
                                                                         const char *name) {
    TZrSize i;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *record =
            (SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(&context->overloadSets, i);
        if (record != ZR_NULL && record->name != ZR_NULL) {
            TZrNativeString nativeName = ZrCore_String_GetNativeString(record->name);
            if (nativeName != ZR_NULL && strcmp(nativeName, name) == 0) {
                return record;
            }
        }
    }

    return ZR_NULL;
}

static const SZrSemanticTypeRecord *find_semantic_type_record(SZrSemanticContext *context,
                                                           const char *name,
                                                           EZrSemanticTypeKind kind) {
    TZrSize i;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < context->types.length; i++) {
        SZrSemanticTypeRecord *record =
            (SZrSemanticTypeRecord *)ZrCore_Array_Get(&context->types, i);
        if (record != ZR_NULL && record->name != ZR_NULL && record->kind == kind) {
            TZrNativeString nativeName = ZrCore_String_GetNativeString(record->name);
            if (nativeName != ZR_NULL && strcmp(nativeName, name) == 0) {
                return record;
            }
        }
    }

    return ZR_NULL;
}

// 测试初始化和清理
void setUp(void) {}

void tearDown(void) {}

// ==================== 类型推断测试 ====================

// 测试整数字面量类型推断
static void test_type_inference_integer_literal(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Integer Literal";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Integer literal type inference", "Testing type inference for integer literal: 123");

    // 解析整数表达式
    const char *source = "123;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse integer literal");
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 获取表达式节点
    SZrAstNode *expr = ZR_NULL;
    if (ast->data.script.statements != ZR_NULL && ast->data.script.statements->count > 0) {
        SZrAstNode *stmt = ast->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_EXPRESSION_STATEMENT && stmt->data.expressionStatement.expr != ZR_NULL) {
            expr = stmt->data.expressionStatement.expr;
        }
    }

    if (expr == ZR_NULL || expr->type != ZR_AST_INTEGER_LITERAL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to get integer literal node");
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TZrBool success = ZrParser_LiteralType_Infer(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    ZrParser_InferredType_Free(state, &result);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// 测试小整数字面量仍然默认推断为 int64
static void test_type_inference_small_integer_literal_defaults_to_int64(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Small Integer Literal Defaults To Int64";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Small integer literal default inference", "Testing type inference for small integer literal: 1");

    const char *source = "1;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse small integer literal");
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    SZrAstNode *expr = ZR_NULL;
    if (ast->data.script.statements != ZR_NULL && ast->data.script.statements->count > 0) {
        SZrAstNode *stmt = ast->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_EXPRESSION_STATEMENT && stmt->data.expressionStatement.expr != ZR_NULL) {
            expr = stmt->data.expressionStatement.expr;
        }
    }

    if (expr == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to get small integer literal node");
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    SZrInferredType result;
    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TZrBool success = ZrParser_ExpressionType_Infer(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    ZrParser_InferredType_Free(state, &result);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_compiler_state_initializes_semantic_context(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Compiler State - Initializes Semantic Context";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Compiler state semantic bootstrap",
              "Testing that compiler state creates semantic context and shares it with type environment");

    TEST_ASSERT_TRUE(cs->semanticContext != ZR_NULL);
    TEST_ASSERT_TRUE(cs->typeEnv != ZR_NULL);
    TEST_ASSERT_TRUE(cs->semanticContext == cs->typeEnv->semanticContext);
    TEST_ASSERT_TRUE(cs->hirModule == ZR_NULL);

    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_environment_registers_semantic_records(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Environment - Registers Semantic Records";
    SZrInferredType intType;
    SZrInferredType returnType;
    SZrArray paramTypes;
    const SZrSemanticSymbolRecord *varRecord;
    const SZrSemanticSymbolRecord *funcRecord;
    const SZrSemanticSymbolRecord *typeRecord;

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Type environment semantic emission",
              "Testing variable/function/type registration writes semantic type, symbol and overload records");

    ZrParser_InferredType_Init(state, &intType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &returnType, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Construct(&paramTypes);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        state, cs->typeEnv, ZrCore_String_Create(state, "value", 5), &intType));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
        state, cs->typeEnv, ZrCore_String_Create(state, "add", 3), &returnType, &paramTypes));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(
        state, cs->typeEnv, ZrCore_String_Create(state, "Point", 5)));

    TEST_ASSERT_TRUE(cs->semanticContext->types.length > 0);
    TEST_ASSERT_TRUE(cs->semanticContext->symbols.length > 0);
    TEST_ASSERT_TRUE(cs->semanticContext->overloadSets.length > 0);

    varRecord = find_semantic_symbol_record(cs->semanticContext, "value", ZR_SEMANTIC_SYMBOL_KIND_VARIABLE);
    funcRecord = find_semantic_symbol_record(cs->semanticContext, "add", ZR_SEMANTIC_SYMBOL_KIND_FUNCTION);
    typeRecord = find_semantic_symbol_record(cs->semanticContext, "Point", ZR_SEMANTIC_SYMBOL_KIND_TYPE);

    TEST_ASSERT_NOT_NULL(varRecord);
    TEST_ASSERT_NOT_NULL(funcRecord);
    TEST_ASSERT_NOT_NULL(typeRecord);
    TEST_ASSERT_NOT_EQUAL(0, funcRecord->overloadSetId);
    TEST_ASSERT_NOT_EQUAL(0, varRecord->typeId);
    TEST_ASSERT_NOT_EQUAL(0, funcRecord->typeId);
    TEST_ASSERT_NOT_EQUAL(0, typeRecord->typeId);

    ZrParser_InferredType_Free(state, &returnType);
    ZrParser_InferredType_Free(state, &intType);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_environment_registers_function_overloads(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Environment - Registers Function Overloads";
    SZrInferredType intType;
    SZrInferredType boolType;
    SZrInferredType doubleType;
    SZrArray intParams;
    SZrArray doubleParams;
    const SZrSemanticOverloadSetRecord *overloadRecord;

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Function overload registration",
              "Testing that same-name functions with different signatures are retained in the type environment");

    ZrParser_InferredType_Init(state, &intType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &boolType, ZR_VALUE_TYPE_BOOL);
    ZrParser_InferredType_Init(state, &doubleType, ZR_VALUE_TYPE_DOUBLE);
    ZrCore_Array_Init(state, &intParams, sizeof(SZrInferredType), 1);
    ZrCore_Array_Init(state, &doubleParams, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(state, &intParams, &intType);
    ZrCore_Array_Push(state, &doubleParams, &doubleType);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
        state, cs->typeEnv, ZrCore_String_Create(state, "pick", 4), &intType, &intParams));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
        state, cs->typeEnv, ZrCore_String_Create(state, "pick", 4), &boolType, &doubleParams));
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)cs->typeEnv->functionReturnTypes.length);

    overloadRecord = find_semantic_overload_set_record(cs->semanticContext, "pick");
    TEST_ASSERT_NOT_NULL(overloadRecord);
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)overloadRecord->members.length);

    ZrCore_Array_Free(state, &doubleParams);
    ZrCore_Array_Free(state, &intParams);
    ZrParser_InferredType_Free(state, &doubleType);
    ZrParser_InferredType_Free(state, &boolType);
    ZrParser_InferredType_Free(state, &intType);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_resolves_best_function_overload(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Resolves Best Function Overload";
    SZrInferredType intType;
    SZrInferredType boolType;
    SZrInferredType doubleType;
    SZrInferredType result;
    SZrArray intParams;
    SZrArray doubleParams;

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Function overload resolution",
              "Testing that function-call inference resolves to the uniquely best overload instead of first-name lookup");

    ZrParser_InferredType_Init(state, &intType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(state, &boolType, ZR_VALUE_TYPE_BOOL);
    ZrParser_InferredType_Init(state, &doubleType, ZR_VALUE_TYPE_DOUBLE);
    ZrCore_Array_Init(state, &intParams, sizeof(SZrInferredType), 1);
    ZrCore_Array_Init(state, &doubleParams, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(state, &intParams, &intType);
    ZrCore_Array_Push(state, &doubleParams, &doubleType);

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
        state, cs->typeEnv, ZrCore_String_Create(state, "value", 5), &doubleType));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
        state, cs->typeEnv, ZrCore_String_Create(state, "pick", 4), &intType, &intParams));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterFunction(
        state, cs->typeEnv, ZrCore_String_Create(state, "pick", 4), &boolType, &doubleParams));

    {
        const char *source = "var result = pick(value);";
        SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_TRUE(ast->data.script.statements->count > 0);

        {
            SZrAstNode *stmt = ast->data.script.statements->nodes[0];
            TEST_ASSERT_NOT_NULL(stmt);
            TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, stmt->type);
            expr = stmt->data.variableDeclaration.value;
        }

        TEST_ASSERT_NOT_NULL(expr);
        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
        ZrParser_InferredType_Free(state, &result);
        ZrParser_Ast_Free(state, ast);
    }

    ZrCore_Array_Free(state, &doubleParams);
    ZrCore_Array_Free(state, &intParams);
    ZrParser_InferredType_Free(state, &doubleType);
    ZrParser_InferredType_Free(state, &boolType);
    ZrParser_InferredType_Free(state, &intType);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_convert_ast_type_registers_generic_instance_semantics(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Registers Generic Instance Semantics";
    const SZrSemanticTypeRecord *genericRecord;
    SZrInferredType convertedType;

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Generic type conversion",
              "Testing that generic AST types produce canonical inferred types and semantic generic-instance records");

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(
        state, cs->typeEnv, ZrCore_String_Create(state, "Box", 3)));

    {
        const char *source = "makeBox(value: int): Box<int> { return value; }";
        SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunctionDeclaration *funcDecl;
        SZrInferredType *genericArg;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_TRUE(ast->data.script.statements->count > 0);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, ast->data.script.statements->nodes[0]->type);

        funcDecl = &ast->data.script.statements->nodes[0]->data.functionDeclaration;
        TEST_ASSERT_NOT_NULL(funcDecl->returnType);

        ZrParser_InferredType_Init(state, &convertedType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(cs, funcDecl->returnType, &convertedType));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, convertedType.baseType);
        TEST_ASSERT_NOT_NULL(convertedType.typeName);
        TEST_ASSERT_EQUAL_STRING("Box<int>", ZrCore_String_GetNativeString(convertedType.typeName));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)convertedType.elementTypes.length);

        genericArg = (SZrInferredType *)ZrCore_Array_Get(&convertedType.elementTypes, 0);
        TEST_ASSERT_NOT_NULL(genericArg);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, genericArg->baseType);

        genericRecord = find_semantic_type_record(cs->semanticContext,
                                               "Box<int>",
                                               ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE);
        TEST_ASSERT_NOT_NULL(genericRecord);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, genericRecord->baseType);

        ZrParser_InferredType_Free(state, &convertedType);
        ZrParser_Ast_Free(state, ast);
    }

    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_convert_ast_type_preserves_ownership_qualifier(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Preserves Ownership Qualifier";
    const SZrSemanticTypeRecord *ownedRecord;
    SZrInferredType convertedType;

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Ownership-qualified type conversion",
              "Testing that %unique/%shared/%borrowed qualifiers survive AST->inferred-type conversion and semantic registration");

    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(
        state, cs->typeEnv, ZrCore_String_Create(state, "Resource", 8)));
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterType(
        state, cs->typeEnv, ZrCore_String_Create(state, "Box", 3)));

    {
        const char *source =
            "var owned: %unique Resource;"
            "var sharedRef: %shared Box<int>;"
            "var weakRef: %weak Resource;"
            "var borrowedRef: %borrowed Resource;";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_types_test.zr", 23);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *ownedDecl;
        SZrAstNode *sharedDecl;
        SZrAstNode *borrowedDecl;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_TRUE(ast->data.script.statements->count >= 4);

        ownedDecl = ast->data.script.statements->nodes[0];
        sharedDecl = ast->data.script.statements->nodes[1];
        borrowedDecl = ast->data.script.statements->nodes[3];
        TEST_ASSERT_NOT_NULL(ownedDecl);
        TEST_ASSERT_NOT_NULL(sharedDecl);
        TEST_ASSERT_NOT_NULL(borrowedDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, ownedDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, sharedDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, borrowedDecl->type);
        TEST_ASSERT_NOT_NULL(ownedDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(sharedDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(borrowedDecl->data.variableDeclaration.typeInfo);

        ZrParser_InferredType_Init(state, &convertedType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            cs, ownedDecl->data.variableDeclaration.typeInfo, &convertedType));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, convertedType.baseType);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              convertedType.ownershipQualifier);
        TEST_ASSERT_NOT_NULL(convertedType.typeName);
        TEST_ASSERT_EQUAL_STRING("Resource", ZrCore_String_GetNativeString(convertedType.typeName));
        ZrParser_InferredType_Free(state, &convertedType);

        ZrParser_InferredType_Init(state, &convertedType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            cs, sharedDecl->data.variableDeclaration.typeInfo, &convertedType));
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED,
                              convertedType.ownershipQualifier);
        TEST_ASSERT_NOT_NULL(convertedType.typeName);
        TEST_ASSERT_EQUAL_STRING("Box<int>", ZrCore_String_GetNativeString(convertedType.typeName));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)convertedType.elementTypes.length);
        ZrParser_InferredType_Free(state, &convertedType);

        ZrParser_InferredType_Init(state, &convertedType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            cs, borrowedDecl->data.variableDeclaration.typeInfo, &convertedType));
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_BORROWED,
                              convertedType.ownershipQualifier);
        TEST_ASSERT_NOT_NULL(convertedType.typeName);
        TEST_ASSERT_EQUAL_STRING("Resource", ZrCore_String_GetNativeString(convertedType.typeName));

        ownedRecord = find_semantic_type_record(cs->semanticContext,
                                             "Resource",
                                             ZR_SEMANTIC_TYPE_KIND_REFERENCE);
        TEST_ASSERT_NOT_NULL(ownedRecord);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              ownedRecord->ownershipQualifier);

        ZrParser_InferredType_Free(state, &convertedType);
        ZrParser_Ast_Free(state, ast);
    }

    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_construct_expression_preserves_ownership_qualifier(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Construct Expression Preserves Ownership Qualifier";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var owner = %unique new Holder();"
                "%shared(owner);";
        SZrString *sourceName = ZrCore_String_Create(state, "construct_ownership_type_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *uniqueExpr = ZR_NULL;
        SZrAstNode *sharedExpr = ZR_NULL;
        SZrInferredType result;
        SZrTypePrototypeInfo holderInfo;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        init_test_type_prototype(state, &holderInfo, "Holder", ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        register_test_type_prototype(state, cs, &holderInfo);

        uniqueExpr = ast->data.script.statements->nodes[0]->data.variableDeclaration.value;
        sharedExpr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(uniqueExpr);
        TEST_ASSERT_NOT_NULL(sharedExpr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, uniqueExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE, result.ownershipQualifier);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Holder", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, sharedExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED, result.ownershipQualifier);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Holder", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_convert_ast_type_preserves_qualified_root_module_member_name(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Preserves Qualified Root Module Member Name";
    SZrInferredType convertedType;

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source = "var patch: zr.DecoratorPatch = null;";
        SZrString *sourceName = ZrCore_String_Create(state, "qualified_root_module_member_type_test.zr", 42);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *decl;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        decl = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(decl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, decl->type);
        TEST_ASSERT_NOT_NULL(decl->data.variableDeclaration.typeInfo);

        ZrParser_InferredType_Init(state, &convertedType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(
                ZrParser_AstTypeToInferredType_Convert(cs, decl->data.variableDeclaration.typeInfo, &convertedType));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, convertedType.baseType);
        TEST_ASSERT_NOT_NULL(convertedType.typeName);
        TEST_ASSERT_EQUAL_STRING("zr.DecoratorPatch", ZrCore_String_GetNativeString(convertedType.typeName));

        ZrParser_InferredType_Free(state, &convertedType);
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void expect_ownership_builtin_type_inference_failure(const char *source,
                                                            const char *sourceNameText,
                                                            const char *expectedMessage) {
    SZrState *state = create_test_state();
    SZrCompilerState *cs = create_test_compiler_state(state);
    SZrString *sourceName = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrTypePrototypeInfo holderInfo;
    SZrAstNode *exprStatement = ZR_NULL;
    SZrAstNode *expr = ZR_NULL;
    SZrInferredType result;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);
    TEST_ASSERT_NOT_NULL(expectedMessage);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);

    init_test_type_prototype(state, &holderInfo, "Holder", ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
    register_test_type_prototype(state, cs, &holderInfo);

    cs->currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(cs->currentFunction);

    for (index = 0; index + 1 < ast->data.script.statements->count; index++) {
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[index]);
        TEST_ASSERT_FALSE(cs->hasError);
    }

    exprStatement = ast->data.script.statements->nodes[ast->data.script.statements->count - 1];
    TEST_ASSERT_NOT_NULL(exprStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, exprStatement->type);
    expr = exprStatement->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL(expr);

    cs->hasError = ZR_FALSE;
    cs->errorMessage = ZR_NULL;
    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    TEST_ASSERT_TRUE(cs->hasError);
    TEST_ASSERT_NOT_NULL(cs->errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, expectedMessage));
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, cs->currentFunction);
    cs->currentFunction = ZR_NULL;
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);
}

static void test_ownership_builtin_type_inference_rejects_invalid_operands(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Ownership Builtins Reject Invalid Operands";

    TEST_START(testSummary);
    timer.startTime = clock();
    TEST_INFO("Ownership builtin operand typing",
              "Testing that invalid Rust-first ownership builtin operands are rejected during type inference");

    expect_ownership_builtin_type_inference_failure(
            "var owner = %unique new Holder();"
            "%weak(owner);",
            "ownership_invalid_weak_unique_test.zr",
            "'%weak' requires a %shared owner");

    expect_ownership_builtin_type_inference_failure(
            "var seed = %unique new Holder();"
            "var owner = %shared(seed);"
            "%upgrade(owner);",
            "ownership_invalid_upgrade_shared_test.zr",
            "'%upgrade' requires a %weak owner");

    expect_ownership_builtin_type_inference_failure(
            "var seed = %unique new Holder();"
            "var owner = %shared(seed);"
            "%loan(owner);",
            "ownership_invalid_loan_shared_test.zr",
            "'%loan' requires a %unique owner");

    expect_ownership_builtin_type_inference_failure(
            "var owner = %unique new Holder();"
            "var shared = %shared(owner);"
            "var borrowed = %borrow(shared);"
            "%release(borrowed);",
            "ownership_invalid_release_borrowed_test.zr",
            "'%release' requires a %unique or %shared owner");

    expect_ownership_builtin_type_inference_failure(
            "var seed = %unique new Holder();"
            "var owner = %shared(seed);"
            "var watcher = %weak(owner);"
            "%detach(watcher);",
            "ownership_invalid_detach_weak_test.zr",
            "'%detach' requires a %unique or %shared owner");

    expect_ownership_builtin_type_inference_failure(
            "var seed = %unique new Holder();"
            "var owner = %shared(seed);"
            "%shared(owner);",
            "ownership_invalid_share_shared_test.zr",
            "'%shared' requires a %unique owner");

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_unique_instance_only_calls_borrowed_methods(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Unique Instance Only Calls Borrowed Methods";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var holder = %unique new Holder();"
                "holder.peek();"
                "holder.take();";
        SZrString *sourceName = ZrCore_String_Create(state, "unique_borrowed_method_test.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *borrowedCallExpr = ZR_NULL;
        SZrAstNode *uniqueCallExpr = ZR_NULL;
        SZrInferredType result;
        SZrTypePrototypeInfo holderInfo;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        init_test_type_prototype(state, &holderInfo, "Holder", ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        add_test_method_member(state, &holderInfo, "peek", "int", ZR_OWNERSHIP_QUALIFIER_BORROWED);
        add_test_method_member(state, &holderInfo, "take", "int", ZR_OWNERSHIP_QUALIFIER_UNIQUE);
        register_test_type_prototype(state, cs, &holderInfo);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        borrowedCallExpr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        uniqueCallExpr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(borrowedCallExpr);
        TEST_ASSERT_NOT_NULL(uniqueCallExpr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, borrowedCallExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        cs->hasError = ZR_FALSE;
        cs->errorMessage = ZR_NULL;
        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, uniqueCallExpr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Unique-owned receivers can only call %borrowed methods"));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_unique_value_is_compatible_with_borrowed_parameter(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Unique Value Is Compatible With Borrowed Parameter";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var holder = %unique new Holder();"
                "return Observe(holder);";
        SZrString *sourceName = ZrCore_String_Create(state, "borrowed_param_compat_test.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;
        SZrTypePrototypeInfo holderInfo;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        init_test_type_prototype(state, &holderInfo, "Holder", ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        register_test_type_prototype(state, cs, &holderInfo);
        register_test_function_with_one_param(state,
                                              cs,
                                              "Observe",
                                              ZR_VALUE_TYPE_INT64,
                                              ZR_NULL,
                                              ZR_VALUE_TYPE_OBJECT,
                                              "Holder",
                                              ZR_OWNERSHIP_QUALIFIER_BORROWED);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.returnStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_borrowed_value_cannot_flow_to_plain_parameter(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Borrowed Value Cannot Flow To Plain Parameter";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source = "return Observe(this);";
        SZrString *sourceName = ZrCore_String_Create(state, "borrowed_escape_type_test.zr", 28);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrString *thisName = ZR_NULL;
        SZrInferredType thisType;
        SZrInferredType result;
        SZrTypePrototypeInfo holderInfo;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        init_test_type_prototype(state, &holderInfo, "Holder", ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        register_test_type_prototype(state, cs, &holderInfo);
        register_test_function_with_one_param(state,
                                              cs,
                                              "Observe",
                                              ZR_VALUE_TYPE_INT64,
                                              ZR_NULL,
                                              ZR_VALUE_TYPE_OBJECT,
                                              "Holder",
                                              ZR_OWNERSHIP_QUALIFIER_NONE);

        thisName = ZrCore_String_Create(state, "this", 4);
        TEST_ASSERT_NOT_NULL(thisName);
        init_test_object_type(state, &thisType, "Holder", ZR_OWNERSHIP_QUALIFIER_BORROWED);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(state, cs->typeEnv, thisName, &thisType));
        ZrParser_InferredType_Free(state, &thisType);

        expr = ast->data.script.statements->nodes[0]->data.returnStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        cs->hasError = ZR_FALSE;
        cs->errorMessage = ZR_NULL;
        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_TRUE(strstr(cs->errorMessage, "Argument type mismatch") != ZR_NULL ||
                         strstr(cs->errorMessage, "No matching overload") != ZR_NULL);
        ZrParser_InferredType_Free(state, &result);

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// 测试浮点数字面量类型推断
static void test_type_inference_float_literal(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Float Literal";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Float literal type inference", "Testing type inference for float literal: 1.5");

    // 解析浮点数表达式
    const char *source = "1.5;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse float literal");
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 获取表达式节点
    SZrAstNode *expr = ZR_NULL;
    if (ast->data.script.statements != ZR_NULL && ast->data.script.statements->count > 0) {
        SZrAstNode *stmt = ast->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_EXPRESSION_STATEMENT && stmt->data.expressionStatement.expr != ZR_NULL) {
            expr = stmt->data.expressionStatement.expr;
        }
    }

    if (expr == ZR_NULL || expr->type != ZR_AST_FLOAT_LITERAL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to get float literal node");
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TZrBool success = ZrParser_LiteralType_Infer(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.baseType);

    ZrParser_InferredType_Free(state, &result);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// 测试字符串字面量类型推断
static void test_type_inference_string_literal(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - String Literal";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("String literal type inference", "Testing type inference for string literal: \"hello\"");

    // 解析字符串表达式
    const char *source = "\"hello\";";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse string literal");
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 获取表达式节点
    SZrAstNode *expr = ZR_NULL;
    if (ast->data.script.statements != ZR_NULL && ast->data.script.statements->count > 0) {
        SZrAstNode *stmt = ast->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_EXPRESSION_STATEMENT && stmt->data.expressionStatement.expr != ZR_NULL) {
            expr = stmt->data.expressionStatement.expr;
        }
    }

    if (expr == ZR_NULL || expr->type != ZR_AST_STRING_LITERAL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to get string literal node");
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TZrBool success = ZrParser_LiteralType_Infer(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.baseType);

    ZrParser_InferredType_Free(state, &result);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// 测试布尔字面量类型推断
static void test_type_inference_boolean_literal(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Boolean Literal";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Boolean literal type inference", "Testing type inference for boolean literal: true");

    // 解析布尔表达式
    const char *source = "true;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse boolean literal");
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 获取表达式节点
    SZrAstNode *expr = ZR_NULL;
    if (ast->data.script.statements != ZR_NULL && ast->data.script.statements->count > 0) {
        SZrAstNode *stmt = ast->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_EXPRESSION_STATEMENT && stmt->data.expressionStatement.expr != ZR_NULL) {
            expr = stmt->data.expressionStatement.expr;
        }
    }

    if (expr == ZR_NULL || expr->type != ZR_AST_BOOLEAN_LITERAL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to get boolean literal node");
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TZrBool success = ZrParser_LiteralType_Infer(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);

    ZrParser_InferredType_Free(state, &result);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// 测试二元表达式类型推断
static void test_type_inference_binary_expression(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Binary Expression";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Binary expression type inference", "Testing type inference for binary expression: 1 + 2");

    // 解析二元表达式
    const char *source = "1 + 2;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse binary expression");
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 获取表达式节点
    SZrAstNode *expr = ZR_NULL;
    if (ast->data.script.statements != ZR_NULL && ast->data.script.statements->count > 0) {
        SZrAstNode *stmt = ast->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_EXPRESSION_STATEMENT && stmt->data.expressionStatement.expr != ZR_NULL) {
            expr = stmt->data.expressionStatement.expr;
        }
    }

    if (expr == ZR_NULL || expr->type != ZR_AST_BINARY_EXPRESSION) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to get binary expression node");
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TZrBool success = ZrParser_BinaryExpressionType_Infer(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    // 整数相加应该返回整数类型
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    ZrParser_InferredType_Free(state, &result);
    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_parser_supports_ownership_types_and_template_strings(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Parser - Ownership Types And Template Strings";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Ownership-qualified type parsing and template string parsing",
              "Testing %unique/%shared/%weak/%borrowed type annotations and backtick template strings with interpolation");

    const char *source =
        "var owned: %unique Resource;"
        "var sharedRef: %shared Box<int>;"
        "var weakRef: %weak Resource;"
        "var borrowedRef: %borrowed Resource;"
        "var message = `hello ${1}`;";
    SZrString *sourceName = ZrCore_String_Create(state, "ownership_template_test.zr", 26);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 5);

    {
        SZrAstNode *ownedDecl = ast->data.script.statements->nodes[0];
        SZrAstNode *sharedDecl = ast->data.script.statements->nodes[1];
        SZrAstNode *weakDecl = ast->data.script.statements->nodes[2];
        SZrAstNode *borrowedDecl = ast->data.script.statements->nodes[3];
        SZrAstNode *messageDecl = ast->data.script.statements->nodes[4];
        SZrAstNode *templateLiteral;

        TEST_ASSERT_NOT_NULL(ownedDecl);
        TEST_ASSERT_NOT_NULL(sharedDecl);
        TEST_ASSERT_NOT_NULL(weakDecl);
        TEST_ASSERT_NOT_NULL(borrowedDecl);
        TEST_ASSERT_NOT_NULL(messageDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, ownedDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, sharedDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, weakDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, borrowedDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, messageDecl->type);
        TEST_ASSERT_NOT_NULL(ownedDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(sharedDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(weakDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(borrowedDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              ownedDecl->data.variableDeclaration.typeInfo->ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED,
                              sharedDecl->data.variableDeclaration.typeInfo->ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_WEAK,
                              weakDecl->data.variableDeclaration.typeInfo->ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_BORROWED,
                              borrowedDecl->data.variableDeclaration.typeInfo->ownershipQualifier);

        templateLiteral = messageDecl->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(templateLiteral);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TEMPLATE_STRING_LITERAL, templateLiteral->type);
        TEST_ASSERT_NOT_NULL(templateLiteral->data.templateStringLiteral.segments);
        TEST_ASSERT_EQUAL_INT(3, (int)templateLiteral->data.templateStringLiteral.segments->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL,
                              templateLiteral->data.templateStringLiteral.segments->nodes[0]->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_INTERPOLATED_SEGMENT,
                              templateLiteral->data.templateStringLiteral.segments->nodes[1]->type);
    }

    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_using_statement_compilation_records_cleanup_plan(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Semantic - Using Statement Cleanup Plan";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Using statement semantic plan",
              "Testing that compiling a using statement appends deterministic cleanup metadata");

    {
        const char *source =
                "var resource = \"x\";\n"
                "%using resource;";
        SZrString *sourceName =
                ZrCore_String_Create(state, "using_cleanup_test.zr", strlen("using_cleanup_test.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *resourceDecl;
        SZrAstNode *usingStmt;
        const SZrDeterministicCleanupStep *step;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        resourceDecl = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(resourceDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, resourceDecl->type);

        usingStmt = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(usingStmt);
        TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingStmt->type);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Statement_Compile(cs, resourceDecl);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_Statement_Compile(cs, usingStmt);

        TEST_ASSERT_FALSE(cs->hasError);
        TEST_ASSERT_EQUAL_INT(1, (int)cs->semanticContext->cleanupPlan.length);

        step = (const SZrDeterministicCleanupStep *)ZrCore_Array_Get(&cs->semanticContext->cleanupPlan, 0);
        TEST_ASSERT_NOT_NULL(step);
        TEST_ASSERT_TRUE(step->regionId > 0);
        TEST_ASSERT_TRUE(step->symbolId > 0);
        TEST_ASSERT_TRUE(step->callsClose);
        TEST_ASSERT_TRUE(step->callsDestructor);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
    }

    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_template_string_compilation_records_semantic_segments(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Semantic - Template String Segments";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Template string semantic segments",
              "Testing that template-string compilation stores ordered static and interpolation segments");

    {
        const char *source = "`hello ${1}`;";
        SZrString *sourceName = ZrCore_String_Create(state, "template_segments_test.zr", 25);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *exprStmt;
        SZrAstNode *templateLiteral;
        const SZrTemplateSegment *first;
        const SZrTemplateSegment *second;
        const SZrTemplateSegment *third;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        exprStmt = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(exprStmt);
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, exprStmt->type);
        templateLiteral = exprStmt->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(templateLiteral);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TEMPLATE_STRING_LITERAL, templateLiteral->type);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Expression_Compile(cs, templateLiteral);

        TEST_ASSERT_FALSE(cs->hasError);
        TEST_ASSERT_EQUAL_INT(3, (int)cs->semanticContext->templateSegments.length);

        first = (const SZrTemplateSegment *)ZrCore_Array_Get(&cs->semanticContext->templateSegments, 0);
        second = (const SZrTemplateSegment *)ZrCore_Array_Get(&cs->semanticContext->templateSegments, 1);
        third = (const SZrTemplateSegment *)ZrCore_Array_Get(&cs->semanticContext->templateSegments, 2);

        TEST_ASSERT_NOT_NULL(first);
        TEST_ASSERT_NOT_NULL(second);
        TEST_ASSERT_NOT_NULL(third);

        TEST_ASSERT_FALSE(first->isInterpolation);
        TEST_ASSERT_NOT_NULL(first->staticText);
        TEST_ASSERT_EQUAL_STRING("hello ", ZrCore_String_GetNativeString(first->staticText));

        TEST_ASSERT_TRUE(second->isInterpolation);
        TEST_ASSERT_NOT_NULL(second->expression);
        TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, second->expression->type);

        TEST_ASSERT_FALSE(third->isInterpolation);
        TEST_ASSERT_NOT_NULL(third->staticText);
        TEST_ASSERT_EQUAL_STRING("", ZrCore_String_GetNativeString(third->staticText));

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
    }

    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_template_string_literal_is_string(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Template String Literal";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = create_test_compiler_state(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Template string literal type inference", "Testing type inference for `hello ${1}`");

    {
        const char *source = "`hello ${1}`;";
        SZrString *sourceName = ZrCore_String_Create(state, "template_string_test.zr", 23);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_TRUE(ast->data.script.statements->count > 0);
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT,
                              ast->data.script.statements->nodes[0]->type);

        expr = ast->data.script.statements->nodes[0]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TEMPLATE_STRING_LITERAL, expr->type);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        success = ZrParser_ExpressionType_Infer(cs, expr, &result);

        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrParser_Ast_Free(state, ast);
    }

    destroy_test_compiler_state(cs);
    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_import_native_module_keeps_module_name(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Import Native Module Keeps Module Name";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source = "var math = %import(\"zr.math\");";
        SZrString *sourceName = ZrCore_String_Create(state, "native_import_test.zr", 21);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;
        TZrBool success;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_TRUE(ast->data.script.statements->count > 0);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION,
                              ast->data.script.statements->nodes[0]->type);

        expr = ast->data.script.statements->nodes[0]->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, expr->type);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        success = ZrParser_ExpressionType_Infer(cs, expr, &result);

        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("zr.math", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_move_only_struct_assignment_rejects_implicit_copy(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Move-Only Struct Assignment Rejects Implicit Copy";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        SZrTypePrototypeInfo handleBoxInfo;
        SZrInferredType leftType;
        SZrInferredType rightType;
        SZrFileRange location = {{0, 1, 1}, {0, 1, 8}, ZR_NULL};

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);

        init_test_type_prototype(state, &handleBoxInfo, "HandleBox", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT);
        add_test_field_member(state,
                              &handleBoxInfo,
                              "resource",
                              "int",
                              ZR_OWNERSHIP_QUALIFIER_UNIQUE);
        register_test_type_prototype(state, cs, &handleBoxInfo);

        init_test_object_type(state, &leftType, "HandleBox", ZR_OWNERSHIP_QUALIFIER_NONE);
        init_test_object_type(state, &rightType, "HandleBox", ZR_OWNERSHIP_QUALIFIER_NONE);

        cs->hasError = ZR_FALSE;
        cs->errorMessage = ZR_NULL;
        TEST_ASSERT_FALSE(ZrParser_AssignmentCompatibility_Check(cs, &leftType, &rightType, location));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "move-only struct"));

        ZrParser_InferredType_Free(state, &leftType);
        ZrParser_InferredType_Free(state, &rightType);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_move_only_struct_argument_rejects_by_value_call(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Move-Only Struct Argument Rejects By-Value Call";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source = "return Observe(box);";
        SZrString *sourceName = ZrCore_String_Create(state, "move_only_struct_call_test.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType boxType;
        SZrInferredType result;
        SZrTypePrototypeInfo handleBoxInfo;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        init_test_type_prototype(state, &handleBoxInfo, "HandleBox", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT);
        add_test_field_member(state,
                              &handleBoxInfo,
                              "resource",
                              "int",
                              ZR_OWNERSHIP_QUALIFIER_UNIQUE);
        register_test_type_prototype(state, cs, &handleBoxInfo);
        register_test_function_with_one_param(state,
                                              cs,
                                              "Observe",
                                              ZR_VALUE_TYPE_INT64,
                                              ZR_NULL,
                                              ZR_VALUE_TYPE_OBJECT,
                                              "HandleBox",
                                              ZR_OWNERSHIP_QUALIFIER_NONE);

        init_test_object_type(state, &boxType, "HandleBox", ZR_OWNERSHIP_QUALIFIER_NONE);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(state,
                                                                   cs->typeEnv,
                                                                   create_test_string(state, "box"),
                                                                   &boxType));
        ZrParser_InferredType_Free(state, &boxType);

        expr = ast->data.script.statements->nodes[0]->data.returnStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        cs->hasError = ZR_FALSE;
        cs->errorMessage = ZR_NULL;
        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "move-only struct"));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_type_query_returns_reflection_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Type Query Returns Reflection Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var math = %import(\"zr.math\");\n"
                "var reflection = %type(math);";
        SZrString *sourceName = ZrCore_String_Create(state, "type_query_inference_test.zr", 28);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_QUERY_EXPRESSION, expr->type);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("zr.builtin.TypeInfo", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_convert_function_ast_type_to_callable_inferred_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Convert %func AST Type To Callable";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source = "var f: %func(int)->int = null;";
        SZrString *sourceName = ZrCore_String_Create(state, "function_type_convert_test.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *decl;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        decl = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(decl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, decl->type);
        TEST_ASSERT_NOT_NULL(decl->data.variableDeclaration.typeInfo);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(
                ZrParser_AstTypeToInferredType_Convert(cs, decl->data.variableDeclaration.typeInfo, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("%func(int)->int", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_function_type_annotation_accepts_lambda_assignment(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - %func Annotation Accepts Lambda Assignment";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source = "var f: %func(int)->int = (x:int)->{ return x; };";
        SZrString *sourceName = ZrCore_String_Create(state, "function_type_lambda_assign_test.zr", 35);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_type_query_function_type_returns_callable_reflection_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - %type(%func(...)) Returns Callable Reflection Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source = "var reflection = %type(%func(%ref value:int)->%async int);";
        SZrString *sourceName = ZrCore_String_Create(state, "callable_type_query_test.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        expr = ast->data.script.statements->nodes[0]->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_QUERY_EXPRESSION, expr->type);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("zr.builtin.TypeInfo", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_builtin_names_require_explicit_import(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Builtin Names Require Explicit Import";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var iter: IEnumerable<int> = null;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "builtin_names_require_import_test.zr", 37);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "IEnumerable"));
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "zr.builtin"));

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_builtin_names_accept_module_qualified_and_destructured_imports(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Builtin Names Accept Module Qualified And Destructured Imports";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var builtin = %import(\"zr.builtin\");\n"
                "var iter: builtin.IEnumerable<int> = null;\n"
                "var {TypeInfo, Integer} = %import(\"zr.builtin\");\n"
                "var meta: TypeInfo = %type(int);\n"
                "var boxed: Integer = null;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "builtin_names_explicit_import_ok_test.zr", 41);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(5, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        for (TZrSize index = 0; index < 5; index++) {
            compile_test_top_level_statement(cs, ast->data.script.statements->nodes[index]);
            TEST_ASSERT_FALSE(cs->hasError);
        }

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_builtin_value_helpers_require_explicit_import(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Builtin Value Helpers Require Explicit Import";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var builtin = %import(\"zr.builtin\");\n"
                "builtin.Object.type(null);\n"
                "Object.type(null);\n";
        SZrString *sourceName = ZrCore_String_Create(state, "builtin_value_helpers_require_import_test.zr", 45);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[2]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Object"));
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "zr.builtin.Object"));

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_legacy_builtin_aliases_fail_with_canonical_diagnostics(void) {
    typedef struct {
        const char *source;
        const char *legacyName;
        const char *canonicalName;
    } LegacyBuiltinDiagnosticCase;

    static const LegacyBuiltinDiagnosticCase cases[] = {
            {"var iter: Iterable<int> = null;\n", "Iterable", "zr.builtin.IEnumerable"},
            {"var iter: Iterator<int> = null;\n", "Iterator", "zr.builtin.IEnumerator"},
            {"var items: ArrayLike<int> = null;\n", "ArrayLike", "zr.builtin.IArrayLike"},
            {"var item: Equatable<int> = null;\n", "Equatable", "zr.builtin.IEquatable"},
            {"var item: Hashable = null;\n", "Hashable", "zr.builtin.IHashable"},
            {"var item: Comparable<int> = null;\n", "Comparable", "zr.builtin.IComparable"},
            {"var meta: zr.system.reflect.Type = null;\n", "zr.system.reflect.Type", "zr.builtin.TypeInfo"},
            {"var meta: zr.system.reflect.CallableType = null;\n",
             "zr.system.reflect.CallableType",
             "zr.builtin.TypeInfo"}
    };
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Legacy Builtin Aliases Fail With Canonical Diagnostics";

    TEST_START(testSummary);
    timer.startTime = clock();

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(cases); index++) {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        SZrString *sourceName;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);

        sourceName = ZrCore_String_Create(state, "legacy_builtin_alias_diagnostic_test.zr", 39);
        ast = ZrParser_Parse(state, cases[index].source, strlen(cases[index].source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, cases[index].legacyName));
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, cases[index].canonicalName));

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void assert_type_inference_function_type_value_alias_case(void) {
    SZrState *state = create_test_state();
    SZrCompilerState *cs = create_test_compiler_state(state);
    const char *source =
            "var f = %func(int)->int;\n"
            "var c:f = (x:int)->{ return x; };";
    SZrString *sourceName = ZrCore_String_Create(state, "type_value_alias_inference_test.zr", 34);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

    cs->scriptAst = ast;
    compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);

    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);
}

static void assert_type_inference_array_type_value_alias_case(void) {
    SZrState *state = create_test_state();
    SZrCompilerState *cs = create_test_compiler_state(state);
    const char *source =
            "var cubeType = int[][][];\n"
            "var value:cubeType = null;\n"
            "var container = %import(\"zr.container\");\n"
            "var jaggedType = container.Array<int[]>[];\n"
            "var jagged:jaggedType = null;";
    SZrString *sourceName = ZrCore_String_Create(state, "array_type_value_alias_inference_test.zr", 40);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(5, (int)ast->data.script.statements->count);

    cs->scriptAst = ast;
    compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(cs->hasError);
    compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(cs->hasError);
    compile_test_top_level_statement(cs, ast->data.script.statements->nodes[2]);
    TEST_ASSERT_FALSE(cs->hasError);
    compile_test_top_level_statement(cs, ast->data.script.statements->nodes[3]);
    TEST_ASSERT_FALSE(cs->hasError);
    compile_test_top_level_statement(cs, ast->data.script.statements->nodes[4]);
    TEST_ASSERT_FALSE(cs->hasError);

    ZrParser_Ast_Free(state, ast);
    destroy_test_compiler_state(cs);
    destroy_test_state(state);
}

static void test_type_inference_type_value_aliases_can_be_used_in_type_position(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Type Value Aliases Can Be Used In Type Position";

    TEST_START(testSummary);
    timer.startTime = clock();

    assert_type_inference_function_type_value_alias_case();
    assert_type_inference_array_type_value_alias_case();

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_prototype_construction_returns_native_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Prototype Construction Returns Native Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var math = %import(\"zr.math\");"
                "$math.Vector3(1.0, 2.0, 3.0);";
        SZrString *sourceName = ZrCore_String_Create(state, "native_vector3_type_test.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Vector3", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_rejects_ordinary_prototype_call(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Rejects Ordinary Prototype Call";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var math = %import(\"zr.math\");"
                "math.Vector3(1.0, 2.0, 3.0);";
        SZrString *sourceName = ZrCore_String_Create(state, "native_vector3_plain_call_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage,
                                    "Prototype references are not callable; use $target(...) or new target(...)"));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_boxed_new_returns_registered_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Boxed New Returns Registered Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var math = %import(\"zr.math\");"
                "new math.Tensor([2, 2], [1.0, 2.0, 3.0, 4.0]);";
        SZrString *sourceName = ZrCore_String_Create(state, "native_tensor_new_type_test.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Tensor", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_enum_construction_returns_enum_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Enum Construction Returns Enum Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var probe = %import(\"probe.native_shapes\");"
                "$probe.NativeMode(1);";
        SZrString *sourceName = ZrCore_String_Create(state, "native_enum_type_test.zr", 24);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_TRUE(register_probe_native_module(state));
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("NativeMode", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_enum_member_access_returns_enum_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Enum Member Access Returns Enum Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var probe = %import(\"probe.native_shapes\");"
                "probe.NativeMode.On;";
        SZrString *sourceName = ZrCore_String_Create(state, "native_enum_member_type_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_TRUE(register_probe_native_module(state));
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("NativeMode", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_interface_construction_is_rejected(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Interface Construction Is Rejected";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var probe = %import(\"probe.native_shapes\");"
                "new probe.NativeReadable();";
        SZrString *sourceName = ZrCore_String_Create(state, "native_interface_new_test.zr", 28);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_TRUE(register_probe_native_module(state));
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Interfaces cannot be constructed"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_interface_members_flow_through_implements_chain(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Interface Members Flow Through Implements Chain";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var {NativeDevice} = %import(\"probe.native_shapes\");"
                "var device: NativeDevice = null;"
                "device.read();";
        SZrString *sourceName = ZrCore_String_Create(state, "native_interface_chain_type_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_TRUE(register_probe_native_module(state));
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_extern_function_call_uses_declared_return_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Extern Function Call Uses Declared Return Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "%extern(\"fixture\") {\n"
                "  #zr.ffi.entry(\"zr_ffi_add_i32\")# Add(lhs:i32, rhs:i32): i32;\n"
                "}\n"
                "return Add(<i32> 2, <i32> 4);";
        SZrString *sourceName = ZrCore_String_Create(state, "source_extern_function_type_test.zr", 35);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, ast->data.script.statements->nodes[1]->type);
        expr = ast->data.script.statements->nodes[1]->data.returnStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT32, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_extern_enum_member_access_returns_enum_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Extern Enum Member Access Returns Enum Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "%extern(\"fixture\") {\n"
                "  #zr.ffi.underlying(\"i32\")#\n"
                "  enum Mode {\n"
                "    #zr.ffi.value(0)# Off,\n"
                "    #zr.ffi.value(1)# On\n"
                "  }\n"
                "}\n"
                "Mode.On;";
        SZrString *sourceName = ZrCore_String_Create(state, "source_extern_enum_member_type_test.zr", 38);
        SZrAstNode *ast = ZR_NULL;
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Mode", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_extern_struct_construction_returns_struct_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Extern Struct Construction Returns Struct Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "%extern(\"fixture\") {\n"
                "  struct Pair {\n"
                "    var x:i32;\n"
                "    var y:i32;\n"
                "  }\n"
                "}\n"
                "$Pair(1, 2);";
        SZrString *sourceName = ZrCore_String_Create(state, "source_extern_struct_construct_type_test.zr", 42);
        SZrAstNode *ast = ZR_NULL;
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Pair", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_generic_boxed_new_returns_closed_registered_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Generic Boxed New Returns Closed Registered Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var {NativeBox} = %import(\"probe.native_shapes\");"
                "new NativeBox<int>();";
        SZrString *sourceName = ZrCore_String_Create(state, "native_generic_box_new_type_test.zr", 35);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;
        SZrInferredType *elementType;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_TRUE(register_probe_native_module(state));
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("NativeBox<int>", ZrCore_String_GetNativeString(result.typeName));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)result.elementTypes.length);
        elementType = (SZrInferredType *)ZrCore_Array_Get(&result.elementTypes, 0);
        TEST_ASSERT_NOT_NULL(elementType);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, elementType->baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_generic_constraint_mismatch_is_rejected(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Generic Constraint Mismatch Is Rejected";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var {NativeLookup, NativeMode} = %import(\"probe.native_shapes\");"
                "new NativeLookup<NativeMode, int>();";
        SZrString *sourceName = ZrCore_String_Create(state, "native_generic_constraint_type_test.zr", 39);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_TRUE(register_probe_native_module(state));
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "does not satisfy constraint"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_class_boxed_new_returns_closed_registered_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Generic Class Boxed New Returns Closed Registered Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "class Box<T> { var value: T; }\n"
                "new Box<int>();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_box_new_type_test.zr", 35);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        const SZrTypePrototypeInfo *openPrototype;
        const SZrTypePrototypeInfo *closedPrototype;
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        openPrototype = find_test_type_prototype(cs, "Box");
        TEST_ASSERT_NOT_NULL(openPrototype);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)openPrototype->genericParameters.length);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Box<int>", ZrCore_String_GetNativeString(result.typeName));

        closedPrototype = find_test_type_prototype(cs, "Box<int>");
        TEST_ASSERT_NOT_NULL(closedPrototype);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_class_boxed_new_succeeds_without_prior_declaration_compile(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Class Boxed New Succeeds Without Prior Declaration Compile";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "class BaseHero {\n"
                "    pub @constructor(seed: int) { }\n"
                "}\n"
                "class BossHero: BaseHero {\n"
                "    pub @constructor(seed: int) super(seed) { }\n"
                "}\n"
                "new BossHero(30);";
        SZrString *sourceName =
                ZrCore_String_Create(state,
                                     "source_class_new_without_compile_type_test.zr",
                                     strlen("source_class_new_without_compile_type_test.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("BossHero", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_class_member_substitutes_closed_field_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Generic Class Member Substitutes Closed Field Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "class Box<T> { var value: T; }\n"
                "new Box<int>().value;";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_box_field_type_test.zr", 37);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_struct_boxed_new_returns_closed_registered_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Generic Struct Boxed New Returns Closed Registered Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "struct Pair<TLeft, TRight> { var left: TLeft; var right: TRight; }\n"
                "new Pair<int, string>();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_pair_new_type_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        const SZrTypePrototypeInfo *openPrototype;
        const SZrTypePrototypeInfo *closedPrototype;
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        openPrototype = find_test_type_prototype(cs, "Pair");
        TEST_ASSERT_NOT_NULL(openPrototype);
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)openPrototype->genericParameters.length);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Pair<int, string>", ZrCore_String_GetNativeString(result.typeName));

        closedPrototype = find_test_type_prototype(cs, "Pair<int, string>");
        TEST_ASSERT_NOT_NULL(closedPrototype);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_const_generic_boxed_new_returns_closed_registered_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Const Generic Boxed New Returns Closed Registered Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var {Array} = %import(\"zr.container\");\n"
                "class Matrix<T, const N: int> { var rows: Array<T>[N]; }\n"
                "new Matrix<int, 2 + 2>();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_const_generic_matrix_new_type_test.zr", 45);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        const SZrTypePrototypeInfo *openPrototype;
        const SZrTypePrototypeInfo *closedPrototype;
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;
        SZrInferredType *constArgument;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        openPrototype = find_test_type_prototype(cs, "Matrix");
        TEST_ASSERT_NOT_NULL(openPrototype);
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)openPrototype->genericParameters.length);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Matrix<int, 4>", ZrCore_String_GetNativeString(result.typeName));
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)result.elementTypes.length);

        constArgument = (SZrInferredType *)ZrCore_Array_Get(&result.elementTypes, 1);
        TEST_ASSERT_NOT_NULL(constArgument);
        TEST_ASSERT_NOT_NULL(constArgument->typeName);
        TEST_ASSERT_EQUAL_STRING("4", ZrCore_String_GetNativeString(constArgument->typeName));

        closedPrototype = find_test_type_prototype(cs, "Matrix<int, 4>");
        TEST_ASSERT_NOT_NULL(closedPrototype);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_const_generic_boxed_new_succeeds_without_prior_declaration_compile(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Const Generic Boxed New Succeeds Without Prior Declaration Compile";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var {Array} = %import(\"zr.container\");\n"
                "class Matrix<T, const N: int> { var rows: Array<T>[N]; }\n"
                "new Matrix<int, 2 + 2>();";
        SZrString *sourceName =
                ZrCore_String_Create(state,
                                     "source_const_generic_new_without_compile_type_test.zr",
                                     strlen("source_const_generic_new_without_compile_type_test.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Matrix<int, 4>", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_function_supports_explicit_arguments_and_inference(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Generic Function Supports Explicit Arguments And Inference";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "func identity<T>(value: T): T { return value; }\n"
                "identity(1);\n"
                "identity<string>(\"hello\");";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_function_type_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *inferredExpr = ZR_NULL;
        SZrAstNode *explicitExpr = ZR_NULL;
        SZrInferredType result;
        SZrFunctionTypeInfo *functionInfo = ZR_NULL;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunction(cs->typeEnv,
                                                                 ast->data.script.statements->nodes[0]
                                                                         ->data.functionDeclaration.name->name,
                                                                 &functionInfo));
        TEST_ASSERT_NOT_NULL(functionInfo);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)functionInfo->genericParameters.length);

        inferredExpr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        explicitExpr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(inferredExpr);
        TEST_ASSERT_NOT_NULL(explicitExpr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, inferredExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, explicitExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_function_reports_explicit_arity_mismatch(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Generic Function Reports Explicit Arity Mismatch";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "func identity<T>(value: T): T { return value; }\n"
                "identity<int, string>(1);";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_function_arity_diag_test.zr", 41);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Generic function 'identity': expected 1 generic argument(s), got 2"));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_function_reports_cannot_infer_type_argument(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Generic Function Reports Cannot Infer Type Argument";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "func create<T>(): T { return null; }\n"
                "create();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_function_infer_diag_test.zr", 41);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Generic function 'create': cannot infer generic argument 'T'"));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_function_reports_type_argument_kind_mismatch(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Generic Function Reports Type Argument Kind Mismatch";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "func identity<T>(value: T): T { return value; }\n"
                "identity<1>(1);";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_function_kind_diag_test.zr", 40);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Generic function 'identity': generic argument 'T' expects a type argument"));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_function_reports_conflicting_inference(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Generic Function Reports Conflicting Inference";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "func pick<T>(left: T, right: T): T { return left; }\n"
                "pick(1, \"two\");";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_function_conflict_diag_test.zr", 44);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Generic function 'pick': conflicting inferences for generic argument 'T'"));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_method_supports_explicit_arguments_inference_and_receiver_closure(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Generic Method Supports Explicit Arguments Inference And Receiver Closure";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "class Box<T> {\n"
                "    var value: T;\n"
                "    func echo<U>(input: U): U { return input; }\n"
                "    func currentOr<U>(fallback: U): T { return this.value; }\n"
                "}\n"
                "var box = new Box<int>();\n"
                "box.echo(\"hello\");\n"
                "box.echo<string>(\"world\");\n"
                "box.currentOr(\"fallback\");";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_method_type_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *inferredExpr = ZR_NULL;
        SZrAstNode *explicitExpr = ZR_NULL;
        SZrAstNode *receiverClosedExpr = ZR_NULL;
        SZrInferredType result;
        const SZrTypePrototypeInfo *closedPrototype;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(5, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        closedPrototype = find_test_type_prototype(cs, "Box<int>");
        TEST_ASSERT_NOT_NULL(closedPrototype);

        inferredExpr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        explicitExpr = ast->data.script.statements->nodes[3]->data.expressionStatement.expr;
        receiverClosedExpr = ast->data.script.statements->nodes[4]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(inferredExpr);
        TEST_ASSERT_NOT_NULL(explicitExpr);
        TEST_ASSERT_NOT_NULL(receiverClosedExpr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, inferredExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, explicitExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, receiverClosedExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_const_generic_method_supports_explicit_arguments_and_inference(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Const Generic Method Supports Explicit Arguments And Inference";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "class Matrix<T, const N: int> { }\n"
                "class Box<T> {\n"
                "    func shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }\n"
                "}\n"
                "var box = new Box<int>();\n"
                "var m = new Matrix<int, 4>();\n"
                "box.shape(m);\n"
                "box.shape<2 + 2>(m);";
        SZrString *sourceName = ZrCore_String_Create(state, "source_const_generic_method_type_test.zr", 41);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *inferredExpr = ZR_NULL;
        SZrAstNode *explicitExpr = ZR_NULL;
        SZrAstNode *inferredCallNode = ZR_NULL;
        const SZrTypePrototypeInfo *closedBoxPrototype = ZR_NULL;
        const SZrTypeMemberInfo *shapeMember = ZR_NULL;
        SZrResolvedCallSignature resolvedSignature;
        EZrGenericCallResolveStatus signatureStatus;
        TZrChar diagnostic[ZR_PARSER_ERROR_BUFFER_LENGTH];
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(6, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[2]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[3]);
        TEST_ASSERT_FALSE(cs->hasError);

        closedBoxPrototype = find_test_type_prototype(cs, "Box<int>");
        TEST_ASSERT_NOT_NULL(closedBoxPrototype);
        for (TZrSize memberIndex = 0; memberIndex < closedBoxPrototype->members.length; memberIndex++) {
            const SZrTypeMemberInfo *candidate =
                    (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&closedBoxPrototype->members, memberIndex);
            if (candidate != ZR_NULL &&
                candidate->name != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString(candidate->name), "shape") == 0) {
                shapeMember = candidate;
                break;
            }
        }
        TEST_ASSERT_NOT_NULL(shapeMember);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)shapeMember->genericParameters.length);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)shapeMember->parameterTypes.length);
        TEST_ASSERT_NOT_NULL(shapeMember->returnTypeName);
        TEST_ASSERT_EQUAL_STRING("Matrix<int, N>", ZrCore_String_GetNativeString(shapeMember->returnTypeName));

        inferredExpr = ast->data.script.statements->nodes[4]->data.expressionStatement.expr;
        explicitExpr = ast->data.script.statements->nodes[5]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(inferredExpr);
        TEST_ASSERT_NOT_NULL(explicitExpr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, inferredExpr->type);
        TEST_ASSERT_NOT_NULL(inferredExpr->data.primaryExpression.members);
        TEST_ASSERT_EQUAL_INT(2, (int)inferredExpr->data.primaryExpression.members->count);
        inferredCallNode = inferredExpr->data.primaryExpression.members->nodes[1];
        TEST_ASSERT_NOT_NULL(inferredCallNode);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, inferredCallNode->type);

        memset(&resolvedSignature, 0, sizeof(resolvedSignature));
        ZrParser_InferredType_Init(state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
        ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);
        diagnostic[0] = '\0';
        signatureStatus = resolve_generic_member_call_signature_detailed(cs,
                                                                         shapeMember,
                                                                         &inferredCallNode->data.functionCall,
                                                                         &resolvedSignature,
                                                                         diagnostic,
                                                                         sizeof(diagnostic));
        TEST_ASSERT_EQUAL_INT(ZR_GENERIC_CALL_RESOLVE_OK, signatureStatus);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)resolvedSignature.parameterTypes.length);
        TEST_ASSERT_NOT_NULL(((SZrInferredType *)ZrCore_Array_Get(&resolvedSignature.parameterTypes, 0))->typeName);
        TEST_ASSERT_EQUAL_STRING("Matrix<int, 4>",
                                 ZrCore_String_GetNativeString(((SZrInferredType *)ZrCore_Array_Get(
                                         &resolvedSignature.parameterTypes,
                                         0))->typeName));
        TEST_ASSERT_NOT_NULL(resolvedSignature.returnType.typeName);
        TEST_ASSERT_EQUAL_STRING("Matrix<int, 4>",
                                 ZrCore_String_GetNativeString(resolvedSignature.returnType.typeName));
        free_resolved_call_signature(state, &resolvedSignature);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, inferredExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Matrix<int, 4>", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, explicitExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Matrix<int, 4>", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_const_generic_method_reports_const_argument_kind_mismatch(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Const Generic Method Reports Const Argument Kind Mismatch";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "class Matrix<T, const N: int> { }\n"
                "class Box<T> {\n"
                "    func shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }\n"
                "}\n"
                "var box = new Box<int>();\n"
                "var m = new Matrix<int, 4>();\n"
                "box.shape<string>(m);";
        SZrString *sourceName = ZrCore_String_Create(state, "source_const_generic_method_kind_diag_test.zr", 44);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(5, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[2]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[3]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[4]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Generic method 'shape': generic argument 'N' expects a compile-time integer"));
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_parameter_passing_mode_in_rejects_reassignment(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - %in Parameter Rejects Reassignment";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "func consume(%in value: int): void {\n"
                "    value = 1;\n"
                "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "passing_mode_in_reassign_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Cannot assign to const parameter 'value'"));

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_parameter_passing_modes_out_and_ref_require_assignable_arguments(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - %out And %ref Require Assignable Arguments";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "func fill(%out value: int): void {\n"
                "    value = 1;\n"
                "}\n"
                "func swap(%ref value: int): void {\n"
                "}\n"
                "fill(1);\n"
                "swap(1);\n";
        SZrString *sourceName = ZrCore_String_Create(state, "passing_mode_lvalue_call_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(4, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[2]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "%out argument must be an assignable storage location"));

        cs->hasError = ZR_FALSE;
        cs->errorMessage = ZR_NULL;
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[3]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "%ref argument must be an assignable storage location"));

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_out_parameter_requires_definite_assignment_on_all_paths(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - %out Parameter Requires Definite Assignment";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "func maybeAssign(%out value: int, flag: bool): void {\n"
                "    if (flag) {\n"
                "        value = 1;\n"
                "    }\n"
                "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "out_parameter_definite_assignment_test.zr", 40);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "%out parameter 'value' must be assigned on all control-flow paths"));

        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_interface_registration_preserves_variance_and_rejects_construction(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Generic Interface Registration Preserves Variance And Rejects Construction";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "interface IProducer<out T> { next(): T; }\n"
                "new IProducer<int>();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_interface_type_test.zr", 37);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        const SZrTypePrototypeInfo *openPrototype;
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        openPrototype = find_test_type_prototype(cs, "IProducer");
        TEST_ASSERT_NOT_NULL(openPrototype);
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, openPrototype->type);
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)openPrototype->genericParameters.length);
        TEST_ASSERT_FALSE(openPrototype->allowValueConstruction);
        TEST_ASSERT_FALSE(openPrototype->allowBoxedConstruction);
        {
            const SZrTypeGenericParameterInfo *genericInfo =
                    (const SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)&openPrototype->genericParameters,
                                                                          0);
            TEST_ASSERT_NOT_NULL(genericInfo);
            TEST_ASSERT_EQUAL_INT(ZR_GENERIC_VARIANCE_OUT, genericInfo->variance);
        }

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Interfaces cannot be constructed"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_interface_members_flow_through_inheritance_chain(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Interface Members Flow Through Inheritance Chain";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "interface Readable { read(): int; }\n"
                "interface StreamReadable : Readable { available(): int; }\n"
                "class Device : StreamReadable {\n"
                "    read(): int { return 1; }\n"
                "    available(): int { return 2; }\n"
                "}\n"
                "var device: Device = null;\n"
                "device.read();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_interface_chain_type_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(5, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[2]);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[3]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[4]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_constraint_accepts_source_interface_implementation(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Generic Constraint Accepts Source Interface Implementation";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "interface Readable { read(): int; }\n"
                "class Device : Readable { read(): int { return 1; } }\n"
                "class Box<T> where T: Readable { var value: T; }\n"
                "new Box<Device>();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_constraint_accept_test.zr", 40);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(4, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[2]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[3]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Box<Device>", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_constraint_mismatch_rejects_non_implementing_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Generic Constraint Mismatch Rejects Non Implementing Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "interface Readable { read(): int; }\n"
                "class Box<T> where T: Readable { var value: T; }\n"
                "new Box<int>();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_constraint_reject_test.zr", 40);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "does not satisfy constraint"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_generic_inheritance_substitutes_closed_base_member_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Source Generic Inheritance Substitutes Closed Base Member Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "class Base<T> { var value: T; }\n"
                "class Derived<T> : Base<T> { }\n"
                "new Derived<int>().value;";
        SZrString *sourceName = ZrCore_String_Create(state, "source_generic_inheritance_type_test.zr", 40);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        const SZrTypePrototypeInfo *closedPrototype;
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        closedPrototype = find_test_type_prototype(cs, "Derived<int>");
        TEST_ASSERT_NOT_NULL(closedPrototype);
        TEST_ASSERT_TRUE(closedPrototype->inherits.length >= 1);
        {
            const SZrString *inheritName =
                    *(const SZrString **)ZrCore_Array_Get((SZrArray *)&closedPrototype->inherits, 0);
            TEST_ASSERT_NOT_NULL(inheritName);
            TEST_ASSERT_EQUAL_STRING("Base<int>", ZrCore_String_GetNativeString(inheritName));
        }

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_class_and_struct_constraints_are_enforced(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Class And Struct Constraints Are Enforced";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "class RefType { }\n"
                "struct ValueType { }\n"
                "class NeedClass<T> where T: class { var value: T; }\n"
                "class NeedStruct<T> where T: struct { var value: T; }\n"
                "new NeedClass<RefType>();\n"
                "new NeedStruct<ValueType>();\n"
                "new NeedClass<ValueType>();";
        SZrString *sourceName = ZrCore_String_Create(state, "source_kind_constraint_test.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *successExpr1 = ZR_NULL;
        SZrAstNode *successExpr2 = ZR_NULL;
        SZrAstNode *failureExpr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(7, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[2]);
        TEST_ASSERT_FALSE(cs->hasError);
        compile_test_top_level_statement(cs, ast->data.script.statements->nodes[3]);
        TEST_ASSERT_FALSE(cs->hasError);

        successExpr1 = ast->data.script.statements->nodes[4]->data.expressionStatement.expr;
        successExpr2 = ast->data.script.statements->nodes[5]->data.expressionStatement.expr;
        failureExpr = ast->data.script.statements->nodes[6]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(successExpr1);
        TEST_ASSERT_NOT_NULL(successExpr2);
        TEST_ASSERT_NOT_NULL(failureExpr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, successExpr1, &result));
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("NeedClass<RefType>", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, successExpr2, &result));
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("NeedStruct<ValueType>", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, failureExpr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "class constraint"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_import_function_call_uses_exported_signature(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Import Function Call Uses Exported Signature";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *moduleSource = "add(lhs: int, rhs: int): int { return lhs + rhs; }";
        const char *source = "var calc = %import(\"calc\"); calc.add(1, 2);";
        SZrString *sourceName = ZrCore_String_Create(state, "source_import_signature_type_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        install_import_test_fixture(state,
                                    "calc",
                                    (const TZrByte *)moduleSource,
                                    strlen(moduleSource),
                                    ZR_FALSE);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        state->global->sourceLoader = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_import_function_call_rejects_argument_mismatch(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Import Function Call Rejects Argument Mismatch";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *moduleSource = "add(lhs: int, rhs: int): int { return lhs + rhs; }";
        const char *source = "var calc = %import(\"calc\"); calc.add(1, 2.0);";
        SZrString *sourceName = ZrCore_String_Create(state, "source_import_signature_fail_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        install_import_test_fixture(state,
                                    "calc",
                                    (const TZrByte *)moduleSource,
                                    strlen(moduleSource),
                                    ZR_FALSE);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Argument type mismatch"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        state->global->sourceLoader = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_import_member_chain_uses_returned_prototype_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Import Member Chain Uses Returned Prototype Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *moduleSource =
                "struct Pair { var left: int; var right: int; }\n"
                "makePair(): Pair { return $Pair(1, 2); }";
        const char *source = "var shapes = %import(\"shapes\"); shapes.makePair().left;";
        SZrString *sourceName = ZrCore_String_Create(state, "source_import_member_chain_type_test.zr", 38);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        install_import_test_fixture(state,
                                    "shapes",
                                    (const TZrByte *)moduleSource,
                                    strlen(moduleSource),
                                    ZR_FALSE);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        state->global->sourceLoader = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_destructured_native_type_import_allows_unqualified_generic_usage(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Destructured Native Type Import Allows Unqualified Generic Usage";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var {Pair} = %import(\"zr.container\");"
                "var pair: Pair<int,float> = $Pair<int,float>(1, 2.0);"
                "pair.first;";
        SZrString *sourceName = ZrCore_String_Create(state, "destructured_native_pair_import_type_test.zr", 45);
        SZrString *leftName = create_test_string(state, "first");
        SZrString *pairName = create_test_string(state, "pair");
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrTypePrototypeInfo *pairPrototype = ZR_NULL;
        SZrTypeMemberInfo *leftMember = ZR_NULL;
        SZrInferredType pairType;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_NOT_NULL(leftName);
        TEST_ASSERT_NOT_NULL(pairName);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_InferredType_Init(state, &pairType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupVariable(state, cs->typeEnv, pairName, &pairType));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, pairType.baseType);
        TEST_ASSERT_NOT_NULL(pairType.typeName);
        TEST_ASSERT_EQUAL_STRING("Pair<int, float>", ZrCore_String_GetNativeString(pairType.typeName));
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)pairType.elementTypes.length);

        pairPrototype = find_compiler_type_prototype_inference(cs, pairType.typeName);
        TEST_ASSERT_NOT_NULL(pairPrototype);

        for (TZrSize memberIndex = 0; memberIndex < pairPrototype->members.length; memberIndex++) {
            SZrTypeMemberInfo *candidate =
                    (SZrTypeMemberInfo *)ZrCore_Array_Get(&pairPrototype->members, memberIndex);
            if (candidate != ZR_NULL &&
                candidate->name != ZR_NULL &&
                ZrCore_String_Equal(candidate->name, leftName)) {
                leftMember = candidate;
                break;
            }
        }
        TEST_ASSERT_NOT_NULL(leftMember);
        TEST_ASSERT_NOT_NULL(leftMember->fieldTypeName);
        TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(leftMember->fieldTypeName));

        ZrParser_InferredType_Free(state, &pairType);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_destructured_native_type_import_registers_closed_variable_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Destructured Native Type Import Registers Closed Variable Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var {Pair} = %import(\"zr.container\");"
                "var pair: Pair<int,float> = $Pair<int,float>(1, 2.0);";
        SZrString *sourceName = ZrCore_String_Create(state, "destructured_native_pair_import_variable_type_test.zr", 54);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrString *pairName = create_test_string(state, "pair");
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_NOT_NULL(pairName);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupVariable(state, cs->typeEnv, pairName, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Pair<int, float>", ZrCore_String_GetNativeString(result.typeName));
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)result.elementTypes.length);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_qualified_native_type_annotation_accepts_matching_destructured_generic_instance(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Qualified Native Type Annotation Accepts Matching Destructured Generic Instance";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "var container = %import(\"zr.container\");\n"
                "var {Pair} = %import(\"zr.container\");\n"
                "var pair1: container.Pair<int, float> = $container.Pair<int, float>(1, 2.0);\n"
                "var pair2: Pair<int, float> = $Pair<int, float>(1, 2.0);\n"
                "return pair1.first + <int> pair2.first;\n";
        SZrString *sourceName =
                ZrCore_String_Create(state,
                                     "qualified_native_type_annotation_pair_import_test.zr",
                                     53);
        SZrFunction *entryFunction = ZR_NULL;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(sourceName);

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);

        ZrCore_Function_Free(state, entryFunction);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_import_requires_explicit_binding_for_unqualified_generic_type_annotation(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Native Import Requires Explicit Binding For Unqualified Generic Type Annotation";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var container = %import(\"zr.container\");"
                "var pair: Pair<int,float> = $Pair<int,float>(1, 2.0);";
        SZrString *sourceName = ZrCore_String_Create(state, "native_pair_requires_explicit_binding_type_test.zr", 51);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "explicit"));

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_import_requires_explicit_binding_for_unqualified_generic_construct_target(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Native Import Requires Explicit Binding For Unqualified Generic Construct Target";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var container = %import(\"zr.container\");"
                "$Pair<int,float>(1, 2.0);";
        SZrString *sourceName =
                ZrCore_String_Create(state, "native_pair_requires_explicit_binding_construct_test.zr", 56);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "explicit"));

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_destructured_native_type_import_rejects_second_pair_declaration(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Inference - Destructured Native Type Import Rejects Second Pair Declaration";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var {Pair} = %import(\"zr.container\");"
                "struct Pair { var left: int; var right: int; }";
        SZrString *sourceName = ZrCore_String_Create(state, "destructured_native_pair_collision_test.zr", 41);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        if (!cs->hasError) {
            compile_test_top_level_statement(cs, ast->data.script.statements->nodes[1]);
        }

        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Pair"));
        TEST_ASSERT_TRUE(strstr(cs->errorMessage, "already") != ZR_NULL ||
                         strstr(cs->errorMessage, "duplicate") != ZR_NULL);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_source_import_array_assignment_rejects_incompatible_value(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Source Import Array Assignment Rejects Incompatible Value";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *moduleSource = "pub var numbers = [1, 2, 3];";
        const char *source = "var data = %import(\"data\"); data.numbers[0] = 1.5;";
        SZrString *sourceName = ZrCore_String_Create(state, "source_import_array_assign_type_test.zr", 39);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        install_import_test_fixture(state,
                                    "data",
                                    (const TZrByte *)moduleSource,
                                    strlen(moduleSource),
                                    ZR_FALSE);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Assignment type mismatch"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        state->global->sourceLoader = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_binary_import_function_call_uses_exported_signature(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Binary Import Function Call Uses Exported Signature";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *moduleSource = "add(lhs: int, rhs: int): int { return lhs + rhs; }";
        const char *source = "var calc = %import(\"calc\"); calc.add(1, 2);";
        const char *binaryPath = "test_type_inference_import_fixture.zro";
        TZrByte *binaryBytes = ZR_NULL;
        TZrSize binaryLength = 0;
        SZrString *sourceName = ZrCore_String_Create(state, "binary_import_signature_type_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        ZrParser_ToGlobalState_Register(state);
        binaryBytes = build_binary_import_fixture(state, moduleSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        install_import_test_fixture(state, "calc", binaryBytes, binaryLength, ZR_TRUE);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        state->global->sourceLoader = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        free(binaryBytes);
        remove(binaryPath);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_binary_import_function_call_rejects_argument_mismatch(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Binary Import Function Call Rejects Argument Mismatch";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *moduleSource = "add(lhs: int, rhs: int): int { return lhs + rhs; }";
        const char *source = "var calc = %import(\"calc\"); calc.add(1, 2.0);";
        const char *binaryPath = "test_type_inference_import_fixture_fail.zro";
        TZrByte *binaryBytes = ZR_NULL;
        TZrSize binaryLength = 0;
        SZrString *sourceName = ZrCore_String_Create(state, "binary_import_signature_fail_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        ZrParser_ToGlobalState_Register(state);
        binaryBytes = build_binary_import_fixture(state, moduleSource, binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        install_import_test_fixture(state, "calc", binaryBytes, binaryLength, ZR_TRUE);
        TEST_ASSERT_NOT_NULL(ast);
        cs->scriptAst = ast;
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Argument type mismatch"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        state->global->sourceLoader = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        free(binaryBytes);
        remove(binaryPath);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_method_call_rejects_registered_parameter_mismatch(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Method Call Rejects Registered Parameter Mismatch";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var probe = %import(\"probe.native_shapes\");"
                "var device = new probe.NativeDevice();"
                "device.configure(\"bad\");";
        SZrString *sourceName = ZrCore_String_Create(state, "native_probe_method_mismatch_test.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_TRUE(register_probe_native_module(state));
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->scriptAst = ast;
        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_TRUE(cs->hasError);
        TEST_ASSERT_NOT_NULL(cs->errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(cs->errorMessage, "Argument type mismatch"));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_instance_method_uses_registered_return_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Instance Method Uses Registered Return Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var math = %import(\"zr.math\");"
                "var v = $math.Vector3(1.0, 2.0, 3.0);"
                "v.length();";
        SZrString *sourceName = ZrCore_String_Create(state, "native_vector3_method_type_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[2]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_struct_field_access_uses_registered_field_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Struct Field Access Uses Registered Field Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var system = %import(\"zr.system\");"
                "system.vm.state().loadedModuleCount;";
        SZrString *sourceName = ZrCore_String_Create(state, "native_vm_state_field_type_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_nested_module_method_call_returns_null(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Nested Module Method Call Returns Null";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var system = %import(\"zr.system\");"
                "system.console.printLine(\"hello\");";
        SZrString *sourceName = ZrCore_String_Create(state, "native_system_console_type_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_fs_info_field_uses_registered_field_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Fs Info Field Uses Registered Field Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var system = %import(\"zr.system\");"
                "system.fs.getInfo(system.fs.currentDirectory()).modifiedMilliseconds;";
        SZrString *sourceName = ZrCore_String_Create(state, "native_system_fs_info_type_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_process_arguments_is_array(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Process Arguments Is Array";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var system = %import(\"zr.system\");"
                "system.process.arguments;";
        SZrString *sourceName = ZrCore_String_Create(state, "native_system_process_arguments_type_test.zr", 43);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_native_vm_loaded_modules_element_field_uses_registered_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Vm Loaded Modules Element Field Uses Registered Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var system = %import(\"zr.system\");"
                "system.vm.loadedModules()[0].sourcePath;";
        SZrString *sourceName = ZrCore_String_Create(state, "native_vm_loaded_modules_field_type_test.zr", 43);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.baseType);

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_container_dotted_generic_new_returns_closed_registered_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Container Dotted Generic New Returns Closed Registered Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var container = %import(\"zr.container\");"
                "new container.Array<int>();";
        SZrString *sourceName = ZrCore_String_Create(state, "container_dotted_generic_new_type_test.zr", 42);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);

        expr = ast->data.script.statements->nodes[1]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Array<int>", ZrCore_String_GetNativeString(result.typeName));

        ZrParser_InferredType_Free(state, &result);
        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_container_computed_access_uses_registered_meta_types(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Container Computed Access Uses Registered Meta Types";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var container = %import(\"zr.container\");"
                "var {Array, Map} = %import(\"zr.container\");"
                "var xs: Array<int> = new container.Array<int>();"
                "var map: Map<string,int> = new container.Map<string,int>();"
                "xs[0];"
                "map[\"answer\"];";
        SZrString *sourceName = ZrCore_String_Create(state, "container_computed_access_type_test.zr", 39);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *arrayExpr = ZR_NULL;
        SZrAstNode *mapExpr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(6, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[1]);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[2]);
        TEST_ASSERT_FALSE(cs->hasError);
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[3]);
        TEST_ASSERT_FALSE(cs->hasError);

        arrayExpr = ast->data.script.statements->nodes[4]->data.expressionStatement.expr;
        mapExpr = ast->data.script.statements->nodes[5]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(arrayExpr);
        TEST_ASSERT_NOT_NULL(mapExpr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, arrayExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, mapExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_ffi_pointer_helpers_propagate_registered_pointer_types(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - FFI Pointer Helpers Propagate Registered Pointer Types";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var ffi = %import(\"zr.ffi\");"
                "var buffer = ffi.BufferHandle.allocate(8);"
                "var bytePtr = buffer.pin();"
                "var typedPtr = bytePtr.as({ kind: \"pointer\", to: \"i32\", direction: \"inout\" });"
                "var direct: ffi.Ptr<u8> = bytePtr;"
                "bytePtr;"
                "typedPtr;"
                "direct;"
                "typedPtr.read(\"i32\");";
        SZrString *sourceName = ZrCore_String_Create(state, "ffi_pointer_helper_type_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *bytePtrExpr = ZR_NULL;
        SZrAstNode *typedPtrExpr = ZR_NULL;
        SZrAstNode *directPtrExpr = ZR_NULL;
        SZrAstNode *readExpr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(9, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        for (TZrSize index = 0; index < 5; index++) {
            ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[index]);
            TEST_ASSERT_FALSE(cs->hasError);
        }

        bytePtrExpr = ast->data.script.statements->nodes[5]->data.expressionStatement.expr;
        typedPtrExpr = ast->data.script.statements->nodes[6]->data.expressionStatement.expr;
        directPtrExpr = ast->data.script.statements->nodes[7]->data.expressionStatement.expr;
        readExpr = ast->data.script.statements->nodes[8]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(bytePtrExpr);
        TEST_ASSERT_NOT_NULL(typedPtrExpr);
        TEST_ASSERT_NOT_NULL(directPtrExpr);
        TEST_ASSERT_NOT_NULL(readExpr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, bytePtrExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Ptr<u8>", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, typedPtrExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Ptr<i32>", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, directPtrExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Ptr<u8>", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, readExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT32, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_ffi_pointer_helpers_propagate_extern_wrapper_types(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - FFI Pointer Helpers Propagate Extern Wrapper Types";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "%extern(\"fixture\") {"
                "    struct NativePoint {"
                "        var x: i32;"
                "        var y: i32;"
                "    }"
                "}"
                "var ffi = %import(\"zr.ffi\");"
                "var buffer = ffi.BufferHandle.allocate(8);"
                "var bytePtr = buffer.pin();"
                "var pointPtr = bytePtr.as({ kind: \"pointer\", to: NativePoint, direction: \"inout\" });"
                "var pointValue = pointPtr.read(NativePoint);"
                "pointPtr;"
                "pointValue;"
                "pointValue.y;";
        SZrString *sourceName = ZrCore_String_Create(state, "ffi_pointer_extern_wrapper_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *pointPtrExpr = ZR_NULL;
        SZrAstNode *pointValueExpr = ZR_NULL;
        SZrAstNode *pointFieldExpr = ZR_NULL;
        SZrInferredType result;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(9, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);
        for (TZrSize index = 0; index < 6; index++) {
            ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[index]);
            TEST_ASSERT_FALSE(cs->hasError);
        }

        pointPtrExpr = ast->data.script.statements->nodes[6]->data.expressionStatement.expr;
        pointValueExpr = ast->data.script.statements->nodes[7]->data.expressionStatement.expr;
        pointFieldExpr = ast->data.script.statements->nodes[8]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(pointPtrExpr);
        TEST_ASSERT_NOT_NULL(pointValueExpr);
        TEST_ASSERT_NOT_NULL(pointFieldExpr);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, pointPtrExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Ptr<NativePoint>", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, pointValueExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("NativePoint", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, pointFieldExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT32, result.baseType);
        ZrParser_InferredType_Free(state, &result);

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_system_primitive_named_and_direct_types_compare_equally(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type System - Primitive Named And Direct Types Compare Equally";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrInferredType namedString;
        SZrInferredType directString;
        SZrInferredType namedInt;
        SZrInferredType directInt;
        SZrInferredType int64Alias;
        SZrInferredType namedObject;
        SZrInferredType plainObject;

        TEST_ASSERT_NOT_NULL(state);

        ZrParser_InferredType_InitFull(state,
                                       &namedString,
                                       ZR_VALUE_TYPE_STRING,
                                       ZR_FALSE,
                                       create_test_string(state, "string"));
        ZrParser_InferredType_Init(state, &directString, ZR_VALUE_TYPE_STRING);
        ZrParser_InferredType_InitFull(state,
                                       &namedInt,
                                       ZR_VALUE_TYPE_INT64,
                                       ZR_FALSE,
                                       create_test_string(state, "int"));
        ZrParser_InferredType_Init(state, &directInt, ZR_VALUE_TYPE_INT64);
        ZrParser_InferredType_InitFull(state,
                                       &int64Alias,
                                       ZR_VALUE_TYPE_INT64,
                                       ZR_FALSE,
                                       create_test_string(state, "i64"));
        ZrParser_InferredType_InitFull(state,
                                       &namedObject,
                                       ZR_VALUE_TYPE_OBJECT,
                                       ZR_FALSE,
                                       create_test_string(state, "Widget"));
        ZrParser_InferredType_Init(state, &plainObject, ZR_VALUE_TYPE_OBJECT);

        TEST_ASSERT_TRUE(ZrParser_InferredType_Equal(&namedString, &directString));
        TEST_ASSERT_TRUE(ZrParser_InferredType_IsCompatible(&directString, &namedString));
        TEST_ASSERT_TRUE(ZrParser_InferredType_IsCompatible(&namedString, &directString));

        TEST_ASSERT_TRUE(ZrParser_InferredType_Equal(&namedInt, &directInt));
        TEST_ASSERT_TRUE(ZrParser_InferredType_Equal(&namedInt, &int64Alias));
        TEST_ASSERT_TRUE(ZrParser_InferredType_IsCompatible(&directInt, &namedInt));
        TEST_ASSERT_TRUE(ZrParser_InferredType_IsCompatible(&int64Alias, &directInt));

        TEST_ASSERT_FALSE(ZrParser_InferredType_Equal(&namedObject, &plainObject));
        TEST_ASSERT_FALSE(ZrParser_InferredType_IsCompatible(&plainObject, &namedObject));

        ZrParser_InferredType_Free(state, &plainObject);
        ZrParser_InferredType_Free(state, &namedObject);
        ZrParser_InferredType_Free(state, &int64Alias);
        ZrParser_InferredType_Free(state, &directInt);
        ZrParser_InferredType_Free(state, &namedInt);
        ZrParser_InferredType_Free(state, &directString);
        ZrParser_InferredType_Free(state, &namedString);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_environment_lookup_functions_deduplicates_exact_signature_from_parent_scope(void) {
    SZrTestTimer timer = {0};
    const char *testSummary =
            "Type Environment - Lookup Functions Deduplicates Exact Parent Scope Signature";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        SZrTypeEnvironment *childEnv = ZR_NULL;
        SZrInferredType returnType;
        SZrInferredType paramType;
        SZrArray paramTypes;
        SZrArray results;
        SZrString *functionName;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);

        functionName = create_test_string(state, "NativeAdd");
        TEST_ASSERT_NOT_NULL(functionName);

        childEnv = ZrParser_TypeEnvironment_New(state);
        TEST_ASSERT_NOT_NULL(childEnv);
        childEnv->parent = cs->typeEnv;

        ZrParser_InferredType_Init(state, &returnType, ZR_VALUE_TYPE_INT32);
        ZrParser_InferredType_Init(state, &paramType, ZR_VALUE_TYPE_INT32);
        ZrCore_Array_Init(state, &paramTypes, sizeof(SZrInferredType), 2);
        ZrCore_Array_Push(state, &paramTypes, &paramType);
        ZrCore_Array_Push(state, &paramTypes, &paramType);

        TEST_ASSERT_TRUE(
                ZrParser_TypeEnvironment_RegisterFunction(state, cs->typeEnv, functionName, &returnType, &paramTypes));
        TEST_ASSERT_TRUE(
                ZrParser_TypeEnvironment_RegisterFunction(state, childEnv, functionName, &returnType, &paramTypes));

        ZrCore_Array_Construct(&results);
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunctions(state, childEnv, functionName, &results));
        TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)results.length);

        ZrCore_Array_Free(state, &results);
        ZrCore_Array_Free(state, &paramTypes);
        ZrParser_InferredType_Free(state, &paramType);
        ZrParser_InferredType_Free(state, &returnType);
        ZrParser_TypeEnvironment_Free(state, childEnv);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_foreach_binds_iterated_element_type(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Foreach Binds Iterated Element Type";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var container = %import(\"zr.container\");"
                "var {Array} = %import(\"zr.container\");"
                "var dynamic: Array<int> = new container.Array<int>();"
                "var fixed = [1, 2, 3];"
                "var sum: int = 0;"
                "for (var item in fixed) { sum = sum + item; }"
                "for (var entry in dynamic) { sum = sum + entry; }";
        SZrString *sourceName = ZrCore_String_Create(state, "foreach_binding_type_test.zr", 28);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(cs);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(7, (int)ast->data.script.statements->count);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        for (TZrSize index = 0; index < ast->data.script.statements->count; index++) {
            ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[index]);
            TEST_ASSERT_FALSE(cs->hasError);
        }

        ZrCore_Function_Free(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_cyclic_import_entry_value_read_rejects_same_scc(void) {
    static const SZrImportTestFixtureSource kFixtures[] = {
            {
                    "a",
                    (const TZrByte *)
                            "var b = %import(\"b\");\n"
                            "pub var a1 = b.b1;\n",
                    sizeof("var b = %import(\"b\");\n"
                           "pub var a1 = b.b1;\n") - 1,
                    ZR_FALSE,
            },
            {
                    "b",
                    (const TZrByte *)
                            "var a = %import(\"a\");\n"
                            "pub var b1 = a.a1;\n",
                    sizeof("var a = %import(\"a\");\n"
                           "pub var b1 = a.a1;\n") - 1,
                    ZR_FALSE,
            },
    };
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Cyclic Import Entry Value Read Rejects Same SCC";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source = "var a = %import(\"a\");\nreturn 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "cyclic_import_entry_value_read_reject.zr", 40);
        SZrFunction *entryFunction = ZR_NULL;

        TEST_ASSERT_NOT_NULL(state);
        install_import_test_fixtures(state, kFixtures, ZR_ARRAY_COUNT(kFixtures));

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NULL(entryFunction);

        state->global->sourceLoader = ZR_NULL;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_cyclic_import_unsafe_imported_call_rejects_same_scc(void) {
    static const SZrImportTestFixtureSource kFixtures[] = {
            {
                    "a",
                    (const TZrByte *)
                            "var b = %import(\"b\");\n"
                            "pub var a1 = 7;\n"
                            "pub var value = b.readA();\n",
                    sizeof("var b = %import(\"b\");\n"
                           "pub var a1 = 7;\n"
                           "pub var value = b.readA();\n") - 1,
                    ZR_FALSE,
            },
            {
                    "b",
                    (const TZrByte *)
                            "var a = %import(\"a\");\n"
                            "pub readA(): int {\n"
                            "    return a.a1;\n"
                            "}\n",
                    sizeof("var a = %import(\"a\");\n"
                           "pub readA(): int {\n"
                           "    return a.a1;\n"
                           "}\n") - 1,
                    ZR_FALSE,
            },
    };
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Cyclic Import Unsafe Imported Call Rejects Same SCC";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source = "var a = %import(\"a\");\nreturn 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "cyclic_import_unsafe_call_reject.zr", 34);
        SZrFunction *entryFunction = ZR_NULL;

        TEST_ASSERT_NOT_NULL(state);
        install_import_test_fixtures(state, kFixtures, ZR_ARRAY_COUNT(kFixtures));

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NULL(entryFunction);

        state->global->sourceLoader = ZR_NULL;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_inference_cyclic_import_local_import_outside_entry_path_passes(void) {
    static const SZrImportTestFixtureSource kFixtures[] = {
            {
                    "a",
                    (const TZrByte *)
                            "pub later(): int {\n"
                            "    var b = %import(\"b\");\n"
                            "    return b.value;\n"
                            "}\n"
                            "pub seed(): int {\n"
                            "    return 5;\n"
                            "}\n",
                    sizeof("pub later(): int {\n"
                           "    var b = %import(\"b\");\n"
                           "    return b.value;\n"
                           "}\n"
                           "pub seed(): int {\n"
                           "    return 5;\n"
                           "}\n") - 1,
                    ZR_FALSE,
            },
            {
                    "b",
                    (const TZrByte *)
                            "var a = %import(\"a\");\n"
                            "pub var value = 9;\n",
                    sizeof("var a = %import(\"a\");\n"
                           "pub var value = 9;\n") - 1,
                    ZR_FALSE,
            },
    };
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Cyclic Import Local Import Outside Entry Path Passes";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source = "var a = %import(\"a\");\nreturn a.seed();\n";
        SZrString *sourceName = ZrCore_String_Create(state, "cyclic_import_local_deferred_pass.zr", 35);
        SZrFunction *entryFunction = ZR_NULL;

        TEST_ASSERT_NOT_NULL(state);
        install_import_test_fixtures(state, kFixtures, ZR_ARRAY_COUNT(kFixtures));

        entryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(entryFunction);
        ZrCore_Function_Free(state, entryFunction);

        state->global->sourceLoader = ZR_NULL;
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// 主测试函数
int main(void) {
    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("ZR-VM Type Inference System Unit Tests\n");
    TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();

    // 字面量类型推断测试
    printf("==========\n");
    printf("Literal Type Inference Tests\n");
    printf("==========\n");
    RUN_TEST(test_type_inference_integer_literal);
    RUN_TEST(test_type_inference_small_integer_literal_defaults_to_int64);
    RUN_TEST(test_compiler_state_initializes_semantic_context);
    RUN_TEST(test_type_environment_registers_semantic_records);
    RUN_TEST(test_type_environment_registers_function_overloads);
    RUN_TEST(test_type_inference_resolves_best_function_overload);
    RUN_TEST(test_convert_ast_type_registers_generic_instance_semantics);
    RUN_TEST(test_convert_ast_type_preserves_ownership_qualifier);
    RUN_TEST(test_convert_ast_type_preserves_qualified_root_module_member_name);
    RUN_TEST(test_construct_expression_preserves_ownership_qualifier);
    RUN_TEST(test_ownership_builtin_type_inference_rejects_invalid_operands);
    RUN_TEST(test_unique_instance_only_calls_borrowed_methods);
    RUN_TEST(test_unique_value_is_compatible_with_borrowed_parameter);
    RUN_TEST(test_borrowed_value_cannot_flow_to_plain_parameter);
    RUN_TEST(test_move_only_struct_assignment_rejects_implicit_copy);
    RUN_TEST(test_move_only_struct_argument_rejects_by_value_call);
    RUN_TEST(test_parser_supports_ownership_types_and_template_strings);
    RUN_TEST(test_using_statement_compilation_records_cleanup_plan);
    RUN_TEST(test_template_string_compilation_records_semantic_segments);
    RUN_TEST(test_type_inference_float_literal);
    RUN_TEST(test_type_inference_string_literal);
    RUN_TEST(test_type_inference_template_string_literal_is_string);
    RUN_TEST(test_type_inference_import_native_module_keeps_module_name);
    RUN_TEST(test_type_inference_type_query_returns_reflection_type);
    RUN_TEST(test_convert_function_ast_type_to_callable_inferred_type);
    RUN_TEST(test_type_inference_function_type_annotation_accepts_lambda_assignment);
    RUN_TEST(test_type_inference_type_query_function_type_returns_callable_reflection_type);
    RUN_TEST(test_type_inference_builtin_names_require_explicit_import);
    RUN_TEST(test_type_inference_builtin_names_accept_module_qualified_and_destructured_imports);
    RUN_TEST(test_type_inference_builtin_value_helpers_require_explicit_import);
    RUN_TEST(test_type_inference_legacy_builtin_aliases_fail_with_canonical_diagnostics);
    RUN_TEST(test_type_inference_type_value_aliases_can_be_used_in_type_position);
    RUN_TEST(test_type_inference_native_prototype_construction_returns_native_type);
    RUN_TEST(test_type_inference_rejects_ordinary_prototype_call);
    RUN_TEST(test_type_inference_native_boxed_new_returns_registered_type);
    RUN_TEST(test_type_inference_native_generic_boxed_new_returns_closed_registered_type);
    RUN_TEST(test_type_inference_native_generic_constraint_mismatch_is_rejected);
    RUN_TEST(test_type_inference_source_generic_class_boxed_new_returns_closed_registered_type);
    RUN_TEST(test_type_inference_source_class_boxed_new_succeeds_without_prior_declaration_compile);
    RUN_TEST(test_type_inference_source_generic_class_member_substitutes_closed_field_type);
    RUN_TEST(test_type_inference_source_generic_struct_boxed_new_returns_closed_registered_type);
    RUN_TEST(test_type_inference_source_const_generic_boxed_new_returns_closed_registered_type);
    RUN_TEST(test_type_inference_source_const_generic_boxed_new_succeeds_without_prior_declaration_compile);
    RUN_TEST(test_type_inference_source_generic_function_supports_explicit_arguments_and_inference);
    RUN_TEST(test_type_inference_source_generic_function_reports_explicit_arity_mismatch);
    RUN_TEST(test_type_inference_source_generic_function_reports_cannot_infer_type_argument);
    RUN_TEST(test_type_inference_source_generic_function_reports_type_argument_kind_mismatch);
    RUN_TEST(test_type_inference_source_generic_function_reports_conflicting_inference);
    RUN_TEST(test_type_inference_source_generic_method_supports_explicit_arguments_inference_and_receiver_closure);
    RUN_TEST(test_type_inference_source_const_generic_method_supports_explicit_arguments_and_inference);
    RUN_TEST(test_type_inference_source_const_generic_method_reports_const_argument_kind_mismatch);
    RUN_TEST(test_type_inference_parameter_passing_mode_in_rejects_reassignment);
    RUN_TEST(test_type_inference_parameter_passing_modes_out_and_ref_require_assignable_arguments);
    RUN_TEST(test_type_inference_out_parameter_requires_definite_assignment_on_all_paths);
    RUN_TEST(test_type_inference_source_generic_interface_registration_preserves_variance_and_rejects_construction);
    RUN_TEST(test_type_inference_source_interface_members_flow_through_inheritance_chain);
    RUN_TEST(test_type_inference_source_generic_constraint_accepts_source_interface_implementation);
    RUN_TEST(test_type_inference_source_generic_constraint_mismatch_rejects_non_implementing_type);
    RUN_TEST(test_type_inference_source_generic_inheritance_substitutes_closed_base_member_type);
    RUN_TEST(test_type_inference_source_class_and_struct_constraints_are_enforced);
    RUN_TEST(test_type_inference_container_dotted_generic_new_returns_closed_registered_type);
    RUN_TEST(test_type_inference_container_computed_access_uses_registered_meta_types);
    RUN_TEST(test_type_inference_ffi_pointer_helpers_propagate_registered_pointer_types);
    RUN_TEST(test_type_inference_ffi_pointer_helpers_propagate_extern_wrapper_types);
    RUN_TEST(test_type_system_primitive_named_and_direct_types_compare_equally);
    RUN_TEST(test_type_environment_lookup_functions_deduplicates_exact_signature_from_parent_scope);
    RUN_TEST(test_type_inference_foreach_binds_iterated_element_type);
    RUN_TEST(test_type_inference_native_enum_construction_returns_enum_type);
    RUN_TEST(test_type_inference_native_enum_member_access_returns_enum_type);
    RUN_TEST(test_type_inference_native_interface_construction_is_rejected);
    RUN_TEST(test_type_inference_native_interface_members_flow_through_implements_chain);
    RUN_TEST(test_type_inference_source_extern_function_call_uses_declared_return_type);
    RUN_TEST(test_type_inference_source_extern_enum_member_access_returns_enum_type);
    RUN_TEST(test_type_inference_source_extern_struct_construction_returns_struct_type);
    RUN_TEST(test_type_inference_source_import_function_call_uses_exported_signature);
    RUN_TEST(test_type_inference_source_import_function_call_rejects_argument_mismatch);
    RUN_TEST(test_type_inference_source_import_member_chain_uses_returned_prototype_type);
    RUN_TEST(test_type_inference_cyclic_import_entry_value_read_rejects_same_scc);
    RUN_TEST(test_type_inference_cyclic_import_unsafe_imported_call_rejects_same_scc);
    RUN_TEST(test_type_inference_cyclic_import_local_import_outside_entry_path_passes);
    RUN_TEST(test_type_inference_destructured_native_type_import_allows_unqualified_generic_usage);
    RUN_TEST(test_type_inference_destructured_native_type_import_registers_closed_variable_type);
    RUN_TEST(test_type_inference_qualified_native_type_annotation_accepts_matching_destructured_generic_instance);
    RUN_TEST(test_type_inference_native_import_requires_explicit_binding_for_unqualified_generic_type_annotation);
    RUN_TEST(test_type_inference_native_import_requires_explicit_binding_for_unqualified_generic_construct_target);
    RUN_TEST(test_type_inference_destructured_native_type_import_rejects_second_pair_declaration);
    RUN_TEST(test_type_inference_source_import_array_assignment_rejects_incompatible_value);
    RUN_TEST(test_type_inference_binary_import_function_call_uses_exported_signature);
    RUN_TEST(test_type_inference_binary_import_function_call_rejects_argument_mismatch);
    RUN_TEST(test_type_inference_native_method_call_rejects_registered_parameter_mismatch);
    RUN_TEST(test_type_inference_native_instance_method_uses_registered_return_type);
    RUN_TEST(test_type_inference_native_nested_module_method_call_returns_null);
    RUN_TEST(test_type_inference_native_fs_info_field_uses_registered_field_type);
    RUN_TEST(test_type_inference_native_process_arguments_is_array);
    RUN_TEST(test_type_inference_native_struct_field_access_uses_registered_field_type);
    RUN_TEST(test_type_inference_native_vm_loaded_modules_element_field_uses_registered_type);
    RUN_TEST(test_type_inference_boolean_literal);

    // 表达式类型推断测试
    printf("==========\n");
    printf("Expression Type Inference Tests\n");
    printf("==========\n");
    RUN_TEST(test_type_inference_binary_expression);

    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("All Test Cases Completed\n");
    TEST_MODULE_DIVIDER();
    printf("\n");

    return UNITY_END();
}
