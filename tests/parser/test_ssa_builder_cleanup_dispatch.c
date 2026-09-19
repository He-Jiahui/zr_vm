#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrArray input_array(void *items, TZrSize count, TZrSize width) {
    SZrArray array = {0};

    array.head = (TZrBytePtr)items;
    array.length = count;
    array.capacity = count;
    array.elementSize = width;
    array.isValid = ZR_TRUE;
    return array;
}

static void make_cleanup_dispatch_function(
        SZrSemanticIrFunction *semantic,
        SZrParserCfgBlock blocks[4],
        SZrParserCfgEdge edges[3],
        SZrSemanticIrInstruction instructions[7],
        SZrSemanticIrValue values[3],
        TZrValueId operands[3]) {
    TZrUInt32 index;

    memset(semantic, 0, sizeof(*semantic));
    memset(blocks, 0, sizeof(*blocks) * 4u);
    memset(edges, 0, sizeof(*edges) * 3u);
    memset(instructions, 0, sizeof(*instructions) * 7u);
    memset(values, 0, sizeof(*values) * 3u);

    semantic->symbolId = (TZrSymbolId)45u;
    semantic->cfg.blocks = input_array(blocks, 4u, sizeof(*blocks));
    semantic->cfg.entryBlockId = 0u;
    semantic->cfg.exitBlockId = 3u;
    semantic->instructions = input_array(
            instructions, 7u, sizeof(*instructions));
    semantic->values = input_array(values, 3u, sizeof(*values));
    semantic->valueOperands = input_array(operands, 3u, sizeof(*operands));

    for (index = 0u; index < 4u; index++) {
        blocks[index].id = index;
        blocks[index].kind = ZR_PARSER_CFG_BLOCK_STATEMENT;
    }
    blocks[0].kind = ZR_PARSER_CFG_BLOCK_ENTRY;
    blocks[0].instructionCount = 2u;
    blocks[0].terminatorKind = ZR_PARSER_CFG_TERMINATOR_BRANCH;
    blocks[0].outgoingEdges = input_array(&edges[0], 1u, sizeof(*edges));
    blocks[1].kind = ZR_PARSER_CFG_BLOCK_CLEANUP;
    blocks[1].firstInstructionIndex = 2u;
    blocks[1].instructionCount = 1u;
    blocks[1].terminatorKind = ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH;
    blocks[1].outgoingEdges = input_array(&edges[1], 2u, sizeof(*edges));
    blocks[2].firstInstructionIndex = 3u;
    blocks[2].instructionCount = 2u;
    blocks[2].terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;
    blocks[3].firstInstructionIndex = 5u;
    blocks[3].instructionCount = 2u;
    blocks[3].terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;

    edges[0].fromBlockId = 0u;
    edges[0].toBlockId = 1u;
    edges[0].kind = ZR_PARSER_CFG_EDGE_CLEANUP;
    edges[1].fromBlockId = 1u;
    edges[1].toBlockId = 2u;
    edges[1].kind = ZR_PARSER_CFG_EDGE_SWITCH_CASE;
    edges[2].fromBlockId = 1u;
    edges[2].toBlockId = 3u;
    edges[2].kind = ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT;

    for (index = 0u; index < 7u; index++) {
        instructions[index].id = index + 1u;
        instructions[index].typeId = 1u;
    }
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].opcode = ZR_SEMANTIC_IR_BRANCH;
    instructions[2].opcode = ZR_SEMANTIC_IR_SWITCH;
    instructions[2].operandStart = 0u;
    instructions[2].operandCount = 1u;
    instructions[3].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[3].resultValueId = 2u;
    instructions[4].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[4].operandStart = 1u;
    instructions[4].operandCount = 1u;
    instructions[5].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[5].resultValueId = 3u;
    instructions[6].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[6].operandStart = 2u;
    instructions[6].operandCount = 1u;

    for (index = 0u; index < 3u; index++) {
        values[index].id = index + 1u;
        values[index].typeId = 1u;
    }
    values[0].definitionInstructionId = 1u;
    values[1].definitionInstructionId = 4u;
    values[2].definitionInstructionId = 6u;
    operands[0] = 1u;
    operands[1] = 2u;
    operands[2] = 3u;
}

static void test_cleanup_dispatch_preserves_explicit_selector(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[4];
    SZrParserCfgEdge edges[3];
    SZrSemanticIrInstruction instructions[7];
    SZrSemanticIrValue values[3];
    TZrValueId operands[3];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrInstruction *dispatch;

    make_cleanup_dispatch_function(
            &semantic, blocks, edges, instructions, values, operands);
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(
                  &semantic, NULL, &output, &diagnostic) &&
                  output.blockCount == 4u &&
                  (output.blocks[1].flags &
                   ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0u &&
                  output.blocks[1].successorRange.count == 2u &&
                  output.successors[
                          output.blocks[1].successorRange.start] == 3u &&
                  output.successors[
                          output.blocks[1].successorRange.start + 1u] == 4u,
          "builder did not preserve explicit cleanup dispatch successors");
    dispatch = &output.instructions[2];
    check(dispatch->opcode == ZR_EXEC_IR_OPCODE_SWITCH &&
                  dispatch->operandRange.count == 1u &&
                  output.operands[dispatch->operandRange.start] == 1u,
          "builder dropped the cleanup dispatch pending-state selector");
    output.id = 1u;
    check(ZrCore_ExecIr_VerifyFunction(
                  &output,
                  (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                         ZR_EXEC_IR_VERIFY_SSA),
                  &diagnostic),
          "cleanup dispatch failed structural or SSA verification");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_cleanup_dispatch_rejects_missing_or_unordered_state(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[4];
    SZrParserCfgEdge edges[3];
    SZrSemanticIrInstruction instructions[7];
    SZrSemanticIrValue values[3];
    TZrValueId operands[3];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_cleanup_dispatch_function(
            &semantic, blocks, edges, instructions, values, operands);
    instructions[2].operandCount = 0u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(
                  &semantic, NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
                  diagnostic.functionToken == 45u &&
                  diagnostic.blockId == 2u &&
                  diagnostic.instructionId == 3u &&
                  diagnostic.expectedVersion == 1u &&
                  diagnostic.actualVersion == 0u &&
                  output.blockCount == 0u,
          "builder accepted cleanup dispatch without a selector operand");
    ZrCore_ExecIr_FreeFunction(&output);

    make_cleanup_dispatch_function(
            &semantic, blocks, edges, instructions, values, operands);
    edges[1].kind = ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(
                  &semantic, NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
                  diagnostic.functionToken == 45u &&
                  diagnostic.blockId == 2u &&
                  diagnostic.instructionId == 3u &&
                  diagnostic.expectedVersion ==
                          ZR_PARSER_CFG_EDGE_SWITCH_CASE &&
                  diagnostic.actualVersion ==
                          ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT &&
                  output.blockCount == 0u,
          "builder accepted an unordered cleanup dispatch case list");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_cleanup_dispatch_preserves_explicit_selector();
    test_cleanup_dispatch_rejects_missing_or_unordered_state();
    puts("ssa builder cleanup dispatch PASS");
    return EXIT_SUCCESS;
}
