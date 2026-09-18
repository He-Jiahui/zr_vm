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

static void make_function(SZrSemanticIrFunction *semantic,
                          SZrParserCfgBlock *blocks, TZrUInt32 count) {
    TZrUInt32 i;
    memset(semantic, 0, sizeof(*semantic));
    memset(blocks, 0, (size_t)count * sizeof(*blocks));
    semantic->symbolId = (TZrSymbolId)42u;
    semantic->callableTypeId = (TZrTypeId)55u;
    semantic->cfg.blocks = input_array(blocks, count, sizeof(*blocks));
    semantic->cfg.entryBlockId = 0u;
    semantic->instructions = input_array(NULL, 0u, sizeof(SZrSemanticIrInstruction));
    for (i = 0u; i < count; ++i) {
        blocks[i].id = i;
        blocks[i].kind = i == 0u ? ZR_PARSER_CFG_BLOCK_ENTRY
                                : ZR_PARSER_CFG_BLOCK_STATEMENT;
    }
}

static void test_rejects_use_before_definition(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue values[2] = {{.id = 1u, .typeId = 1u},
                                    {.id = 2u, .typeId = 1u}};
    TZrValueId operand = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, &block, 1u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CALL_TYPED;
    instructions[0].resultValueId = 2u;
    instructions[0].operandCount = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[1].resultValueId = 1u;
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[2].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[2].operandCount = 1u;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.values = input_array(values, 2u, sizeof(*values));
    semantic.valueOperands = input_array(&operand, 1u, sizeof(operand));
    block.instructionCount = 3u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not prepare output for premature operand use");
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 1u &&
              diagnostic.instructionId == 1u && diagnostic.sourceId == 1u &&
              diagnostic.expectedVersion == 2u && diagnostic.actualVersion == 1u &&
              output.blockCount == 1u && output.instructionCount == 0u,
          "builder published a use occurring before its SSA definition");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_accepts_use_after_definition(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue values[2] = {{.id = 1u, .typeId = 1u},
                                    {.id = 2u, .typeId = 1u}};
    TZrValueId operand = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, &block, 1u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_CALL_TYPED;
    instructions[1].resultValueId = 2u;
    instructions[1].operandCount = 1u;
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[2].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[2].operandCount = 1u;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.values = input_array(values, 2u, sizeof(*values));
    semantic.valueOperands = input_array(&operand, 1u, sizeof(operand));
    block.instructionCount = 3u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              output.id == ZR_EXEC_IR_FUNCTION_ID_INVALID &&
              output.values[0].definition == 1u &&
              output.instructions[1].operands.count == 1u,
          "builder rejected a valid use or published a temporary function ID");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_non_dominating_branch_definition(void) {
    SZrParserCfgBlock blocks[4];
    SZrSemanticIrInstruction instructions[4] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    TZrValueId operand = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, blocks, 4u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_BRANCH;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_BRANCH;
    instructions[2].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[3].id = 4u;
    instructions[3].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[3].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[3].operandCount = 1u;
    semantic.instructions = input_array(instructions, 4u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    semantic.valueOperands = input_array(&operand, 1u, sizeof(operand));
    blocks[0].successorCount = 2u;
    blocks[0].successors[0] = 1u;
    blocks[0].successors[1] = 2u;
    blocks[1].instructionCount = 2u;
    blocks[1].successorCount = 1u;
    blocks[1].successors[0] = 3u;
    blocks[2].firstInstructionIndex = 2u;
    blocks[2].instructionCount = 1u;
    blocks[2].successorCount = 1u;
    blocks[2].successors[0] = 3u;
    blocks[3].firstInstructionIndex = 3u;
    blocks[3].instructionCount = 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 4u &&
              diagnostic.instructionId == 4u && diagnostic.sourceId == 4u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 1u &&
              output.blockCount == 0u,
          "builder published a join use only defined along one predecessor");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_rejects_use_before_definition();
    test_accepts_use_after_definition();
    test_rejects_non_dominating_branch_definition();
    puts("ssa builder dominance PASS");
    return EXIT_SUCCESS;
}
