#include "unity.h"
#include <string.h>
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

typedef struct SBuilderDiamondFixture {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[4];
    SZrParserCfgEdge edges[4];
    SZrSemanticIrInstruction instructions[10];
    SZrSemanticIrValue values[4];
    TZrValueId operands[2];
    SZrParserPlace place;
    SZrSemanticIrLocal local;
} SBuilderDiamondFixture;

static SZrArray input_array(void *items, TZrSize count, TZrSize width) {
    SZrArray array = {0};
    array.head = (TZrBytePtr)items;
    array.length = count;
    array.capacity = count;
    array.elementSize = width;
    array.isValid = ZR_TRUE;
    return array;
}

static void make_builder_diamond(SBuilderDiamondFixture *fixture) {
    TZrUInt32 index;
    memset(fixture, 0, sizeof(*fixture));
    fixture->semantic.symbolId = 42u;
    fixture->semantic.callableTypeId = 11u;
    fixture->semantic.cfg.entryBlockId = 0u;
    fixture->semantic.cfg.exitBlockId = 3u;
    fixture->semantic.cfg.blocks = input_array(
            fixture->blocks, 4u, sizeof(fixture->blocks[0]));
    fixture->semantic.instructions = input_array(
            fixture->instructions, 10u, sizeof(fixture->instructions[0]));
    fixture->semantic.values = input_array(
            fixture->values, 4u, sizeof(fixture->values[0]));
    fixture->semantic.valueOperands = input_array(
            fixture->operands, 2u, sizeof(fixture->operands[0]));
    fixture->semantic.places.places = input_array(
            &fixture->place, 1u, sizeof(fixture->place));
    fixture->semantic.locals = input_array(
            &fixture->local, 1u, sizeof(fixture->local));

    for (index = 0u; index < 4u; ++index) {
        fixture->blocks[index].id = index;
        fixture->blocks[index].kind = index == 0u
                ? ZR_PARSER_CFG_BLOCK_ENTRY : ZR_PARSER_CFG_BLOCK_STATEMENT;
        fixture->values[index].id = index + 1u;
        fixture->values[index].typeId = 11u;
        fixture->edges[index].fromBlockId = index < 2u ? 0u : index - 1u;
        fixture->edges[index].toBlockId = index < 2u ? index + 1u : 3u;
        fixture->edges[index].kind = index == 0u
                ? ZR_PARSER_CFG_EDGE_TRUE_BRANCH
                : index == 1u ? ZR_PARSER_CFG_EDGE_FALSE_BRANCH
                              : ZR_PARSER_CFG_EDGE_NORMAL;
    }
    fixture->blocks[0].instructionCount = 2u;
    fixture->blocks[0].terminatorKind = ZR_PARSER_CFG_TERMINATOR_BRANCH;
    fixture->blocks[0].outgoingEdges = input_array(
            fixture->edges, 2u, sizeof(fixture->edges[0]));
    for (index = 1u; index <= 2u; ++index) {
        fixture->blocks[index].firstInstructionIndex = index == 1u ? 2u : 5u;
        fixture->blocks[index].instructionCount = 3u;
        fixture->blocks[index].terminatorKind = ZR_PARSER_CFG_TERMINATOR_BRANCH;
        fixture->blocks[index].outgoingEdges = input_array(
                &fixture->edges[index + 1u], 1u, sizeof(fixture->edges[0]));
    }
    fixture->blocks[3].firstInstructionIndex = 8u;
    fixture->blocks[3].instructionCount = 2u;
    fixture->blocks[3].terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;

    fixture->place.id = 1u;
    fixture->place.typeId = 11u;
    fixture->place.base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    fixture->place.base.identity = 17u;
    fixture->local.symbolId = 17u;
    fixture->local.placeId = 1u;
    fixture->local.typeId = 11u;
    fixture->local.isScalar = ZR_TRUE;
    for (index = 0u; index < 10u; ++index) {
        fixture->instructions[index].id = index + 1u;
        fixture->instructions[index].typeId = 11u;
        fixture->instructions[index].targetBlockId =
                ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }
    fixture->instructions[0].opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    fixture->instructions[0].placeId = 1u;
    fixture->instructions[1].opcode = ZR_SEMANTIC_IR_BRANCH;
    fixture->instructions[1].operandCount = 1u;
    fixture->instructions[2].opcode = ZR_SEMANTIC_IR_CONSTANT;
    fixture->instructions[2].resultValueId = 2u;
    fixture->instructions[3].opcode = ZR_SEMANTIC_IR_STORE;
    fixture->instructions[3].placeId = 1u;
    fixture->instructions[3].valueId = 2u;
    fixture->instructions[4].opcode = ZR_SEMANTIC_IR_BRANCH;
    fixture->instructions[5].opcode = ZR_SEMANTIC_IR_CONSTANT;
    fixture->instructions[5].resultValueId = 3u;
    fixture->instructions[6].opcode = ZR_SEMANTIC_IR_STORE;
    fixture->instructions[6].placeId = 1u;
    fixture->instructions[6].valueId = 3u;
    fixture->instructions[7].opcode = ZR_SEMANTIC_IR_BRANCH;
    fixture->instructions[8].opcode = ZR_SEMANTIC_IR_LOAD;
    fixture->instructions[8].placeId = 1u;
    fixture->instructions[8].resultValueId = 4u;
    fixture->instructions[9].opcode = ZR_SEMANTIC_IR_RETURN;
    fixture->instructions[9].operandStart = 1u;
    fixture->instructions[9].operandCount = 1u;
    fixture->values[1].definitionInstructionId = 3u;
    fixture->values[2].definitionInstructionId = 6u;
    fixture->values[3].definitionInstructionId = 9u;
    fixture->operands[0] = 1u;
    fixture->operands[1] = 4u;
}

