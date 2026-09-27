#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

/* 每个 Unity 用例独享状态；手工 AST 与 CFG 事实都使用其分配器。 */
static SZrState *g_state;

/* 为本组直接 CFG 测试创建状态，不经过源代码编译或 VM 执行。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* 销毁状态；测试体借原生分配的 AST、CFG、语义上下文需先自行释放。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 以同一虚拟源文件和单行偏移构造节点位置及事实查询位置。 */
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
        (TZrNativeString) "cfg_integer_bitwise_conditions_test.zr");
    return range;
}

/* 原生申请一个零初始化 AST 节点；最终由脚本根递归释放。 */
static SZrAstNode *test_node(EZrAstNodeType type, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *node = (SZrAstNode *)ZrCore_Memory_RawMallocWithType(
        g_state->global,
        sizeof(SZrAstNode),
        ZR_MEMORY_NATIVE_TYPE_ARRAY);

    /* BUG: 后续节点申请失败时 Unity 跳过测试体，先前已建的 AST 子树泄漏。 */
    TEST_ASSERT_NOT_NULL(node);
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->location = test_range(startOffset, endOffset);
    return node;
}

/* 把已建 if 交给脚本根，成功后由根节点承担递归析构。 */
static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 80);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    /* BUG: 数组申请失败经 Unity 跳出，script 和传入语句尚未形成可清理树，原生块泄漏。 */
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

/* 为 then 或 else 建语句态块体，并接管传入语句。 */
static SZrAstNode *block_with_statement(SZrAstNode *statement,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1);
    /* BUG: 块体数组申请失败时断言跳过调用方尾部释放，block 和传入语句泄漏。 */
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, statement);
    return block;
}

/* 用直接赋值的整数叶节点隔离 CFG 折叠逻辑。 */
static SZrAstNode *integer_literal(TZrInt64 value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_INTEGER_LITERAL, startOffset, endOffset);

    literal->data.integerLiteral.value = value;
    return literal;
}

/* 将操作数交给二元节点，供位运算与外层等值比较复用。 */
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

/* 为已知真条件提供两侧分支，以 else 不可达事实观察折叠结果。 */
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

/* 在语句内部而非节点边界按位置读取可达性事实。 */
static const SZrSemanticReachabilityFact *reachability_fact_at(SZrSemanticContext *context,
                                                               SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
        context,
        test_range(node->location.start.offset + 1, node->location.start.offset + 1));
}

/* TODO: 当前仅覆盖非负操作数与合法移位计数；需对照 cfg_constants 的拒绝分支，
 * 补负数、63 及更大计数、左移溢出时两侧保持未知的 CFG 事实用例。 */
/* 6 & 3 得 2，真条件应只把 else 标为常量分支不可达。 */
static void test_cfg_folds_nonnegative_integer_bitwise_and_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = binary_expression(integer_literal(6, 5, 6),
                                         "&",
                                         integer_literal(3, 9, 10),
                                         5,
                                         10);
    SZrAstNode *right = integer_literal(2, 15, 16);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    /* BUG: 断言失败会跳过本用例尾部 Cfg/Ast/SemanticContext_Free，已分配的原生资源泄漏。 */
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 4 | 1 得 5，验证或运算结果进入外层等值条件。 */
static void test_cfg_folds_nonnegative_integer_bitwise_or_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = binary_expression(integer_literal(4, 5, 6),
                                         "|",
                                         integer_literal(1, 9, 10),
                                         5,
                                         10);
    SZrAstNode *right = integer_literal(5, 15, 16);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    /* BUG: 断言失败会跳过本用例尾部 Cfg/Ast/SemanticContext_Free，已分配的原生资源泄漏。 */
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 6 ^ 3 得 5，验证异或后的真分支事实及原条件归因。 */
static void test_cfg_folds_nonnegative_integer_bitwise_xor_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = binary_expression(integer_literal(6, 5, 6),
                                         "^",
                                         integer_literal(3, 9, 10),
                                         5,
                                         10);
    SZrAstNode *right = integer_literal(5, 15, 16);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    /* BUG: 断言失败会跳过本用例尾部 Cfg/Ast/SemanticContext_Free，已分配的原生资源泄漏。 */
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 合法左移 3 << 2 得 12，折叠后裁去 else。 */
static void test_cfg_folds_nonnegative_integer_left_shift_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = binary_expression(integer_literal(3, 5, 6),
                                         "<<",
                                         integer_literal(2, 10, 11),
                                         5,
                                         11);
    SZrAstNode *right = integer_literal(12, 16, 18);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 19);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 27, 35);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 51, 59);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 23, 39);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 47, 63);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    /* BUG: 断言失败会跳过本用例尾部 Cfg/Ast/SemanticContext_Free，已分配的原生资源泄漏。 */
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 合法右移 12 >> 2 得 3，保留 then、裁去 else。 */
static void test_cfg_folds_nonnegative_integer_right_shift_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = binary_expression(integer_literal(12, 5, 7),
                                         ">>",
                                         integer_literal(2, 11, 12),
                                         5,
                                         12);
    SZrAstNode *right = integer_literal(3, 17, 18);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 19);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 27, 35);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 51, 59);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 23, 39);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 47, 63);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    /* BUG: 断言失败会跳过本用例尾部 Cfg/Ast/SemanticContext_Free，已分配的原生资源泄漏。 */
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NULL(thenFact);
    TEST_ASSERT_NOT_NULL(elseFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, elseFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, elseFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, elseFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* Unity 注册五个非负位运算与移位用例；目标进入 language_pipeline 套件。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_folds_nonnegative_integer_bitwise_and_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_nonnegative_integer_bitwise_or_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_nonnegative_integer_bitwise_xor_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_nonnegative_integer_left_shift_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_nonnegative_integer_right_shift_equality_true_if_condition);
    return UNITY_END();
}
