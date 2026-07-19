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

typedef struct SDataflowOscillationHarness {
    TZrSize joinCalls;
    TZrSize changeLimit;
} SDataflowOscillationHarness;

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

static TZrBool dataflow_join_bounded_oscillation(void *dst, const void *src, void *userData) {
    SDataflowOscillationHarness *harness = (SDataflowOscillationHarness *)userData;

    ZR_UNUSED_PARAMETER(src);
    if (harness->joinCalls >= harness->changeLimit) {
        return ZR_FALSE;
    }

    harness->joinCalls++;
    *((TZrUInt32 *)dst) ^= 1U;
    return ZR_TRUE;
}

static void dataflow_build_cyclic_cfg(SZrParserCfg *cfg) {
    SZrParserCfgBlock block;

    ZrParser_Cfg_Init(g_state, cfg);

    memset(&block, 0, sizeof(block));
    block.id = 0;
    block.kind = ZR_PARSER_CFG_BLOCK_ENTRY;
    block.successors[0] = 1;
    block.successorCount = 1;
    ZrCore_Array_Push(g_state, &cfg->blocks, &block);

    memset(&block, 0, sizeof(block));
    block.id = 1;
    block.kind = ZR_PARSER_CFG_BLOCK_STATEMENT;
    block.successors[0] = 1;
    block.successors[1] = 2;
    block.successorCount = 2;
    block.predecessorCount = 2;
    ZrCore_Array_Push(g_state, &cfg->blocks, &block);

    memset(&block, 0, sizeof(block));
    block.id = 2;
    block.kind = ZR_PARSER_CFG_BLOCK_EXIT;
    block.predecessorCount = 1;
    ZrCore_Array_Push(g_state, &cfg->blocks, &block);

    cfg->entryBlockId = 0;
    cfg->exitBlockId = 2;
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

static void assert_dataflow_transfers_cleanup_block(
        EZrParserDataflowDirection direction) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    SDataflowVisitLog log;
    SZrAstNode *cleanupStatement =
            test_node(ZR_AST_USING_STATEMENT, 4, 20);
    TZrUInt32 cleanupBlockId;

    memset(&log, 0, sizeof(log));
    ZrParser_Cfg_Init(g_state, &cfg);
    ZrParser_DataflowResult_Init(&result);

    cfg.entryBlockId = ZrParser_Cfg_AppendBlock(
            g_state,
            &cfg,
            ZR_PARSER_CFG_BLOCK_ENTRY,
            ZR_NULL);
    cleanupBlockId = ZrParser_Cfg_AppendBlock(
            g_state,
            &cfg,
            ZR_PARSER_CFG_BLOCK_CLEANUP,
            cleanupStatement);
    cfg.exitBlockId = ZrParser_Cfg_AppendBlock(
            g_state,
            &cfg,
            ZR_PARSER_CFG_BLOCK_EXIT,
            ZR_NULL);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_PARSER_CFG_INVALID_BLOCK_ID, cfg.entryBlockId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_PARSER_CFG_INVALID_BLOCK_ID, cleanupBlockId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_PARSER_CFG_INVALID_BLOCK_ID, cfg.exitBlockId);
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg,
            cfg.entryBlockId,
            cleanupBlockId,
            ZR_PARSER_CFG_EDGE_CLEANUP,
            cleanupStatement));
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &cfg,
            cleanupBlockId,
            cfg.exitBlockId,
            ZR_PARSER_CFG_EDGE_NORMAL,
            cleanupStatement));

    analysis.direction = direction;
    analysis.stateSize = sizeof(TZrUInt32);
    analysis.initEntry = dataflow_init_zero;
    analysis.join = dataflow_join_or;
    analysis.transferStatement = dataflow_record_statement;
    analysis.userData = &log;

    TEST_ASSERT_TRUE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)log.count);
    TEST_ASSERT_EQUAL_PTR(cleanupStatement, log.nodes[0]);

    ZrParser_DataflowResult_Free(g_state, &result);
    ZrParser_Cfg_Free(g_state, &cfg);
    ZrParser_Ast_Free(g_state, cleanupStatement);
}

