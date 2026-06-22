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

static void test_if_else_assignments_join_numeric_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "if (flag) {\n"
            "    narrowed = 1;\n"
            "} else {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "numeric_branch_assignment_dataflow_test.zr",
                                      strlen("numeric_branch_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 0, 0);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
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
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_if_else_assignments_preserve_segment_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const SZrNumericRangeSegment *segment;
    const char *source =
            "if (flag) {\n"
            "    narrowed = 1;\n"
            "} else {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_branch_assignment_segment_dataflow_test.zr",
            strlen("numeric_branch_assignment_segment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 0, 0);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_EQUAL_UINT64(2, result.rangeSegmentCount);
    segment = ZrParser_InferredType_RangeSegmentAt(&result, 0);
    TEST_ASSERT_NOT_NULL(segment);
    TEST_ASSERT_EQUAL_INT64(2, segment->minValue);
    TEST_ASSERT_EQUAL_INT64(2, segment->maxValue);
    segment = ZrParser_InferredType_RangeSegmentAt(&result, 1);
    TEST_ASSERT_NOT_NULL(segment);
    TEST_ASSERT_EQUAL_INT64(11, segment->minValue);
    TEST_ASSERT_EQUAL_INT64(11, segment->maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_EQUAL_UINT64(2, numericFact->rangeSegmentCount);
    segment = ZrParser_SemanticNumericFact_RangeSegmentAt(numericFact, 0);
    TEST_ASSERT_NOT_NULL(segment);
    TEST_ASSERT_EQUAL_INT64(2, segment->minValue);
    TEST_ASSERT_EQUAL_INT64(2, segment->maxValue);
    segment = ZrParser_SemanticNumericFact_RangeSegmentAt(numericFact, 1);
    TEST_ASSERT_NOT_NULL(segment);
    TEST_ASSERT_EQUAL_INT64(11, segment->minValue);
    TEST_ASSERT_EQUAL_INT64(11, segment->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_if_else_multi_statement_assignments_join_numeric_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "if (flag) {\n"
            "    flag;\n"
            "    narrowed = 1;\n"
            "} else {\n"
            "    flag;\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "numeric_multi_statement_branch_assignment_dataflow_test.zr",
                                      strlen("numeric_multi_statement_branch_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 0, 0);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
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
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_if_else_nonterminal_assignment_joins_numeric_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "if (flag) {\n"
            "    narrowed = 1;\n"
            "    flag;\n"
            "} else {\n"
            "    narrowed = 10;\n"
            "    flag;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "numeric_nonterminal_branch_assignment_dataflow_test.zr",
                                      strlen("numeric_nonterminal_branch_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 0, 0);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
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
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_if_else_same_target_sequential_assignments_join_rhs_dependent_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "if (flag) {\n"
            "    narrowed = 1;\n"
            "    narrowed = narrowed + 1;\n"
            "} else {\n"
            "    narrowed = 10;\n"
            "    narrowed = narrowed + 1;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_rhs_dependent_branch_assignment_dataflow_test.zr",
            strlen("numeric_rhs_dependent_branch_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 0, 0);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(3, result.minValue);
    TEST_ASSERT_EQUAL_INT64(12, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(12, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_if_else_multi_target_assignments_join_cross_target_rhs_dependent_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "if (flag) {\n"
            "    low = 1;\n"
            "    high = low + 1;\n"
            "} else {\n"
            "    low = 10;\n"
            "    high = low + 10;\n"
            "}\n"
            "high + low;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_multi_target_branch_assignment_dataflow_test.zr",
            strlen("numeric_multi_target_branch_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "low", 0, 0);
    register_int64_range_variable(cs, "high", 0, 0);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(3, result.minValue);
    TEST_ASSERT_EQUAL_INT64(30, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(3, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(30, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_if_else_nested_assignments_join_numeric_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "if (flag) {\n"
            "    if (inner) {\n"
            "        narrowed = 1;\n"
            "    } else {\n"
            "        narrowed = 2;\n"
            "    }\n"
            "} else {\n"
            "    if (inner) {\n"
            "        narrowed = 10;\n"
            "    } else {\n"
            "        narrowed = 20;\n"
            "    }\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(
            g_state,
            "numeric_nested_branch_assignment_dataflow_test.zr",
            strlen("numeric_nested_branch_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_bool_variable(cs, "inner");
    register_int64_range_variable(cs, "narrowed", 0, 0);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(21, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(21, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
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

static void test_if_then_assignment_joins_pre_branch_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "if (flag) {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "numeric_then_assignment_dataflow_test.zr",
                                      strlen("numeric_then_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
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
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_if_else_only_assignment_joins_pre_branch_range_for_following_expression(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *finalExpression;
    SZrInferredType ifType;
    SZrInferredType result;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
            "if (flag) {\n"
            "    flag;\n"
            "} else {\n"
            "    narrowed = 10;\n"
            "}\n"
            "narrowed + 1;\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "numeric_else_only_assignment_dataflow_test.zr",
                                      strlen("numeric_else_only_assignment_dataflow_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = statement_at(ast, 0);
    finalExpression = expression_statement_expression(statement_at(ast, 1));

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(finalExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, finalExpression->type);

    register_bool_variable(cs, "flag");
    register_int64_range_variable(cs, "narrowed", 5, 5);

    ZrParser_InferredType_Init(g_state, &ifType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, ifStatement, &ifType));
    ZrParser_InferredType_Free(g_state, &ifType);

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, finalExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, finalExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
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
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_if_else_assignments_join_numeric_range_for_following_expression);
    RUN_TEST(test_if_else_assignments_preserve_segment_range_for_following_expression);
    RUN_TEST(test_if_else_multi_statement_assignments_join_numeric_range_for_following_expression);
    RUN_TEST(test_if_else_nonterminal_assignment_joins_numeric_range_for_following_expression);
    RUN_TEST(test_if_else_same_target_sequential_assignments_join_rhs_dependent_range_for_following_expression);
    RUN_TEST(test_if_else_multi_target_assignments_join_cross_target_rhs_dependent_range);
    RUN_TEST(test_if_else_nested_assignments_join_numeric_range_for_following_expression);
    RUN_TEST(test_while_assignment_joins_pre_loop_range_for_following_expression);
    RUN_TEST(test_while_multi_statement_assignment_joins_pre_loop_range_for_following_expression);
    RUN_TEST(test_while_multi_target_assignments_join_cross_target_rhs_dependent_range);
    RUN_TEST(test_if_then_assignment_joins_pre_branch_range_for_following_expression);
    RUN_TEST(test_if_else_only_assignment_joins_pre_branch_range_for_following_expression);
    return UNITY_END();
}
