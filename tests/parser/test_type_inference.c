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
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_inference.h"

extern void compile_expression(SZrCompilerState *cs, SZrAstNode *node);
extern void compile_statement(SZrCompilerState *cs, SZrAstNode *node);

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
        } else {
            return malloc(newSize);
        }
    }
}

// 创建测试用的SZrState
static SZrState *createTestState(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, &callbacks);
    if (!global)
        return ZR_NULL;

    SZrState *mainState = global->mainThreadState;
    if (mainState) {
        ZrGlobalStateInitRegistry(mainState, global);
    }

    return mainState;
}

// 销毁测试用的SZrState
static void destroyTestState(SZrState *state) {
    if (!state)
        return;

    SZrGlobalState *global = state->global;
    if (global) {
        ZrGlobalStateFree(global);
    }
}

// 创建测试用的编译器状态
static SZrCompilerState *createTestCompilerState(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    SZrCompilerState *cs = (SZrCompilerState *) malloc(sizeof(SZrCompilerState));
    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCompilerStateInit(cs, state);
    return cs;
}

// 销毁测试用的编译器状态
static void destroyTestCompilerState(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrCompilerStateFree(cs);
    free(cs);
}

static const SZrSemanticSymbolRecord *findSemanticSymbolRecord(SZrSemanticContext *context,
                                                               const char *name,
                                                               EZrSemanticSymbolKind kind) {
    TZrSize i;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < context->symbols.length; i++) {
        SZrSemanticSymbolRecord *record =
            (SZrSemanticSymbolRecord *)ZrArrayGet(&context->symbols, i);
        TNativeString nativeName;
        if (record != ZR_NULL && record->name != ZR_NULL &&
            record->kind == kind) {
            nativeName = ZrStringGetNativeStringShort(record->name);
            if (nativeName != ZR_NULL && strcmp(nativeName, name) == 0) {
            return record;
            }
        }
    }

    return ZR_NULL;
}

static const SZrSemanticOverloadSetRecord *findSemanticOverloadSetRecord(SZrSemanticContext *context,
                                                                         const char *name) {
    TZrSize i;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *record =
            (SZrSemanticOverloadSetRecord *)ZrArrayGet(&context->overloadSets, i);
        if (record != ZR_NULL && record->name != ZR_NULL) {
            TNativeString nativeName = ZrStringGetNativeString(record->name);
            if (nativeName != ZR_NULL && strcmp(nativeName, name) == 0) {
                return record;
            }
        }
    }

    return ZR_NULL;
}

