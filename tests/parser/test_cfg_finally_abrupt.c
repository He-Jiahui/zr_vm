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

/* 直接构造最小 AST 来锁定 CFG 的完成路径；这些用例不验证源码解析或实际执行 finally。 */
/* 每个 Unity 用例独占状态，使手造节点、CFG 和语义事实均在同一分配域内。 */
static SZrState *g_state;

/* Unity 在每个 RUN_TEST 前调用；测试内的原生分配均以此状态为所有者。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* BUG: 断言失败会由 Unity 的 longjmp 跳过测试末尾的 CFG、AST 和语义上下文释放；
 * 此处只销毁全局状态，未被显式释放的原生块随失败用例泄漏。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* TODO: 文件名字面量为 26 字节，传入的 27 将结尾 NUL 计入 SZrString；
 * 当前节点与查询都复用此工厂而仍可比较，需用真实解析位置核对外部来源匹配。 */
static SZrFileRange test_range(TZrSize startOffset, TZrSize endOffset) {
    SZrFileRange range;

    range.start.offset = startOffset;
    range.start.line = 1;
    range.start.column = (TZrInt32)startOffset + 1;
    range.end.offset = endOffset;
    range.end.line = 1;
    range.end.column = (TZrInt32)endOffset + 1;
    range.source = ZrCore_String_Create(g_state, "cfg_finally_abrupt_test.zr", 27);
    return range;
}

/* 手造节点只填 CFG 本组场景会读取的字段；最终由脚本根统一递归释放。 */
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

/* AST 数组接管传入语句，避免测试在检查 CFG 前另行释放子节点。 */
static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 96);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

/* 第二条脚本语句用于观察 return 经 finally 后的顺序路径仍不可达。 */
static SZrAstNode *script_with_statements(SZrAstNode *first, SZrAstNode *second) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 112);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, first);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, second);
    return script;
}

/* 以下块工厂保留语句属性和顺序，供 try、循环及条件体共享构造契约。 */
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

static SZrAstNode *block_with_two_statements(SZrAstNode *first,
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

static SZrAstNode *block_with_three_statements(SZrAstNode *first,
                                               SZrAstNode *second,
                                               SZrAstNode *third,
                                               TZrSize startOffset,
                                               TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 3);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    block->data.block.isStatement = ZR_TRUE;
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, first);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, second);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, third);
    return block;
}

static SZrAstNode *boolean_literal(TZrBool value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_BOOLEAN_LITERAL, startOffset, endOffset);

    literal->data.booleanLiteral.value = value;
    return literal;
}

/* 未绑定标识符保持条件未知，使 CFG 同时保留 break、continue 和正常分支。 */
static SZrAstNode *identifier_node(const TZrChar *name, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *identifier = test_node(ZR_AST_IDENTIFIER_LITERAL, startOffset, endOffset);

    identifier->data.identifier.name =
            ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name);
    TEST_ASSERT_NOT_NULL(identifier->data.identifier.name);
    return identifier;
}

static SZrAstNode *if_statement(SZrAstNode *condition, SZrAstNode *thenBlock) {
    SZrAstNode *ifNode = test_node(ZR_AST_IF_EXPRESSION, 0, 48);

    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = thenBlock;
    ifNode->data.ifExpression.elseExpr = ZR_NULL;
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    return ifNode;
}

static SZrAstNode *while_statement(SZrAstNode *condition, SZrAstNode *body) {
    SZrAstNode *whileNode = test_node(ZR_AST_WHILE_LOOP, 0, 96);

    whileNode->data.whileLoop.cond = condition;
    whileNode->data.whileLoop.block = body;
    whileNode->data.whileLoop.isStatement = ZR_TRUE;
    return whileNode;
}

/* 无 catch 的 try/finally 夹具专门隔离 abrupt completion 的清理路由。 */
static SZrAstNode *try_statement_with_finally(SZrAstNode *body, SZrAstNode *finallyBody) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 80);

    tryNode->data.tryCatchFinallyStatement.block = body;
    tryNode->data.tryCatchFinallyStatement.finallyBlock = finallyBody;
    return tryNode;
}

