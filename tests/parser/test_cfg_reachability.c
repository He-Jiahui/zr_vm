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

/* 直接构造最小 AST 来隔离 CFG 构建与事实发布；合成偏移只用于定位诊断，不代表真实源码。 */
static SZrState *g_state;

/* Unity 每个用例都取得独立 VM 状态，AST 节点和文件名字符串不能跨用例借用。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* BUG: 构造 AST 后的断言失败会 longjmp 跳过用例末尾的 Ast_Free；
 * Unity 随后调用 tearDown，但状态销毁不持有 RawMalloc 节点指针，故泄漏。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 位置查询会重新创建同名 source；事实查找按字符串内容匹配来源，再用偏移定位。 */
static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, "cfg_reachability_test.zr", 24);
    return range;
}

/* 构造可由根 AST 递归释放的测试节点；不经过 parser，调用者须填好被测 CFG 所需字段。 */
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

/* 根节点拥有传入语句，统一交给 Ast_Free；双语句形式专门观察前句终止后的后句。 */
static SZrAstNode *script_with_statements(SZrAstNode *first, SZrAstNode *second) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 24);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, first);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, second);
    return script;
}

/* 单语句脚本把控制结构作为 CFG 的根入口，避免顶层其它语句影响连边。 */
static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 64);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

/* CFG 按 block.body 展开语句；isStatement 保持测试 AST 的语句块形态，并非 CFG 展开的门槛。 */
static SZrAstNode *block_with_statement(SZrAstNode *statement, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, statement);
    return block;
}

/* 双语句块用于检查 return/break 终止后仍保留不可达节点及原始位置。 */
static SZrAstNode *block_with_statements(SZrAstNode *first,
                                         SZrAstNode *second,
                                         TZrSize startOffset,
                                         TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, first);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, second);
    return block;
}

/* 常量节点直接喂给 CFG 折叠分支；本组不验证词法、解析或表达式求值。 */
static SZrAstNode *boolean_literal(TZrBool value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_BOOLEAN_LITERAL, startOffset, endOffset);

    literal->data.booleanLiteral.value = value;
    return literal;
}

static SZrAstNode *integer_literal(TZrInt64 value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_INTEGER_LITERAL, startOffset, endOffset);

    literal->data.integerLiteral.value = value;
    return literal;
}

static SZrAstNode *float_literal(TZrDouble value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_FLOAT_LITERAL, startOffset, endOffset);

    literal->data.floatLiteral.value = value;
    literal->data.floatLiteral.isSingle = ZR_FALSE;
    return literal;
}

static SZrAstNode *string_literal(const TZrChar *value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_STRING_LITERAL, startOffset, endOffset);

    literal->data.stringLiteral.value = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)value);
    TEST_ASSERT_NOT_NULL(literal->data.stringLiteral.value);
    return literal;
}

static SZrAstNode *char_literal(TZrChar value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_CHAR_LITERAL, startOffset, endOffset);

    literal->data.charLiteral.value = value;
    return literal;
}

/* 控制结构只填 CFG 使用的关系和 statement 标志，避免语义分析引入额外路径。 */
static SZrAstNode *if_statement(SZrAstNode *condition, SZrAstNode *thenBlock, SZrAstNode *elseBlock) {
    SZrAstNode *ifNode = test_node(ZR_AST_IF_EXPRESSION, 0, 64);

    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = thenBlock;
    ifNode->data.ifExpression.elseExpr = elseBlock;
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    return ifNode;
}

static SZrAstNode *while_statement(SZrAstNode *condition, SZrAstNode *body) {
    SZrAstNode *whileNode = test_node(ZR_AST_WHILE_LOOP, 0, 48);

    whileNode->data.whileLoop.cond = condition;
    whileNode->data.whileLoop.block = body;
    whileNode->data.whileLoop.isStatement = ZR_TRUE;
    return whileNode;
}

