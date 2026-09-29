#ifndef ZR_TESTS_REFERENCE_LOAN_NLL_TEST_SUPPORT_H
#define ZR_TESTS_REFERENCE_LOAN_NLL_TEST_SUPPORT_H

#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/semantic_ir.h"

/* 每个 Unity 测试目标在 setUp/tearDown 生命周期内共用本翻译单元的运行时状态。 */
static SZrState *g_state;

/* 一轮语义分析的函数 IR、独立结果缓冲及其函数作用域根 region。 */
typedef struct SLoanFixture {
    SZrSemanticIrFunction function;
    SZrSemanticFlowResult result;
    TZrRegionId regionId;
} SLoanFixture;

/* Unity 在每个 RUN_TEST 前创建状态，供 fixture 与 CFG/flow 分析分配使用。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/* 与 setUp 配对销毁状态；fixture 的数组也由该状态拥有，测试中须先正常释放。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/* 为手工构造的 IR 生成仅含起止行号的合成范围。 */
static SZrFileRange test_range(TZrInt32 line) {
    SZrFileRange range;
    memset(&range, 0, sizeof(range));
    range.start.line = line;
    range.end.line = line;
    return range;
}

/* 初始化空 fixture、分析结果和无父级的函数逃逸 region；调用前须完成 setUp。 */
static void fixture_init(SLoanFixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    ZrParser_SemanticIrFunction_Init(g_state, &fixture->function, 1U, 2U);
    ZrParser_SemanticFlowResult_Init(g_state, &fixture->result);
    fixture->regionId = ZrParser_SemanticIr_AddRegion(
            &fixture->function,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            test_range(1));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_REGION_ID_INVALID, fixture->regionId);
}

/* 在运行时状态仍有效时释放分析结果与函数 IR 的内部数组。 */
static void fixture_free(SLoanFixture *fixture) {
    ZrParser_SemanticFlowResult_Free(g_state, &fixture->result);
    ZrParser_SemanticIrFunction_Free(g_state, &fixture->function);
}

/* 用统一占位类型建立局部 place，并让参数基类同步设置 parameter 标记。 */
static TZrPlaceId add_place(SLoanFixture *fixture,
                            EZrParserPlaceBaseKind kind,
                            TZrUInt32 identity) {
    SZrParserPlaceBase base;
    memset(&base, 0, sizeof(base));
    base.kind = kind;
    base.identity = identity;
    return ZrParser_SemanticIr_AddLocal(
            &fixture->function,
            identity,
            &base,
            5U,
            test_range((TZrInt32)identity),
            kind == ZR_PARSER_PLACE_BASE_PARAMETER);
}

/* 按投影种类填写 union payload，再由 PlaceGraph 建立父子 place 关系。 */
static TZrPlaceId project_place(SLoanFixture *fixture,
                                TZrPlaceId parentId,
                                EZrParserPlaceProjectionKind kind,
                                TZrUInt32 identity) {
    SZrParserPlaceProjection projection;
    memset(&projection, 0, sizeof(projection));
    projection.kind = kind;
    if (kind == ZR_PARSER_PLACE_PROJECTION_INDEX) {
        projection.data.valueId = identity;
    } else if (kind == ZR_PARSER_PLACE_PROJECTION_FIELD) {
        projection.data.symbolId = identity;
    } else {
        projection.data.index = identity;
    }
    return ZrParser_PlaceGraph_Project(
            &fixture->function.places,
            parentId,
            &projection,
            5U,
            test_range((TZrInt32)identity));
}

/* 添加使用占位类型与合成源码范围的 SSA 值。 */
static TZrValueId add_value(SLoanFixture *fixture, TZrInt32 line) {
    return ZrParser_SemanticIr_AddValue(
            &fixture->function, 5U, test_range(line));
}

/* 把测试 loan 关联到 fixture 的函数 region，并记录合成 origin/last-use 范围。 */
static TZrLoanId add_loan(SLoanFixture *fixture,
                          TZrPlaceId placeId,
                          EZrSemanticLoanAccess access,
                          TZrValueId valueId,
                          TZrInt32 line) {
    return ZrParser_SemanticIr_AddLoan(
            &fixture->function,
            placeId,
            access,
            fixture->regionId,
            test_range(line),
            test_range(line),
            valueId);
}

