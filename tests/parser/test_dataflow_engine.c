#include "unity.h"

#include <string.h>

#include "dataflow.h"
#include "dataflow_definite_assignment.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/parser.h"

typedef struct SDataflowVisitLog {
    SZrAstNode *nodes[8];
    TZrSize count;
} SDataflowVisitLog;

typedef struct SDefiniteAssignmentHarness {
    SZrAstNode *assignmentStatement;
    TZrSize symbolCount;
    TZrSize symbolIndex;
} SDefiniteAssignmentHarness;

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
    range.source = ZrCore_String_Create(g_state, "dataflow_test.zr", 16);
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

static SZrAstNode *script_with_statements(SZrAstNode *first, SZrAstNode *second) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 24);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, first);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, second);
    return script;
}

static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 64);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

static SZrAstNode *block_with_statement(SZrAstNode *statement, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, statement);
    return block;
}

static SZrAstNode *if_statement(SZrAstNode *condition, SZrAstNode *thenBlock, SZrAstNode *elseBlock) {
    SZrAstNode *ifNode = test_node(ZR_AST_IF_EXPRESSION, 0, 64);

    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = thenBlock;
    ifNode->data.ifExpression.elseExpr = elseBlock;
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    return ifNode;
}

static void dataflow_init_zero(void *state, void *userData) {
    ZR_UNUSED_PARAMETER(userData);
    *((TZrUInt32 *)state) = 0;
}

static TZrBool dataflow_join_or(void *dst, const void *src, void *userData) {
    TZrUInt32 *dstValue = (TZrUInt32 *)dst;
    TZrUInt32 srcValue = *((const TZrUInt32 *)src);
    TZrUInt32 previous = *dstValue;

    ZR_UNUSED_PARAMETER(userData);
    *dstValue |= srcValue;
    return *dstValue != previous;
}

static void dataflow_record_statement(SZrAstNode *statement, void *state, void *userData) {
    SDataflowVisitLog *log = (SDataflowVisitLog *)userData;

    *((TZrUInt32 *)state) |= 1U;
    TEST_ASSERT_TRUE(log->count < 8);
    log->nodes[log->count++] = statement;
}

static void definite_assignment_init_uninit(void *state, void *userData) {
    SDefiniteAssignmentHarness *harness = (SDefiniteAssignmentHarness *)userData;

    ZrParser_DefiniteAssignment_InitState(
        state,
        harness->symbolCount,
        ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT);
}

static TZrBool definite_assignment_join(void *dst, const void *src, void *userData) {
    SDefiniteAssignmentHarness *harness = (SDefiniteAssignmentHarness *)userData;

    return ZrParser_DefiniteAssignment_Join(dst, src, harness->symbolCount);
}

static void definite_assignment_transfer_assignment(SZrAstNode *statement,
                                                    void *state,
                                                    void *userData) {
    SDefiniteAssignmentHarness *harness = (SDefiniteAssignmentHarness *)userData;

    if (statement == harness->assignmentStatement) {
        ZrParser_DefiniteAssignment_Set(
            state,
            harness->symbolCount,
            harness->symbolIndex,
            ZR_PARSER_DEFINITE_ASSIGNMENT_INIT);
    }
}

static void test_forward_dataflow_skips_unreachable_statement_after_return(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SDataflowVisitLog log;
    SZrParserDataflowAnalysis analysis;
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 0, 7);
    SZrAstNode *nextStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 8, 18);
    SZrAstNode *script = script_with_statements(returnStmt, nextStmt);
    const SZrParserDataflowBlockState *unreachableState;

    memset(&log, 0, sizeof(log));
    ZrParser_Cfg_Init(g_state, &cfg);
    ZrParser_DataflowResult_Init(&result);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = sizeof(TZrUInt32);
    analysis.initEntry = dataflow_init_zero;
    analysis.join = dataflow_join_or;
    analysis.transferStatement = dataflow_record_statement;
    analysis.userData = &log;

    TEST_ASSERT_TRUE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)log.count);
    TEST_ASSERT_EQUAL_PTR(returnStmt, log.nodes[0]);

    unreachableState = ZrParser_Dataflow_GetBlockState(&result, 2);
    TEST_ASSERT_NOT_NULL(unreachableState);
    TEST_ASSERT_FALSE(unreachableState->isReachable);

    ZrParser_DataflowResult_Free(g_state, &result);
    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

