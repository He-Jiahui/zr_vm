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

static void make_iterator_function(
        SZrSemanticIrFunction *semantic,
        SZrParserCfgBlock blocks[3],
        SZrParserCfgEdge edges[2],
        SZrSemanticIrInstruction instructions[4],
        SZrSemanticIrValue values[4],
        TZrValueId operands[3]) {
    memset(semantic, 0, sizeof(*semantic));
    memset(blocks, 0, sizeof(*blocks) * 3u);
    memset(edges, 0, sizeof(*edges) * 2u);
    memset(instructions, 0, sizeof(*instructions) * 4u);
    memset(values, 0, sizeof(*values) * 4u);

    semantic->symbolId = (TZrSymbolId)42u;
    semantic->cfg.blocks = input_array(blocks, 3u, sizeof(*blocks));
    semantic->cfg.entryBlockId = 0u;
    semantic->cfg.exitBlockId = 1u;
    semantic->instructions = input_array(
            instructions, 4u, sizeof(*instructions));
    semantic->values = input_array(values, 4u, sizeof(*values));
    semantic->valueOperands = input_array(operands, 3u, sizeof(*operands));

    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        blocks[index].id = index;
        blocks[index].kind = index == 0u
                ? ZR_PARSER_CFG_BLOCK_ENTRY
                : ZR_PARSER_CFG_BLOCK_STATEMENT;
    }
    blocks[0].instructionCount = 4u;
    blocks[0].terminatorKind = ZR_PARSER_CFG_TERMINATOR_NONE;
    blocks[0].outgoingEdges = input_array(edges, 2u, sizeof(*edges));

    edges[0].fromBlockId = 0u;
    edges[0].toBlockId = 1u;
    edges[0].kind = ZR_PARSER_CFG_EDGE_NORMAL;
    edges[1].fromBlockId = 0u;
    edges[1].toBlockId = 2u;
    edges[1].kind = ZR_PARSER_CFG_EDGE_EXCEPTION;

    for (TZrUInt32 index = 0u; index < 4u; ++index) {
        values[index].id = index + 1u;
        values[index].typeId = 1u;
        instructions[index].id = index + 1u;
        instructions[index].typeId = 1u;
    }
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;

    instructions[1].opcode = ZR_SEMANTIC_IR_ITER_INIT;
    instructions[1].resultValueId = 2u;
    instructions[1].operandStart = 0u;
    instructions[1].operandCount = 1u;

    instructions[2].opcode = ZR_SEMANTIC_IR_ITER_MOVE_NEXT;
    instructions[2].resultValueId = 3u;
    instructions[2].operandStart = 1u;
    instructions[2].operandCount = 1u;

    instructions[3].opcode = ZR_SEMANTIC_IR_ITER_CURRENT;
    instructions[3].resultValueId = 4u;
    instructions[3].operandStart = 2u;
    instructions[3].operandCount = 1u;

    operands[0] = 1u;
    operands[1] = 2u;
    operands[2] = 2u;
}

