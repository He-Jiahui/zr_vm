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
                          SZrParserCfgBlock *block,
                          SZrSemanticIrInstruction *instructions,
                          SZrSemanticIrValue *value,
                          TZrValueId *operand) {
    memset(semantic, 0, sizeof(*semantic));
    memset(block, 0, sizeof(*block));
    memset(instructions, 0, sizeof(*instructions) * 2u);
    semantic->symbolId = (TZrSymbolId)42u;
    semantic->cfg.entryBlockId = 0u;
    semantic->cfg.blocks = input_array(block, 1u, sizeof(*block));
    semantic->instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic->values = input_array(value, 1u, sizeof(*value));
    semantic->valueOperands = input_array(operand, 1u, sizeof(*operand));
    block->kind = ZR_PARSER_CFG_BLOCK_ENTRY;
    block->instructionCount = 2u;
    value->id = 1u;
    value->typeId = 1u;
    *operand = 1u;
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[1].operandCount = 1u;
}

static void test_rejects_value_id_mismatch(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[2];
    SZrSemanticIrValue value = {0};
    TZrValueId operand;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, &block, instructions, &value, &operand);
    value.id = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not prepare existing output for value-ID mismatch");
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 0u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 2u &&
              output.blockCount == 1u && output.instructionCount == 0u,
          "builder silently renumbered a mismatched canonical value ID");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_instruction_id_mismatch(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[2];
    SZrSemanticIrValue value = {0};
    TZrValueId operand;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, &block, instructions, &value, &operand);
    instructions[0].id = 7u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 0u &&
              diagnostic.instructionId == 7u && diagnostic.sourceId == 7u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 7u &&
              output.blockCount == 0u,
          "builder published an instruction with a noncanonical source ID");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_accepts_matching_canonical_ids(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[2];
    SZrSemanticIrValue value = {0};
    TZrValueId operand;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, &block, instructions, &value, &operand);
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              output.valueCount == 1u && output.values[0].id == 1u &&
              output.sourceMapCount == 2u && output.sourceMaps[0].sourceId == 1u &&
              output.sourceMaps[1].sourceId == 2u,
          "builder rejected canonical value/instruction IDs or lost source maps");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_rejects_value_id_mismatch();
    test_rejects_instruction_id_mismatch();
    test_accepts_matching_canonical_ids();
    puts("ssa builder fact identity PASS");
    return EXIT_SUCCESS;
}
