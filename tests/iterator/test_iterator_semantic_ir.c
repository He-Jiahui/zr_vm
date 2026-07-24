#include <string.h>

#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_parser/semantic_ir.h"

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

static SZrFileRange source_range(TZrUInt32 startOffset, TZrUInt32 endOffset) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    range.start.offset = startOffset;
    range.end.offset = endOffset;
    return range;
}

static TZrSemanticInstructionId emit_iterator_instruction(
        SZrSemanticIrFunction *function,
        EZrSemanticIrOpcode opcode,
        TZrTypeId typeId,
        TZrValueId valueId,
        SZrFileRange range) {
    SZrSemanticIrInstructionSpec spec;

    memset(&spec, 0, sizeof(spec));
    spec.opcode = opcode;
    spec.typeId = typeId;
    spec.valueId = valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = range;
    return ZrParser_SemanticIr_Emit(function, &spec);
}

static void test_iterator_yield_sequence_has_canonical_suspension_facts(void) {
    static const TZrChar expected[] =
            "1 yield.value type=7 place=0 value=1 result=0\n"
            "2 yield.suspend type=0 place=0 value=1 result=0\n"
            "3 yield.resume type=0 place=0 value=1 result=0\n"
            "4 iterator.complete type=0 place=0 value=0 result=0\n";
    SZrSemanticIrFunction function;
    const SZrSemanticIrInstruction *yieldValue;
    const SZrSemanticIrInstruction *yieldSuspend;
    const SZrSemanticIrInstruction *yieldResume;
    const SZrSemanticIrInstruction *complete;
    TZrValueId valueId;
    TZrSemanticInstructionId instructionId;
    TZrChar actual[512];

    ZrParser_SemanticIrFunction_Init(g_state, &function, 401U, 402U);
    valueId = ZrParser_SemanticIr_AddValue(&function, 7U, source_range(8U, 13U));
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, valueId);

    instructionId = emit_iterator_instruction(
            &function,
            ZR_SEMANTIC_IR_YIELD_VALUE,
            7U,
            valueId,
            source_range(2U, 13U));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID, instructionId);
    instructionId = emit_iterator_instruction(
            &function,
            ZR_SEMANTIC_IR_YIELD_SUSPEND,
            ZR_SEMANTIC_ID_INVALID,
            valueId,
            source_range(2U, 13U));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID, instructionId);
    instructionId = emit_iterator_instruction(
            &function,
            ZR_SEMANTIC_IR_YIELD_RESUME,
            ZR_SEMANTIC_ID_INVALID,
            valueId,
            source_range(2U, 13U));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID, instructionId);
    instructionId = emit_iterator_instruction(
            &function,
            ZR_SEMANTIC_IR_ITERATOR_COMPLETE,
            ZR_SEMANTIC_ID_INVALID,
            ZR_VALUE_ID_INVALID,
            source_range(14U, 14U));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID, instructionId);

    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&function));
    yieldValue = ZrParser_SemanticIr_InstructionAt(&function, 0U);
    yieldSuspend = ZrParser_SemanticIr_InstructionAt(&function, 1U);
    yieldResume = ZrParser_SemanticIr_InstructionAt(&function, 2U);
    complete = ZrParser_SemanticIr_InstructionAt(&function, 3U);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_YIELD_VALUE, yieldValue->opcode);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_YIELD_SUSPEND, yieldSuspend->opcode);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_YIELD_RESUME, yieldResume->opcode);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_ITERATOR_COMPLETE, complete->opcode);
    TEST_ASSERT_EQUAL_UINT32(2U, yieldValue->sourceRange.start.offset);
    TEST_ASSERT_EQUAL_UINT32(14U, complete->sourceRange.start.offset);
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_FormatGolden(&function, actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    ZrParser_SemanticIrFunction_Free(g_state, &function);
}

static void test_yield_value_requires_a_canonical_value(void) {
    SZrSemanticIrFunction function;
    TZrSemanticInstructionId instructionId;

    ZrParser_SemanticIrFunction_Init(g_state, &function, 411U, 412U);
    instructionId = emit_iterator_instruction(
            &function,
            ZR_SEMANTIC_IR_YIELD_VALUE,
            7U,
            ZR_VALUE_ID_INVALID,
            source_range(2U, 13U));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID, instructionId);
    TEST_ASSERT_FALSE(ZrParser_SemanticIr_Validate(&function));
    ZrParser_SemanticIrFunction_Free(g_state, &function);
}

