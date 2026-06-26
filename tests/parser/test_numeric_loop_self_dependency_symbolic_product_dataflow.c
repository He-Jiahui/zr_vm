#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
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

static void register_bool_variable(SZrCompilerState *cs, const char *name) {
    SZrInferredType type;

    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_BOOL);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state,
            cs->typeEnv,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
            &type));
    ZrParser_InferredType_Free(g_state, &type);
}

static void register_int64_range_variable(SZrCompilerState *cs,
                                           const char *name,
                                           TZrInt64 minValue,
                                           TZrInt64 maxValue) {
    SZrInferredType type;

    ZrParser_InferredType_Init(g_state, &type, ZR_VALUE_TYPE_INT64);
    type.hasRangeConstraint = ZR_TRUE;
    type.minValue = minValue;
    type.maxValue = maxValue;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state,
            cs->typeEnv,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
            &type));
    ZrParser_InferredType_Free(g_state, &type);
}

static SZrAstNode *statement_at(SZrAstNode *ast, TZrSize index) {
    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count <= index) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[index];
}

static SZrAstNode *expression_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static void assert_int64_range_result_and_fact(SZrCompilerState *cs,
                                               SZrAstNode *expression,
                                               SZrInferredType *result,
                                               TZrInt64 expectedMin,
                                               TZrInt64 expectedMax) {
    const SZrSemanticNumericFact *numericFact;

    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expression, result));
    TEST_ASSERT_TRUE(result->hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(expectedMin, result->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, result->maxValue);
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, expression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(expectedMin, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);
}

static void
test_while_self_dependent_target_reading_symbolic_associative_commutative_product_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * scale));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - ((scale * step) * factor);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_associative_commutative_product_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_associative_commutative_product_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -2, 2);
    register_int64_range_variable(cs, "scale", -1, 1);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, 0, 7);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void
test_while_self_dependent_target_reading_symbolic_folded_constant_product_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (2 * 3)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - ((6 * step) * factor);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_folded_constant_product_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_folded_constant_product_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -1, 1);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, -1, 11);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void
test_while_self_dependent_target_reading_symbolic_unary_negative_product_factor_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + ((-step) * factor);\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - ((-1 * factor) * step);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_unary_negative_product_factor_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_unary_negative_product_factor_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -2, 2);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, 0, 7);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void
test_while_self_dependent_target_reading_symbolic_unary_positive_product_factor_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + ((+step) * factor);\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - (factor * step);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_unary_positive_product_factor_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_unary_positive_product_factor_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -2, 2);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, 0, 7);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void
test_while_self_dependent_target_reading_symbolic_double_negative_product_factor_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + ((-(-step)) * factor);\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - (factor * step);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_double_negative_product_factor_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_double_negative_product_factor_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -2, 2);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, 0, 7);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void
test_while_self_dependent_target_reading_symbolic_divided_constant_product_factor_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (6 / 2)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - ((3 * factor) * step);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_divided_constant_product_factor_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_divided_constant_product_factor_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -2, 2);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, -1, 11);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void
test_while_self_dependent_target_reading_symbolic_modulo_constant_product_factor_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (7 % 4)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - ((3 * factor) * step);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_modulo_constant_product_factor_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_modulo_constant_product_factor_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -2, 2);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, -1, 11);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void
test_while_self_dependent_target_reading_symbolic_additive_constant_product_factor_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (1 + 2)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - ((3 * factor) * step);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_additive_constant_product_factor_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_additive_constant_product_factor_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -2, 2);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, -1, 11);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void
test_while_self_dependent_target_reading_symbolic_subtractive_constant_product_factor_cancels_exactly(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *targetExpression;
    SZrAstNode *observerExpression;
    SZrInferredType whileType;
    SZrInferredType targetResult;
    SZrInferredType observerResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step * (factor * (5 - 2)));\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - ((3 * factor) * step);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_symbolic_subtractive_constant_product_factor_exact_cancel_dataflow_test.zr",
            strlen("numeric_while_self_dependent_symbolic_subtractive_constant_product_factor_exact_cancel_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    targetExpression = expression_statement_expression(statement_at(ast, 1));
    observerExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", -1, 1);
    register_int64_range_variable(cs, "factor", -2, 2);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &targetResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &observerResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(targetExpression);
    TEST_ASSERT_NOT_NULL(observerExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    assert_int64_range_result_and_fact(cs, targetExpression, &targetResult, 5, 5);
    assert_int64_range_result_and_fact(cs, observerExpression, &observerResult, -1, 11);

    ZrParser_InferredType_Free(g_state, &observerResult);
    ZrParser_InferredType_Free(g_state, &targetResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_associative_commutative_product_cancels_exactly);
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_folded_constant_product_cancels_exactly);
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_unary_negative_product_factor_cancels_exactly);
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_unary_positive_product_factor_cancels_exactly);
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_double_negative_product_factor_cancels_exactly);
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_divided_constant_product_factor_cancels_exactly);
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_modulo_constant_product_factor_cancels_exactly);
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_additive_constant_product_factor_cancels_exactly);
    RUN_TEST(
            test_while_self_dependent_target_reading_symbolic_subtractive_constant_product_factor_cancels_exactly);
    return UNITY_END();
}