static void test_backward_dataflow_reaches_return_through_exit_edge(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SDataflowVisitLog log;
    SZrParserDataflowAnalysis analysis;
    SZrAstNode *returnStmt = test_node(ZR_AST_RETURN_STATEMENT, 0, 7);
    SZrAstNode *nextStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 8, 18);
    SZrAstNode *script = script_with_statements(returnStmt, nextStmt);
    const SZrParserDataflowBlockState *returnState;
    const SZrParserDataflowBlockState *unreachableState;

    memset(&log, 0, sizeof(log));
    ZrParser_Cfg_Init(g_state, &cfg);
    ZrParser_DataflowResult_Init(&result);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    analysis.direction = ZR_PARSER_DATAFLOW_BACKWARD;
    analysis.stateSize = sizeof(TZrUInt32);
    analysis.initEntry = dataflow_init_zero;
    analysis.join = dataflow_join_or;
    analysis.transferStatement = dataflow_record_statement;
    analysis.userData = &log;

    TEST_ASSERT_TRUE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)log.count);
    TEST_ASSERT_EQUAL_PTR(returnStmt, log.nodes[0]);

    returnState = ZrParser_Dataflow_GetBlockState(&result, 1);
    unreachableState = ZrParser_Dataflow_GetBlockState(&result, 2);
    TEST_ASSERT_NOT_NULL(returnState);
    TEST_ASSERT_TRUE(returnState->isReachable);
    TEST_ASSERT_NOT_NULL(unreachableState);
    TEST_ASSERT_FALSE(unreachableState->isReachable);

    ZrParser_DataflowResult_Free(g_state, &result);
    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

static void test_definite_assignment_single_assignment_reaches_exit_as_init(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    SDefiniteAssignmentHarness harness;
    SZrAstNode *assignmentStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 0, 12);
    SZrAstNode *script = script_with_statement(assignmentStmt);
    const SZrParserDataflowBlockState *exitState;

    memset(&harness, 0, sizeof(harness));
    harness.assignmentStatement = assignmentStmt;
    harness.symbolCount = 1;
    harness.symbolIndex = 0;
    ZrParser_Cfg_Init(g_state, &cfg);
    ZrParser_DataflowResult_Init(&result);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = ZrParser_DefiniteAssignment_StateSize(harness.symbolCount);
    analysis.initEntry = definite_assignment_init_uninit;
    analysis.join = definite_assignment_join;
    analysis.transferStatement = definite_assignment_transfer_assignment;
    analysis.userData = &harness;

    TEST_ASSERT_TRUE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));

    exitState = ZrParser_Dataflow_GetBlockState(&result, cfg.exitBlockId);
    TEST_ASSERT_NOT_NULL(exitState);
    TEST_ASSERT_TRUE(exitState->isReachable);
    TEST_ASSERT_EQUAL_INT(
        ZR_PARSER_DEFINITE_ASSIGNMENT_INIT,
        ZrParser_DefiniteAssignment_Get(exitState->inState, harness.symbolCount, harness.symbolIndex));

    ZrParser_DataflowResult_Free(g_state, &result);
    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

static void test_definite_assignment_join_marks_one_branch_assignment_as_maybe_init(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    SDefiniteAssignmentHarness harness;
    SZrAstNode *condition = test_node(ZR_AST_IDENTIFIER_LITERAL, 4, 8);
    SZrAstNode *assignmentStmt = test_node(ZR_AST_EXPRESSION_STATEMENT, 16, 24);
    SZrAstNode *thenBlock = block_with_statement(assignmentStmt, 12, 28);
    SZrAstNode *ifNode = if_statement(condition, thenBlock, ZR_NULL);
    SZrAstNode *script = script_with_statement(ifNode);
    const SZrParserDataflowBlockState *exitState;

    memset(&harness, 0, sizeof(harness));
    harness.assignmentStatement = assignmentStmt;
    harness.symbolCount = 1;
    harness.symbolIndex = 0;
    ZrParser_Cfg_Init(g_state, &cfg);
    ZrParser_DataflowResult_Init(&result);

    TEST_ASSERT_TRUE(ZrParser_Cfg_Build(g_state, &cfg, script));

    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = ZrParser_DefiniteAssignment_StateSize(harness.symbolCount);
    analysis.initEntry = definite_assignment_init_uninit;
    analysis.join = definite_assignment_join;
    analysis.transferStatement = definite_assignment_transfer_assignment;
    analysis.userData = &harness;

    TEST_ASSERT_TRUE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));

    exitState = ZrParser_Dataflow_GetBlockState(&result, cfg.exitBlockId);
    TEST_ASSERT_NOT_NULL(exitState);
    TEST_ASSERT_TRUE(exitState->isReachable);
    TEST_ASSERT_EQUAL_INT(
        ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT,
        ZrParser_DefiniteAssignment_Get(exitState->inState, harness.symbolCount, harness.symbolIndex));

    ZrParser_DataflowResult_Free(g_state, &result);
    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, script);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_forward_dataflow_skips_unreachable_statement_after_return);
    RUN_TEST(test_backward_dataflow_reaches_return_through_exit_edge);
    RUN_TEST(test_definite_assignment_single_assignment_reaches_exit_as_init);
    RUN_TEST(test_definite_assignment_join_marks_one_branch_assignment_as_maybe_init);
    return UNITY_END();
}
