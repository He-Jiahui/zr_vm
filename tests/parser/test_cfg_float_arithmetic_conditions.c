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

/* 每个 Unity 用例独享状态；AST 节点、CFG 与语义事实均在此状态上构造。 */
static SZrState *g_state;

/* 为手工 AST 和 CFG 创建测试状态，不执行源文件编译或 VM 程序。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* 销毁状态及其托管对象；测试体借原生分配的局部资源仍需自行释放。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 为同一虚拟源文件的 AST 节点和事实查询构造稳定的单行区间。 */
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

/* 申请零初始化节点；位置里的文件名是状态持有的字符串。 */
static SZrAstNode *test_node(EZrAstNodeType type, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *node = (SZrAstNode *)ZrCore_Memory_RawMallocWithType(
        g_state->global,
        sizeof(SZrAstNode),
        ZR_MEMORY_NATIVE_TYPE_ARRAY);

    /* BUG: 若此前已建兄弟节点，本次分配失败的断言会跳过调用方清理，旧节点泄漏。 */
    TEST_ASSERT_NOT_NULL(node);
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->location = test_range(startOffset, endOffset);
    return node;
}

/* 把已建语句交给脚本根，成功后由根节点递归释放。 */
static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 80);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    /* BUG: 数组申请失败会经 Unity 跳出，script 和传入语句均尚未挂入可清理根节点，原生块泄漏。 */
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

/* 构造语句态分支块，并把传入语句的释放责任转入块体数组。 */
static SZrAstNode *block_with_statement(SZrAstNode *statement,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1);
    /* BUG: 块体数组申请失败时断言跳过调用方尾部清理，block 和传入语句泄漏。 */
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, statement);
    return block;
}

/* 以双精度值直接构造浮点叶节点，使本组用例只测 CFG 折叠。 */
static SZrAstNode *float_literal(TZrDouble value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_FLOAT_LITERAL, startOffset, endOffset);

    literal->data.floatLiteral.value = value;
    literal->data.floatLiteral.isSingle = ZR_FALSE;
    return literal;
}

/* 将操作数字树交给二元节点，调用方负责最终接入脚本根。 */
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

/* 将浮点操作数封装为一元节点，供正负号折叠用例复用。 */
static SZrAstNode *unary_expression(const TZrChar *op,
                                    SZrAstNode *argument,
                                    TZrSize startOffset,
                                    TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_UNARY_EXPRESSION, startOffset, endOffset);

    expression->data.unaryExpression.op.op = op;
    expression->data.unaryExpression.argument = argument;
    return expression;
}

/* 构造同时含 then/else 的语句态 if，作为分支可达性事实的观察对象。 */
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

/* 用语句内部位置读取 CFG 发出的事实，避免恰在边界命中相邻节点。 */
static const SZrSemanticReachabilityFact *reachability_fact_at(SZrSemanticContext *context,
                                                               SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
        context,
        test_range(node->location.start.offset + 1, node->location.start.offset + 1));
}

/* 已知真条件应只给 else 分支产生不可达事实，并保留原条件为原因节点。 */
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

/* 无法安全折叠时两侧都不得收到常量分支不可达事实。 */
static void assert_condition_kept_unknown(SZrAstNode *thenStmt,
                                          SZrAstNode *elseStmt,
                                          SZrSemanticContext *context) {
    TEST_ASSERT_NULL(reachability_fact_at(context, thenStmt));
    TEST_ASSERT_NULL(reachability_fact_at(context, elseStmt));
}

/* 搭建算术结果与期望值比较的完整 if，直接核 CFG 事实和分支原因。 */
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

    /* BUG: 此后的断言失败会 longjmp 越过 Cfg/Ast/SemanticContext_Free；tearDown 不接管这些原生资源。 */
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

/* 将等值比较作为浮点二元算术用例的共享入口。 */
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

/* 搭建一元正负号的等值条件，沿同一 CFG 事实路径检查真假分支。 */
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

    /* BUG: 断言失败会跳过局部 CFG、AST 和语义上下文的尾部释放。 */
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

/* 精确可表示的加法等式应判真并裁去 else。 */
static void test_cfg_folds_float_addition_equality_true_if_condition(void) {
    run_float_binary_condition(float_literal(1.5, 5, 8),
                               "+",
                               float_literal(2.25, 11, 15),
                               float_literal(3.75, 20, 24),
                               25,
                               ZR_TRUE);
}

