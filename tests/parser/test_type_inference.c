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
#include "zr_vm_parser/type_inference.h"

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

// 测试初始化和清理
void setUp(void) {}

void tearDown(void) {}

// ==================== 类型推断测试 ====================

// 测试整数字面量类型推断
void test_type_inference_integer_literal(void) {
    SZrTestTimer timer;
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

// 测试浮点数字面量类型推断
void test_type_inference_float_literal(void) {
    SZrTestTimer timer;
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
    SZrTestTimer timer;
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
    SZrTestTimer timer;
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
    SZrTestTimer timer;
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
    RUN_TEST(test_type_inference_float_literal);
    RUN_TEST(test_type_inference_string_literal);
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