void setUp(void) {}
void tearDown(void) {}

static void test_ssa_construction_rejects_missing_semantic_facts(void) {
    SZrExecIrFunction out; SZrExecIrDiagnostic d;
    ZrCore_ExecIr_FunctionInit(&out);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(NULL, NULL, &out, &d));
    TEST_ASSERT_EQUAL(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, d.code);
    ZrCore_ExecIr_FreeFunction(&out);
}

static void test_ssa_construction_empty_semir_is_safe(void) {
    SZrSemanticIrFunction sem; SZrExecIrFunction out; SZrExecIrDiagnostic d;
    memset(&sem, 0, sizeof(sem));
    ZrCore_ExecIr_FunctionInit(&out);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(&sem, NULL, &out, &d));
    ZrCore_ExecIr_FreeFunction(&out);
}

static void test_ssa_construction_dominator_linear_cfg(void) {
    SZrExecIrFunction f; SZrExecIrDiagnostic d; TZrExecIrBlockId a, b;
    ZrCore_ExecIr_FunctionInit(&f);
    a = ZrCore_ExecIr_FunctionAddBlock(&f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    b = ZrCore_ExecIr_FunctionAddBlock(&f, 0u);
    f.entryBlockId = a;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, &b, 1u, &f.blocks[a - 1u].successorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
            &f, &a, 1u, &f.blocks[b - 1u].predecessorRange));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_ComputeDominators(&f, &d));
    TEST_ASSERT_EQUAL(a, f.blocks[b - 1u].immediateDominator);
    ZrCore_ExecIr_FreeFunction(&f);
}

static void test_ssa_construction_dominator_diamond_cfg(void) {
    SZrExecIrFunction f;
    SZrExecIrDiagnostic d;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId left;
    TZrExecIrBlockId right;
    TZrExecIrBlockId merge;
    TZrExecIrBlockId entrySuccessors[2];
    TZrExecIrBlockId mergePredecessors[2];

    ZrCore_ExecIr_FunctionInit(&f);
    entry = ZrCore_ExecIr_FunctionAddBlock(&f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    left = ZrCore_ExecIr_FunctionAddBlock(&f, 0u);
    right = ZrCore_ExecIr_FunctionAddBlock(&f, 0u);
    merge = ZrCore_ExecIr_FunctionAddBlock(&f, 0u);
    f.entryBlockId = entry;
    entrySuccessors[0] = left;
    entrySuccessors[1] = right;
    mergePredecessors[0] = left;
    mergePredecessors[1] = right;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, entrySuccessors, 2u, &f.blocks[entry - 1u].successorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, &merge, 1u, &f.blocks[left - 1u].successorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, &merge, 1u, &f.blocks[right - 1u].successorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
            &f, &entry, 1u, &f.blocks[left - 1u].predecessorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
            &f, &entry, 1u, &f.blocks[right - 1u].predecessorRange));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendPredecessors(
            &f, mergePredecessors, 2u, &f.blocks[merge - 1u].predecessorRange));

    TEST_ASSERT_TRUE(ZrParser_ExecIr_ComputeDominators(&f, &d));
    TEST_ASSERT_EQUAL(entry, f.blocks[left - 1u].immediateDominator);
    TEST_ASSERT_EQUAL(entry, f.blocks[right - 1u].immediateDominator);
    TEST_ASSERT_EQUAL(entry, f.blocks[merge - 1u].immediateDominator);
    ZrCore_ExecIr_FreeFunction(&f);
}