/* 精确可表示的减法等式沿同一真分支事实路径。 */
static void test_cfg_folds_float_subtraction_equality_true_if_condition(void) {
    run_float_binary_condition(float_literal(5.5, 5, 8),
                               "-",
                               float_literal(2.25, 11, 15),
                               float_literal(3.25, 20, 24),
                               25,
                               ZR_TRUE);
}

/* 有限乘积的等值比较应可折叠为真。 */
static void test_cfg_folds_float_multiplication_equality_true_if_condition(void) {
    run_float_binary_condition(float_literal(1.5, 5, 8),
                               "*",
                               float_literal(2.5, 11, 14),
                               float_literal(3.75, 19, 23),
                               24,
                               ZR_TRUE);
}

/* 非零除数与有限商允许裁去 else。 */
static void test_cfg_folds_float_division_equality_true_if_condition(void) {
    run_float_binary_condition(float_literal(7.5, 5, 8),
                               "/",
                               float_literal(2.5, 11, 14),
                               float_literal(3.0, 19, 22),
                               24,
                               ZR_TRUE);
}

/* 浮点算术折叠结果还能进入关系比较并生成同一分支事实。 */
static void test_cfg_folds_folded_float_relational_true_if_condition(void) {
    run_float_binary_comparison_condition(float_literal(1.5, 5, 8),
                                          "+",
                                          float_literal(2.25, 11, 15),
                                          ">",
                                          float_literal(3.0, 19, 22),
                                          24,
                                          ZR_TRUE);
}

/* 一元正号保留有限字面量值，等式为真。 */
static void test_cfg_folds_float_unary_plus_equality_true_if_condition(void) {
    run_float_unary_condition("+",
                              float_literal(1.5, 6, 9),
                              float_literal(1.5, 14, 17),
                              18,
                              ZR_TRUE);
}

/* 一元负号取反后仍由等式确定真分支。 */
static void test_cfg_folds_float_unary_minus_equality_true_if_condition(void) {
    run_float_unary_condition("-",
                              float_literal(1.5, 6, 9),
                              float_literal(-1.5, 14, 18),
                              19,
                              ZR_TRUE);
}

/* 溢出为非有限结果时不得据宿主浮点结果裁去分支。 */
static void test_cfg_keeps_overflowed_float_addition_condition_unknown(void) {
    run_float_binary_condition(float_literal(DBL_MAX, 5, 12),
                               "+",
                               float_literal(DBL_MAX, 15, 22),
                               float_literal(DBL_MAX, 27, 34),
                               35,
                               ZR_FALSE);
}

/* 负向减法溢出同样保持两条分支可达性未知。 */
static void test_cfg_keeps_overflowed_float_subtraction_condition_unknown(void) {
    run_float_binary_condition(float_literal(-DBL_MAX, 5, 12),
                               "-",
                               float_literal(DBL_MAX, 15, 22),
                               float_literal(-DBL_MAX, 27, 34),
                               35,
                               ZR_FALSE);
}

/* 乘法产生非有限值时保守保留两个分支。 */
static void test_cfg_keeps_overflowed_float_multiplication_condition_unknown(void) {
    run_float_binary_condition(float_literal(DBL_MAX, 5, 12),
                               "*",
                               float_literal(2.0, 15, 18),
                               float_literal(DBL_MAX, 23, 30),
                               31,
                               ZR_FALSE);
}

/* 零除数不产生可用于 CFG 剪枝的常量。 */
static void test_cfg_keeps_zero_divisor_float_division_condition_unknown(void) {
    run_float_binary_condition(float_literal(7.5, 5, 8),
                               "/",
                               float_literal(0.0, 11, 14),
                               float_literal(7.5, 19, 22),
                               24,
                               ZR_FALSE);
}

/* 有限且非零的除数仍可能使商溢出，应保持未知。 */
static void test_cfg_keeps_overflowed_float_division_condition_unknown(void) {
    run_float_binary_condition(float_literal(DBL_MAX, 5, 12),
                               "/",
                               float_literal(0.5, 15, 18),
                               float_literal(DBL_MAX, 23, 30),
                               31,
                               ZR_FALSE);
}

/* Unity 注册七个可折叠和五个保守未知场景；CMake 将可执行文件纳入 language_pipeline。 */
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
