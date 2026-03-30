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

extern void ZrParser_Expression_Compile(SZrCompilerState *cs, SZrAstNode *node);
extern void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node);

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
        ZrVmLibMath_Register(global);
        ZrVmLibSystem_Register(global);
        ZrVmLibFfi_Register(global);
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

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static SZrString *create_test_string(SZrState *state, const char *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_CreateFromNative(state, (TZrNativeString)value);
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
void test_type_inference_integer_literal(void) {
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
void test_type_inference_small_integer_literal_defaults_to_int64(void) {
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

void test_compiler_state_initializes_semantic_context(void) {
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

void test_type_environment_registers_semantic_records(void) {
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

void test_type_environment_registers_function_overloads(void) {
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

void test_type_inference_resolves_best_function_overload(void) {
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

void test_convert_ast_type_registers_generic_instance_semantics(void) {
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

void test_convert_ast_type_preserves_ownership_qualifier(void) {
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

void test_construct_expression_preserves_ownership_qualifier(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Construct Expression Preserves Ownership Qualifier";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "%unique new Holder();"
                "%shared new Holder();";
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

        uniqueExpr = ast->data.script.statements->nodes[0]->data.expressionStatement.expr;
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

        ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, sharedExpr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.baseType);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED, result.ownershipQualifier);
        TEST_ASSERT_NOT_NULL(result.typeName);
        TEST_ASSERT_EQUAL_STRING("Holder", ZrCore_String_GetNativeString(result.typeName));
        ZrParser_InferredType_Free(state, &result);
        ZrParser_Ast_Free(state, ast);
        destroy_test_compiler_state(cs);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_unique_instance_only_calls_borrowed_methods(void) {
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

void test_unique_value_is_compatible_with_borrowed_parameter(void) {
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

void test_borrowed_value_cannot_flow_to_plain_parameter(void) {
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
void test_type_inference_float_literal(void) {
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
void test_type_inference_string_literal(void) {
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
void test_type_inference_boolean_literal(void) {
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
void test_type_inference_binary_expression(void) {
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

void test_parser_supports_ownership_types_and_template_strings(void) {
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

void test_using_statement_compilation_records_cleanup_plan(void) {
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
        const char *source = "using resource;";
        SZrString *sourceName = ZrCore_String_Create(state, "using_cleanup_test.zr", 21);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *usingStmt;
        const SZrDeterministicCleanupStep *step;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        usingStmt = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(usingStmt);
        TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingStmt->type);

        cs->currentFunction = ZrCore_Function_New(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

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

void test_template_string_compilation_records_semantic_segments(void) {
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

void test_type_inference_template_string_literal_is_string(void) {
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

void test_type_inference_import_native_module_keeps_module_name(void) {
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

void test_type_inference_native_prototype_construction_returns_native_type(void) {
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

void test_type_inference_rejects_ordinary_prototype_call(void) {
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

void test_type_inference_native_boxed_new_returns_registered_type(void) {
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

void test_type_inference_native_enum_construction_returns_enum_type(void) {
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

void test_type_inference_native_enum_member_access_returns_enum_type(void) {
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

void test_type_inference_native_interface_construction_is_rejected(void) {
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

void test_type_inference_native_interface_members_flow_through_implements_chain(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Native Interface Members Flow Through Implements Chain";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCompilerState *cs = create_test_compiler_state(state);
        const char *source =
                "var probe = %import(\"probe.native_shapes\");"
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

void test_type_inference_source_extern_function_call_uses_declared_return_type(void) {
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
        ZrParser_Statement_Compile(cs, ast->data.script.statements->nodes[0]);
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

void test_type_inference_source_extern_enum_member_access_returns_enum_type(void) {
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

void test_type_inference_source_extern_struct_construction_returns_struct_type(void) {
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

void test_type_inference_native_instance_method_uses_registered_return_type(void) {
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

void test_type_inference_native_struct_field_access_uses_registered_field_type(void) {
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

void test_type_inference_native_nested_module_method_call_returns_null(void) {
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

void test_type_inference_native_fs_info_field_uses_registered_field_type(void) {
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

void test_type_inference_native_process_arguments_is_array(void) {
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

void test_type_inference_native_vm_loaded_modules_element_field_uses_registered_type(void) {
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
    RUN_TEST(test_construct_expression_preserves_ownership_qualifier);
    RUN_TEST(test_unique_instance_only_calls_borrowed_methods);
    RUN_TEST(test_unique_value_is_compatible_with_borrowed_parameter);
    RUN_TEST(test_borrowed_value_cannot_flow_to_plain_parameter);
    RUN_TEST(test_parser_supports_ownership_types_and_template_strings);
    RUN_TEST(test_using_statement_compilation_records_cleanup_plan);
    RUN_TEST(test_template_string_compilation_records_semantic_segments);
    RUN_TEST(test_type_inference_float_literal);
    RUN_TEST(test_type_inference_string_literal);
    RUN_TEST(test_type_inference_template_string_literal_is_string);
    RUN_TEST(test_type_inference_import_native_module_keeps_module_name);
    RUN_TEST(test_type_inference_native_prototype_construction_returns_native_type);
    RUN_TEST(test_type_inference_rejects_ordinary_prototype_call);
    RUN_TEST(test_type_inference_native_boxed_new_returns_registered_type);
    RUN_TEST(test_type_inference_native_enum_construction_returns_enum_type);
    RUN_TEST(test_type_inference_native_enum_member_access_returns_enum_type);
    RUN_TEST(test_type_inference_native_interface_construction_is_rejected);
    RUN_TEST(test_type_inference_native_interface_members_flow_through_implements_chain);
    RUN_TEST(test_type_inference_source_extern_function_call_uses_declared_return_type);
    RUN_TEST(test_type_inference_source_extern_enum_member_access_returns_enum_type);
    RUN_TEST(test_type_inference_source_extern_struct_construction_returns_struct_type);
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
