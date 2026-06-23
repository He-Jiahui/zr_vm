#include "unity.h"

#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_type_conf.h"
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

static void register_int64_array_variable(SZrCompilerState *cs, const char *name) {
    SZrInferredType arrayType;
    SZrInferredType elementType;

    ZrParser_InferredType_Init(g_state, &arrayType, ZR_VALUE_TYPE_ARRAY);
    ZrParser_InferredType_Init(g_state, &elementType, ZR_VALUE_TYPE_INT64);
    ZrCore_Array_Init(g_state, &arrayType.elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &arrayType.elementTypes, &elementType);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state,
            cs->typeEnv,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
            &arrayType));
    ZrParser_InferredType_Free(g_state, &elementType);
    ZrParser_InferredType_Free(g_state, &arrayType);
}

static void register_int64_range_array_variable(SZrCompilerState *cs,
                                                 const char *name,
                                                 TZrInt64 minValue,
                                                 TZrInt64 maxValue) {
    SZrInferredType arrayType;
    SZrInferredType elementType;

    ZrParser_InferredType_Init(g_state, &arrayType, ZR_VALUE_TYPE_ARRAY);
    ZrParser_InferredType_Init(g_state, &elementType, ZR_VALUE_TYPE_INT64);
    elementType.hasRangeConstraint = ZR_TRUE;
    elementType.minValue = minValue;
    elementType.maxValue = maxValue;
    ZrCore_Array_Init(g_state, &arrayType.elementTypes, sizeof(SZrInferredType), 1);
    ZrCore_Array_Push(g_state, &arrayType.elementTypes, &elementType);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
            g_state,
            cs->typeEnv,
            ZrCore_String_Create(g_state, (TZrNativeString)name, strlen(name)),
            &arrayType));
    ZrParser_InferredType_Free(g_state, &elementType);
    ZrParser_InferredType_Free(g_state, &arrayType);
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

