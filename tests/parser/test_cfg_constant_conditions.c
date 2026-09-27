#include "unity.h"

#include <stdint.h>
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

/* Unity 每个用例重新建 VM；所有手工 AST、CFG 和语义事实都以该状态为资源域。 */
static SZrState *g_state;

/* Unity 在每次 RUN_TEST 前建立独立状态，隔离常量条件用例的 GC 与语义事实。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* Unity 即使在断言中止后也调用此入口；它仅销毁 VM 状态，不代替局部 CFG/AST 清理。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 为手工 AST 和位置查询提供同一虚拟源文件，保证事实查找只比较目标语句的范围。 */
static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, "cfg_constant_conditions_test.zr", 31);
    return range;
}

/* 测试直接构造原生 AST，绕开解析器；成功后节点须接入 script 并由 Ast_Free 递归释放。 */
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

/* 把待测控制语句作为脚本唯一入口，让 CFG 从 entry 节点遍历该场景。 */
static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 80);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

/* 用单语句块隔离分支或循环体；事实应落在内部语句，而非仅落在控制节点。 */
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

/* 将已知布尔值交给 CFG 条件折叠路径，供取反和逻辑组合场景复用。 */
static SZrAstNode *boolean_literal(TZrBool value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_BOOLEAN_LITERAL, startOffset, endOffset);

    literal->data.booleanLiteral.value = value;
    return literal;
}

/* 提供整数字面量，包括溢出边界，以区分可折叠和必须保守处理的条件。 */
static SZrAstNode *integer_literal(TZrInt64 value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_INTEGER_LITERAL, startOffset, endOffset);

    literal->data.integerLiteral.value = value;
    return literal;
}

/* 固定使用双精度字面量；本组只覆盖有限普通数的关系判断。 */
static SZrAstNode *float_literal(TZrDouble value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_FLOAT_LITERAL, startOffset, endOffset);

    literal->data.floatLiteral.value = value;
    literal->data.floatLiteral.isSingle = ZR_FALSE;
    return literal;
}

/* 提供 GC 字符串参与常量相等判断；AST 持有节点，字符串由测试状态管理。 */
static SZrAstNode *string_literal(const TZrChar *value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_STRING_LITERAL, startOffset, endOffset);

    literal->data.stringLiteral.value = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)value);
    TEST_ASSERT_NOT_NULL(literal->data.stringLiteral.value);
    return literal;
}

/* 供字符比较用例检验 CFG 对相同类型标量的关系判断。 */
static SZrAstNode *char_literal(TZrChar value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_CHAR_LITERAL, startOffset, endOffset);

    literal->data.charLiteral.value = value;
    return literal;
}

/* 模拟尚无常量值的运行时标识符，验证短路条件不会被未知右操作数阻断。 */
static SZrAstNode *identifier_node(const TZrChar *name, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *identifier = test_node(ZR_AST_IDENTIFIER_LITERAL, startOffset, endOffset);

    identifier->data.identifier.name = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name);
    TEST_ASSERT_NOT_NULL(identifier->data.identifier.name);
    return identifier;
}

/* 将操作符和子表达式接入手工 AST；操作符使用静态字面量，生命期覆盖整个用例。 */
static SZrAstNode *unary_expression(const TZrChar *op,
                                    SZrAstNode *argument,
                                    TZrSize startOffset,
                                    TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_UNARY_EXPRESSION, startOffset, endOffset);

    expression->data.unaryExpression.op.op = op;
    expression->data.unaryExpression.argument = argument;
    return expression;
}

/* 把布尔取反场景统一交给一元表达式夹具，防止构造差异掩盖折叠结果。 */
static SZrAstNode *unary_not_expression(SZrAstNode *argument,
                                        TZrSize startOffset,
                                        TZrSize endOffset) {
    return unary_expression("!", argument, startOffset, endOffset);
}

/* 构造短路逻辑条件，检验 CFG 在部分操作数未知时的保守性。 */
static SZrAstNode *logical_expression(SZrAstNode *left,
                                      const TZrChar *op,
                                      SZrAstNode *right,
                                      TZrSize startOffset,
                                      TZrSize endOffset) {
    SZrAstNode *expression = test_node(ZR_AST_LOGICAL_EXPRESSION, startOffset, endOffset);

    expression->data.logicalExpression.left = left;
    expression->data.logicalExpression.op = op;
    expression->data.logicalExpression.right = right;
    return expression;
}

/* 让同一比较入口承接标量比较和嵌套算术折叠，避免借助解析器预处理。 */
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

