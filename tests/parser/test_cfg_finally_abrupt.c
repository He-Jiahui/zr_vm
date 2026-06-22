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
    range.source = ZrCore_String_Create(g_state, "cfg_finally_abrupt_test.zr", 27);
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
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 96);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

static SZrAstNode *script_with_statements(SZrAstNode *first, SZrAstNode *second) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 112);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, first);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, second);
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

static SZrAstNode *try_statement_with_finally(SZrAstNode *body, SZrAstNode *finallyBody) {
    SZrAstNode *tryNode = test_node(ZR_AST_TRY_CATCH_FINALLY_STATEMENT, 0, 80);

    tryNode->data.tryCatchFinallyStatement.block = body;
    tryNode->data.tryCatchFinallyStatement.finallyBlock = finallyBody;
    return tryNode;
}

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
        if (block->successors[index] == successorId) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

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
            SZrParserCfgBlock *successor = block_at(cfg, block->successors[index]);

            if (successor != ZR_NULL && successor->kind == kind) {
                return successor->id;
            }
        }
    }

    return ZR_PARSER_CFG_INVALID_BLOCK_ID;
}

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
            SZrParserCfgBlock *successor = block_at(cfg, block->successors[index]);

            if (successor != ZR_NULL && successor->kind == kind) {
                count++;
            }
        }
    }

    return count;
}

static void test_cfg_reaches_finally_body_after_try_return(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrParserCfg cfg;
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 8, 14);
    SZrAstNode *tryBody = block_with_statement(returnStmt, 6, 18);
    SZrAstNode *finallyStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 40, 46);
    SZrAstNode *finallyBody = block_with_statement(finallyStmt, 36, 50);
    SZrAstNode *tryNode = try_statement_with_finally(tryBody, finallyBody);
    SZrAstNode *script = script_with_statement(tryNode);

    TEST_ASSERT_NOT_NULL(context);
    ZrParser_Cfg_Init(g_state, &cfg);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));
    TEST_ASSERT_TRUE(ZrParser_Cfg_EmitReachabilityFacts(context, &cfg));
    TEST_ASSERT_NULL(reachability_fact_at(context, finallyStmt));

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

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

    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
    ZrParser_SemanticContext_Free(context);
}

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
