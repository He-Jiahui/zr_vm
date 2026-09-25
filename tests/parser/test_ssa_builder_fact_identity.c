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

static void test_preserves_value_facts(TZrBool external) {
    static const EZrExecIrOwnership expected[] = {
        ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
        ZR_EXEC_IR_OWNERSHIP_BORROWED, ZR_EXEC_IR_OWNERSHIP_UNIQUE,
        ZR_EXEC_IR_OWNERSHIP_SHARED, ZR_EXEC_IR_OWNERSHIP_SHARED,
        ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_OWNERSHIP_GC
    };
    TZrUInt32 kind;
    for (kind = 0u; kind < ZR_SEMANTIC_VALUE_OWNERSHIP_COUNT; ++kind) {
        SZrParserCfgBlock block;
        SZrSemanticIrInstruction instructions[2];
        SZrSemanticIrValue value = {0};
        TZrValueId operand;
        SZrSemanticIrFunction semantic;
        SZrExecIrFunction output;
        SZrExecIrDiagnostic diagnostic;
        make_function(&semantic, &block, instructions, &value, &operand);
        value.facts.typeId = value.typeId;
        value.facts.ownership = (EZrSemanticValueOwnership)kind;
        value.facts.nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE;
        if (external) {
            semantic.instructions.head = (TZrBytePtr)&instructions[1];
            semantic.instructions.length = 1u;
            semantic.instructions.capacity = 1u;
            block.instructionCount = 1u;
            instructions[1].id = 1u;
        }
        ZrCore_ExecIr_FunctionInit(&output);
        check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic),
              "builder rejected valid value facts");
        check(output.values[0].ownership == expected[kind] &&
                  output.values[0].nullability == ZR_EXEC_IR_NULLABILITY_NULLABLE &&
                  output.values[0].typeToken == value.typeId,
              "builder lost canonical value facts");
        ZrCore_ExecIr_FreeFunction(&output);
    }
}

static void test_rejects_stale_or_invalid_facts(void) {
    TZrUInt32 mode;
    for (mode = 0u; mode < 6u; ++mode) {
        SZrParserCfgBlock block;
        SZrSemanticIrInstruction instructions[2];
        SZrSemanticIrValue value = {0};
        TZrValueId operand;
        SZrSemanticIrFunction semantic;
        SZrExecIrFunction output;
        SZrExecIrDiagnostic diagnostic;
        SZrExecIrBlock *originalBlocks;
        make_function(&semantic, &block, instructions, &value, &operand);
        value.facts.typeId = value.typeId;
        value.facts.ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_UNIQUE;
        value.facts.nullability = ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL;
        switch (mode) {
            case 0u: value.facts.typeId++; break;
            case 1u: value.facts.typeId = 0u; break;
            case 2u: value.facts.ownership = ZR_SEMANTIC_VALUE_OWNERSHIP_COUNT; break;
            case 3u: value.facts.nullability = ZR_SEMANTIC_VALUE_NULLABILITY_COUNT; break;
            case 4u: value.facts.ownership = (EZrSemanticValueOwnership)-1; break;
            default: value.facts.nullability = (EZrSemanticValueNullability)-1; break;
        }
        ZrCore_ExecIr_FunctionInit(&output);
        check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
              "could not prepare output for malformed fact test");
        originalBlocks = output.blocks;
        check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
                  output.blocks == originalBlocks && output.blockCount == 1u &&
                  output.valueCount == 0u,
              "invalid/stale value facts changed published output");
        ZrCore_ExecIr_FreeFunction(&output);
    }
}

static void test_preserves_explicit_constant_pool_indices(void) {
    static const TZrUInt32 indices[] = {0u, 9u, UINT32_MAX};
    TZrUInt32 index;
    for (index = 0u; index < sizeof(indices) / sizeof(indices[0]); ++index) {
        SZrParserCfgBlock block;
        SZrSemanticIrInstruction instructions[2];
        SZrSemanticIrValue value = {0};
        TZrValueId operand;
        SZrSemanticIrFunction semantic;
        SZrExecIrFunction output;
        SZrExecIrDiagnostic diagnostic;
        make_function(&semantic, &block, instructions, &value, &operand);
        instructions[0].hasConstantPoolIndex = ZR_TRUE;
        instructions[0].constantPoolIndex = indices[index];
        ZrCore_ExecIr_FunctionInit(&output);
        check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
                  output.instructions[0].opcode == ZR_EXEC_IR_OPCODE_CONSTANT &&
                  output.instructions[0].layoutId == indices[index],
              "builder lost explicit source constant pool index");
        ZrCore_ExecIr_FreeFunction(&output);
        instructions[0].hasConstantPoolIndex = ZR_FALSE;
        ZrCore_ExecIr_FunctionInit(&output);
        check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
                  output.instructions[0].layoutId == 0u,
              "builder interpreted absent constant pool metadata as an index");
        ZrCore_ExecIr_FreeFunction(&output);
    }
}

int main(void) {
    test_rejects_value_id_mismatch();
    test_rejects_instruction_id_mismatch();
    test_accepts_matching_canonical_ids();
    test_preserves_value_facts(ZR_FALSE);
    test_preserves_value_facts(ZR_TRUE);
    test_rejects_stale_or_invalid_facts();
    test_preserves_explicit_constant_pool_indices();
    puts("ssa builder fact identity PASS");
    return EXIT_SUCCESS;
}