/* 缺省 step 的 for 与显式 step 分开构造，测试 continue 的实际目标是否随之改变。 */
static SZrAstNode *for_statement(SZrAstNode *condition, SZrAstNode *body) {
    SZrAstNode *forNode = test_node(ZR_AST_FOR_LOOP, 0, 56);

    forNode->data.forLoop.cond = condition;
    forNode->data.forLoop.block = body;
    forNode->data.forLoop.isStatement = ZR_TRUE;
    return forNode;
}

static SZrAstNode *for_statement_with_step(SZrAstNode *condition,
                                           SZrAstNode *step,
                                           SZrAstNode *body) {
    SZrAstNode *forNode = for_statement(condition, body);

    forNode->data.forLoop.step = step;
    return forNode;
}

/* foreach 的迭代源在这些测试里无关；仅检验循环头、退出 join 与 abrupt 边。 */
static SZrAstNode *foreach_statement(SZrAstNode *body) {
    SZrAstNode *foreachNode = test_node(ZR_AST_FOREACH_LOOP, 0, 56);

    foreachNode->data.foreachLoop.block = body;
    foreachNode->data.foreachLoop.isStatement = ZR_TRUE;
    return foreachNode;
}

/* try/catch/finally 使用结构形状驱动 CFG；测试不执行异常，也不推断 catch 类型。 */
static SZrAstNode *try_statement(SZrAstNode *body) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 64);

    tryNode->data.tryCatchFinallyStatement.block = body;
    return tryNode;
}

static SZrAstNode *catch_clause(SZrAstNode *body) {
    SZrAstNode *catchNode = test_node(ZR_AST_CATCH_CLAUSE, 40, 72);

    catchNode->data.catchClause.block = body;
    return catchNode;
}

static SZrAstNode *try_statement_with_catch(SZrAstNode *body, SZrAstNode *catchNode) {
    SZrAstNode *tryNode = try_statement(body);

    tryNode->data.tryCatchFinallyStatement.catchClauses = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(tryNode->data.tryCatchFinallyStatement.catchClauses);
    ZrParser_AstNodeArray_Add(g_state, tryNode->data.tryCatchFinallyStatement.catchClauses, catchNode);
    return tryNode;
}

static SZrAstNode *try_statement_with_finally(SZrAstNode *body, SZrAstNode *finallyBody) {
    SZrAstNode *tryNode = try_statement(body);

    tryNode->data.tryCatchFinallyStatement.finallyBlock = finallyBody;
    return tryNode;
}

/* switch case/default 的 AST 关系决定常量选择器可走的边，未命中分支仍须保留供诊断。 */
static SZrAstNode *switch_case_node(SZrAstNode *value, SZrAstNode *body) {
    SZrAstNode *caseNode = test_node(ZR_AST_SWITCH_CASE, 24, 72);

    caseNode->data.switchCase.value = value;
    caseNode->data.switchCase.block = body;
    return caseNode;
}

static SZrAstNode *switch_default_node(SZrAstNode *body) {
    SZrAstNode *defaultNode = test_node(ZR_AST_SWITCH_DEFAULT, 76, 96);

    defaultNode->data.switchDefault.block = body;
    return defaultNode;
}

static SZrAstNode *switch_statement_with_case(SZrAstNode *expr, SZrAstNode *caseNode) {
    SZrAstNode *switchNode = test_node(ZR_AST_SWITCH_EXPRESSION, 0, 80);

    switchNode->data.switchExpression.expr = expr;
    switchNode->data.switchExpression.cases = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(switchNode->data.switchExpression.cases);
    ZrParser_AstNodeArray_Add(g_state, switchNode->data.switchExpression.cases, caseNode);
    switchNode->data.switchExpression.isStatement = ZR_TRUE;
    return switchNode;
}

static SZrAstNode *switch_statement_with_case_and_default(SZrAstNode *expr,
                                                         SZrAstNode *caseNode,
                                                         SZrAstNode *defaultNode) {
    SZrAstNode *switchNode = switch_statement_with_case(expr, caseNode);

    switchNode->data.switchExpression.defaultCase = defaultNode;
    return switchNode;
}