/* 事实表只收录不可达节点；查询落在合成范围内部，null 因而表示未被标为不可达。 */
static const SZrSemanticReachabilityFact *reachability_fact_at(SZrSemanticContext *context,
                                                               SZrAstNode *node) {
    return ZrParser_SemanticFacts_FindReachabilityAtPosition(
        context,
        test_range(node->location.start.offset + 1, node->location.start.offset + 1));
}

static SZrParserCfgBlock *block_at(SZrParserCfg *cfg, TZrUInt32 blockId) {
    if (cfg == ZR_NULL || !cfg->blocks.isValid ||
        blockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        blockId >= cfg->blocks.length) {
        return ZR_NULL;
    }

    return (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, blockId);
}

/* CFG 会为同一个 finally AST 节点建多份块，按节点身份查找比按范围更可靠。 */
static SZrParserCfgBlock *block_for_statement(SZrParserCfg *cfg, SZrAstNode *statement) {
    TZrSize index;

    if (cfg == ZR_NULL || !cfg->blocks.isValid || statement == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);

        if (block != ZR_NULL && block->statement == statement) {
            return block;
        }
    }

    return ZR_NULL;
}

static TZrBool block_has_successor(SZrParserCfgBlock *block, TZrUInt32 successorId) {
    TZrUInt32 index;

    if (block == ZR_NULL || successorId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }

    for (index = 0; index < block->successorCount; index++) {
        if (ZrParser_Cfg_BlockSuccessorIdAt(block, index) == successorId) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 边种类区分 return 接管与一般后继，单看目标块不能证明完成方式。 */
static TZrBool block_has_edge_kind_to(const SZrParserCfgBlock *block,
                                      EZrParserCfgEdgeKind kind,
                                      TZrUInt32 successorId) {
    TZrSize index;

    if (block == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < block->successorCount; index++) {
        const SZrParserCfgEdge *edge = ZrParser_Cfg_BlockEdgeAt(block, index);
        if (edge != ZR_NULL && edge->kind == kind &&
            edge->toBlockId == successorId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 检查预期的三份 finally 克隆；各副本的入口和出口仍需另行核对。 */
static TZrUInt32 statement_block_count(SZrParserCfg *cfg, SZrAstNode *statement) {
    TZrSize index;
    TZrUInt32 count = 0;

    if (cfg == ZR_NULL || !cfg->blocks.isValid || statement == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);

        if (block != ZR_NULL && block->statement == statement) {
            count++;
        }
    }

    return count;
}

/* 在所有克隆中找指定后继；break/continue 的目标不要求是首个 finally 副本。 */
static TZrBool statement_block_has_successor(SZrParserCfg *cfg,
                                             SZrAstNode *statement,
                                             TZrUInt32 successorId) {
    TZrSize index;

    if (cfg == ZR_NULL || !cfg->blocks.isValid || statement == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);

        if (block != ZR_NULL &&
            block->statement == statement &&
            block_has_successor(block, successorId)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 只检查指定的两个目标是否从同一克隆出发，供局部合流断言使用。 */
static TZrBool statement_block_has_both_successors(SZrParserCfg *cfg,
                                                   SZrAstNode *statement,
                                                   TZrUInt32 firstSuccessorId,
                                                   TZrUInt32 secondSuccessorId) {
    TZrSize index;

    if (cfg == ZR_NULL || !cfg->blocks.isValid || statement == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserCfgBlock *block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, index);

        if (block != ZR_NULL &&
            block->statement == statement &&
            block_has_successor(block, firstSuccessorId) &&
            block_has_successor(block, secondSuccessorId)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 挑选一个 JOIN 目标作局部结构断言；不能据此覆盖其它 finally 克隆。 */
static TZrUInt32 statement_block_first_successor_kind_id(SZrParserCfg *cfg,
                                                         SZrAstNode *statement,
                                                         EZrParserCfgBlockKind kind) {
    TZrUInt32 index;
    TZrSize blockIndex;

    if (cfg == ZR_NULL || !cfg->blocks.isValid || statement == ZR_NULL) {
        return ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }

    for (blockIndex = 0; blockIndex < cfg->blocks.length; blockIndex++) {
        SZrParserCfgBlock *block =
                (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, blockIndex);

        if (block == ZR_NULL || block->statement != statement) {
            continue;
        }
        for (index = 0; index < block->successorCount; index++) {
            SZrParserCfgBlock *successor = block_at(
                    cfg,
                    ZrParser_Cfg_BlockSuccessorIdAt(block, index));

            if (successor != ZR_NULL && successor->kind == kind) {
                return successor->id;
            }
        }
    }

    return ZR_PARSER_CFG_INVALID_BLOCK_ID;
}

/* 保留 JOIN 后继的出现次数，以便混合路径证明存在多个指向 JOIN 的完成边。 */
static TZrUInt32 statement_block_successor_kind_count(SZrParserCfg *cfg,
                                                      SZrAstNode *statement,
                                                      EZrParserCfgBlockKind kind) {
    TZrUInt32 index;
    TZrUInt32 count = 0;
    TZrSize blockIndex;

    if (cfg == ZR_NULL || !cfg->blocks.isValid || statement == ZR_NULL) {
        return 0;
    }

    for (blockIndex = 0; blockIndex < cfg->blocks.length; blockIndex++) {
        SZrParserCfgBlock *block =
                (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, blockIndex);

        if (block == ZR_NULL || block->statement != statement) {
            continue;
        }
        for (index = 0; index < block->successorCount; index++) {
            SZrParserCfgBlock *successor = block_at(
                    cfg,
                    ZrParser_Cfg_BlockSuccessorIdAt(block, index));

            if (successor != ZR_NULL && successor->kind == kind) {
                count++;
            }
        }
    }

    return count;
}

/* return 先进入 finally，再由其末端承接返回到函数出口；否则清理体会被跳过。 */
static void test_cfg_reaches_finally_body_after_try_return(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 8, 14);
    SZrAstNode *tryBody = block_with_statement(returnStmt, 6, 18);
    SZrAstNode *finallyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *finallyBody = block_with_statement(finallyStmt, 36, 50);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *script = script_with_statement(tryNode);
    SZrParserCfgBlock *returnBlock;
    SZrParserCfgBlock *finallyBlock;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, finallyStmt));
    returnBlock = block_for_statement(&cfg, returnStmt);
    finallyBlock = block_for_statement(&cfg, finallyStmt);
    TEST_ASSERT_NOT_NULL(returnBlock);
    TEST_ASSERT_NOT_NULL(finallyBlock);
    TEST_ASSERT_FALSE(block_has_edge_kind_to(
            returnBlock,
            ZR_PARSER_CFG_EDGE_RETURN,
            cfg.exitBlockId));
    TEST_ASSERT_TRUE(block_has_edge_kind_to(
            finallyBlock,
            ZR_PARSER_CFG_EDGE_RETURN,
            cfg.exitBlockId));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 即使 finally 本身可达，原 return 完成后 try 之后的普通语句仍须不可达。 */
static void test_cfg_keeps_statement_after_try_return_with_finally_unreachable(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 8, 14);
    SZrAstNode *tryBody = block_with_statement(returnStmt, 6, 18);
    SZrAstNode *finallyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *finallyBody = block_with_statement(finallyStmt, 36, 50);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *afterTryStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 84, 90);
    SZrAstNode *script = script_with_statements(tryNode, afterTryStmt);
    const SZrSemanticReachabilityFact *fact;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));

    fact = reachability_fact_at(context, afterTryStmt);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_REACHABILITY_UNREACHABLE, fact->state);

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* break 跳出循环前仍必须访问 finally；这里只检查事实，结构边由下一用例覆盖。 */
static void test_cfg_reaches_finally_body_after_try_break(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 20, 26);
    SZrAstNode *tryBody = block_with_statement(breakStmt, 18, 30);
    SZrAstNode *finallyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 56, 62);
    SZrAstNode *finallyBody = block_with_statement(finallyStmt, 52, 66);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *whileBody = block_with_statement(tryNode, 14, 72);
    SZrAstNode *whileNode = while_statement(condition, whileBody);
    SZrAstNode *script = script_with_statement(whileNode);

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, finallyStmt));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* continue 重进循环条件前仍必须访问 finally；对应边由结构用例继续验证。 */
static void test_cfg_reaches_finally_body_after_try_continue(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *continueStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 20, 29);
    SZrAstNode *tryBody = block_with_statement(continueStmt, 18, 33);
    SZrAstNode *finallyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 56, 62);
    SZrAstNode *finallyBody = block_with_statement(finallyStmt, 52, 66);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *whileBody = block_with_statement(tryNode, 14, 72);
    SZrAstNode *whileNode = while_statement(condition, whileBody);
    SZrAstNode *script = script_with_statement(whileNode);

    continueStmt->data.breakContinueStatement.isBreak = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, finallyStmt));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* break 不得直接进入选定的 JOIN 目标，必须让 finally 副本先完成该控制转移。 */