/* 构造一条语义 IR 指令；loan 指令引用 fixture region，CFG 归属稍后绑定。 */
static TZrSemanticInstructionId emit_instruction(
        SLoanFixture *fixture,
        EZrSemanticIrOpcode opcode,
        TZrPlaceId placeId,
        TZrValueId valueId,
        TZrValueId resultValueId,
        TZrLoanId loanId,
        TZrInt32 line) {
    SZrSemanticIrInstructionSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.opcode = opcode;
    spec.typeId = 5U;
    spec.placeId = placeId;
    spec.valueId = valueId;
    spec.resultValueId = resultValueId;
    spec.loanId = loanId;
    spec.regionId = loanId != ZR_SEMANTIC_LOAN_ID_INVALID
                            ? fixture->regionId
                            : ZR_SEMANTIC_REGION_ID_INVALID;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = test_range(line);
    return ZrParser_SemanticIr_Emit(&fixture->function, &spec);
}

/* 向 fixture CFG 追加基本块，并在测试分配失败时立即中止。 */
static TZrUInt32 append_block(SLoanFixture *fixture,
                              EZrParserCfgBlockKind kind) {
    TZrUInt32 blockId = ZrParser_Cfg_AppendBlock(
            g_state, &fixture->function.cfg, kind, ZR_NULL);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_CFG_INVALID_BLOCK_ID, blockId);
    return blockId;
}

/* 将一段指令索引范围及其终结类型绑定到指定 CFG block。 */
static void bind_block(SLoanFixture *fixture,
                       TZrUInt32 blockId,
                       TZrUInt32 first,
                       TZrUInt32 count,
                       EZrParserCfgTerminatorKind terminator) {
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_BindBlockRange(
            &fixture->function,
            &fixture->function.cfg,
            blockId,
            first,
            count,
            terminator));
}

/* 为直线场景建立 entry→exit 返回边；分支或循环场景需显式构图。 */
static void bind_linear_cfg(SLoanFixture *fixture) {
    TZrUInt32 entry = append_block(fixture, ZR_PARSER_CFG_BLOCK_ENTRY);
    TZrUInt32 exit = append_block(fixture, ZR_PARSER_CFG_BLOCK_EXIT);
    fixture->function.cfg.entryBlockId = entry;
    fixture->function.cfg.exitBlockId = exit;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &fixture->function.cfg,
            entry,
            exit,
            ZR_PARSER_CFG_EDGE_RETURN,
            ZR_NULL));
    bind_block(
            fixture,
            entry,
            0U,
            (TZrUInt32)fixture->function.instructions.length,
            ZR_PARSER_CFG_TERMINATOR_RETURN);
    bind_block(
            fixture,
            exit,
            (TZrUInt32)fixture->function.instructions.length,
            0U,
            ZR_PARSER_CFG_TERMINATOR_EXIT);
}

/* 仅查找指定指令的 loan-conflict 诊断；返回的地址借用 fixture 结果数组。 */
static const SZrSemanticFlowDiagnostic *diagnostic_at_instruction(
        const SLoanFixture *fixture,
        TZrSemanticInstructionId instructionId) {
    for (TZrSize index = 0U; index < fixture->result.diagnostics.length; index++) {
        const SZrSemanticFlowDiagnostic *diagnostic =
                (const SZrSemanticFlowDiagnostic *)ZrCore_Array_Get(
                        (SZrArray *)&fixture->result.diagnostics, index);
        if (diagnostic != ZR_NULL &&
            diagnostic->kind == ZR_SEMANTIC_FLOW_LOAN_CONFLICT &&
            diagnostic->instructionId == instructionId) {
            return diagnostic;
        }
    }
    return ZR_NULL;
}

/* 仅用于预期 IR/CFG 有效的场景；先断言验证通过，再要求 flow 分析成功。 */
static void analyze(SLoanFixture *fixture) {
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&fixture->function));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_Analyze(
            g_state,
            &fixture->function,
            &fixture->function.cfg,
            &fixture->result));
}

#endif /* ZR_TESTS_REFERENCE_LOAN_NLL_TEST_SUPPORT_H */
