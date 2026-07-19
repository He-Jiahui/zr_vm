#ifndef ZR_TESTS_REFERENCE_LOAN_NLL_TEST_SUPPORT_H
#define ZR_TESTS_REFERENCE_LOAN_NLL_TEST_SUPPORT_H

#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/semantic_ir.h"

static SZrState *g_state;

typedef struct SLoanFixture {
    SZrSemanticIrFunction function;
    SZrSemanticFlowResult result;
    TZrRegionId regionId;
} SLoanFixture;

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

static SZrFileRange test_range(TZrInt32 line) {
    SZrFileRange range;
    memset(&range, 0, sizeof(range));
    range.start.line = line;
    range.end.line = line;
    return range;
}

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

static void fixture_free(SLoanFixture *fixture) {
    ZrParser_SemanticFlowResult_Free(g_state, &fixture->result);
    ZrParser_SemanticIrFunction_Free(g_state, &fixture->function);
}

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

static TZrValueId add_value(SLoanFixture *fixture, TZrInt32 line) {
    return ZrParser_SemanticIr_AddValue(
            &fixture->function, 5U, test_range(line));
}

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

static TZrUInt32 append_block(SLoanFixture *fixture,
                              EZrParserCfgBlockKind kind) {
    TZrUInt32 blockId = ZrParser_Cfg_AppendBlock(
            g_state, &fixture->function.cfg, kind, ZR_NULL);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_CFG_INVALID_BLOCK_ID, blockId);
    return blockId;
}

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

static void analyze(SLoanFixture *fixture) {
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&fixture->function));
    TEST_ASSERT_TRUE(ZrParser_SemanticFlow_Analyze(
            g_state,
            &fixture->function,
            &fixture->function.cfg,
            &fixture->result));
}

#endif /* ZR_TESTS_REFERENCE_LOAN_NLL_TEST_SUPPORT_H */
