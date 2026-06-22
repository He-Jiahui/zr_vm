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

static SZrAstNode *first_statement(SZrAstNode *ast) {
    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count == 0) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[0];
}

static SZrAstNode *first_block_expression_statement_expression(SZrAstNode *blockNode) {
    SZrAstNode *statement;

    if (blockNode == ZR_NULL ||
        blockNode->type != ZR_AST_BLOCK ||
        blockNode->data.block.body == ZR_NULL ||
        blockNode->data.block.body->count == 0) {
        return ZR_NULL;
    }

    statement = blockNode->data.block.body->nodes[0];
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static void test_true_branch_less_than_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed < 10) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_less_than_interval_range_test.zr",
                                      strlen("true_branch_less_than_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(1, result.minValue);
    TEST_ASSERT_EQUAL_INT64(10, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(1, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(10, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_equal_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed == 10) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_equal_interval_range_test.zr",
                                      strlen("true_branch_equal_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(11, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_edge_not_equal_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed != 0) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_edge_not_equal_interval_range_test.zr",
                                      strlen("true_branch_edge_not_equal_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_logical_and_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed > 2 && seed < 10) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_logical_and_interval_range_test.zr",
                                      strlen("true_branch_logical_and_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(4, result.minValue);
    TEST_ASSERT_EQUAL_INT64(10, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(4, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(10, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_logical_or_same_direction_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed < 10 || seed < 20) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_logical_or_same_direction_interval_range_test.zr",
                                      strlen("true_branch_logical_or_same_direction_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(1, result.minValue);
    TEST_ASSERT_EQUAL_INT64(20, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(1, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(20, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_logical_or_disjoint_refines_integer_segment_ranges(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed < 10 || seed > 20) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_logical_or_disjoint_segment_range_test.zr",
                                      strlen("true_branch_logical_or_disjoint_segment_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(1, result.minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.maxValue);
    TEST_ASSERT_EQUAL_UINT64(2, result.rangeSegmentCount);
    TEST_ASSERT_EQUAL_INT64(1, result.rangeSegments[0].minValue);
    TEST_ASSERT_EQUAL_INT64(10, result.rangeSegments[0].maxValue);
    TEST_ASSERT_EQUAL_INT64(22, result.rangeSegments[1].minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.rangeSegments[1].maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(1, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->maxValue);
    TEST_ASSERT_EQUAL_UINT64(2, numericFact->rangeSegmentCount);
    TEST_ASSERT_EQUAL_INT64(1, numericFact->rangeSegments[0].minValue);
    TEST_ASSERT_EQUAL_INT64(10, numericFact->rangeSegments[0].maxValue);
    TEST_ASSERT_EQUAL_INT64(22, numericFact->rangeSegments[1].minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->rangeSegments[1].maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_logical_or_nested_and_refines_integer_segment_ranges(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if ((seed > 2 && seed < 10) || seed == 20) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_logical_or_nested_and_segment_range_test.zr",
                                      strlen("true_branch_logical_or_nested_and_segment_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(4, result.minValue);
    TEST_ASSERT_EQUAL_INT64(21, result.maxValue);
    TEST_ASSERT_EQUAL_UINT64(2, result.rangeSegmentCount);
    TEST_ASSERT_EQUAL_INT64(4, result.rangeSegments[0].minValue);
    TEST_ASSERT_EQUAL_INT64(10, result.rangeSegments[0].maxValue);
    TEST_ASSERT_EQUAL_INT64(21, result.rangeSegments[1].minValue);
    TEST_ASSERT_EQUAL_INT64(21, result.rangeSegments[1].maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(4, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(21, numericFact->maxValue);
    TEST_ASSERT_EQUAL_UINT64(2, numericFact->rangeSegmentCount);
    TEST_ASSERT_EQUAL_INT64(4, numericFact->rangeSegments[0].minValue);
    TEST_ASSERT_EQUAL_INT64(10, numericFact->rangeSegments[0].maxValue);
    TEST_ASSERT_EQUAL_INT64(21, numericFact->rangeSegments[1].minValue);
    TEST_ASSERT_EQUAL_INT64(21, numericFact->rangeSegments[1].maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_unary_not_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (!(seed < 10)) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_unary_not_interval_range_test.zr",
                                      strlen("true_branch_unary_not_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_NOT_NULL(ifStatement->data.ifExpression.condition);
    TEST_ASSERT_EQUAL_INT(ZR_AST_UNARY_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(11, result.minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_else_if_chain_preserves_outer_false_scope_for_inner_true_branch(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *outerIf;
    SZrAstNode *innerIf;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope outerFalseScope;
    SZrTypeInferenceBranchScope innerTrueScope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed < 10) {\n"
        "    seed + 100;\n"
        "} else if (seed < 20) {\n"
        "    seed + 1;\n"
        "} else {\n"
        "    seed + 200;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "else_if_chain_outer_false_inner_true_range_test.zr",
                                      strlen("else_if_chain_outer_false_inner_true_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    outerIf = first_statement(ast);

    TEST_ASSERT_NOT_NULL(outerIf);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, outerIf->type);
    innerIf = outerIf->data.ifExpression.elseExpr;
    TEST_ASSERT_NOT_NULL(innerIf);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, innerIf->type);
    branchExpression = first_block_expression_statement_expression(innerIf->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
            cs,
            outerIf->data.ifExpression.condition,
            &outerFalseScope));
    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            innerIf->data.ifExpression.condition,
            &innerTrueScope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(11, result.minValue);
    TEST_ASSERT_EQUAL_INT64(20, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(20, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &innerTrueScope);
    ZrParser_TypeInference_PopBranchScope(cs, &outerFalseScope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_false_branch_less_than_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed < 10) {\n"
        "    seed + 1;\n"
        "} else {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "false_branch_less_than_interval_range_test.zr",
                                      strlen("false_branch_less_than_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.elseExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(11, result.minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_false_branch_edge_equal_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed == 0) {\n"
        "    seed + 100;\n"
        "} else {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "false_branch_edge_equal_interval_range_test.zr",
                                      strlen("false_branch_edge_equal_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.elseExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(2, result.minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(2, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_false_branch_not_equal_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed != 10) {\n"
        "    seed + 100;\n"
        "} else {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "false_branch_not_equal_interval_range_test.zr",
                                      strlen("false_branch_not_equal_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.elseExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(11, result.minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_false_branch_logical_or_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed <= 2 || seed >= 10) {\n"
        "    seed + 100;\n"
        "} else {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "false_branch_logical_or_interval_range_test.zr",
                                      strlen("false_branch_logical_or_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.elseExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(4, result.minValue);
    TEST_ASSERT_EQUAL_INT64(10, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(4, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(10, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_false_branch_logical_and_same_direction_refines_integer_interval_range(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed < 10 && seed < 20) {\n"
        "    seed + 100;\n"
        "} else {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "false_branch_logical_and_same_direction_interval_range_test.zr",
                                      strlen("false_branch_logical_and_same_direction_interval_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.elseExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(11, result.minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_false_branch_logical_and_disjoint_refines_integer_segment_ranges(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const char *source =
        "if (seed > 10 && seed < 20) {\n"
        "    seed + 100;\n"
        "} else {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "false_branch_logical_and_disjoint_segment_range_test.zr",
                                      strlen("false_branch_logical_and_disjoint_segment_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.elseExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);
    TEST_ASSERT_EQUAL_STRING("+", branchExpression->data.binaryExpression.op.op);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushFalseBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.baseType);
    TEST_ASSERT_TRUE(result.hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(1, result.minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.maxValue);
    TEST_ASSERT_EQUAL_UINT64(2, result.rangeSegmentCount);
    TEST_ASSERT_EQUAL_INT64(1, result.rangeSegments[0].minValue);
    TEST_ASSERT_EQUAL_INT64(11, result.rangeSegments[0].maxValue);
    TEST_ASSERT_EQUAL_INT64(21, result.rangeSegments[1].minValue);
    TEST_ASSERT_EQUAL_INT64(256, result.rangeSegments[1].maxValue);
    TEST_ASSERT_NOT_NULL(numericFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, numericFact->kind);
    TEST_ASSERT_TRUE(numericFact->hasRange);
    TEST_ASSERT_EQUAL_INT64(1, numericFact->minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->maxValue);
    TEST_ASSERT_EQUAL_UINT64(2, numericFact->rangeSegmentCount);
    TEST_ASSERT_EQUAL_INT64(1, numericFact->rangeSegments[0].minValue);
    TEST_ASSERT_EQUAL_INT64(11, numericFact->rangeSegments[0].maxValue);
    TEST_ASSERT_EQUAL_INT64(21, numericFact->rangeSegments[1].minValue);
    TEST_ASSERT_EQUAL_INT64(256, numericFact->rangeSegments[1].maxValue);
    TEST_ASSERT_FALSE(numericFact->mayOverflow);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_true_branch_less_than_refines_integer_interval_range);
    RUN_TEST(test_true_branch_equal_refines_integer_interval_range);
    RUN_TEST(test_true_branch_edge_not_equal_refines_integer_interval_range);
    RUN_TEST(test_true_branch_logical_and_refines_integer_interval_range);
    RUN_TEST(test_true_branch_logical_or_same_direction_refines_integer_interval_range);
    RUN_TEST(test_true_branch_logical_or_disjoint_refines_integer_segment_ranges);
    RUN_TEST(test_true_branch_logical_or_nested_and_refines_integer_segment_ranges);
    RUN_TEST(test_true_branch_unary_not_refines_integer_interval_range);
    RUN_TEST(test_else_if_chain_preserves_outer_false_scope_for_inner_true_branch);
    RUN_TEST(test_false_branch_less_than_refines_integer_interval_range);
    RUN_TEST(test_false_branch_edge_equal_refines_integer_interval_range);
    RUN_TEST(test_false_branch_not_equal_refines_integer_interval_range);
    RUN_TEST(test_false_branch_logical_or_refines_integer_interval_range);
    RUN_TEST(test_false_branch_logical_and_same_direction_refines_integer_interval_range);
    RUN_TEST(test_false_branch_logical_and_disjoint_refines_integer_segment_ranges);
    return UNITY_END();
}
