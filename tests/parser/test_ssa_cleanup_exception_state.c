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

static void make_cleanup_exception_function(
        SZrSemanticIrFunction *semantic,
        SZrParserCfgBlock blocks[6],
        SZrParserCfgEdge edges[6],
        SZrSemanticIrInstruction instructions[9],
        SZrSemanticIrValue values[4],
        TZrValueId operands[3]) {
    TZrUInt32 index;

    memset(semantic, 0, sizeof(*semantic));
    memset(blocks, 0, sizeof(*blocks) * 6u);
    memset(edges, 0, sizeof(*edges) * 6u);
    memset(instructions, 0, sizeof(*instructions) * 9u);
    memset(values, 0, sizeof(*values) * 4u);

    semantic->symbolId = (TZrSymbolId)46u;
    semantic->cfg.blocks = input_array(blocks, 6u, sizeof(*blocks));
    semantic->cfg.entryBlockId = 0u;
    semantic->cfg.exitBlockId = 5u;
    semantic->instructions = input_array(
            instructions, 9u, sizeof(*instructions));
    semantic->values = input_array(values, 4u, sizeof(*values));
    semantic->valueOperands = input_array(operands, 3u, sizeof(*operands));

    for (index = 0u; index < 6u; index++) {
        blocks[index].id = index;
        blocks[index].kind = ZR_PARSER_CFG_BLOCK_STATEMENT;
    }
    blocks[0].kind = ZR_PARSER_CFG_BLOCK_ENTRY;
    blocks[0].instructionCount = 2u;
    blocks[0].outgoingEdges = input_array(&edges[0], 2u, sizeof(*edges));
    blocks[1].firstInstructionIndex = 2u;
    blocks[1].instructionCount = 1u;
    blocks[1].terminatorKind = ZR_PARSER_CFG_TERMINATOR_BRANCH;
    blocks[1].outgoingEdges = input_array(&edges[2], 1u, sizeof(*edges));
    blocks[2].firstInstructionIndex = 3u;
    blocks[2].instructionCount = 1u;
    blocks[2].terminatorKind = ZR_PARSER_CFG_TERMINATOR_BRANCH;
    blocks[2].outgoingEdges = input_array(&edges[3], 1u, sizeof(*edges));
    blocks[3].kind = ZR_PARSER_CFG_BLOCK_CLEANUP;
    blocks[3].firstInstructionIndex = 4u;
    blocks[3].instructionCount = 1u;
    blocks[3].terminatorKind = ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH;
    blocks[3].outgoingEdges = input_array(&edges[4], 2u, sizeof(*edges));
    blocks[4].firstInstructionIndex = 5u;
    blocks[4].instructionCount = 2u;
    blocks[4].terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;
    blocks[5].firstInstructionIndex = 7u;
    blocks[5].instructionCount = 2u;
    blocks[5].terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;

    edges[0].fromBlockId = 0u;
    edges[0].toBlockId = 1u;
    edges[0].kind = ZR_PARSER_CFG_EDGE_NORMAL;
    edges[1].fromBlockId = 0u;
    edges[1].toBlockId = 2u;
    edges[1].kind = ZR_PARSER_CFG_EDGE_EXCEPTION;
    edges[2].fromBlockId = 1u;
    edges[2].toBlockId = 3u;
    edges[2].kind = ZR_PARSER_CFG_EDGE_CLEANUP;
    edges[3].fromBlockId = 2u;
    edges[3].toBlockId = 3u;
    edges[3].kind = ZR_PARSER_CFG_EDGE_CLEANUP;
    edges[4].fromBlockId = 3u;
    edges[4].toBlockId = 4u;
    edges[4].kind = ZR_PARSER_CFG_EDGE_SWITCH_CASE;
    edges[5].fromBlockId = 3u;
    edges[5].toBlockId = 5u;
    edges[5].kind = ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT;

    for (index = 0u; index < 9u; index++) {
        instructions[index].id = index + 1u;
        instructions[index].typeId = 1u;
    }
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].opcode = ZR_SEMANTIC_IR_CALL_TYPED;
    instructions[1].resultValueId = 2u;
    instructions[2].opcode = ZR_SEMANTIC_IR_BRANCH;
    instructions[3].opcode = ZR_SEMANTIC_IR_BRANCH;
    instructions[4].opcode = ZR_SEMANTIC_IR_SWITCH;
    instructions[4].operandStart = 0u;
    instructions[4].operandCount = 1u;
    instructions[5].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[5].resultValueId = 3u;
    instructions[6].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[6].operandStart = 1u;
    instructions[6].operandCount = 1u;
    instructions[7].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[7].resultValueId = 4u;
    instructions[8].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[8].operandStart = 2u;
    instructions[8].operandCount = 1u;

    for (index = 0u; index < 4u; index++) {
        values[index].id = index + 1u;
        values[index].typeId = 1u;
    }
    values[0].definitionInstructionId = 1u;
    values[1].definitionInstructionId = 2u;
    values[2].definitionInstructionId = 6u;
    values[3].definitionInstructionId = 8u;
    operands[0] = 1u;
    operands[1] = 3u;
    operands[2] = 4u;
}

static void test_preinvoke_state_reaches_exceptional_cleanup(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[6];
    SZrParserCfgEdge edges[6];
    SZrSemanticIrInstruction instructions[9];
    SZrSemanticIrValue values[4];
    TZrValueId operands[3];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_cleanup_exception_function(
            &semantic, blocks, edges, instructions, values, operands);
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(
                  &semantic, NULL, &output, &diagnostic) &&
                  output.blockCount == 6u &&
                  (output.blocks[2].flags &
                   ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u &&
                  (output.blocks[3].flags &
                   ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) != 0u &&
                  output.instructions[4].opcode == ZR_EXEC_IR_OPCODE_SWITCH &&
                  output.operands[
                          output.instructions[4].operandRange.start] == 1u,
          "pre-invoke pending state did not survive exceptional cleanup entry");
    output.id = 1u;
    check(ZrCore_ExecIr_VerifyFunction(
                  &output,
                  (EZrExecIrVerifyLevel)(ZR_EXEC_IR_VERIFY_STRUCTURE |
                                         ZR_EXEC_IR_VERIFY_SSA),
                  &diagnostic),
          "exceptional cleanup graph failed structural or SSA verification");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_invoke_result_cannot_select_exceptional_cleanup_exit(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[6];
    SZrParserCfgEdge edges[6];
    SZrSemanticIrInstruction instructions[9];
    SZrSemanticIrValue values[4];
    TZrValueId operands[3];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_cleanup_exception_function(
            &semantic, blocks, edges, instructions, values, operands);
    operands[0] = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(
                  &semantic, NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE &&
                  diagnostic.functionToken == 46u &&
                  diagnostic.blockId == 4u &&
                  diagnostic.instructionId == 5u &&
                  diagnostic.expectedVersion == 2u &&
                  diagnostic.actualVersion == 2u,
          "cleanup dispatch read an interrupted INVOKE result");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_preinvoke_state_reaches_exceptional_cleanup();
    test_invoke_result_cannot_select_exceptional_cleanup_exit();
    puts("ssa cleanup exception state PASS");
    return EXIT_SUCCESS;
}
