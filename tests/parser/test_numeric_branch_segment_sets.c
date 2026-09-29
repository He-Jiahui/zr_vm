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

/** @brief 当前 Unity 用例共享的 VM 状态；由 setUp 创建并由 tearDown 结束生命周期。 */
static SZrState *g_state;

/** @brief 为每个分段范围用例创建独立 VM 状态，避免类型环境跨用例残留。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/**
 * @brief 在用例边界释放共享 VM 状态。
 * BUG: Unity 断言失败会从测试函数跳回 runner 后仍调用本清理；若失败发生在
 * CompilerState 创建之后、正常销毁之前，局部 cs 已不可达，而这里只销毁 g_state，
 * 因此 create_compiler_state 分配的宿主堆块会泄漏。
 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/** @brief 为直接类型推断用例建立独立 CompilerState，并让其语义上下文与类型环境关联。 */
static SZrCompilerState *create_compiler_state(void) {
    SZrCompilerState *cs = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));

    TEST_ASSERT_NOT_NULL(cs);
    memset(cs, 0, sizeof(*cs));
    ZrParser_CompilerState_Init(cs, g_state);
    TEST_ASSERT_NOT_NULL(cs->semanticContext);
    TEST_ASSERT_NOT_NULL(cs->typeEnv);
    return cs;
}

/**
 * @brief 先释放编译器持有的语义事实与类型环境，再释放宿主堆上的状态壳。
 * @pre cs 仍关联到尚未销毁的测试 VM 状态。
 */
static void destroy_compiler_state(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    ZrParser_CompilerState_Free(cs);
    free(cs);
}

/** @brief 将带闭区间约束的 int64 种子写入类型环境，复制完成后释放临时推断类型。 */
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

/** @brief 逐段检查推断结果，防止只保留 min/max 包络而丢失集合中的间隔。 */
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

/**
 * @brief 独立核对语义数值事实与推断类型中的分段集合。
 * @note fact 由 CompilerState 的语义上下文持有，只能在该上下文销毁前读取。
 */
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

/**
 * @brief 验证 true 分支的不等比较挖去域内值后，后续加法仍保留两个不相连的整数段。
 * @note 同时核对推断类型与语义事实，避免用连续包络掩盖条件排除的值。
 */
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

/** @brief 验证三个逻辑或分支的区间并集经过加法后仍保留三段结果及对应语义事实。 */
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

/** @brief 验证六个离散等值分支经过加法后仍逐段保留，防止多路并集退化为单一包络。 */
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

/** @brief 注册分段推断用例；CTest 的 language_pipeline 会启动该 Unity 可执行文件。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_true_branch_not_equal_interior_refines_integer_hole_segments);
    RUN_TEST(test_true_branch_logical_or_builds_three_integer_segments);
    RUN_TEST(test_true_branch_logical_or_preserves_six_integer_segments);
    return UNITY_END();
}