/* 生成有两支的语句型 if；测试借此核对被排除分支的原因节点仍是原条件。 */
static SZrAstNode *if_statement(SZrAstNode *condition,
                                SZrAstNode *thenBlock,
                                SZrAstNode *elseBlock) {
    /* TODO: 溢出用例 elseBlock 结束于 74，超出固定范围 64；增加父节点位置查询前核对夹具范围。 */
    SZrAstNode *ifNode = test_node(ZR_AST_IF_EXPRESSION, 0, 64);
    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = thenBlock;
    ifNode->data.ifExpression.elseExpr = elseBlock;
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    return ifNode;
}

/* 生成语句型 while；常假条件应让循环体不可达，但不使整个脚本不可达。 */
static SZrAstNode *while_statement(SZrAstNode *condition, SZrAstNode *body) {
    SZrAstNode *whileNode = test_node(ZR_AST_WHILE_LOOP, 0, 64);

    whileNode->data.whileLoop.cond = condition;
    whileNode->data.whileLoop.block = body;
    whileNode->data.whileLoop.isStatement = ZR_TRUE;
    return whileNode;
}

/* 在语句起点内部查借用事实；无记录在本测试中表示该语句未被标为不可达。 */
static const SZrSemanticReachabilityFact *reachability_fact_at(SZrSemanticContext *context,
                                                               SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
        context,
        test_range(node->location.start.offset + 1, node->location.start.offset + 1));
}

/* !false 应保留 then 路径，并把 else 的不可达原因追溯到完整取反条件。 */
static void test_cfg_folds_unary_not_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *innerLiteral = boolean_literal(ZR_FALSE, 5, 10);
    SZrAstNode *condition = unary_not_expression(innerLiteral, 4, 10);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 18, 26);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 42, 50);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 14, 30);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 38, 54);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    /* BUG: 本文件各用例在取得原生 AST、CFG 或 context 后用 Unity 断言；任一失败
     * 会跳过局部 Free，仅由 tearDown 销毁 VM，导致这些原生块泄漏。需用失败路径也
     * 能执行的清理入口持有三者。 */
    TEST_ASSERT_NOT_NULL(context);
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

/* !true 使循环零次执行；循环体事实应标为条件假而非普通分支淘汰。 */
static void test_cfg_folds_unary_not_true_while_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *innerLiteral = boolean_literal(ZR_TRUE, 8, 12);
    SZrAstNode *condition = unary_not_expression(innerLiteral, 7, 12);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 20, 30);
    SZrAstNode *body = block_with_statement(bodyStmt, 16, 34);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *bodyFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    bodyFact = reachability_fact_at(context, bodyStmt);
    TEST_ASSERT_NOT_NULL(bodyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, bodyFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, bodyFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, bodyFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* true && false 应淘汰 then，验证逻辑表达式可作为 if 的常量条件。 */
static void test_cfg_folds_logical_and_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_TRUE, 4, 8);
    SZrAstNode *right = boolean_literal(ZR_FALSE, 12, 17);
    SZrAstNode *condition = logical_expression(left, "&&", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* false || false 保持循环入口可退出，并将循环体归因为条件假。 */
static void test_cfg_folds_logical_or_false_while_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_FALSE, 8, 13);
    SZrAstNode *right = boolean_literal(ZR_FALSE, 17, 22);
    SZrAstNode *condition = logical_expression(left, "||", right, 8, 22);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 30, 40);
    SZrAstNode *body = block_with_statement(bodyStmt, 26, 44);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *bodyFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    bodyFact = reachability_fact_at(context, bodyStmt);
    TEST_ASSERT_NOT_NULL(bodyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, bodyFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, bodyFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, bodyFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* false && 未知标识符仍可确定整体为假；未知右项不应保留 then 路径。 */
static void test_cfg_folds_short_circuit_false_and_unknown_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_FALSE, 4, 9);
    SZrAstNode *right = identifier_node("flag", 13, 17);
    SZrAstNode *condition = logical_expression(left, "&&", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* true || 未知标识符仍可确定整体为真；未知右项不应保留 else 路径。 */
static void test_cfg_folds_short_circuit_true_or_unknown_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = boolean_literal(ZR_TRUE, 4, 8);
    SZrAstNode *right = identifier_node("flag", 12, 16);
    SZrAstNode *condition = logical_expression(left, "||", right, 4, 16);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 24, 32);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 20, 36);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 44, 60);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 不等的整数常量淘汰 then，核对比较折叠与不可达事实的连接。 */