static void test_forward_dataflow_transfers_cleanup_block_statement(void) {
    assert_dataflow_transfers_cleanup_block(ZR_PARSER_DATAFLOW_FORWARD);
}

static void test_backward_dataflow_transfers_cleanup_block_statement(void) {
    assert_dataflow_transfers_cleanup_block(ZR_PARSER_DATAFLOW_BACKWARD);
}

static void test_dataflow_iteration_budget_degrades_with_partial_result(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    SDataflowOscillationHarness harness;

    memset(&harness, 0, sizeof(harness));
    harness.changeLimit = 4096;
    dataflow_build_cyclic_cfg(&cfg);
    ZrParser_DataflowResult_Init(&result);

    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = sizeof(TZrUInt32);
    analysis.initEntry = dataflow_init_zero;
    analysis.join = dataflow_join_bounded_oscillation;
    analysis.transferStatement = ZR_NULL;
    analysis.userData = &harness;

    TEST_ASSERT_FALSE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));
    TEST_ASSERT_TRUE(harness.joinCalls > 0);
    TEST_ASSERT_TRUE(harness.joinCalls < harness.changeLimit);
    TEST_ASSERT_NOT_NULL(ZrParser_Dataflow_GetBlockState(&result, cfg.entryBlockId));

    ZrParser_DataflowResult_Free(g_state, &result);
    ZrParser_Cfg_Free(g_state, &cfg);
}

static void test_dataflow_invalid_analysis_degrades_without_allocating_result(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;

    dataflow_build_cyclic_cfg(&cfg);
    ZrParser_DataflowResult_Init(&result);
    memset(&analysis, 0, sizeof(analysis));
    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = sizeof(TZrUInt32);
    analysis.initEntry = dataflow_init_zero;

    TEST_ASSERT_FALSE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));
    TEST_ASSERT_FALSE(result.blockStates.isValid);

    ZrParser_DataflowResult_Free(g_state, &result);
    ZrParser_Cfg_Free(g_state, &cfg);
}

static void test_dataflow_oversized_cfg_degrades_without_allocating_result(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;

    memset(&cfg, 0, sizeof(cfg));
    cfg.blocks.isValid = ZR_TRUE;
    cfg.blocks.length = ZR_PARSER_DATAFLOW_MAX_BLOCK_COUNT + 1U;
    cfg.entryBlockId = 0;
    ZrParser_DataflowResult_Init(&result);

    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = sizeof(TZrUInt32);
    analysis.initEntry = dataflow_init_zero;
    analysis.join = dataflow_join_or;
    analysis.transferStatement = ZR_NULL;
    analysis.userData = ZR_NULL;

    TEST_ASSERT_FALSE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));
    TEST_ASSERT_FALSE(result.blockStates.isValid);

    ZrParser_DataflowResult_Free(g_state, &result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_forward_dataflow_skips_unreachable_statement_after_return);
    RUN_TEST(test_backward_dataflow_reaches_return_through_exit_edge);
    RUN_TEST(test_definite_assignment_single_assignment_reaches_exit_as_init);
    RUN_TEST(test_definite_assignment_join_marks_one_branch_assignment_as_maybe_init);
    RUN_TEST(test_forward_dataflow_transfers_cleanup_block_statement);
    RUN_TEST(test_backward_dataflow_transfers_cleanup_block_statement);
    RUN_TEST(test_dataflow_iteration_budget_degrades_with_partial_result);
    RUN_TEST(test_dataflow_invalid_analysis_degrades_without_allocating_result);
    RUN_TEST(test_dataflow_oversized_cfg_degrades_without_allocating_result);
    return UNITY_END();
}