static const SZrSemanticTypeRecord *findSemanticTypeRecord(SZrSemanticContext *context,
                                                           const char *name,
                                                           EZrSemanticTypeKind kind) {
    TZrSize i;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (i = 0; i < context->types.length; i++) {
        SZrSemanticTypeRecord *record =
            (SZrSemanticTypeRecord *)ZrArrayGet(&context->types, i);
        if (record != ZR_NULL && record->name != ZR_NULL && record->kind == kind) {
            TNativeString nativeName = ZrStringGetNativeString(record->name);
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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Integer literal type inference", "Testing type inference for integer literal: 123");

    // 解析整数表达式
    const char *source = "123;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse integer literal");
        destroyTestCompilerState(cs);
        destroyTestState(state);
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
        ZrParserFreeAst(state, ast);
        destroyTestCompilerState(cs);
        destroyTestState(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TBool success = infer_literal_type(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    ZrInferredTypeFree(state, &result);
    ZrParserFreeAst(state, ast);
    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Small integer literal default inference", "Testing type inference for small integer literal: 1");

    const char *source = "1;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse small integer literal");
        destroyTestCompilerState(cs);
        destroyTestState(state);
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
        ZrParserFreeAst(state, ast);
        destroyTestCompilerState(cs);
        destroyTestState(state);
        return;
    }

    SZrInferredType result;
    ZrInferredTypeInit(state, &result, ZR_VALUE_TYPE_OBJECT);
    TBool success = infer_expression_type(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    ZrInferredTypeFree(state, &result);
    ZrParserFreeAst(state, ast);
    destroyTestCompilerState(cs);
    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_compiler_state_initializes_semantic_context(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Compiler State - Initializes Semantic Context";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Compiler state semantic bootstrap",
              "Testing that compiler state creates semantic context and shares it with type environment");

    TEST_ASSERT_TRUE(cs->semanticContext != ZR_NULL);
    TEST_ASSERT_TRUE(cs->typeEnv != ZR_NULL);
    TEST_ASSERT_TRUE(cs->semanticContext == cs->typeEnv->semanticContext);
    TEST_ASSERT_TRUE(cs->hirModule == ZR_NULL);

    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Type environment semantic emission",
              "Testing variable/function/type registration writes semantic type, symbol and overload records");

    ZrInferredTypeInit(state, &intType, ZR_VALUE_TYPE_INT64);
    ZrInferredTypeInit(state, &returnType, ZR_VALUE_TYPE_INT64);
    ZrArrayConstruct(&paramTypes);

    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterVariable(
        state, cs->typeEnv, ZrStringCreate(state, "value", 5), &intType));
    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterFunction(
        state, cs->typeEnv, ZrStringCreate(state, "add", 3), &returnType, &paramTypes));
    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterType(
        state, cs->typeEnv, ZrStringCreate(state, "Point", 5)));

    TEST_ASSERT_TRUE(cs->semanticContext->types.length > 0);
    TEST_ASSERT_TRUE(cs->semanticContext->symbols.length > 0);
    TEST_ASSERT_TRUE(cs->semanticContext->overloadSets.length > 0);

    varRecord = findSemanticSymbolRecord(cs->semanticContext, "value", ZR_SEMANTIC_SYMBOL_KIND_VARIABLE);
    funcRecord = findSemanticSymbolRecord(cs->semanticContext, "add", ZR_SEMANTIC_SYMBOL_KIND_FUNCTION);
    typeRecord = findSemanticSymbolRecord(cs->semanticContext, "Point", ZR_SEMANTIC_SYMBOL_KIND_TYPE);

    TEST_ASSERT_NOT_NULL(varRecord);
    TEST_ASSERT_NOT_NULL(funcRecord);
    TEST_ASSERT_NOT_NULL(typeRecord);
    TEST_ASSERT_NOT_EQUAL(0, funcRecord->overloadSetId);
    TEST_ASSERT_NOT_EQUAL(0, varRecord->typeId);
    TEST_ASSERT_NOT_EQUAL(0, funcRecord->typeId);
    TEST_ASSERT_NOT_EQUAL(0, typeRecord->typeId);

    ZrInferredTypeFree(state, &returnType);
    ZrInferredTypeFree(state, &intType);
    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Function overload registration",
              "Testing that same-name functions with different signatures are retained in the type environment");

    ZrInferredTypeInit(state, &intType, ZR_VALUE_TYPE_INT64);
    ZrInferredTypeInit(state, &boolType, ZR_VALUE_TYPE_BOOL);
    ZrInferredTypeInit(state, &doubleType, ZR_VALUE_TYPE_DOUBLE);
    ZrArrayInit(state, &intParams, sizeof(SZrInferredType), 1);
    ZrArrayInit(state, &doubleParams, sizeof(SZrInferredType), 1);
    ZrArrayPush(state, &intParams, &intType);
    ZrArrayPush(state, &doubleParams, &doubleType);

    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterFunction(
        state, cs->typeEnv, ZrStringCreate(state, "pick", 4), &intType, &intParams));
    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterFunction(
        state, cs->typeEnv, ZrStringCreate(state, "pick", 4), &boolType, &doubleParams));
    TEST_ASSERT_EQUAL_UINT32(2, (TUInt32)cs->typeEnv->functionReturnTypes.length);

    overloadRecord = findSemanticOverloadSetRecord(cs->semanticContext, "pick");
    TEST_ASSERT_NOT_NULL(overloadRecord);
    TEST_ASSERT_EQUAL_UINT32(2, (TUInt32)overloadRecord->members.length);

    ZrArrayFree(state, &doubleParams);
    ZrArrayFree(state, &intParams);
    ZrInferredTypeFree(state, &doubleType);
    ZrInferredTypeFree(state, &boolType);
    ZrInferredTypeFree(state, &intType);
    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Function overload resolution",
              "Testing that function-call inference resolves to the uniquely best overload instead of first-name lookup");

    ZrInferredTypeInit(state, &intType, ZR_VALUE_TYPE_INT64);
    ZrInferredTypeInit(state, &boolType, ZR_VALUE_TYPE_BOOL);
    ZrInferredTypeInit(state, &doubleType, ZR_VALUE_TYPE_DOUBLE);
    ZrArrayInit(state, &intParams, sizeof(SZrInferredType), 1);
    ZrArrayInit(state, &doubleParams, sizeof(SZrInferredType), 1);
    ZrArrayPush(state, &intParams, &intType);
    ZrArrayPush(state, &doubleParams, &doubleType);

    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterVariable(
        state, cs->typeEnv, ZrStringCreate(state, "value", 5), &doubleType));
    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterFunction(
        state, cs->typeEnv, ZrStringCreate(state, "pick", 4), &intType, &intParams));
    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterFunction(
        state, cs->typeEnv, ZrStringCreate(state, "pick", 4), &boolType, &doubleParams));

    {
        const char *source = "var result = pick(value);";
        SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
        SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);
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
        ZrInferredTypeInit(state, &result, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(infer_expression_type(cs, expr, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
        ZrInferredTypeFree(state, &result);
        ZrParserFreeAst(state, ast);
    }

    ZrArrayFree(state, &doubleParams);
    ZrArrayFree(state, &intParams);
    ZrInferredTypeFree(state, &doubleType);
    ZrInferredTypeFree(state, &boolType);
    ZrInferredTypeFree(state, &intType);
    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Generic type conversion",
              "Testing that generic AST types produce canonical inferred types and semantic generic-instance records");

    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterType(
        state, cs->typeEnv, ZrStringCreate(state, "Box", 3)));

    {
        const char *source = "makeBox(value: int): Box<int> { return value; }";
        SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
        SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);
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

        ZrInferredTypeInit(state, &convertedType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(convert_ast_type_to_inferred_type(cs, funcDecl->returnType, &convertedType));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, convertedType.baseType);
        TEST_ASSERT_NOT_NULL(convertedType.typeName);
        TEST_ASSERT_EQUAL_STRING("Box<int>", ZrStringGetNativeString(convertedType.typeName));
        TEST_ASSERT_EQUAL_UINT32(1, (TUInt32)convertedType.elementTypes.length);

        genericArg = (SZrInferredType *)ZrArrayGet(&convertedType.elementTypes, 0);
        TEST_ASSERT_NOT_NULL(genericArg);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, genericArg->baseType);

        genericRecord = findSemanticTypeRecord(cs->semanticContext,
                                               "Box<int>",
                                               ZR_SEMANTIC_TYPE_KIND_GENERIC_INSTANCE);
        TEST_ASSERT_NOT_NULL(genericRecord);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, genericRecord->baseType);

        ZrInferredTypeFree(state, &convertedType);
        ZrParserFreeAst(state, ast);
    }

    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Ownership-qualified type conversion",
              "Testing that unique/shared/weak wrappers survive AST->inferred-type conversion and semantic registration");

    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterType(
        state, cs->typeEnv, ZrStringCreate(state, "Resource", 8)));
    TEST_ASSERT_TRUE(ZrTypeEnvironmentRegisterType(
        state, cs->typeEnv, ZrStringCreate(state, "Box", 3)));

    {
        const char *source =
            "var owned: unique<Resource>;"
            "var borrowed: shared<Box<int>>;"
            "var weakRef: weak<Resource>;";
        SZrString *sourceName = ZrStringCreate(state, "ownership_types_test.zr", 23);
        SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);
        SZrAstNode *ownedDecl;
        SZrAstNode *borrowedDecl;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_TRUE(ast->data.script.statements->count >= 2);

        ownedDecl = ast->data.script.statements->nodes[0];
        borrowedDecl = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(ownedDecl);
        TEST_ASSERT_NOT_NULL(borrowedDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, ownedDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, borrowedDecl->type);
        TEST_ASSERT_NOT_NULL(ownedDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(borrowedDecl->data.variableDeclaration.typeInfo);

        ZrInferredTypeInit(state, &convertedType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(convert_ast_type_to_inferred_type(
            cs, ownedDecl->data.variableDeclaration.typeInfo, &convertedType));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, convertedType.baseType);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              convertedType.ownershipQualifier);
        TEST_ASSERT_NOT_NULL(convertedType.typeName);
        TEST_ASSERT_EQUAL_STRING("Resource", ZrStringGetNativeString(convertedType.typeName));
        ZrInferredTypeFree(state, &convertedType);

        ZrInferredTypeInit(state, &convertedType, ZR_VALUE_TYPE_OBJECT);
        TEST_ASSERT_TRUE(convert_ast_type_to_inferred_type(
            cs, borrowedDecl->data.variableDeclaration.typeInfo, &convertedType));
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED,
                              convertedType.ownershipQualifier);
        TEST_ASSERT_NOT_NULL(convertedType.typeName);
        TEST_ASSERT_EQUAL_STRING("Box<int>", ZrStringGetNativeString(convertedType.typeName));
        TEST_ASSERT_EQUAL_UINT32(1, (TUInt32)convertedType.elementTypes.length);

        ownedRecord = findSemanticTypeRecord(cs->semanticContext,
                                             "Resource",
                                             ZR_SEMANTIC_TYPE_KIND_REFERENCE);
        TEST_ASSERT_NOT_NULL(ownedRecord);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              ownedRecord->ownershipQualifier);

        ZrInferredTypeFree(state, &convertedType);
        ZrParserFreeAst(state, ast);
    }

    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Float literal type inference", "Testing type inference for float literal: 1.5");

    // 解析浮点数表达式
    const char *source = "1.5;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse float literal");
        destroyTestCompilerState(cs);
        destroyTestState(state);
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
        ZrParserFreeAst(state, ast);
        destroyTestCompilerState(cs);
        destroyTestState(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TBool success = infer_literal_type(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, result.baseType);

    ZrInferredTypeFree(state, &result);
    ZrParserFreeAst(state, ast);
    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("String literal type inference", "Testing type inference for string literal: \"hello\"");

    // 解析字符串表达式
    const char *source = "\"hello\";";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse string literal");
        destroyTestCompilerState(cs);
        destroyTestState(state);
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
        ZrParserFreeAst(state, ast);
        destroyTestCompilerState(cs);
        destroyTestState(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TBool success = infer_literal_type(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.baseType);

    ZrInferredTypeFree(state, &result);
    ZrParserFreeAst(state, ast);
    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Boolean literal type inference", "Testing type inference for boolean literal: true");

    // 解析布尔表达式
    const char *source = "true;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse boolean literal");
        destroyTestCompilerState(cs);
        destroyTestState(state);
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
        ZrParserFreeAst(state, ast);
        destroyTestCompilerState(cs);
        destroyTestState(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TBool success = infer_literal_type(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);

    ZrInferredTypeFree(state, &result);
    ZrParserFreeAst(state, ast);
    destroyTestCompilerState(cs);
    destroyTestState(state);

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

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Binary expression type inference", "Testing type inference for binary expression: 1 + 2");

    // 解析二元表达式
    const char *source = "1 + 2;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse binary expression");
        destroyTestCompilerState(cs);
        destroyTestState(state);
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
        ZrParserFreeAst(state, ast);
        destroyTestCompilerState(cs);
        destroyTestState(state);
        return;
    }

    // 推断类型
    SZrInferredType result;
    TBool success = infer_binary_expression_type(cs, expr, &result);

    TEST_ASSERT_TRUE(success);
    // 整数相加应该返回整数类型
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);

    ZrInferredTypeFree(state, &result);
    ZrParserFreeAst(state, ast);
    destroyTestCompilerState(cs);
    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_parser_supports_ownership_types_and_template_strings(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Parser - Ownership Types And Template Strings";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Ownership-qualified type parsing and template string parsing",
              "Testing unique/shared/weak type annotations and backtick template strings with interpolation");

    const char *source =
        "var owned: unique<Resource>;"
        "var borrowed: shared<Box<int>>;"
        "var weakRef: weak<Resource>;"
        "var message = `hello ${1}`;";
    SZrString *sourceName = ZrStringCreate(state, "ownership_template_test.zr", 26);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 4);

    {
        SZrAstNode *ownedDecl = ast->data.script.statements->nodes[0];
        SZrAstNode *borrowedDecl = ast->data.script.statements->nodes[1];
        SZrAstNode *weakDecl = ast->data.script.statements->nodes[2];
        SZrAstNode *messageDecl = ast->data.script.statements->nodes[3];
        SZrAstNode *templateLiteral;

        TEST_ASSERT_NOT_NULL(ownedDecl);
        TEST_ASSERT_NOT_NULL(borrowedDecl);
        TEST_ASSERT_NOT_NULL(weakDecl);
        TEST_ASSERT_NOT_NULL(messageDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, ownedDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, borrowedDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, weakDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, messageDecl->type);
        TEST_ASSERT_NOT_NULL(ownedDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(borrowedDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(weakDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              ownedDecl->data.variableDeclaration.typeInfo->ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED,
                              borrowedDecl->data.variableDeclaration.typeInfo->ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_WEAK,
                              weakDecl->data.variableDeclaration.typeInfo->ownershipQualifier);

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

    ZrParserFreeAst(state, ast);
    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_using_statement_compilation_records_cleanup_plan(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Semantic - Using Statement Cleanup Plan";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Using statement semantic plan",
              "Testing that compiling a using statement appends deterministic cleanup metadata");

    {
        const char *source = "using resource;";
        SZrString *sourceName = ZrStringCreate(state, "using_cleanup_test.zr", 21);
        SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);
        SZrAstNode *usingStmt;
        const SZrDeterministicCleanupStep *step;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        usingStmt = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(usingStmt);
        TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, usingStmt->type);

        cs->currentFunction = ZrFunctionNew(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_statement(cs, usingStmt);

        TEST_ASSERT_FALSE(cs->hasError);
        TEST_ASSERT_EQUAL_INT(1, (int)cs->semanticContext->cleanupPlan.length);

        step = (const SZrDeterministicCleanupStep *)ZrArrayGet(&cs->semanticContext->cleanupPlan, 0);
        TEST_ASSERT_NOT_NULL(step);
        TEST_ASSERT_TRUE(step->regionId > 0);
        TEST_ASSERT_TRUE(step->symbolId > 0);
        TEST_ASSERT_TRUE(step->callsClose);
        TEST_ASSERT_TRUE(step->callsDestructor);

        ZrFunctionFree(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParserFreeAst(state, ast);
    }

    destroyTestCompilerState(cs);
    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_template_string_compilation_records_semantic_segments(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Semantic - Template String Segments";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);

    TEST_INFO("Template string semantic segments",
              "Testing that template-string compilation stores ordered static and interpolation segments");

    {
        const char *source = "`hello ${1}`;";
        SZrString *sourceName = ZrStringCreate(state, "template_segments_test.zr", 25);
        SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);
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

        cs->currentFunction = ZrFunctionNew(state);
        TEST_ASSERT_NOT_NULL(cs->currentFunction);

        compile_expression(cs, templateLiteral);

        TEST_ASSERT_FALSE(cs->hasError);
        TEST_ASSERT_EQUAL_INT(3, (int)cs->semanticContext->templateSegments.length);

        first = (const SZrTemplateSegment *)ZrArrayGet(&cs->semanticContext->templateSegments, 0);
        second = (const SZrTemplateSegment *)ZrArrayGet(&cs->semanticContext->templateSegments, 1);
        third = (const SZrTemplateSegment *)ZrArrayGet(&cs->semanticContext->templateSegments, 2);

        TEST_ASSERT_NOT_NULL(first);
        TEST_ASSERT_NOT_NULL(second);
        TEST_ASSERT_NOT_NULL(third);

        TEST_ASSERT_FALSE(first->isInterpolation);
        TEST_ASSERT_NOT_NULL(first->staticText);
        TEST_ASSERT_EQUAL_STRING("hello ", ZrStringGetNativeString(first->staticText));

        TEST_ASSERT_TRUE(second->isInterpolation);
        TEST_ASSERT_NOT_NULL(second->expression);
        TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, second->expression->type);

        TEST_ASSERT_FALSE(third->isInterpolation);
        TEST_ASSERT_NOT_NULL(third->staticText);
        TEST_ASSERT_EQUAL_STRING("", ZrStringGetNativeString(third->staticText));

        ZrFunctionFree(state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
        ZrParserFreeAst(state, ast);
    }

    destroyTestCompilerState(cs);
    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

void test_type_inference_template_string_literal_is_string(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Type Inference - Template String Literal";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    SZrCompilerState *cs = createTestCompilerState(state);
    TEST_ASSERT_NOT_NULL(cs);

    TEST_INFO("Template string literal type inference", "Testing type inference for `hello ${1}`");

    {
        const char *source = "`hello ${1}`;";
        SZrString *sourceName = ZrStringCreate(state, "template_string_test.zr", 23);
        SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);
        SZrAstNode *expr = ZR_NULL;
        SZrInferredType result;
        TBool success;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_TRUE(ast->data.script.statements->count > 0);
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT,
                              ast->data.script.statements->nodes[0]->type);

        expr = ast->data.script.statements->nodes[0]->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TEMPLATE_STRING_LITERAL, expr->type);

        ZrInferredTypeInit(state, &result, ZR_VALUE_TYPE_OBJECT);
        success = infer_expression_type(cs, expr, &result);

        TEST_ASSERT_TRUE(success);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.baseType);

        ZrInferredTypeFree(state, &result);
        ZrParserFreeAst(state, ast);
    }

    destroyTestCompilerState(cs);
    destroyTestState(state);

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
    RUN_TEST(test_parser_supports_ownership_types_and_template_strings);
    RUN_TEST(test_using_statement_compilation_records_cleanup_plan);
    RUN_TEST(test_template_string_compilation_records_semantic_segments);
    RUN_TEST(test_type_inference_float_literal);
    RUN_TEST(test_type_inference_string_literal);
    RUN_TEST(test_type_inference_template_string_literal_is_string);
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