static void test_cfg_routes_try_break_through_finally_before_loop_join(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 20, 26);
    SZrAstNode *tryBody = block_with_statement(breakStmt, 18, 30);
    SZrAstNode *finallyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 56, 62);
    SZrAstNode *finallyBody = block_with_statement(finallyStmt, 52, 66);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *whileBody = block_with_statement(tryNode, 14, 72);
    SZrAstNode *whileNode = while_statement(condition, whileBody);
    SZrAstNode *script = script_with_statement(whileNode);
    SZrParserCfgBlock *whileBlock;
    SZrParserCfgBlock *breakBlock;
    SZrParserCfgBlock *finallyBlock;
    TZrUInt32 finallyTargetId;

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    whileBlock = block_for_statement(&cfg, whileNode);
    breakBlock = block_for_statement(&cfg, breakStmt);
    finallyBlock = block_for_statement(&cfg, finallyStmt);
    finallyTargetId = statement_block_first_successor_kind_id(&cfg,
                                                              finallyStmt,
                                                              ZR_PARSER_CFG_BLOCK_JOIN);

    TEST_ASSERT_NOT_NULL(whileBlock);
    TEST_ASSERT_NOT_NULL(breakBlock);
    TEST_ASSERT_NOT_NULL(finallyBlock);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_PARSER_CFG_INVALID_BLOCK_ID, finallyTargetId);
    TEST_ASSERT_FALSE(block_has_successor(breakBlock, finallyTargetId));
    TEST_ASSERT_TRUE(block_has_successor(finallyBlock, finallyTargetId));
    /* TODO: 此处只检查某个 JOIN 后继，未直接核对它就是 while 的 break 目标；
     * 常真条件没有退出边，需另设后继观察点来独立验证目标归属。 */

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* continue 不得从 try 直接回到循环头，避免跳过 finally 清理。 */
static void test_cfg_routes_try_continue_through_finally_before_loop_header(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *continueStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 20, 29);
    SZrAstNode *tryBody = block_with_statement(continueStmt, 18, 33);
    SZrAstNode *finallyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 56, 62);
    SZrAstNode *finallyBody = block_with_statement(finallyStmt, 52, 66);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *whileBody = block_with_statement(tryNode, 14, 72);
    SZrAstNode *whileNode = while_statement(condition, whileBody);
    SZrAstNode *script = script_with_statement(whileNode);
    SZrParserCfgBlock *whileBlock;
    SZrParserCfgBlock *continueBlock;
    SZrParserCfgBlock *finallyBlock;

    continueStmt->data.breakContinueStatement.isBreak = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    whileBlock = block_for_statement(&cfg, whileNode);
    continueBlock = block_for_statement(&cfg, continueStmt);
    finallyBlock = block_for_statement(&cfg, finallyStmt);

    TEST_ASSERT_NOT_NULL(whileBlock);
    TEST_ASSERT_NOT_NULL(continueBlock);
    TEST_ASSERT_NOT_NULL(finallyBlock);
    TEST_ASSERT_FALSE(block_has_successor(continueBlock, whileBlock->id));
    TEST_ASSERT_TRUE(block_has_successor(finallyBlock, whileBlock->id));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 三种完成方式共用一个源码 finally 节点，但 CFG 应为各路径保留独立副本。 */
