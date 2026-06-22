#include "unity.h"

#include <float.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

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

static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_CreateFromNative(
        g_state,
        (TZrNativeString) "cfg_float_arithmetic_conditions_test.zr");
    return range;
}

static SZrAstNode *test_node(EZrAstNodeType type, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *node = (SZrAstNode *)ZrCore_Memory_RawMallocWithType(
        g_state->global,
        sizeof(SZrAstNode),
        ZR_MEMORY_NATIVE_TYPE_ARRAY);

    TEST_ASSERT_NOT_NULL(node);
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->location = test_range(startOffset, endOffset);
    return node;
}

static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 80);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

static SZrAstNode *block_with_statement(SZrAstNode *statement,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, statement);
    return block;
}

static SZrAstNode *float_literal(TZrDouble value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_FLOAT_LITERAL, startOffset, endOffset);

    literal->data.floatLiteral.value = value;
    literal->data.floatLiteral.isSingle = ZR_FALSE;
    return literal;
}

static SZrAstNode *binary_expression(SZrAstNode *left,
                                     const TZrChar *op,
                                     SZrAstNode *right,
                                     TZrSize startOffset,
                                     TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_BINARY_EXPRESSION, startOffset, endOffset);

    expression->data.binaryExpression.left = left;
    expression->data.binaryExpression.op.op = op;
    expression->data.binaryExpression.right = right;
    return expression;
}

static SZrAstNode *unary_expression(const TZrChar *op,
                                    SZrAstNode *argument,
                                    TZrSize startOffset,
                                    TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_UNARY_EXPRESSION, startOffset, endOffset);

    expression->data.unaryExpression.op.op = op;
    expression->data.unaryExpression.argument = argument;
    return expression;
}

static SZrAstNode *if_statement(SZrAstNode *condition,
                                SZrAstNode *thenBlock,
                                SZrAstNode *elseBlock) {
    SZrAstNode *ifNode = test_node(ZR_AST_IF_EXPRESSION, 0, 64);

    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = thenBlock;
    ifNode->data.ifExpression.elseExpr = elseBlock;
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    return ifNode;
}

static const SZrSemanticReachabilityFact *reachability_fact_at(SZrSemanticContext *context,
                                                               SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
        context,
        test_range(node->location.start.offset + 1, node->location.start.offset + 1));
}

static void assert_else_branch_pruned_by_constant(SZrAstNode *condition,
                                                  SZrAstNode *thenStmt,
                                                  SZrAstNode *elseStmt,
                                                  SZrSemanticContext *context) {
    const SZrSemanticReachabilityFact *thenFact = reachability_fact_at(context, thenStmt);
    const SZrSemanticReachabilityFact *elseFact = reachability_fact_at(context, elseStmt);

    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);
}

static void assert_condition_kept_unknown(SZrAstNode *thenStmt,
                                          SZrAstNode *elseStmt,
                                          SZrSemanticContext *context) {
    TEST_ASSERT_NULL(reachability_fact_at(context, thenStmt));
    TEST_ASSERT_NULL(reachability_fact_at(context, elseStmt));
}

static void run_float_binary_comparison_condition(SZrAstNode *left,
                                                  const TZrChar *arithmeticOp,
                                                  SZrAstNode *right,
                                                  const TZrChar *comparisonOp,
                                                  SZrAstNode *expected,
                                                  TZrSize conditionEnd,
                                                  TZrBool expectElsePruned) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *arithmetic = binary_expression(left, arithmeticOp, right, 5, conditionEnd - 10);
    SZrAstNode *condition = binary_expression(arithmetic, comparisonOp, expected, 4, conditionEnd);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, conditionEnd + 8, conditionEnd + 16);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, conditionEnd + 32, conditionEnd + 40);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, conditionEnd + 4, conditionEnd + 20);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, conditionEnd + 28, conditionEnd + 44);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    if (expectElsePruned) {
        assert_else_branch_pruned_by_constant(condition, thenStmt, elseStmt, context);
    } else {
        assert_condition_kept_unknown(thenStmt, elseStmt, context);
    }

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void run_float_binary_condition(SZrAstNode *left,
                                       const TZrChar *arithmeticOp,
                                       SZrAstNode *right,
                                       SZrAstNode *expected,
                                       TZrSize conditionEnd,
                                       TZrBool expectElsePruned) {
    run_float_binary_comparison_condition(left,
                                          arithmeticOp,
                                          right,
                                          "==",
                                          expected,
                                          conditionEnd,
                                          expectElsePruned);
}