static void check_iterator_schema(EZrExecIrOpcode opcode) {
    const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(opcode);
    const TZrUInt32 memoryMask =
            (1u << ZR_EXEC_IR_MEMORY_MANAGED_HEAP) |
            (1u << ZR_EXEC_IR_MEMORY_NATIVE_FFI);
    check(info != ZR_NULL && info->resultArity == 1u &&
                  info->minimumOperands == 1u &&
                  info->maximumOperands == 1u &&
                  (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) != 0u &&
                  (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u &&
                  (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u &&
                  info->memoryReads == memoryMask &&
                  info->memoryWrites == memoryMask,
          "iterator invoke schema lost arity or conservative effects");
}

static void test_iterator_operations_lower_to_ordered_invokes(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[3];
    SZrParserCfgEdge edges[2];
    SZrSemanticIrInstruction instructions[4];
    SZrSemanticIrValue values[4];
    TZrValueId operands[3];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_iterator_function(
            &semantic, blocks, edges, instructions, values, operands);
    ZrCore_ExecIr_FunctionInit(&output);

    check(ZrParser_ExecIr_Build(&semantic, ZR_NULL, &output, &diagnostic),
          "valid iterator invoke chain did not build");
    check(output.blockCount == 5u && output.instructionCount == 4u &&
                  output.instructions[1].opcode ==
                          ZR_EXEC_IR_OPCODE_ITER_INIT &&
                  output.instructions[2].opcode ==
                          ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT &&
                  output.instructions[3].opcode ==
                          ZR_EXEC_IR_OPCODE_ITER_CURRENT,
          "iterator operations lost identity or were not split");

    for (TZrUInt32 index = 1u; index < 4u; ++index) {
        const SZrExecIrInstruction *instruction = &output.instructions[index];
        TZrExecIrBlockId expectedNormal = index < 3u ? index + 1u : 4u;
        check(instruction->operands.count == 1u &&
                      instruction->results.count == 1u &&
                      instruction->successorRange.count == 2u &&
                      output.successors[instruction->successorRange.start] ==
                              expectedNormal &&
                      output.successors[
                              instruction->successorRange.start + 1u] == 5u,
              "iterator invoke lost ordered normal/exception successors");
    }
    check(output.blocks[0].instructionRange.count == 2u &&
                  output.blocks[1].instructionRange.count == 1u &&
                  output.blocks[2].instructionRange.count == 1u &&
                  (output.blocks[4].flags &
                   ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u,
          "iterator invoke blocks or exception handler flag are incorrect");
    output.id = 1u;
    check(ZrCore_ExecIr_VerifyFunction(
                  &output, ZR_EXEC_IR_VERIFY_STRUCTURE, &diagnostic),
          "iterator invoke chain failed structural verification");

    check_iterator_schema(ZR_EXEC_IR_OPCODE_ITER_INIT);
    check_iterator_schema(ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT);
    check_iterator_schema(ZR_EXEC_IR_OPCODE_ITER_CURRENT);
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_iterator_invoke_rejects_reversed_edges(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[3];
    SZrParserCfgEdge edges[2];
    SZrSemanticIrInstruction instructions[4];
    SZrSemanticIrValue values[4];
    TZrValueId operands[3];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_iterator_function(
            &semantic, blocks, edges, instructions, values, operands);
    edges[0].kind = ZR_PARSER_CFG_EDGE_EXCEPTION;
    edges[1].kind = ZR_PARSER_CFG_EDGE_NORMAL;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(
                  &semantic, ZR_NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
                  diagnostic.blockId == 1u && diagnostic.sourceId == 4u &&
                  diagnostic.expectedVersion ==
                          ZR_PARSER_CFG_EDGE_EXCEPTION &&
                  diagnostic.actualVersion == ZR_PARSER_CFG_EDGE_NORMAL,
          "reversed iterator edges silently changed exception meaning");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_iterator_invoke_requires_typed_edges(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[3];
    SZrParserCfgEdge edges[2];
    SZrSemanticIrInstruction instructions[4];
    SZrSemanticIrValue values[4];
    TZrValueId operands[3];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_iterator_function(
            &semantic, blocks, edges, instructions, values, operands);
    edges[1].kind = ZR_PARSER_CFG_EDGE_NORMAL;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(
                  &semantic, ZR_NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
                  diagnostic.blockId == 1u && diagnostic.sourceId == 4u &&
                  diagnostic.expectedVersion ==
                          ZR_PARSER_CFG_EDGE_EXCEPTION &&
                  diagnostic.actualVersion == ZR_PARSER_CFG_EDGE_NORMAL,
          "iterator invoke accepted two normal edges");
    ZrCore_ExecIr_FreeFunction(&output);

    make_iterator_function(
            &semantic, blocks, edges, instructions, values, operands);
    memset(&blocks[0].outgoingEdges, 0, sizeof(blocks[0].outgoingEdges));
    blocks[0].successors[0] = 1u;
    blocks[0].successors[1] = 2u;
    blocks[0].successorCount = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(
                  &semantic, ZR_NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
                  diagnostic.blockId == 1u && diagnostic.sourceId == 4u &&
                  diagnostic.expectedVersion ==
                          ZR_PARSER_CFG_EDGE_EXCEPTION &&
                  diagnostic.actualVersion == UINT32_MAX,
          "iterator invoke accepted untyped inline successors");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_core_verifier_requires_iterator_exception_marker(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[3];
    SZrParserCfgEdge edges[2];
    SZrSemanticIrInstruction instructions[4];
    SZrSemanticIrValue values[4];
    TZrValueId operands[3];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_iterator_function(
            &semantic, blocks, edges, instructions, values, operands);
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, ZR_NULL, &output, &diagnostic),
          "iterator verifier fixture did not build");
    output.id = 1u;

    output.blocks[4].flags &= ~ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    check(!ZrCore_ExecIr_VerifyFunction(
                  &output, ZR_EXEC_IR_VERIFY_STRUCTURE, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE &&
                  diagnostic.blockId == 1u && diagnostic.instructionId == 2u,
          "core verifier accepted an unmarked iterator exception target");
    output.blocks[4].flags |= ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;

    output.blocks[1].flags |= ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
    check(!ZrCore_ExecIr_VerifyFunction(
                  &output, ZR_EXEC_IR_VERIFY_STRUCTURE, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE &&
                  diagnostic.blockId == 1u && diagnostic.instructionId == 2u,
          "core verifier accepted an exception-marked normal target");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_iterator_result_is_unavailable_on_exception_edge(void) {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock blocks[3];
    SZrParserCfgEdge edges[2];
    SZrSemanticIrInstruction instructions[5];
    SZrSemanticIrValue values[4];
    TZrValueId operands[4];
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_iterator_function(
            &semantic, blocks, edges, instructions, values, operands);
    memset(&instructions[4], 0, sizeof(instructions[4]));
    instructions[4].id = 5u;
    instructions[4].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[4].operandStart = 3u;
    instructions[4].operandCount = 1u;
    operands[3] = 4u;
    semantic.instructions = input_array(
            instructions, 5u, sizeof(*instructions));
    semantic.valueOperands = input_array(operands, 4u, sizeof(*operands));
    blocks[2].firstInstructionIndex = 4u;
    blocks[2].instructionCount = 1u;
    blocks[2].terminatorKind = ZR_PARSER_CFG_TERMINATOR_RETURN;

    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(
                  &semantic, ZR_NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE &&
                  diagnostic.blockId == 5u &&
                  diagnostic.instructionId == 5u,
          "exception path used an iterator result that exists only normally");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_iterator_operations_lower_to_ordered_invokes();
    test_iterator_invoke_rejects_reversed_edges();
    test_iterator_invoke_requires_typed_edges();
    test_core_verifier_requires_iterator_exception_marker();
    test_iterator_result_is_unavailable_on_exception_edge();
    puts("ssa builder iterator invokes PASS");
    return EXIT_SUCCESS;
}
