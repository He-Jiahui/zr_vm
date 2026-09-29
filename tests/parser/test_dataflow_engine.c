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

/** @brief 通用转移回调把实际访问的语句写入此日志，供方向与可达性用例复用。 */
typedef struct SDataflowVisitLog {
    SZrAstNode *nodes[8];
    TZrSize count;
} SDataflowVisitLog;

/** @brief 将单个 AST 赋值节点映射到状态向量槽位，供通用分析回调共享。 */
typedef struct SDefiniteAssignmentHarness {
    SZrAstNode *assignmentStatement;
    TZrSize symbolCount;
    TZrSize symbolIndex;
} SDefiniteAssignmentHarness;

/** @brief 仅供预算边界用例驱动非收敛 join；不是业务分析的格运算。 */
typedef struct SDataflowOscillationHarness {
    TZrSize joinCalls;
    TZrSize changeLimit;
} SDataflowOscillationHarness;

/** @brief 隔离每个 Unity 用例的运行时 state；局部 raw AST、CFG 与结果仍由对应 Free 显式收束。 */
static SZrState *g_state;

/** @brief UnityDefaultTestRun 在每个用例前创建隔离状态，避免数据流 fixture 串扰。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/** @brief Unity 断言 longjmp 后仍进入此钩子以销毁运行时 state。
 * BUG: Run 返回后若后续断言 longjmp，会跳过局部 raw AST、CFG、result 的 Free；global shim 不登记 raw 块所有权，失败路径块留到测试进程退出。
 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/** @brief 为合成 AST 提供一致的单行源码范围，source 字符串与本用例 state 同寿命。 */
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

/** @brief 统一通过当前 state 的分配器创建 AST 节点，调用方最终显式调用 AST Free。 */
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

/** @brief 双语句脚本用于验证终止语句之后的 CFG 位置不会被正向传播误访。 */
static SZrAstNode *script_with_statements(SZrAstNode *first, SZrAstNode *second) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 24);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 2);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, first);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, second);
    return script;
}

/** @brief 单语句脚本承载赋值与分支场景，使 CFG 由生产构造器生成。 */
static SZrAstNode *script_with_statement(SZrAstNode *statement) {
    SZrAstNode *script = test_node(ZR_AST_SCRIPT, 0, 64);

    script->data.script.statements = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    ZrParser_AstNodeArray_Add(g_state, script->data.script.statements, statement);
    return script;
}

/** @brief 分支体保留独立 block 边界，便于检查两条路径汇合时的状态合并。 */
static SZrAstNode *block_with_statement(SZrAstNode *statement, TZrSize startOffset, TZrSize endOffset) {
    SZrAstNode *block = test_node(ZR_AST_BLOCK, startOffset, endOffset);

    block->data.block.body = ZrParser_AstNodeArray_New(g_state, 1);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    ZrParser_AstNodeArray_Add(g_state, block->data.block.body, statement);
    return block;
}

/** @brief 统一生成语句形式的 if 节点，调用方可有意省略一侧分支。 */
static SZrAstNode *if_statement(SZrAstNode *condition, SZrAstNode *thenBlock, SZrAstNode *elseBlock) {
    SZrAstNode *ifNode = test_node(ZR_AST_IF_EXPRESSION, 0, 64);

    ifNode->data.ifExpression.condition = condition;
    ifNode->data.ifExpression.thenExpr = thenBlock;
    ifNode->data.ifExpression.elseExpr = elseBlock;
    ifNode->data.ifExpression.isStatement = ZR_TRUE;
    return ifNode;
}

/** @brief 通用位集分析以零状态初始化边界块，供正向入口与反向出口求解共用。 */
static void dataflow_init_zero(void *state, void *userData) {
    ZR_UNUSED_PARAMETER(userData);
    *((TZrUInt32 *)state) = 0;
}

/** @brief 位集合并用单调 OR 汇合路径事实，并准确反馈 worklist 是否需要重排。 */
static TZrBool dataflow_join_or(void *dst, const void *src, void *userData) {
    TZrUInt32 *dstValue = (TZrUInt32 *)dst;
    TZrUInt32 srcValue = *((const TZrUInt32 *)src);
    TZrUInt32 previous = *dstValue;

    ZR_UNUSED_PARAMETER(userData);
    *dstValue |= srcValue;
    return *dstValue != previous;
}

/** @brief 故意制造状态变化以触发求解预算；达到哨兵后停止，避免 fixture 无限运行。 */
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

