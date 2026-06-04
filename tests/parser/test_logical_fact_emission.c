#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

static SZrAstNode *first_expression_statement_expression(SZrAstNode *ast) {
    SZrAstNode *statement;

    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count == 0) {
        return ZR_NULL;
    }

    statement = ast->data.script.statements->nodes[0];
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static SZrFileRange expression_position_at_offset(SZrAstNode *expr, TZrSize offset) {
    SZrFileRange position = expr->location;

    position.start.offset = offset;
    position.end.offset = offset;
    position.start.column = (TZrInt32)offset;
    position.end.column = (TZrInt32)offset;
    return position;
}

static void assert_constant_comparison_records_logical_fact(
        const char *source,
        const char *sourceNameText,
        TZrSize operatorOffset,
        EZrSemanticLogicalFactKind expectedKind,
        TZrBool expectedValue) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticLogicalFact *logicalAtOperator;

    sourceName = ZrCore_String_Create(g_state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext, expr);
    logicalAtOperator = ZrParser_SemanticFacts_FindLogicalAtPosition(
            cs->semanticContext,
            expression_position_at_offset(expr, operatorOffset));

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_EXPRESSION_FACT_BINARY, expressionFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_KIND_BOOL, expressionFact->valueKind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, expressionFact->inferredType.baseType);
    TEST_ASSERT_TRUE(expressionFact->hasConstant);
    if (expectedValue) {
        TEST_ASSERT_TRUE(expressionFact->constantValue.boolValue);
    } else {
        TEST_ASSERT_FALSE(expressionFact->constantValue.boolValue);
    }

    TEST_ASSERT_NOT_NULL(logicalFact);
    TEST_ASSERT_EQUAL_INT(expectedKind, logicalFact->kind);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_FACT_EXACT, logicalFact->exactness);
    TEST_ASSERT_TRUE(logicalFact->hasKnownValue);
    if (expectedValue) {
        TEST_ASSERT_TRUE(logicalFact->knownValue);
    } else {
        TEST_ASSERT_FALSE(logicalFact->knownValue);
    }
    TEST_ASSERT_EQUAL_PTR(expr, logicalFact->node);
    TEST_ASSERT_EQUAL_UINT64(expr->location.start.offset, logicalFact->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64(expr->location.end.offset, logicalFact->range.end.offset);
    TEST_ASSERT_EQUAL_PTR(logicalFact, logicalAtOperator);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_constant_integer_comparison_records_true_logical_fact(void) {
    assert_constant_comparison_records_logical_fact(
            "1 < 2;",
            "constant_comparison_true_fact_test.zr",
            2,
            ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE,
            ZR_TRUE);
}

static void test_constant_integer_comparison_records_false_logical_fact(void) {
    assert_constant_comparison_records_logical_fact(
            "3 <= 2;",
            "constant_comparison_false_fact_test.zr",
            2,
            ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE,
            ZR_FALSE);
}

static void test_unary_comparison_records_false_logical_constant(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticLogicalFact *logicalFact;
    const char *source = "!(1 < 2);";

    sourceName = ZrCore_String_Create(g_state,
                                      "unary_comparison_logical_fact_test.zr",
                                      strlen("unary_comparison_logical_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNARY_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext, expr);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_KIND_BOOL, expressionFact->valueKind);
    TEST_ASSERT_TRUE(expressionFact->hasConstant);
    TEST_ASSERT_FALSE(expressionFact->constantValue.boolValue);
    TEST_ASSERT_NOT_NULL(logicalFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE, logicalFact->kind);
    TEST_ASSERT_TRUE(logicalFact->hasKnownValue);
    TEST_ASSERT_FALSE(logicalFact->knownValue);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_comparison_logical_and_records_true_logical_constant(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *expr;
    SZrInferredType result;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticLogicalFact *logicalAtOperator;
    const char *source = "(1 < 2) && (3 < 4);";

    sourceName = ZrCore_String_Create(g_state,
                                      "comparison_and_logical_fact_test.zr",
                                      strlen("comparison_and_logical_fact_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    expr = first_expression_statement_expression(ast);

    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, expr->type);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expr, &result));
    expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(cs->semanticContext, expr);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext, expr);
    logicalAtOperator = ZrParser_SemanticFacts_FindLogicalAtPosition(
            cs->semanticContext,
            expression_position_at_offset(expr, 8));

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.baseType);
    TEST_ASSERT_NOT_NULL(expressionFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_VALUE_KIND_BOOL, expressionFact->valueKind);
    TEST_ASSERT_TRUE(expressionFact->hasConstant);
    TEST_ASSERT_TRUE(expressionFact->constantValue.boolValue);
    TEST_ASSERT_NOT_NULL(logicalFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE, logicalFact->kind);
    TEST_ASSERT_TRUE(logicalFact->hasKnownValue);
    TEST_ASSERT_TRUE(logicalFact->knownValue);
    TEST_ASSERT_EQUAL_PTR(logicalFact, logicalAtOperator);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_constant_integer_comparison_records_true_logical_fact);
    RUN_TEST(test_constant_integer_comparison_records_false_logical_fact);
    RUN_TEST(test_unary_comparison_records_false_logical_constant);
    RUN_TEST(test_comparison_logical_and_records_true_logical_constant);
    return UNITY_END();
}