static void test_while_assignment_joins_pre_loop_range_for_following_expression(void) {
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
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_branch_assignment_dataflow_test.zr",
            strlen("numeric_while_branch_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(6, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_multi_statement_assignment_joins_pre_loop_range_for_following_expression(void) {
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
            "    flag;\n"
            "    narrowed = 10;\n"
            "    flag;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_multi_statement_assignment_dataflow_test.zr",
            strlen("numeric_while_multi_statement_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(6, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_nested_if_assignment_joins_pre_loop_range_for_following_expression(void) {
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
            "    if (inner) {\n"
            "        narrowed = 1;\n"
            "    } else {\n"
            "        narrowed = 10;\n"
            "    }\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_nested_if_assignment_dataflow_test.zr",
            strlen("numeric_while_nested_if_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_bool_variable(cs, "inner");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_multi_target_assignments_join_cross_target_rhs_dependent_range(void) {
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
            "    low = 1;\n"
            "    high = low + 10;\n"
            "}\n"
            "high + low;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_multi_target_assignment_dataflow_test.zr",
            strlen("numeric_while_multi_target_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "low", 5, 5);
    register_int64_range_variable(cs, "high", 20, 20);

    ZrParser_InferredType_Init(g_state, &whileType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);

    TEST_ASSERT_NOT_NULL(whileStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_WHILE_LOOP, whileStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, whileStatement, &whileType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(12, result.minValue);
    TEST_ASSERT_EQUAL_INT64(25, result.maxValue);

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(12, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(25, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &whileType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_while_self_dependent_increment_assignment_widens_upper_bound(void) {
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
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_assignment_dataflow_test.zr",
            strlen("numeric_while_self_dependent_assignment_dataflow_test.zr"));
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

static void assert_while_self_dependent_delta_assignment_widens_upper_bound(
        TZrInt64 stepMinValue,
        TZrInt64 stepMaxValue,
        const char *sourceNameText) {
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
            (TZrNativeString)sourceNameText,
            strlen(sourceNameText));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    whileStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);
    register_int64_range_variable(cs, "step", stepMinValue, stepMaxValue);

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

static void test_while_self_dependent_singleton_delta_assignment_widens_upper_bound(void) {
    assert_while_self_dependent_delta_assignment_widens_upper_bound(
            1,
            1,
            "numeric_while_self_dependent_singleton_delta_assignment_dataflow_test.zr");
}

static void test_while_self_dependent_positive_range_delta_widens_upper_bound(void) {
    assert_while_self_dependent_delta_assignment_widens_upper_bound(
            1,
            3,
            "numeric_while_self_dependent_positive_range_delta_assignment_dataflow_test.zr");
}

static void test_while_self_dependent_decrement_assignment_widens_lower_bound(void) {
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
            "    narrowed = narrowed - 1;\n"
            "}\n"
            "narrowed + 0;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_while_self_dependent_decrement_assignment_dataflow_test.zr",
            strlen("numeric_while_self_dependent_decrement_assignment_dataflow_test.zr"));
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

static void test_for_assignment_joins_pre_loop_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (; flag; ) {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_assignment_dataflow_test.zr",
            strlen("numeric_for_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(6, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_for_init_assignment_joins_init_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (narrowed = 1; flag; ) {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_init_assignment_dataflow_test.zr",
            strlen("numeric_for_init_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_for_false_condition_init_assignment_keeps_init_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (narrowed = 1; false; ) {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_false_condition_init_assignment_dataflow_test.zr",
            strlen("numeric_for_false_condition_init_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(2, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_for_step_assignment_joins_step_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (; flag; narrowed = 10) {\n"
            "    flag;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_step_assignment_dataflow_test.zr",
            strlen("numeric_for_step_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(6, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_for_non_assignment_step_joins_body_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (; flag; flag) {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_non_assignment_step_dataflow_test.zr",
            strlen("numeric_for_non_assignment_step_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(6, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_for_var_init_assignment_uses_init_binding_for_body_replay(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *forStatement;
    SZrAstNode *finalExpression;
    SZrInferredType forType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (var step: int = 10; flag; ) {\n"
            "    narrowed = step;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_for_var_init_assignment_dataflow_test.zr",
            strlen("numeric_for_var_init_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    forStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &forType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(forStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, forStatement, &forType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(6, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &forType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_foreach_assignment_joins_pre_loop_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *foreachStatement;
    SZrAstNode *finalExpression;
    SZrInferredType foreachType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (var item in items) {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_foreach_assignment_dataflow_test.zr",
            strlen("numeric_foreach_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    foreachStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_int64_array_variable(cs, "items");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &foreachType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(foreachStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOREACH_LOOP, foreachStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, foreachStatement, &foreachType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(6, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &foreachType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_foreach_item_assignment_joins_item_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *foreachStatement;
    SZrAstNode *finalExpression;
    SZrInferredType foreachType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "for (var item in items) {\n"
            "    narrowed = item;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_foreach_item_assignment_dataflow_test.zr",
            strlen("numeric_foreach_item_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    foreachStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));
    register_int64_range_array_variable(cs, "items", 2, 4);
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &foreachType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(foreachStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOREACH_LOOP, foreachStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, foreachStatement, &foreachType));
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(3, result.minValue);
    TEST_ASSERT_EQUAL_INT64(6, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(6, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &foreachType);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_while_assignment_joins_pre_loop_range_for_following_expression);
    RUN_TEST(test_while_multi_statement_assignment_joins_pre_loop_range_for_following_expression);
    RUN_TEST(test_while_nested_if_assignment_joins_pre_loop_range_for_following_expression);
    RUN_TEST(test_while_multi_target_assignments_join_cross_target_rhs_dependent_range);
    RUN_TEST(test_while_self_dependent_increment_assignment_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_singleton_delta_assignment_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_positive_range_delta_widens_upper_bound);
    RUN_TEST(test_while_self_dependent_decrement_assignment_widens_lower_bound);
    RUN_TEST(test_for_assignment_joins_pre_loop_range_for_following_expression);
    RUN_TEST(test_for_init_assignment_joins_init_range_for_following_expression);
    RUN_TEST(test_for_false_condition_init_assignment_keeps_init_range);
    RUN_TEST(test_for_step_assignment_joins_step_range_for_following_expression);
    RUN_TEST(test_for_non_assignment_step_joins_body_range_for_following_expression);
    RUN_TEST(test_for_var_init_assignment_uses_init_binding_for_body_replay);
    RUN_TEST(test_foreach_assignment_joins_pre_loop_range_for_following_expression);
    RUN_TEST(test_foreach_item_assignment_joins_item_range_for_following_expression);
    return UNITY_END();
}
