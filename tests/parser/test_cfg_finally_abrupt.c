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

static SZrAstNode *boolean_literal(TZrBool value, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *literal = test_node(ZR_AST_BOOLEAN_LITERAL, startOffset, endOffset);

    literal->data.booleanLiteral.value = value;
    return literal;
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cfg_reaches_finally_body_after_try_return);
    RUN_TEST(test_cfg_keeps_statement_after_try_return_with_finally_unreachable);
    RUN_TEST(test_cfg_reaches_finally_body_after_try_break);
    RUN_TEST(test_cfg_reaches_finally_body_after_try_continue);
    return UNITY_END();
}