static void run_float_unary_condition(const TZrChar *unaryOp,
                                      SZrAstNode *operand,
                                      SZrAstNode *expected,
                                      TZrSize conditionEnd,
                                      TZrBool expectElsePruned) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *unary = unary_expression(unaryOp, operand, 5, conditionEnd - 10);
    SZrAstNode *condition = binary_expression(unary, "==", expected, 4, conditionEnd);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, conditionEnd + 8, conditionEnd + 16);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, conditionEnd + 32, conditionEnd + 40);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, conditionEnd + 4, conditionEnd + 20);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, conditionEnd + 28, conditionEnd + 44);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    if (expectElsePruned) {
        assert_else_branch_pruned_by_constant(condition, thenStmt, elseStmt, context);
    } else {
        assert_condition_kept_unknown(thenStmt, elseStmt, context);
    }

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_folds_float_addition_equality_true_if_condition(void) {
    run_float_binary_condition(float_literal(1.5, 5, 8),
                               "+",
                               float_literal(2.25, 11, 15),
                               float_literal(3.75, 20, 24),
                               25,
                               ZR_TRUE);
}

static void test_cfg_folds_float_subtraction_equality_true_if_condition(void) {
    run_float_binary_condition(float_literal(5.5, 5, 8),
                               "-",
                               float_literal(2.25, 11, 15),
                               float_literal(3.25, 20, 24),
                               25,
                               ZR_TRUE);
}

static void test_cfg_folds_float_multiplication_equality_true_if_condition(void) {
    run_float_binary_condition(float_literal(1.5, 5, 8),
                               "*",
                               float_literal(2.5, 11, 14),
                               float_literal(3.75, 19, 23),
                               24,
                               ZR_TRUE);
}

static void test_cfg_folds_float_division_equality_true_if_condition(void) {
    run_float_binary_condition(float_literal(7.5, 5, 8),
                               "/",
                               float_literal(2.5, 11, 14),
                               float_literal(3.0, 19, 22),
                               24,
                               ZR_TRUE);
}

static void test_cfg_folds_folded_float_relational_true_if_condition(void) {
    run_float_binary_comparison_condition(float_literal(1.5, 5, 8),
                                          "+",
                                          float_literal(2.25, 11, 15),
                                          ">",
                                          float_literal(3.0, 19, 22),
                                          24,
                                          ZR_TRUE);
}

static void test_cfg_folds_float_unary_plus_equality_true_if_condition(void) {
    run_float_unary_condition("+",
                              float_literal(1.5, 6, 9),
                              float_literal(1.5, 14, 17),
                              18,
                              ZR_TRUE);
}

static void test_cfg_folds_float_unary_minus_equality_true_if_condition(void) {
    run_float_unary_condition("-",
                              float_literal(1.5, 6, 9),
                              float_literal(-1.5, 14, 18),
                              19,
                              ZR_TRUE);
}

static void test_cfg_keeps_overflowed_float_addition_condition_unknown(void) {
    run_float_binary_condition(float_literal(DBL_MAX, 5, 12),
                               "+",
                               float_literal(DBL_MAX, 15, 22),
                               float_literal(DBL_MAX, 27, 34),
                               35,
                               ZR_FALSE);
}

static void test_cfg_keeps_overflowed_float_subtraction_condition_unknown(void) {
    run_float_binary_condition(float_literal(-DBL_MAX, 5, 12),
                               "-",
                               float_literal(DBL_MAX, 15, 22),
                               float_literal(-DBL_MAX, 27, 34),
                               35,
                               ZR_FALSE);
}

static void test_cfg_keeps_overflowed_float_multiplication_condition_unknown(void) {
    run_float_binary_condition(float_literal(DBL_MAX, 5, 12),
                               "*",
                               float_literal(2.0, 15, 18),
                               float_literal(DBL_MAX, 23, 30),
                               31,
                               ZR_FALSE);
}

static void test_cfg_keeps_zero_divisor_float_division_condition_unknown(void) {
    run_float_binary_condition(float_literal(7.5, 5, 8),
                               "/",
                               float_literal(0.0, 11, 14),
                               float_literal(7.5, 19, 22),
                               24,
                               ZR_FALSE);
}

static void test_cfg_keeps_overflowed_float_division_condition_unknown(void) {
    run_float_binary_condition(float_literal(DBL_MAX, 5, 12),
                               "/",
                               float_literal(0.5, 15, 18),
                               float_literal(DBL_MAX, 23, 30),
                               31,
                               ZR_FALSE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_folds_float_addition_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_float_subtraction_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_float_multiplication_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_float_division_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_folded_float_relational_true_if_condition);
    RUN_TEST(test_cfg_folds_float_unary_plus_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_float_unary_minus_equality_true_if_condition);
    RUN_TEST(test_cfg_keeps_overflowed_float_addition_condition_unknown);
    RUN_TEST(test_cfg_keeps_overflowed_float_subtraction_condition_unknown);
    RUN_TEST(test_cfg_keeps_overflowed_float_multiplication_condition_unknown);
    RUN_TEST(test_cfg_keeps_zero_divisor_float_division_condition_unknown);
    RUN_TEST(test_cfg_keeps_overflowed_float_division_condition_unknown);
    return UNITY_END();
}
