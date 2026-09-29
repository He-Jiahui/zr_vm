#include "test_span_semantic_ir_cases.h"

#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/semantic_ir.h"

/* 用无位置信息的范围填充手工构造的 IR，避免测试夹具依赖虚构源码坐标。 */
static SZrFileRange span_empty_range(void) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    return range;
}

/* 将测试需要的最小字段集中映射到 Emit 契约，统一使用无效目标块。 */
static TZrSemanticInstructionId span_emit_semantic_instruction(
        SZrSemanticIrFunction *function,
        EZrSemanticIrOpcode opcode,
        TZrPlaceId placeId,
        TZrValueId valueId,
        TZrValueId resultValueId,
        TZrTypeId typeId,
        TZrLoanId loanId,
        TZrRegionId regionId) {
    SZrSemanticIrInstructionSpec spec;

    memset(&spec, 0, sizeof(spec));
    spec.opcode = opcode;
    spec.placeId = placeId;
    spec.valueId = valueId;
    spec.resultValueId = resultValueId;
    spec.typeId = typeId;
    spec.loanId = loanId;
    spec.regionId = regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = span_empty_range();
    return ZrParser_SemanticIr_Emit(function, &spec);
}

/* 用最小的 place/value/CFG 图复现来源借用有效时的生命周期冲突。 */
static void assert_contiguous_source_lifecycle_conflict(
        EZrSemanticContiguousSourceKind sourceKind,
        EZrSemanticLoanAccess loanAccess,
        EZrSemanticIrOpcode lifecycleOpcode) {
    const TZrTypeId sourceTypeId = 71U;
    const TZrTypeId viewTypeId = 72U;
    const TZrTypeId indexTypeId = 5U;
    SZrState *state = ZrContainerTests_CreateState();
    SZrSemanticIrFunction function;
    SZrSemanticFlowResult flowResult;
    SZrParserPlaceBase sourceBase;
    SZrParserPlaceBase viewBase;
    SZrSemanticContiguousViewFact viewFact;
    const SZrSemanticContiguousViewFact *storedFact;
    TZrPlaceId sourcePlaceId;
    TZrPlaceId viewPlaceId;
    TZrValueId sourceValueId;
    TZrValueId viewValueId;
    TZrValueId startValueId;
    TZrValueId lengthValueId;
    TZrValueId borrowValueId;
    TZrValueId viewLoadValueId;
    TZrValueId lifecycleValueId = ZR_VALUE_ID_INVALID;
    TZrRegionId regionId;
    TZrLoanId loanId;
    TZrUInt32 entryBlockId;
    TZrUInt32 exitBlockId;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_SemanticIrFunction_Init(state, &function, 700U, 701U);
    ZrParser_SemanticFlowResult_Init(state, &flowResult);

    memset(&sourceBase, 0, sizeof(sourceBase));
    sourceBase.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    sourceBase.identity = 700U;
    sourcePlaceId = ZrParser_SemanticIr_AddLocal(
            &function,
            700U,
            &sourceBase,
            sourceTypeId,
            span_empty_range(),
            ZR_FALSE);
    memset(&viewBase, 0, sizeof(viewBase));
    viewBase.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    viewBase.identity = 701U;
    viewPlaceId = ZrParser_SemanticIr_AddLocal(
            &function,
            701U,
            &viewBase,
            viewTypeId,
            span_empty_range(),
            ZR_TRUE);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, sourcePlaceId);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, viewPlaceId);

    sourceValueId = ZrParser_SemanticIr_AddValue(
            &function, sourceTypeId, span_empty_range());
    viewValueId = ZrParser_SemanticIr_AddValue(
            &function, viewTypeId, span_empty_range());
    startValueId = ZrParser_SemanticIr_AddValue(
            &function, indexTypeId, span_empty_range());
    lengthValueId = ZrParser_SemanticIr_AddValue(
            &function, indexTypeId, span_empty_range());
    borrowValueId = ZrParser_SemanticIr_AddValue(
            &function, sourceTypeId, span_empty_range());
    viewLoadValueId = ZrParser_SemanticIr_AddValue(
            &function, viewTypeId, span_empty_range());
    /* MOVE 有结果值而 DROP 没有，避免两条生命周期路径共用错误的 IR 形状。 */
    if (lifecycleOpcode == ZR_SEMANTIC_IR_MOVE) {
        lifecycleValueId = ZrParser_SemanticIr_AddValue(
                &function, sourceTypeId, span_empty_range());
    }
    regionId = ZrParser_SemanticIr_AddRegion(
            &function,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            span_empty_range());

    memset(&viewFact, 0, sizeof(viewFact));
    viewFact.viewPlaceId = viewPlaceId;
    viewFact.viewValueId = viewValueId;
    viewFact.sourcePlaceId = sourcePlaceId;
    viewFact.startValueId = startValueId;
    viewFact.lengthValueId = lengthValueId;
    viewFact.regionId = regionId;
    viewFact.sourceKind = sourceKind;
    viewFact.hasKnownStart = ZR_TRUE;
    viewFact.hasKnownLength = ZR_TRUE;
    viewFact.knownLength = 4;
    viewFact.sourceRange = span_empty_range();
    /* 这两类视图不能脱离来源 loan；首次拒收后再补入真实 loan 形成有效事实。 */
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID,
            ZrParser_SemanticIr_AddContiguousViewFact(&function, &viewFact));

    loanId = ZrParser_SemanticIr_AddLoan(
            &function,
            sourcePlaceId,
            loanAccess,
            regionId,
            span_empty_range(),
            span_empty_range(),
            borrowValueId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_LOAN_ID_INVALID, loanId);
    viewFact.sourceLoanId = loanId;
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID,
            ZrParser_SemanticIr_AddContiguousViewFact(&function, &viewFact));
    storedFact = ZrParser_SemanticIr_FindContiguousViewFact(
            &function, viewPlaceId);
    TEST_ASSERT_NOT_NULL(storedFact);
    TEST_ASSERT_EQUAL_UINT32(viewValueId, storedFact->viewValueId);
    TEST_ASSERT_EQUAL_UINT32(loanId, storedFact->sourceLoanId);
    TEST_ASSERT_EQUAL_INT(sourceKind, storedFact->sourceKind);

    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            span_emit_semantic_instruction(
                    &function,
                    ZR_SEMANTIC_IR_INITIALIZE,
                    sourcePlaceId,
                    sourceValueId,
                    ZR_VALUE_ID_INVALID,
                    sourceTypeId,
                    ZR_SEMANTIC_LOAN_ID_INVALID,
                    ZR_SEMANTIC_REGION_ID_INVALID));
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            span_emit_semantic_instruction(
                    &function,
                    ZR_SEMANTIC_IR_INITIALIZE,
                    viewPlaceId,
                    viewValueId,
                    ZR_VALUE_ID_INVALID,
                    viewTypeId,
                    ZR_SEMANTIC_LOAN_ID_INVALID,
                    ZR_SEMANTIC_REGION_ID_INVALID));
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            span_emit_semantic_instruction(
                    &function,
                    loanAccess == ZR_SEMANTIC_LOAN_MUTABLE
                            ? ZR_SEMANTIC_IR_BORROW_MUT
                            : ZR_SEMANTIC_IR_BORROW_SHARED,
                    sourcePlaceId,
                    ZR_VALUE_ID_INVALID,
                    borrowValueId,
                    sourceTypeId,
                    loanId,
                    regionId));
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            span_emit_semantic_instruction(
                    &function,
                    lifecycleOpcode,
                    sourcePlaceId,
                    ZR_VALUE_ID_INVALID,
                    lifecycleValueId,
                    sourceTypeId,
                    ZR_SEMANTIC_LOAN_ID_INVALID,
                    ZR_SEMANTIC_REGION_ID_INVALID));
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            span_emit_semantic_instruction(
                    &function,
                    ZR_SEMANTIC_IR_LOAD,
                    viewPlaceId,
                    ZR_VALUE_ID_INVALID,
                    viewLoadValueId,
                    viewTypeId,
                    ZR_SEMANTIC_LOAN_ID_INVALID,
                    ZR_SEMANTIC_REGION_ID_INVALID));
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            span_emit_semantic_instruction(
                    &function,
                    ZR_SEMANTIC_IR_RETURN,
                    viewPlaceId,
                    viewLoadValueId,
                    ZR_VALUE_ID_INVALID,
                    viewTypeId,
                    ZR_SEMANTIC_LOAN_ID_INVALID,
                    ZR_SEMANTIC_REGION_ID_INVALID));

    /* 把完整的六条 IR 排入唯一可达 return 块，保留末尾视图读取的借用活性。 */
    entryBlockId = ZrParser_Cfg_AppendBlock(
            state, &function.cfg, ZR_PARSER_CFG_BLOCK_ENTRY, ZR_NULL);
    exitBlockId = ZrParser_Cfg_AppendBlock(
            state, &function.cfg, ZR_PARSER_CFG_BLOCK_EXIT, ZR_NULL);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_CFG_INVALID_BLOCK_ID, entryBlockId);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_CFG_INVALID_BLOCK_ID, exitBlockId);
    function.cfg.entryBlockId = entryBlockId;
    function.cfg.exitBlockId = exitBlockId;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            &function.cfg,
            entryBlockId,
            exitBlockId,
            ZR_PARSER_CFG_EDGE_RETURN,
            ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_BindBlockRange(
            &function,
            &function.cfg,
            entryBlockId,
            0U,
            6U,
            ZR_PARSER_CFG_TERMINATOR_RETURN));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_BindBlockRange(
            &function,
            &function.cfg,
            exitBlockId,
            6U,
            0U,
            ZR_PARSER_CFG_TERMINATOR_EXIT));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&function));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_Analyze(
            state, &function, &function.cfg, &flowResult));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_HasDiagnostic(
            &flowResult,
            ZR_SEMANTIC_FLOW_LOAN_CONFLICT,
            sourcePlaceId));

    /* BUG: state/function/result 建立后的断言失败会由 Unity longjmp 中止本 helper，
     * 跳过下方释放；test_span_core.c 的空 tearDown 无法取得这些局部句柄，
     * 因此失败运行会泄漏其资源。该路径由 main 中的 RUN_TEST 可达。 */
    ZrParser_SemanticFlowResult_Free(state, &flowResult);
    ZrParser_SemanticIrFunction_Free(state, &function);
    ZrContainerTests_DestroyState(state);
}

/* 先验证 owner/native-pinned 视图缺少来源 loan 时被拒，再挂接真实借用；
 * 后续 view load/return 使该借用跨过 MOVE 或 DROP，覆盖冲突诊断。 */
void test_span_owner_move_and_native_drop_conflict_with_active_view(void) {
    assert_contiguous_source_lifecycle_conflict(
            ZR_SEMANTIC_CONTIGUOUS_SOURCE_OWNER,
            ZR_SEMANTIC_LOAN_MUTABLE,
            ZR_SEMANTIC_IR_MOVE);
    assert_contiguous_source_lifecycle_conflict(
            ZR_SEMANTIC_CONTIGUOUS_SOURCE_NATIVE_PINNED,
            ZR_SEMANTIC_LOAN_SHARED,
            ZR_SEMANTIC_IR_DROP);
}
