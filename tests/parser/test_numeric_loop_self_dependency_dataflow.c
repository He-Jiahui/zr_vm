#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_type_conf.h"
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

static void test_while_self_dependent_positive_expression_delta_widens_upper_bound(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + (step + 1);\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_expression_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_expression_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", 1, 3);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_zero_inclusive_positive_range_delta_widens_upper_bound(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_zero_inclusive_positive_range_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_zero_inclusive_positive_range_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", 0, 3);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_zero_inclusive_negative_range_delta_widens_lower_bound(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_zero_inclusive_negative_range_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_zero_inclusive_negative_range_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", -3, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MIN, result.minValue);
    TEST_ASSERT_EQUAL_INT64(5, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MIN, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_sign_crossing_delta_widens_full_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_sign_crossing_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_sign_crossing_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", -3, 3);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MIN, result.minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MIN, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_same_loop_written_delta_widens_upper_bound(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    step = 2;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_same_loop_written_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_same_loop_written_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_sequence_extends_same_loop_written_delta(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    step = 2;\n"
            "    narrowed = narrowed + 1;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_sequence_same_loop_written_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_sequence_same_loop_written_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_multiple_same_loop_written_delta_widens_upper_bound(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    step = 2;\n"
            "    narrowed = narrowed + step;\n"
            "    narrowed = narrowed + step;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_multiple_same_loop_written_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_multiple_same_loop_written_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_same_loop_written_expression_delta_widens_upper_bound(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    step = 2;\n"
            "    narrowed = narrowed + (step + 1);\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_same_loop_written_expression_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_same_loop_written_expression_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_zero_only_delta_keeps_other_assignment_join(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + 0;\n"
            "    other = 10;\n"
            "}\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_zero_only_delta_keeps_other_assignment_dataflow_test.zr",
            strlen("numeric_while_self_dependent_zero_only_delta_keeps_other_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(0, result.minValue);
    TEST_ASSERT_EQUAL_INT64(10, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(0, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(10, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_zero_only_then_positive_delta_widens_upper_bound(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + 0;\n"
            "    narrowed = narrowed + 1;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_zero_only_then_positive_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_zero_only_then_positive_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(ZR_TYPE_RANGE_INT64_MAX, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_positive_then_negative_delta_keeps_net_zero_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + 1;\n"
            "    narrowed = narrowed - 1;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_positive_then_negative_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_positive_then_negative_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(5, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_interleaved_positive_then_negative_delta_keeps_net_zero_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *finalExpression;
    SZrInferredType whileType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + 1;\n"
            "    other = 10;\n"
            "    narrowed = narrowed - 1;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_interleaved_positive_then_negative_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_interleaved_positive_then_negative_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(5, result.minValue);
    TEST_ASSERT_EQUAL_INT64(5, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(5, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_target_reading_interleaved_net_zero_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *narrowedExpression;
    SZrAstNode *otherExpression;
    SZrInferredType whileType;
    SZrInferredType narrowedResult;
    SZrInferredType otherResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + 1;\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - 1;\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_target_reading_interleaved_net_zero_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_target_reading_interleaved_net_zero_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    narrowedExpression = expression_statement_expression(statement_at(ast, 1));
    otherExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &narrowedResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &otherResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(narrowedExpression);
    TEST_ASSERT_NOT_NULL(otherExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));

    assert_int64_range_result_and_fact(cs, narrowedExpression, &narrowedResult, 5, 5);
    assert_int64_range_result_and_fact(cs, otherExpression, &otherResult, 0, 6);

    ZrParser_InferredType_Free(g_state, &otherResult);
    ZrParser_InferredType_Free(g_state, &narrowedResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_target_reading_replay_resolved_net_zero_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *narrowedExpression;
    SZrAstNode *otherExpression;
    SZrInferredType whileType;
    SZrInferredType narrowedResult;
    SZrInferredType otherResult;
    const char *source =
            "while (flag) {\n"
            "    step = 1;\n"
            "    narrowed = narrowed + step;\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - step;\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_target_reading_replay_resolved_net_zero_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_target_reading_replay_resolved_net_zero_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    narrowedExpression = expression_statement_expression(statement_at(ast, 1));
    otherExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &narrowedResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &otherResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(narrowedExpression);
    TEST_ASSERT_NOT_NULL(otherExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));

    assert_int64_range_result_and_fact(cs, narrowedExpression, &narrowedResult, 5, 5);
    assert_int64_range_result_and_fact(cs, otherExpression, &otherResult, 0, 6);

    ZrParser_InferredType_Free(g_state, &otherResult);
    ZrParser_InferredType_Free(g_state, &narrowedResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_target_reading_replay_resolved_net_negative_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *narrowedExpression;
    SZrAstNode *otherExpression;
    SZrInferredType whileType;
    SZrInferredType narrowedResult;
    SZrInferredType otherResult;
    const char *source =
            "while (flag) {\n"
            "    step = 1;\n"
            "    narrowed = narrowed + step;\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - (step + 1);\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_target_reading_replay_resolved_net_negative_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_target_reading_replay_resolved_net_negative_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    narrowedExpression = expression_statement_expression(statement_at(ast, 1));
    otherExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);
    register_int64_range_variable(cs, "step", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &narrowedResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &otherResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(narrowedExpression);
    TEST_ASSERT_NOT_NULL(otherExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));

    assert_int64_range_result_and_fact(
            cs,
            narrowedExpression,
            &narrowedResult,
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    assert_int64_range_result_and_fact(
            cs,
            otherExpression,
            &otherResult,
            ZR_TYPE_RANGE_INT64_MIN,
            6);

    ZrParser_InferredType_Free(g_state, &otherResult);
    ZrParser_InferredType_Free(g_state, &narrowedResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_target_reading_interleaved_net_negative_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *whileStatement;
    SZrAstNode *narrowedExpression;
    SZrAstNode *otherExpression;
    SZrInferredType whileType;
    SZrInferredType narrowedResult;
    SZrInferredType otherResult;
    const char *source =
            "while (flag) {\n"
            "    narrowed = narrowed + 1;\n"
            "    other = narrowed;\n"
            "    narrowed = narrowed - 2;\n"
            "}\n"
            "narrowed + 0;\n"
            "other + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_target_reading_interleaved_net_negative_delta_dataflow_test.zr",
            strlen("numeric_while_self_dependent_target_reading_interleaved_net_negative_delta_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    narrowedExpression = expression_statement_expression(statement_at(ast, 1));
    otherExpression = expression_statement_expression(statement_at(ast, 2));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "other", 0, 0);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &narrowedResult, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &otherResult, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(narrowedExpression);
    TEST_ASSERT_NOT_NULL(otherExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));

    assert_int64_range_result_and_fact(
            cs,
            narrowedExpression,
            &narrowedResult,
            ZR_TYPE_RANGE_INT64_MIN,
            5);
    assert_int64_range_result_and_fact(
            cs,
            otherExpression,
            &otherResult,
            ZR_TYPE_RANGE_INT64_MIN,
            6);

    ZrParser_InferredType_Free(g_state, &otherResult);
    ZrParser_InferredType_Free(g_state, &narrowedResult);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_while_self_dependent_positive_expression_delta_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_zero_inclusive_positive_range_delta_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_zero_inclusive_negative_range_delta_widens_lower_bound);
    RUN_TEST(test_while_self_dependent_sign_crossing_delta_widens_full_range);
    RUN_TEST(test_while_self_dependent_same_loop_written_delta_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_sequence_extends_same_loop_written_delta);
    RUN_TEST(test_while_self_dependent_multiple_same_loop_written_delta_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_same_loop_written_expression_delta_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_zero_only_delta_keeps_other_assignment_join);
    RUN_TEST(test_while_self_dependent_zero_only_then_positive_delta_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_positive_then_negative_delta_keeps_net_zero_range);
    RUN_TEST(test_while_self_dependent_interleaved_positive_then_negative_delta_keeps_net_zero_range);
    RUN_TEST(test_while_self_dependent_target_reading_interleaved_net_zero_range);
    RUN_TEST(test_while_self_dependent_target_reading_replay_resolved_net_zero_range);
    RUN_TEST(test_while_self_dependent_target_reading_replay_resolved_net_negative_range);
    RUN_TEST(test_while_self_dependent_target_reading_interleaved_net_negative_range);
    return UNITY_END();
}