/** @brief 手工搭建带自环且可达 exit 的最小图，隔离检查迭代上限与部分结果契约。 */
static void dataflow_build_cyclic_cfg(SZrParserCfg *cfg) {
    SZrParserCfgBlock block;

    /* 自环迫使状态反复回访，旁路出口则保留完整的入图/出图结构。 */
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

/** @brief 把转移执行记录成可观察事实，使测试能区分“块可达”和“回调已运行”。 */
static void dataflow_record_statement(SZrAstNode *statement, void *state, void *userData) {
    SDataflowVisitLog *log = (SDataflowVisitLog *)userData;

    *((TZrUInt32 *)state) |= 1U;
    TEST_ASSERT_TRUE(log->count < 8);
    log->nodes[log->count++] = statement;
}

/** @brief 适配器把通用入口初始化委托给生产 definite-assignment 状态表示。 */
static void definite_assignment_init_uninit(void *state, void *userData) {
    SDefiniteAssignmentHarness *harness = (SDefiniteAssignmentHarness *)userData;

    ZrParser_DefiniteAssignment_InitState(
        state,
        harness->symbolCount,
        ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT);
}

/** @brief 适配器保留生产格合并语义，让引擎只负责 worklist 与 CFG 顺序。 */
static TZrBool definite_assignment_join(void *dst, const void *src, void *userData) {
    SDefiniteAssignmentHarness *harness = (SDefiniteAssignmentHarness *)userData;

    return ZrParser_DefiniteAssignment_Join(dst, src, harness->symbolCount);
}

/** @brief 只把目标赋值节点写入 INIT，隔离检验引擎与 definite-assignment 的接缝。 */
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

/** @brief 锁定正向传播在 return 后截断：不可达块应保持不可达且不执行转移。 */
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

/** @brief 锁定反向求解从 exit 沿 CFG 前驱抵达 return，而不把其后的语句误作可达。 */
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

/** @brief 验证单一路径上的赋值事实能从入口经转移传播到 CFG 出口。 */
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

/** @brief 验证仅一侧分支写入时，汇合结果保持生产格定义的 MAYBE_INIT。 */
static void test_definite_assignment_join_marks_one_branch_assignment_as_maybe_init(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    SDefiniteAssignmentHarness harness;
    /* 只让 then 路径写入；省略 else 形成未赋值路径，交给生产 CFG 汇合。 */
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

/** @brief 共享断言场景检查 cleanup block 在两种方向中都参与语句转移。 */
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

    /* 显式标出清理边与后续普通边，避免 AST 构造掩盖 cleanup 块语义。 */
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

/** @brief 正向用例把 cleanup 处理纳入入口至出口的转移契约。 */
static void test_forward_dataflow_transfers_cleanup_block_statement(void) {
    assert_dataflow_transfers_cleanup_block(ZR_PARSER_DATAFLOW_FORWARD);
}

/** @brief 反向用例与正向镜像配对，防止 cleanup 只在一个遍历方向生效。 */
static void test_backward_dataflow_transfers_cleanup_block_statement(void) {
    assert_dataflow_transfers_cleanup_block(ZR_PARSER_DATAFLOW_BACKWARD);
}

/** @brief 非收敛回调触及求解预算时应失败返回，但保留可查询、可释放的部分结果。 */
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

/** @brief 缺少必需 join 回调时在准备结果前拒绝分析，保护调用方传入的空结果对象。 */
static void test_dataflow_invalid_analysis_degrades_without_allocating_result(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;

    dataflow_build_cyclic_cfg(&cfg);
    ZrParser_DataflowResult_Init(&result);
    /* 该用例只缺少必需 join；其余字段有效，以隔离分析描述符校验分支。 */
    memset(&analysis, 0, sizeof(analysis));
    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = sizeof(TZrUInt32);
    analysis.initEntry = dataflow_init_zero;

    TEST_ASSERT_FALSE(ZrParser_Dataflow_Run(g_state, &cfg, &analysis, &result));
    TEST_ASSERT_FALSE(result.blockStates.isValid);

    ZrParser_DataflowResult_Free(g_state, &result);
    ZrParser_Cfg_Free(g_state, &cfg);
}

/** @brief 超过块数上限时必须先拒绝；fixture 故意不提供对应 blocks 存储以守住边界。 */
static void test_dataflow_oversized_cfg_degrades_without_allocating_result(void) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;

    /* 只声明有效容器和超限长度，不分配 backing array；Run 必须在读取块前退出。 */
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

/** @brief 单独可执行入口注册引擎、状态格、cleanup 与失败退化四组回归场景。 */
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
