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

static void assert_segmented_type(const SZrInferredType *type,
                                  TZrInt64 expectedMin,
                                  TZrInt64 expectedMax,
                                  const SZrNumericRangeSegment *segments,
                                  TZrSize segmentCount) {
    TZrSize index;

    TEST_ASSERT_NOT_NULL(type);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, type->baseType);
    TEST_ASSERT_TRUE(type->hasRangeConstraint);
    TEST_ASSERT_EQUAL_INT64(expectedMin, type->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, type->maxValue);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)segmentCount, (TZrUInt64)type->rangeSegmentCount);
    for (index = 0; index < segmentCount; index++) {
        const SZrNumericRangeSegment *actualSegment =
            ZrParser_InferredType_RangeSegmentAt(type, index);
        TEST_ASSERT_NOT_NULL(actualSegment);
        TEST_ASSERT_EQUAL_INT64(segments[index].minValue, actualSegment->minValue);
        TEST_ASSERT_EQUAL_INT64(segments[index].maxValue, actualSegment->maxValue);
    }
}

static void assert_segmented_numeric_fact(const SZrSemanticNumericFact *fact,
                                          TZrInt64 expectedMin,
                                          TZrInt64 expectedMax,
                                          const SZrNumericRangeSegment *segments,
                                          TZrSize segmentCount) {
    TZrSize index;

    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_NUMERIC_FACT_PROMOTION, fact->kind);
    TEST_ASSERT_TRUE(fact->hasRange);
    TEST_ASSERT_EQUAL_INT64(expectedMin, fact->minValue);
    TEST_ASSERT_EQUAL_INT64(expectedMax, fact->maxValue);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)segmentCount, (TZrUInt64)fact->rangeSegmentCount);
    for (index = 0; index < segmentCount; index++) {
        const SZrNumericRangeSegment *actualSegment =
            ZrParser_SemanticNumericFact_RangeSegmentAt(fact, index);
        TEST_ASSERT_NOT_NULL(actualSegment);
        TEST_ASSERT_EQUAL_INT64(segments[index].minValue, actualSegment->minValue);
        TEST_ASSERT_EQUAL_INT64(segments[index].maxValue, actualSegment->maxValue);
    }
    TEST_ASSERT_FALSE(fact->mayOverflow);
}

static void test_true_branch_not_equal_interior_refines_integer_hole_segments(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const SZrNumericRangeSegment expectedSegments[] = {
        {1, 10},
        {12, 21},
    };
    const char *source =
        "if (seed != 10) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_not_equal_hole_segment_range_test.zr",
                                      strlen("true_branch_not_equal_hole_segment_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);

    register_int64_range_variable(cs, "seed", 0, 20);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    assert_segmented_type(&result, 1, 21, expectedSegments, 2);
    assert_segmented_numeric_fact(numericFact, 1, 21, expectedSegments, 2);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_logical_or_builds_three_integer_segments(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const SZrNumericRangeSegment expectedSegments[] = {
        {1, 5},
        {11, 11},
        {22, 256},
    };
    const char *source =
        "if (seed < 5 || seed == 10 || seed > 20) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_logical_or_three_segment_range_test.zr",
                                      strlen("true_branch_logical_or_three_segment_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    assert_segmented_type(&result, 1, 256, expectedSegments, 3);
    assert_segmented_numeric_fact(numericFact, 1, 256, expectedSegments, 3);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

static void test_true_branch_logical_or_preserves_six_integer_segments(void) {
    SZrCompilerState *cs = create_compiler_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *ifStatement;
    SZrAstNode *branchExpression;
    SZrInferredType result;
    SZrTypeInferenceBranchScope scope;
    const SZrSemanticNumericFact *numericFact;
    const SZrNumericRangeSegment expectedSegments[] = {
        {2, 2},
        {4, 4},
        {6, 6},
        {8, 8},
        {10, 10},
        {12, 12},
    };
    const char *source =
        "if (seed == 1 || seed == 3 || seed == 5 || seed == 7 || seed == 9 || seed == 11) {\n"
        "    seed + 1;\n"
        "}\n";

    sourceName = ZrCore_String_Create(g_state,
                                      "true_branch_logical_or_six_segment_range_test.zr",
                                      strlen("true_branch_logical_or_six_segment_range_test.zr"));
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    ifStatement = first_statement(ast);

    TEST_ASSERT_NOT_NULL(ifStatement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IF_EXPRESSION, ifStatement->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LOGICAL_EXPRESSION, ifStatement->data.ifExpression.condition->type);
    branchExpression = first_block_expression_statement_expression(ifStatement->data.ifExpression.thenExpr);
    TEST_ASSERT_NOT_NULL(branchExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, branchExpression->type);

    register_int64_range_variable(cs, "seed", 0, 255);

    TEST_ASSERT_TRUE(
        ZrParser_TypeInference_PushTrueBranchNumericRangeScope(
            cs,
            ifStatement->data.ifExpression.condition,
            &scope));

    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, branchExpression, &result));
    numericFact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, branchExpression);

    assert_segmented_type(&result, 2, 12, expectedSegments, 6);
    assert_segmented_numeric_fact(numericFact, 2, 12, expectedSegments, 6);

    ZrParser_TypeInference_PopBranchScope(cs, &scope);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_Ast_Free(g_state, ast);
    destroy_compiler_state(cs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_true_branch_not_equal_interior_refines_integer_hole_segments);
    RUN_TEST(test_true_branch_logical_or_builds_three_integer_segments);
    RUN_TEST(test_true_branch_logical_or_preserves_six_integer_segments);
    return UNITY_END();
}