static void test_cfg_folds_integer_equality_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = integer_literal(1, 4, 5);
    SZrAstNode *right = integer_literal(2, 9, 10);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 10);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 18, 26);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 42, 50);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 14, 30);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 38, 54);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 常假的整数关系条件应阻止进入 while 体，并保留条件节点为诊断来源。 */
static void test_cfg_folds_integer_relational_false_while_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = integer_literal(1, 8, 9);
    SZrAstNode *right = integer_literal(0, 12, 13);
    SZrAstNode *condition = binary_expression(left, "<", right, 8, 13);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 21, 31);
    SZrAstNode *body = block_with_statement(bodyStmt, 17, 35);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *bodyFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    bodyFact = reachability_fact_at(context, bodyStmt);
    TEST_ASSERT_NOT_NULL(bodyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, bodyFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, bodyFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, bodyFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 不同字符串内容的比较应淘汰 then；此处只验证 CFG 的局部常量判断。 */
static void test_cfg_folds_string_equality_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = string_literal("red", 4, 9);
    SZrAstNode *right = string_literal("blue", 13, 19);
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
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 相同字符的 != 结果为假；循环体应带 CONDITION_FALSE 原因。 */
static void test_cfg_folds_char_inequality_false_while_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = char_literal('a', 8, 11);
    SZrAstNode *right = char_literal('a', 15, 18);
    SZrAstNode *condition = binary_expression(left, "!=", right, 8, 18);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 26, 36);
    SZrAstNode *body = block_with_statement(bodyStmt, 22, 40);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *bodyFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    bodyFact = reachability_fact_at(context, bodyStmt);
    TEST_ASSERT_NOT_NULL(bodyFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, bodyFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, bodyFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, bodyFact->causeNode);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 双精度常量的 >= 结果为假；验证浮点比较能驱动 if 分支剔除。 */
static void test_cfg_folds_float_relational_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = float_literal(1.5, 4, 7);
    SZrAstNode *right = float_literal(2.5, 11, 14);
    SZrAstNode *condition = binary_expression(left, ">=", right, 4, 14);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 22, 30);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 46, 54);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 18, 34);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 42, 58);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 跨类型字面量相等比较在 CFG 局部路径折为假；夹具不检验源语言类型合法性。 */
static void test_cfg_folds_mixed_kind_equality_false_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = integer_literal(1, 4, 5);
    SZrAstNode *right = string_literal("1", 9, 12);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 12);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 20, 28);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 44, 52);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 16, 32);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 40, 56);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    thenFact = reachability_fact_at(context, thenStmt);
    elseFact = reachability_fact_at(context, elseStmt);
    TEST_ASSERT_NOT_NULL(thenFact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, thenFact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, thenFact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, thenFact->causeNode);
    TEST_ASSERT_NULL(elseFact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 先折叠整数加法再比较；缺少该折叠会使 else 的不可达事实缺失。 */
static void test_cfg_folds_integer_addition_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *sum = binary_expression(integer_literal(1, 5, 6),
                                        "+",
                                        integer_literal(1, 9, 10),
                                        5,
                                        10);
    SZrAstNode *right = integer_literal(2, 15, 16);
    SZrAstNode *condition = binary_expression(sum, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 检验减法结果进入比较条件，保证 CFG 能淘汰恒假的 else。 */
static void test_cfg_folds_integer_subtraction_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *difference = binary_expression(integer_literal(3, 5, 6),
                                               "-",
                                               integer_literal(1, 9, 10),
                                               5,
                                               10);
    SZrAstNode *right = integer_literal(2, 15, 16);
    SZrAstNode *condition = binary_expression(difference, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 检验乘法的安全常量结果可继续参与相等判断与分支剔除。 */
static void test_cfg_folds_integer_multiplication_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *product = binary_expression(integer_literal(2, 5, 6),
                                            "*",
                                            integer_literal(3, 9, 10),
                                            5,
                                            10);
    SZrAstNode *right = integer_literal(6, 15, 16);
    SZrAstNode *condition = binary_expression(product, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 非零除数的整除常量可驱动分支选择；这里只覆盖可定义的算术输入。 */
static void test_cfg_folds_integer_division_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *quotient = binary_expression(integer_literal(9, 5, 6),
                                             "/",
                                             integer_literal(3, 9, 10),
                                             5,
                                             10);
    SZrAstNode *right = integer_literal(3, 15, 16);
    SZrAstNode *condition = binary_expression(quotient, "==", right, 4, 17);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 25, 33);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 49, 57);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 21, 37);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 45, 61);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 非零除数的取余常量可驱动分支选择，防止该运算被误判为未知。 */