static SZrAstNode *switch_statement_with_two_cases_and_default(SZrAstNode *expr,
                                                              SZrAstNode *firstCase,
                                                              SZrAstNode *secondCase,
                                                              SZrAstNode *defaultNode) {
    SZrAstNode *switchNode = test_node(ZR_AST_SWITCH_EXPRESSION, 0, 104);

    switchNode->data.switchExpression.expr = expr;
    switchNode->data.switchExpression.cases = ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(switchNode->data.switchExpression.cases);
    ZrParser_AstNodeArray_Add(g_state, switchNode->data.switchExpression.cases, firstCase);
    ZrParser_AstNodeArray_Add(g_state, switchNode->data.switchExpression.cases, secondCase);
    switchNode->data.switchExpression.defaultCase = defaultNode;
    switchNode->data.switchExpression.isStatement = ZR_TRUE;
    return switchNode;
}

/* 仅供简单循环断言：同一 AST 在 finally 展开时可能对应多个 CFG 块，不能据此推断唯一性。 */
static const SZrParserCfgBlock *find_block_for_statement(SZrParserCfg *cfg,
                                                         SZrAstNode *statement) {
    TZrSize index;

    if (cfg == ZR_NULL || statement == ZR_NULL || !cfg->blocks.isValid) {
        return ZR_NULL;
    }

    for (index = 0; index < cfg->blocks.length; index++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);
        if (block != ZR_NULL && block->statement == statement) {
            return block;
        }
    }

    return ZR_NULL;
}

/* 事实查询同时锁定不可达状态、原因节点及范围；单纯找不到事实表示该位置仍可达。 */
static void test_cfg_marks_statement_after_return_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 0, 7);
    SZrAstNode *nextStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 8, 18);
    SZrAstNode *script = script_with_statements(returnStmt, nextStmt);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_EQUAL_UINT32(4, (TZrUInt32)cfg.blocks.length);
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(10, 10));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_AFTER_RETURN, fact->cause);
    TEST_ASSERT_EQUAL_PTR(returnStmt, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(nextStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* return 仍须连到函数 exit，后继语句则不能借此获得来自 entry 的路径。 */
static void test_cfg_connects_return_terminator_to_exit(void) {
    SZrParserCfg cfg;
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 0, 7);
    SZrAstNode *nextStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 8, 18);
    SZrAstNode *script = script_with_statements(returnStmt, nextStmt);
    const SZrParserCfgBlock *returnBlock;
    const SZrParserCfgBlock *exitBlock;

    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    returnBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, 1);
    exitBlock = (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, cfg.exitBlockId);

    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_NOT_NULL(exitBlock);
    TEST_ASSERT_TRUE(returnBlock->isTerminator);
    TEST_ASSERT_EQUAL_UINT32(1, returnBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(cfg.exitBlockId, returnBlock->successors[0]);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

static void test_cfg_leaves_statement_after_expression_reachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *firstStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 0, 5);
    SZrAstNode *nextStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 6, 12);
    SZrAstNode *script = script_with_statements(firstStmt, nextStmt);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(8, 8));
    TEST_ASSERT_NULL(fact);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 常量条件应剪除错误分支，同时保留被剪语句以发布带条件节点的诊断。 */