static void test_ssa_construction_dominator_rejects_invalid_successor(void) {
    SZrExecIrFunction f;
    SZrExecIrDiagnostic d;
    TZrExecIrBlockId entry;
    TZrExecIrBlockId invalidSuccessor = 2u;

    ZrCore_ExecIr_FunctionInit(&f);
    entry = ZrCore_ExecIr_FunctionAddBlock(&f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    f.entryBlockId = entry;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendSuccessors(
            &f, &invalidSuccessor, 1u,
            &f.blocks[entry - 1u].successorRange));

    TEST_ASSERT_FALSE(ZrParser_ExecIr_ComputeDominators(&f, &d));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK, d.code);
    TEST_ASSERT_EQUAL(entry, d.blockId);
    TEST_ASSERT_EQUAL(1u, d.expectedVersion);
    TEST_ASSERT_EQUAL(invalidSuccessor, d.actualVersion);
    ZrCore_ExecIr_FreeFunction(&f);
}

static void test_ssa_construction_builds_diamond_place_phi(void) {
    SBuilderDiamondFixture fixture;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrPhi *phi;
    SZrExecIrBlock *savedBlocks;
    SZrExecIrPhi *savedPhis;

    make_builder_diamond(&fixture);
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            &fixture.semantic, NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL(4u, output.blockCount);
    TEST_ASSERT_EQUAL(1u, output.phiCount);
    TEST_ASSERT_EQUAL(1u, output.blocks[3].phis.count);
    phi = &output.phiPool[output.blocks[3].phis.start];
    TEST_ASSERT_EQUAL(2u, phi->incomings.count);
    TEST_ASSERT_EQUAL(2u, output.phiIncoming[phi->incomings.start].predecessor);
    TEST_ASSERT_EQUAL(2u, output.phiIncoming[phi->incomings.start].value);
    TEST_ASSERT_EQUAL(3u, output.phiIncoming[phi->incomings.start + 1u].predecessor);
    TEST_ASSERT_EQUAL(3u, output.phiIncoming[phi->incomings.start + 1u].value);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_COPY, output.instructions[8].opcode);
    TEST_ASSERT_EQUAL(phi->result,
            output.operands[output.instructions[8].operands.start]);
    output.id = 1u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &output, ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA,
            &diagnostic));

    savedBlocks = output.blocks;
    savedPhis = output.phiPool;
    fixture.instructions[6] = fixture.instructions[7];
    fixture.instructions[6].id = 7u;
    fixture.instructions[7] = fixture.instructions[8];
    fixture.instructions[7].id = 8u;
    fixture.instructions[8] = fixture.instructions[9];
    fixture.instructions[8].id = 9u;
    fixture.semantic.instructions.length = 9u;
    fixture.semantic.instructions.capacity = 9u;
    fixture.blocks[2].instructionCount = 2u;
    fixture.blocks[3].firstInstructionIndex = 7u;
    fixture.values[3].definitionInstructionId = 8u;
    TEST_ASSERT_FALSE(ZrParser_ExecIr_Build(
            &fixture.semantic, NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, diagnostic.code);
    TEST_ASSERT_EQUAL(42u, diagnostic.functionToken);
    TEST_ASSERT_EQUAL(4u, diagnostic.blockId);
    TEST_ASSERT_EQUAL(9u, diagnostic.instructionId);
    TEST_ASSERT_EQUAL(9u, diagnostic.sourceId);
    TEST_ASSERT_EQUAL_PTR(savedBlocks, output.blocks);
    TEST_ASSERT_EQUAL_PTR(savedPhis, output.phiPool);
    TEST_ASSERT_EQUAL(1u, output.phiCount);
    TEST_ASSERT_EQUAL(phi->result, output.phiPool[output.blocks[3].phis.start].result);
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_ssa_construction_prunes_unread_place_phi(void) {
    SBuilderDiamondFixture fixture;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_builder_diamond(&fixture);
    fixture.instructions[8].opcode = ZR_SEMANTIC_IR_CONSTANT;
    fixture.instructions[8].placeId = ZR_PLACE_ID_INVALID;
    ZrCore_ExecIr_FunctionInit(&output);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_Build(
            &fixture.semantic, NULL, &output, &diagnostic));
    TEST_ASSERT_EQUAL(0u, output.phiCount);
    TEST_ASSERT_EQUAL(0u, output.blocks[3].phis.count);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_OPCODE_CONSTANT, output.instructions[8].opcode);
    output.id = 1u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(
            &output, ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_SSA,
            &diagnostic));
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ssa_construction_rejects_missing_semantic_facts);
    RUN_TEST(test_ssa_construction_empty_semir_is_safe);
    RUN_TEST(test_ssa_construction_dominator_linear_cfg);
    RUN_TEST(test_ssa_construction_dominator_diamond_cfg);
    RUN_TEST(test_ssa_construction_dominator_rejects_invalid_successor);
    RUN_TEST(test_ssa_construction_builds_diamond_place_phi);
    RUN_TEST(test_ssa_construction_prunes_unread_place_phi);
    return UNITY_END();
}