static void test_cfg_builds_mixed_normal_break_continue_finally_paths(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *condition = boolean_literal(ZR_TRUE, 6, 10);
    SZrAstNode *breakCondition = identifier_node("shouldBreak", 22, 33);
    SZrAstNode *breakStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 37, 43);
    SZrAstNode *breakBlock = block_with_statement(breakStmt, 35, 45);
    SZrAstNode *breakIf = if_statement(breakCondition, breakBlock);
    SZrAstNode *continueCondition = identifier_node("shouldContinue", 49, 63);
    SZrAstNode *continueStmt = test_node(ZR_AST_BREAK_CONTINUE_STATEMENT, 67, 76);
    SZrAstNode *continueBlock = block_with_statement(continueStmt, 65, 78);
    SZrAstNode *continueIf = if_statement(continueCondition, continueBlock);
    SZrAstNode *normalStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 82, 88);
    SZrAstNode *tryBody = block_with_three_statements(breakIf, continueIf, normalStmt, 18, 90);
    SZrAstNode *finallyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 104, 110);
    SZrAstNode *finallyBody = block_with_statement(finallyStmt, 100, 114);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *afterTryStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 118, 124);
    SZrAstNode *whileBody = block_with_two_statements(tryNode, afterTryStmt, 14, 128);
    SZrAstNode *whileNode = while_statement(condition, whileBody);
    SZrAstNode *script = script_with_statement(whileNode);
    /* TODO: 此混合夹具的父节点固定范围短于 finally/afterTry 子节点；
     * 本例的事实查询只检查子节点位置，需核查父范围相关的语义查询和诊断。 */
    SZrParserCfgBlock *whileBlock;

    breakStmt->data.breakContinueStatement.isBreak = ZR_TRUE;
    continueStmt->data.breakContinueStatement.isBreak = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, finallyStmt));
    TEST_ASSERT_NULL(reachability_fact_at(context, afterTryStmt));

    whileBlock = block_for_statement(&cfg, whileNode);
    TEST_ASSERT_NOT_NULL(whileBlock);
    TEST_ASSERT_EQUAL_UINT32(3, statement_block_count(&cfg, finallyStmt));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            2,
            statement_block_successor_kind_count(&cfg,
                                                 finallyStmt,
                                                 ZR_PARSER_CFG_BLOCK_JOIN));
    TEST_ASSERT_TRUE(statement_block_has_successor(&cfg, finallyStmt, whileBlock->id));
    /* TODO: 此负向断言只采用首个 JOIN 目标；需逐个检查所有克隆及目标，
     * 才能排除某个非首个 JOIN 与 continue 回边在同一副本中错误合流。 */
    TEST_ASSERT_FALSE(statement_block_has_both_successors(&cfg,
                                                          finallyStmt,
                                                          statement_block_first_successor_kind_id(
                                                                  &cfg,
                                                                  finallyStmt,
                                                                  ZR_PARSER_CFG_BLOCK_JOIN),
                                                          whileBlock->id));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

/* 由 CMake 的 zr_vm_cfg_finally_abrupt_test 入口执行七个互补回归场景。
 * TODO: CFG abrupt 路由还包括 throw；本入口与相邻 catch 回归未覆盖
 * throw 经 finally 的出口边归属，后续需补该路径的定向场景。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_reaches_finally_body_after_try_return);
    RUN_TEST(test_cfg_keeps_statement_after_try_return_with_finally_unreachable);
    RUN_TEST(test_cfg_reaches_finally_body_after_try_break);
    RUN_TEST(test_cfg_reaches_finally_body_after_try_continue);
    RUN_TEST(test_cfg_routes_try_break_through_finally_before_loop_join);
    RUN_TEST(test_cfg_routes_try_continue_through_finally_before_loop_header);
    RUN_TEST(test_cfg_builds_mixed_normal_break_continue_finally_paths);
    return UNITY_END();
}