static void test_cfg_folds_integer_modulo_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *remainder = binary_expression(integer_literal(10, 5, 7),
                                              "%",
                                              integer_literal(4, 10, 11),
                                              5,
                                              11);
    SZrAstNode *right = integer_literal(2, 16, 17);
    SZrAstNode *condition = binary_expression(remainder, "==", right, 4, 18);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 26, 34);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 50, 58);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 22, 38);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 46, 62);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 算术折叠结果仍应进入关系比较，而不局限于与字面量做相等判断。 */
static void test_cfg_folds_folded_integer_relational_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *sum = binary_expression(integer_literal(1, 5, 6),
                                        "+",
                                        integer_literal(2, 9, 10),
                                        5,
                                        10);
    SZrAstNode *right = integer_literal(2, 14, 15);
    SZrAstNode *condition = binary_expression(sum, ">", right, 4, 16);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 24, 32);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 56);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 20, 36);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 44, 60);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 安全的一元取负应产生可比较的整数常量，淘汰恒假的 else。 */
static void test_cfg_folds_integer_unary_minus_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = unary_expression("-",
                                        integer_literal(3, 6, 7),
                                        5,
                                        7);
    SZrAstNode *right = integer_literal(-3, 12, 14);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 15);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 23, 31);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 47, 55);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 19, 35);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 43, 59);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 一元正号保持数值不变；CFG 应继续使用其结果确定 if 路径。 */
static void test_cfg_folds_integer_unary_plus_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = unary_expression("+",
                                        integer_literal(3, 6, 7),
                                        5,
                                        7);
    SZrAstNode *right = integer_literal(3, 12, 13);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 14);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 22, 30);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 46, 54);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 18, 34);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 42, 58);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* 位取反的确定结果应进入相等判断，避免把所有一元运算视为未知。 */
static void test_cfg_folds_integer_bitwise_not_equality_true_if_condition(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = unary_expression("~",
                                        integer_literal(0, 6, 7),
                                        5,
                                        7);
    SZrAstNode *right = integer_literal(-1, 12, 14);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 15);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 23, 31);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 47, 55);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 19, 35);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 43, 59);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *thenFact;
    const SZrSemanticReachabilityFact *elseFact;

    TEST_ASSERT_NOT_NULL(context);
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

/* -INT64_MIN 不可表示；CFG 必须保留两支可达，防止以溢出结果错误裁剪路径。 */
static void test_cfg_keeps_overflowed_integer_unary_minus_condition_unknown(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *left = unary_expression("-",
                                        integer_literal(INT64_MIN, 6, 15),
                                        5,
                                        15);
    SZrAstNode *right = integer_literal(INT64_MIN, 20, 29);
    SZrAstNode *condition = binary_expression(left, "==", right, 4, 30);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 38, 46);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 62, 70);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 34, 50);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 58, 74);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    TEST_ASSERT_NULL(reachability_fact_at(context, thenStmt));
    TEST_ASSERT_NULL(reachability_fact_at(context, elseStmt));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* Unity 显式注册全部常量条件场景；CMake 目标把本入口纳入语言流水线测试。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_folds_unary_not_false_if_condition);
    RUN_TEST(test_cfg_folds_unary_not_true_while_condition);
    RUN_TEST(test_cfg_folds_logical_and_false_if_condition);
    RUN_TEST(test_cfg_folds_logical_or_false_while_condition);
    RUN_TEST(test_cfg_folds_short_circuit_false_and_unknown_if_condition);
    RUN_TEST(test_cfg_folds_short_circuit_true_or_unknown_if_condition);
    RUN_TEST(test_cfg_folds_integer_equality_false_if_condition);
    RUN_TEST(test_cfg_folds_integer_relational_false_while_condition);
    RUN_TEST(test_cfg_folds_string_equality_false_if_condition);
    RUN_TEST(test_cfg_folds_char_inequality_false_while_condition);
    RUN_TEST(test_cfg_folds_float_relational_false_if_condition);
    RUN_TEST(test_cfg_folds_mixed_kind_equality_false_if_condition);
    RUN_TEST(test_cfg_folds_integer_addition_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_subtraction_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_multiplication_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_division_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_modulo_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_folded_integer_relational_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_unary_plus_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_unary_minus_equality_true_if_condition);
    RUN_TEST(test_cfg_folds_integer_bitwise_not_equality_true_if_condition);
    RUN_TEST(test_cfg_keeps_overflowed_integer_unary_minus_condition_unknown);
    return UNITY_END();
}