static void test_cfg_marks_false_branch_of_constant_true_if_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 4, 8);
    SZrAstNode *thenStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 16, 24);
    SZrAstNode *elseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 48);
    SZrAstNode *thenBlock = block_with_statement(thenStmt, 10, 28);
    SZrAstNode *elseBlock = block_with_statement(elseStmt, 34, 52);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, elseBlock);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(42, 42));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(elseStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_constant_false_while_body_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_FALSE, 6, 11);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 18, 28);
    SZrAstNode *body = block_with_statement(bodyStmt, 14, 32);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(20, 20));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, fact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(bodyStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_constant_false_for_body_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_FALSE, 8, 13);
    SZrAstNode *bodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 24, 34);
    SZrAstNode *body = block_with_statement(bodyStmt, 20, 38);
    SZrAstNode *forNode = for_statement(condition, body);
    SZrAstNode *script = script_with_statement(forNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(26, 26));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE, fact->cause);
    TEST_ASSERT_EQUAL_PTR(condition, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(bodyStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_statement_after_foreach_break_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 18, 24);
    SZrAstNode *afterBreakStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 26, 36);
    SZrAstNode *body = block_with_statements(breakStmt, afterBreakStmt, 14, 40);
    SZrAstNode *foreachNode = foreach_statement(body);
    SZrAstNode *script = script_with_statement(foreachNode);
    const SZrSemanticReachabilityFact *fact;

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(28, 28));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_AFTER_BREAK, fact->cause);
    TEST_ASSERT_EQUAL_PTR(breakStmt, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(afterBreakStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 测试 break/continue 的边目标而非只测事实：错误目标也可能碰巧保留部分诊断。 */
static void test_cfg_connects_while_break_to_loop_join(void) {
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 18, 24);
    SZrAstNode *body = block_with_statement(breakStmt, 14, 28);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrParserCfgBlock *whileBlock;
    const SZrParserCfgBlock *breakBlock;
    const SZrParserCfgBlock *breakTarget;

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;

    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    whileBlock = find_block_for_statement(&cfg, whileNode);
    breakBlock = find_block_for_statement(&cfg, breakStmt);
    TEST_ASSERT_NOT_NULL(whileBlock);
    TEST_ASSERT_NOT_NULL(breakBlock);
    TEST_ASSERT_TRUE(breakBlock->isTerminator);
    TEST_ASSERT_EQUAL_UINT32(1, whileBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1, breakBlock->successorCount);

    breakTarget = (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, breakBlock->successors[0]);
    TEST_ASSERT_NOT_NULL(breakTarget);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, breakTarget->kind);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

static void test_cfg_connects_while_continue_to_loop_header(void) {
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *continueStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 18, 27);
    SZrAstNode *body = block_with_statement(continueStmt, 14, 31);
    SZrAstNode *whileNode = while_statement(condition, body);
    SZrAstNode *script = script_with_statement(whileNode);
    const SZrParserCfgBlock *whileBlock;
    const SZrParserCfgBlock *continueBlock;

    continueStmt->data.breakContinueStatement.isBreak = ZR_FALSE;

    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    whileBlock = find_block_for_statement(&cfg, whileNode);
    continueBlock = find_block_for_statement(&cfg, continueStmt);
    TEST_ASSERT_NOT_NULL(whileBlock);
    TEST_ASSERT_NOT_NULL(continueBlock);
    TEST_ASSERT_TRUE(continueBlock->isTerminator);
    TEST_ASSERT_EQUAL_UINT32(1, continueBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(whileBlock->id, continueBlock->successors[0]);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

static void test_cfg_connects_for_break_to_loop_join(void) {
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 18, 24);
    SZrAstNode *body = block_with_statement(breakStmt, 14, 28);
    SZrAstNode *forNode = for_statement(condition, body);
    SZrAstNode *script = script_with_statement(forNode);
    const SZrParserCfgBlock *forBlock;
    const SZrParserCfgBlock *breakBlock;
    const SZrParserCfgBlock *breakTarget;

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;

    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    forBlock = find_block_for_statement(&cfg, forNode);
    breakBlock = find_block_for_statement(&cfg, breakStmt);
    TEST_ASSERT_NOT_NULL(forBlock);
    TEST_ASSERT_NOT_NULL(breakBlock);
    TEST_ASSERT_TRUE(breakBlock->isTerminator);
    TEST_ASSERT_EQUAL_UINT32(1, forBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1, breakBlock->successorCount);

    breakTarget = (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, breakBlock->successors[0]);
    TEST_ASSERT_NOT_NULL(breakTarget);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, breakTarget->kind);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

/* 无条件 for 不能凭空增加零迭代出口；break 是离开循环的唯一已建路径。 */
static void test_cfg_connects_unconditional_for_break_to_loop_join(void) {
    SZrParserCfg cfg;
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 18, 24);
    SZrAstNode *body = block_with_statement(breakStmt, 14, 28);
    SZrAstNode *forNode = for_statement(ZR_NULL, body);
    SZrAstNode *script = script_with_statement(forNode);
    const SZrParserCfgBlock *forBlock;
    const SZrParserCfgBlock *breakBlock;
    const SZrParserCfgBlock *breakTarget;

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;

    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    forBlock = find_block_for_statement(&cfg, forNode);
    breakBlock = find_block_for_statement(&cfg, breakStmt);
    TEST_ASSERT_NOT_NULL(forBlock);
    TEST_ASSERT_NOT_NULL(breakBlock);
    TEST_ASSERT_TRUE(breakBlock->isTerminator);
    TEST_ASSERT_EQUAL_UINT32(1, forBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1, breakBlock->successorCount);

    breakTarget = (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, breakBlock->successors[0]);
    TEST_ASSERT_NOT_NULL(breakTarget);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, breakTarget->kind);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