static void test_yield_suspension_keeps_borrowed_value_live_until_resume(void) {
    SZrSemanticIrFunction function;
    SZrSemanticFlowResult result;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;
    SZrParserCfg *cfg;
    TZrPlaceId ownerPlace;
    TZrValueId ownerValue;
    TZrValueId borrowedValue;
    TZrRegionId regionId;
    TZrLoanId loanId;
    TZrSemanticInstructionId suspendId;
    TZrSemanticInstructionId resumeId;
    TZrUInt32 entryBlock;
    TZrUInt32 exitBlock;

    ZrParser_SemanticIrFunction_Init(g_state, &function, 421U, 422U);
    ZrParser_SemanticFlowResult_Init(g_state, &result);
    cfg = &function.cfg;
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = 421U;
    ownerPlace = ZrParser_SemanticIr_AddLocal(
            &function, 421U, &base, 7U, source_range(0U, 5U), ZR_FALSE);
    ownerValue = ZrParser_SemanticIr_AddValue(&function, 7U, source_range(0U, 5U));
    borrowedValue = ZrParser_SemanticIr_AddValue(&function, 7U, source_range(6U, 11U));
    regionId = ZrParser_SemanticIr_AddRegion(
            &function,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            source_range(6U, 11U));
    loanId = ZrParser_SemanticIr_AddLoan(
            &function,
            ownerPlace,
            ZR_SEMANTIC_LOAN_SHARED,
            regionId,
            source_range(6U, 11U),
            source_range(12U, 17U),
            borrowedValue);
    TEST_ASSERT_NOT_EQUAL(ZR_PLACE_ID_INVALID, ownerPlace);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, ownerValue);
    TEST_ASSERT_NOT_EQUAL(ZR_VALUE_ID_INVALID, borrowedValue);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_REGION_ID_INVALID, regionId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_LOAN_ID_INVALID, loanId);

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_INITIALIZE;
    spec.typeId = 7U;
    spec.placeId = ownerPlace;
    spec.valueId = ownerValue;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = source_range(0U, 5U);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            ZrParser_SemanticIr_Emit(&function, &spec));

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_BORROW_SHARED;
    spec.typeId = 7U;
    spec.placeId = ownerPlace;
    spec.resultValueId = borrowedValue;
    spec.loanId = loanId;
    spec.regionId = regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = source_range(6U, 11U);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            ZrParser_SemanticIr_Emit(&function, &spec));

    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_INSTRUCTION_ID_INVALID,
            emit_iterator_instruction(
                    &function,
                    ZR_SEMANTIC_IR_YIELD_VALUE,
                    7U,
                    borrowedValue,
                    source_range(12U, 17U)));
    suspendId = emit_iterator_instruction(
            &function,
            ZR_SEMANTIC_IR_YIELD_SUSPEND,
            ZR_SEMANTIC_ID_INVALID,
            borrowedValue,
            source_range(12U, 17U));
    resumeId = emit_iterator_instruction(
            &function,
            ZR_SEMANTIC_IR_YIELD_RESUME,
            ZR_SEMANTIC_ID_INVALID,
            borrowedValue,
            source_range(12U, 17U));
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID, suspendId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_INSTRUCTION_ID_INVALID, resumeId);

    entryBlock = ZrParser_Cfg_AppendBlock(
            g_state, cfg, ZR_PARSER_CFG_BLOCK_ENTRY, ZR_NULL);
    exitBlock = ZrParser_Cfg_AppendBlock(
            g_state, cfg, ZR_PARSER_CFG_BLOCK_EXIT, ZR_NULL);
    cfg->entryBlockId = entryBlock;
    cfg->exitBlockId = exitBlock;
    TEST_ASSERT_TRUE(ZrParser_Cfg_Connect(
            cfg, entryBlock, exitBlock, ZR_PARSER_CFG_EDGE_RETURN, ZR_NULL));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_BindBlockRange(
            &function,
            cfg,
            entryBlock,
            0U,
            5U,
            ZR_PARSER_CFG_TERMINATOR_RETURN));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_BindBlockRange(
            &function,
            cfg,
            exitBlock,
            5U,
            0U,
            ZR_PARSER_CFG_TERMINATOR_EXIT));
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&function));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_Analyze(g_state, &function, cfg, &result));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &result, suspendId, loanId, ZR_FALSE));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsLiveAt(
            &result, resumeId, loanId, ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsActiveAt(
            &result, suspendId, loanId, ZR_FALSE));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_LoanIsActiveAt(
            &result, resumeId, loanId, ZR_TRUE));

    ZrParser_SemanticFlowResult_Free(g_state, &result);
    ZrParser_SemanticIrFunction_Free(g_state, &function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_iterator_yield_sequence_has_canonical_suspension_facts);
    RUN_TEST(test_yield_value_requires_a_canonical_value);
    RUN_TEST(test_yield_suspension_keeps_borrowed_value_live_until_resume);
    return UNITY_END();
}