/* for 的 continue 先进入 step-entry join，执行 step 后才回到条件头。 */
static void test_cfg_connects_for_continue_to_step(void) {
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *continueStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 18, 27);
    SZrAstNode *stepStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 36, 42);
    SZrAstNode *body = block_with_statement(continueStmt, 14, 31);
    SZrAstNode *forNode = for_statement_with_step(condition, stepStmt, body);
    SZrAstNode *script = script_with_statement(forNode);
    const SZrParserCfgBlock *forBlock;
    const SZrParserCfgBlock *continueBlock;
    const SZrParserCfgBlock *continueTarget;
    const SZrParserCfgBlock *stepBlock;

    continueStmt->data.breakContinueStatement.isBreak = ZR_FALSE;

    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    forBlock = find_block_for_statement(&cfg, forNode);
    continueBlock = find_block_for_statement(&cfg, continueStmt);
    stepBlock = find_block_for_statement(&cfg, stepStmt);
    TEST_ASSERT_NOT_NULL(forBlock);
    TEST_ASSERT_NOT_NULL(continueBlock);
    TEST_ASSERT_NOT_NULL(stepBlock);
    TEST_ASSERT_TRUE(continueBlock->isTerminator);
    TEST_ASSERT_EQUAL_UINT32(1, continueBlock->successorCount);
    continueTarget =
            (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, continueBlock->successors[0]);
    TEST_ASSERT_NOT_NULL(continueTarget);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, continueTarget->kind);
    TEST_ASSERT_EQUAL_UINT32(1, continueTarget->successorCount);
    TEST_ASSERT_EQUAL_UINT32(stepBlock->id, continueTarget->successors[0]);
    TEST_ASSERT_EQUAL_UINT32(1, stepBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(forBlock->id, stepBlock->successors[0]);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

static void test_cfg_connects_foreach_break_to_loop_join(void) {
    SZrParserCfg cfg;
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 18, 24);
    SZrAstNode *body = block_with_statement(breakStmt, 14, 28);
    SZrAstNode *foreachNode = foreach_statement(body);
    SZrAstNode *script = script_with_statement(foreachNode);
    const SZrParserCfgBlock *foreachBlock;
    const SZrParserCfgBlock *breakBlock;
    const SZrParserCfgBlock *breakTarget;

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;

    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    foreachBlock = find_block_for_statement(&cfg, foreachNode);
    breakBlock = find_block_for_statement(&cfg, breakStmt);
    TEST_ASSERT_NOT_NULL(foreachBlock);
    TEST_ASSERT_NOT_NULL(breakBlock);
    TEST_ASSERT_TRUE(breakBlock->isTerminator);
    TEST_ASSERT_EQUAL_UINT32(2, foreachBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(1, breakBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(foreachBlock->successors[1], breakBlock->successors[0]);

    breakTarget = (const SZrParserCfgBlock *)ZrCore_Array_Get(&cfg.blocks, breakBlock->successors[0]);
    TEST_ASSERT_NOT_NULL(breakTarget);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_CFG_BLOCK_JOIN, breakTarget->kind);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

static void test_cfg_connects_foreach_continue_to_loop_header(void) {
    SZrParserCfg cfg;
    SZrAstNode *continueStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 18, 27);
    SZrAstNode *body = block_with_statement(continueStmt, 14, 31);
    SZrAstNode *foreachNode = foreach_statement(body);
    SZrAstNode *script = script_with_statement(foreachNode);
    const SZrParserCfgBlock *foreachBlock;
    const SZrParserCfgBlock *continueBlock;

    continueStmt->data.breakContinueStatement.isBreak = ZR_FALSE;

    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    foreachBlock = find_block_for_statement(&cfg, foreachNode);
    continueBlock = find_block_for_statement(&cfg, continueStmt);
    TEST_ASSERT_NOT_NULL(foreachBlock);
    TEST_ASSERT_NOT_NULL(continueBlock);
    TEST_ASSERT_TRUE(continueBlock->isTerminator);
    TEST_ASSERT_EQUAL_UINT32(1, continueBlock->successorCount);
    TEST_ASSERT_EQUAL_UINT32(foreachBlock->id, continueBlock->successors[0]);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

/* 嵌套保护区域仍须把 return 之后的语句归因于原 return，而非外层 try。 */
static void test_cfg_marks_statement_after_return_in_try_body_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 16, 22);
    SZrAstNode *afterReturnStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 24, 34);
    SZrAstNode *body = block_with_statements(returnStmt, afterReturnStmt, 12, 38);
    SZrAstNode *tryNode = try_statement(body);
    SZrAstNode *script = script_with_statement(tryNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(26, 26));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_AFTER_RETURN, fact->cause);
    TEST_ASSERT_EQUAL_PTR(returnStmt, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(afterReturnStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_statement_after_return_in_catch_body_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryBodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 12, 20);
    SZrAstNode *tryBody = block_with_statement(tryBodyStmt, 10, 24);
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 48, 54);
    SZrAstNode *afterReturnStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 56, 66);
    SZrAstNode *catchBody = block_with_statements(returnStmt, afterReturnStmt, 44, 70);
    SZrAstNode *catchNode = catch_clause(catchBody);
    SZrAstNode *tryNode = try_statement_with_catch(tryBody, catchNode);
    SZrAstNode *script = script_with_statement(tryNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(58, 58));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_AFTER_RETURN, fact->cause);
    TEST_ASSERT_EQUAL_PTR(returnStmt, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(afterReturnStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_statement_after_return_in_finally_body_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *tryBodyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 12, 20);
    SZrAstNode *tryBody = block_with_statement(tryBodyStmt, 10, 24);
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 48, 54);
    SZrAstNode *afterReturnStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 56, 66);
    SZrAstNode *finallyBody = block_with_statements(returnStmt, afterReturnStmt, 44, 70);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *script = script_with_statement(tryNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(58, 58));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_AFTER_RETURN, fact->cause);
    TEST_ASSERT_EQUAL_PTR(returnStmt, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(afterReturnStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* switch 与异常体一样，保留不可达 case 内语句，供位置事实追溯终止节点。 */
static void test_cfg_marks_statement_after_return_in_switch_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = boolean_literal(ZR_TRUE, 8, 12);
    SZrAstNode *caseValue = boolean_literal(ZR_TRUE, 28, 32);
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 40, 46);
    SZrAstNode *afterReturnStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 48, 58);
    SZrAstNode *caseBody = block_with_statements(returnStmt, afterReturnStmt, 36, 62);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *switchNode = switch_statement_with_case(expr, caseNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(50, 50));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_AFTER_RETURN, fact->cause);
    TEST_ASSERT_EQUAL_PTR(returnStmt, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(afterReturnStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 已知选择器只连接匹配 case；未匹配 case/default 的事实应指向选择器。 */
static void test_cfg_marks_non_matching_boolean_switch_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = boolean_literal(ZR_TRUE, 8, 12);
    SZrAstNode *caseValue = boolean_literal(ZR_FALSE, 28, 34);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *switchNode = switch_statement_with_case(expr, caseNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(42, 42));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(caseNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_non_matching_integer_switch_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = integer_literal(1, 8, 9);
    SZrAstNode *caseValue = integer_literal(2, 28, 29);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *switchNode = switch_statement_with_case(expr, caseNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(42, 42));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(caseNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_integer_switch_default_after_matching_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = integer_literal(1, 8, 9);
    SZrAstNode *caseValue = integer_literal(1, 28, 29);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *defaultStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *defaultBody = block_with_statement(defaultStmt, 82, 94);
    SZrAstNode *defaultNode = switch_default_node(defaultBody);
    SZrAstNode *switchNode = switch_statement_with_case_and_default(expr, caseNode, defaultNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(86, 86));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(defaultNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* TODO: 本例只断言后继不可达及范围，未核 cause/causeNode；需追踪匹配 case 的 return 归因。 */
static void test_cfg_marks_after_integer_switch_matching_return_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = integer_literal(1, 8, 9);
    SZrAstNode *caseValue = integer_literal(1, 28, 29);
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(returnStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *switchNode = switch_statement_with_case(expr, caseNode);
    SZrAstNode *afterSwitchStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *script = script_with_statements(switchNode, afterSwitchStmt);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(86, 86));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_UINT64(afterSwitchStmt->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_non_matching_string_switch_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = string_literal("red", 8, 13);
    SZrAstNode *caseValue = string_literal("blue", 28, 34);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *switchNode = switch_statement_with_case(expr, caseNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(42, 42));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(caseNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_string_switch_default_after_matching_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = string_literal("red", 8, 13);
    SZrAstNode *caseValue = string_literal("red", 28, 33);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *defaultStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *defaultBody = block_with_statement(defaultStmt, 82, 94);
    SZrAstNode *defaultNode = switch_default_node(defaultBody);
    SZrAstNode *switchNode = switch_statement_with_case_and_default(expr, caseNode, defaultNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(86, 86));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(defaultNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_non_matching_char_switch_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = char_literal('a', 8, 11);
    SZrAstNode *caseValue = char_literal('b', 28, 31);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *switchNode = switch_statement_with_case(expr, caseNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(42, 42));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(caseNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_char_switch_default_after_matching_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = char_literal('a', 8, 11);
    SZrAstNode *caseValue = char_literal('a', 28, 31);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *defaultStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *defaultBody = block_with_statement(defaultStmt, 82, 94);
    SZrAstNode *defaultNode = switch_default_node(defaultBody);
    SZrAstNode *switchNode = switch_statement_with_case_and_default(expr, caseNode, defaultNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(86, 86));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(defaultNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* TODO: 浮点 case 仅覆盖普通有限字面量；需检查 cfg_constants.c 对 NaN 和有符号零的等价规则。 */
static void test_cfg_marks_non_matching_float_switch_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = float_literal(1.5, 8, 11);
    SZrAstNode *caseValue = float_literal(2.5, 28, 31);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *switchNode = switch_statement_with_case(expr, caseNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(42, 42));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(caseNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

static void test_cfg_marks_float_switch_default_after_matching_case_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = float_literal(1.5, 8, 11);
    SZrAstNode *caseValue = float_literal(1.5, 28, 31);
    SZrAstNode *caseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *caseBody = block_with_statement(caseStmt, 36, 50);
    SZrAstNode *caseNode = switch_case_node(caseValue, caseBody);
    SZrAstNode *defaultStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *defaultBody = block_with_statement(defaultStmt, 82, 94);
    SZrAstNode *defaultNode = switch_default_node(defaultBody);
    SZrAstNode *switchNode = switch_statement_with_case_and_default(expr, caseNode, defaultNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(86, 86));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(defaultNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* TODO: 此例常量 true 已命中 case，只证明 default 无前驱；需用未知选择器核对布尔全集穷尽规则。 */
static void test_cfg_marks_exhaustive_boolean_switch_default_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *expr = boolean_literal(ZR_TRUE, 8, 12);
    SZrAstNode *trueCaseValue = boolean_literal(ZR_TRUE, 28, 32);
    SZrAstNode *trueCaseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 44);
    SZrAstNode *trueCaseBody = block_with_statement(trueCaseStmt, 36, 48);
    SZrAstNode *trueCase = switch_case_node(trueCaseValue, trueCaseBody);
    SZrAstNode *falseCaseValue = boolean_literal(ZR_FALSE, 52, 58);
    SZrAstNode *falseCaseStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 64, 68);
    SZrAstNode *falseCaseBody = block_with_statement(falseCaseStmt, 60, 72);
    SZrAstNode *falseCase = switch_case_node(falseCaseValue, falseCaseBody);
    SZrAstNode *defaultStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *defaultBody = block_with_statement(defaultStmt, 82, 94);
    SZrAstNode *defaultNode = switch_default_node(defaultBody);
    SZrAstNode *switchNode = switch_statement_with_two_cases_and_default(expr,
                                                                         trueCase,
                                                                         falseCase,
                                                                         defaultNode);
    SZrAstNode *script = script_with_statement(switchNode);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(context, test_range(78, 78));
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH, fact->cause);
    TEST_ASSERT_EQUAL_PTR(expr, fact->causeNode);
    TEST_ASSERT_EQUAL_UINT64(defaultNode->location.start.offset, fact->range.start.offset);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* Unity 负责在每次 RUN_TEST 前后调用 setUp/tearDown；这里注册全部控制流族用例。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_marks_statement_after_return_unreachable);
    RUN_TEST(test_cfg_connects_return_terminator_to_exit);
    RUN_TEST(test_cfg_leaves_statement_after_expression_reachable);
    RUN_TEST(test_cfg_marks_false_branch_of_constant_true_if_unreachable);
    RUN_TEST(test_cfg_marks_constant_false_while_body_unreachable);
    RUN_TEST(test_cfg_marks_constant_false_for_body_unreachable);
    RUN_TEST(test_cfg_marks_statement_after_foreach_break_unreachable);
    RUN_TEST(test_cfg_connects_while_break_to_loop_join);
    RUN_TEST(test_cfg_connects_while_continue_to_loop_header);
    RUN_TEST(test_cfg_connects_for_break_to_loop_join);
    RUN_TEST(test_cfg_connects_unconditional_for_break_to_loop_join);
    RUN_TEST(test_cfg_connects_for_continue_to_step);
    RUN_TEST(test_cfg_connects_foreach_break_to_loop_join);
    RUN_TEST(test_cfg_connects_foreach_continue_to_loop_header);
    RUN_TEST(test_cfg_marks_statement_after_return_in_try_body_unreachable);
    RUN_TEST(test_cfg_marks_statement_after_return_in_catch_body_unreachable);
    RUN_TEST(test_cfg_marks_statement_after_return_in_finally_body_unreachable);
    RUN_TEST(test_cfg_marks_statement_after_return_in_switch_case_unreachable);
    RUN_TEST(test_cfg_marks_non_matching_boolean_switch_case_unreachable);
    RUN_TEST(test_cfg_marks_non_matching_integer_switch_case_unreachable);
    RUN_TEST(test_cfg_marks_integer_switch_default_after_matching_case_unreachable);
    RUN_TEST(test_cfg_marks_after_integer_switch_matching_return_unreachable);
    RUN_TEST(test_cfg_marks_non_matching_string_switch_case_unreachable);
    RUN_TEST(test_cfg_marks_string_switch_default_after_matching_case_unreachable);
    RUN_TEST(test_cfg_marks_non_matching_char_switch_case_unreachable);
    RUN_TEST(test_cfg_marks_char_switch_default_after_matching_case_unreachable);
    RUN_TEST(test_cfg_marks_non_matching_float_switch_case_unreachable);
    RUN_TEST(test_cfg_marks_float_switch_default_after_matching_case_unreachable);
    RUN_TEST(test_cfg_marks_exhaustive_boolean_switch_default_unreachable);
    return UNITY_END();
}
