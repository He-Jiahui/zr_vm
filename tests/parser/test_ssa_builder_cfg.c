#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrArray input_array(void *elements, TZrSize count, TZrSize elementSize) {
    SZrArray array;
    memset(&array, 0, sizeof(array));
    array.head = (TZrBytePtr)elements;
    array.elementSize = elementSize;
    array.length = count;
    array.capacity = count;
    array.isValid = ZR_TRUE;
    return array;
}

static void make_semantic_function(SZrSemanticIrFunction *semantic,
                                   SZrParserCfgBlock *blocks,
                                   TZrUInt32 blockCount) {
    memset(semantic, 0, sizeof(*semantic));
    memset(blocks, 0, (size_t)blockCount * sizeof(*blocks));
    semantic->symbolId = (TZrSymbolId)42u;
    semantic->callableTypeId = (TZrTypeId)55u;
    semantic->cfg.blocks = input_array(blocks, blockCount, sizeof(*blocks));
    semantic->cfg.entryBlockId = 0u;
    semantic->instructions = input_array(NULL, 0u, sizeof(SZrSemanticIrInstruction));
    for (TZrUInt32 index = 0u; index < blockCount; ++index) {
        blocks[index].id = index;
        blocks[index].kind = index == 0u ? ZR_PARSER_CFG_BLOCK_ENTRY
                                             : ZR_PARSER_CFG_BLOCK_STATEMENT;
    }
}

static void test_diamond_preserves_every_edge_and_predecessor(void) {
    SZrParserCfgBlock blocks[4];
    SZrParserCfgEdge entryEdges[2] = {{.toBlockId = 1u}, {.toBlockId = 2u}};
    SZrParserCfgEdge leftEdge[1] = {{.fromBlockId = 1u, .toBlockId = 3u}};
    SZrParserCfgEdge rightEdge[1] = {{.fromBlockId = 2u, .toBlockId = 3u}};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 4u);
    blocks[0].outgoingEdges = input_array(entryEdges, 2u, sizeof(*entryEdges));
    blocks[1].outgoingEdges = input_array(leftEdge, 1u, sizeof(*leftEdge));
    blocks[2].outgoingEdges = input_array(rightEdge, 1u, sizeof(*rightEdge));
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic),
          "valid semantic diamond did not build");
    check(output.blockCount == 4u &&
              output.blocks[0].successorRange.count == 2u &&
              output.successors[output.blocks[0].successorRange.start] == 2u &&
              output.successors[output.blocks[0].successorRange.start + 1u] == 3u,
          "builder lost the first outgoing edge");
    check(output.blocks[3].predecessorRange.count == 2u &&
              output.predecessors[output.blocks[3].predecessorRange.start] == 2u &&
              output.predecessors[output.blocks[3].predecessorRange.start + 1u] == 3u &&
              output.blocks[3].immediateDominator == ZR_EXEC_IR_BLOCK_ID_ENTRY,
          "builder lost a merge predecessor or its dominator");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_out_of_range_semantic_target(void) {
    SZrParserCfgBlock block;
    SZrParserCfgEdge badEdge = {.toBlockId = 1u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    block.outgoingEdges = input_array(&badEdge, 1u, sizeof(badEdge));
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not prepare existing builder output");
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic),
          "builder silently dropped an invalid target block");
    check(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 2u,
          "invalid semantic edge diagnostic lost source and target");
    check(output.blockCount == 1u && output.successorCount == 0u,
          "rejected semantic edge changed the caller's existing output");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_edge_with_wrong_source_block(void) {
    SZrParserCfgBlock blocks[2];
    SZrParserCfgEdge edge = {.fromBlockId = 0u, .toBlockId = 1u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    blocks[0].successorCount = 1u;
    blocks[0].successors[0] = 1u;
    blocks[1].outgoingEdges = input_array(&edge, 1u, sizeof(edge));
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not prepare existing output for source-edge failure");
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 2u &&
              diagnostic.expectedVersion == 2u && diagnostic.actualVersion == 1u &&
              output.blockCount == 1u && output.successorCount == 0u,
          "builder silently reattributed an edge from another semantic block");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_accepts_edge_with_matching_source_block(void) {
    SZrParserCfgBlock blocks[2];
    SZrParserCfgEdge edge = {.fromBlockId = 1u, .toBlockId = 1u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    blocks[0].successorCount = 1u;
    blocks[0].successors[0] = 1u;
    blocks[1].outgoingEdges = input_array(&edge, 1u, sizeof(edge));
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              output.blocks[1].successorRange.count == 1u &&
              output.predecessors[output.blocks[1].predecessorRange.start] == 1u &&
              output.predecessors[output.blocks[1].predecessorRange.start + 1u] == 2u,
          "builder rejected a valid non-entry self edge and its predecessors");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_cfg_block_with_mismatched_id(void) {
    SZrParserCfgBlock blocks[2];
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    blocks[0].successorCount = 1u;
    blocks[0].successors[0] = 1u;
    blocks[1].id = 0u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 2u &&
              diagnostic.expectedVersion == 2u && diagnostic.actualVersion == 1u &&
              output.blockCount == 0u,
          "builder silently renumbered a malformed canonical CFG block");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_unknown_dynamic_edge_kind(void) {
    SZrParserCfgBlock blocks[2];
    SZrParserCfgEdge edge = {.fromBlockId = 0u, .toBlockId = 1u,
                             .kind = ZR_PARSER_CFG_EDGE_ENUM_MAX};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    blocks[0].outgoingEdges = input_array(&edge, 1u, sizeof(edge));
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 1u &&
              diagnostic.expectedVersion == ZR_PARSER_CFG_EDGE_ENUM_MAX - 1u &&
              diagnostic.actualVersion == ZR_PARSER_CFG_EDGE_ENUM_MAX &&
              output.blockCount == 0u,
          "builder accepted an unknown canonical CFG edge kind");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_inline_successors_preserve_both_edges(void) {
    SZrParserCfgBlock blocks[3];
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 3u);
    blocks[0].successorCount = 2u;
    blocks[0].successors[0] = 1u;
    blocks[0].successors[1] = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic),
          "valid inline successor CFG did not build");
    check(output.blocks[0].successorRange.count == 2u &&
              output.successors[output.blocks[0].successorRange.start] == 2u &&
              output.successors[output.blocks[0].successorRange.start + 1u] == 3u &&
              output.blocks[1].predecessorRange.count == 1u &&
              output.blocks[2].predecessorRange.count == 1u,
          "builder did not preserve inline edges and their predecessors");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_malformed_outgoing_edge_storage(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    block.outgoingEdges = input_array(NULL, 1u, sizeof(SZrParserCfgEdge));
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY,
          "builder did not diagnose missing outgoing edge storage");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_outgoing_edges_beyond_declared_capacity(void) {
    SZrParserCfgBlock blocks[2];
    SZrParserCfgEdge edge = {.toBlockId = 1u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    blocks[0].outgoingEdges = input_array(&edge, 1u, sizeof(edge));
    blocks[0].outgoingEdges.capacity = 0u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not prepare output for edge-capacity failure");
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
              output.blockCount == 1u && output.successorCount == 0u,
          "builder accepted an outgoing edge count exceeding its capacity");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_rejects_excess_inline_successors(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    block.successorCount = ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY + 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY,
          "builder read past the bounded inline successor array");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_preserves_instruction_ranges_and_branch_successors(void) {
    SZrParserCfgBlock blocks[2];
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    TZrValueId returnValue = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = returnValue;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_BRANCH;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[2].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[2].operandCount = 1u;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    semantic.valueOperands = input_array(&returnValue, 1u, sizeof(returnValue));
    blocks[0].firstInstructionIndex = 0u;
    blocks[0].instructionCount = 2u;
    blocks[0].successorCount = 1u;
    blocks[0].successors[0] = 1u;
    blocks[1].firstInstructionIndex = 2u;
    blocks[1].instructionCount = 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    TZrBool built = ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic);
    if (!built) {
        fprintf(stderr, "builder diagnostic code=%u block=%u instruction=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId);
    }
    check(built,
          "two-block branch fixture did not build");
    check(output.blocks[0].instructionRange.start == 0u &&
              output.blocks[0].instructionRange.count == 2u &&
              output.blocks[0].terminatorInstructionId == 2u &&
              output.instructions[1].successorRange.start ==
                  output.blocks[0].successorRange.start &&
              output.instructions[1].successorRange.count == 1u &&
              output.blocks[1].instructionRange.start == 2u &&
              output.blocks[1].terminatorInstructionId == 3u,
          "builder shifted instruction ranges or detached branch successor");
    output.id = 1u; /* Module-level construction normally assigns this ID. */
    check(ZrCore_ExecIr_VerifyFunction(&output, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                       &diagnostic),
          "builder output was not structurally verifiable ExecIR");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_module_builder_preserves_assigned_identity(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrFunction semantic;
    SZrExecIrBuildInput input = {0};
    SZrExecIrModule module;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrFunction *function;

    make_semantic_function(&semantic, &block, 1u);
    input.semanticFunction = &semantic;
    input.functionToken = 77u;
    input.signatureHash = 99u;
    ZrCore_ExecIr_ModuleInit(&module);
    check(ZrParser_ExecIr_BuildModule(&input, &module, &diagnostic),
          "module builder rejected valid canonical facts");
    function = ZrCore_ExecIr_ModuleFunctionAtConst(&module, 1u);
    check(module.functionCount == 1u && function != NULL &&
              function->id == 1u && function->functionToken == 77u &&
              function->signatureHash == 99u &&
              function->contract.targetToken == 77u &&
              function->contract.signatureHash == 99u &&
              function->contract.generation == 1u,
          "module builder lost the module-assigned function identity");
    check(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                       &diagnostic),
          "module builder published an invalid function ID or contract");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_module_builder_failure_does_not_append_partial_function(void) {
    SZrParserCfgBlock validBlock, invalidBlock;
    SZrSemanticIrFunction validSemantic, invalidSemantic;
    SZrExecIrBuildInput input = {0};
    SZrExecIrModule module;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&validSemantic, &validBlock, 1u);
    make_semantic_function(&invalidSemantic, &invalidBlock, 1u);
    invalidBlock.successorCount = 1u;
    invalidBlock.successors[0] = 1u; /* There is no target block 1. */
    input.semanticFunction = &invalidSemantic;
    input.functionToken = 77u;
    input.signatureHash = 99u;
    ZrCore_ExecIr_ModuleInit(&module);
    input.functionToken = 0u;
    check(!ZrParser_ExecIr_BuildModule(&input, &module, &diagnostic) &&
              diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT &&
              module.functionCount == 0u,
          "module builder accepted a missing function token");
    input.functionToken = 77u;
    check(!ZrParser_ExecIr_BuildModule(&input, &module, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              diagnostic.functionToken == 77u &&
              module.functionCount == 0u,
          "failed initial module build published a partial function");
    input.semanticFunction = &validSemantic;
    check(ZrParser_ExecIr_BuildModule(&input, &module, &diagnostic),
          "valid module build failed after a rejected input");
    input.semanticFunction = &invalidSemantic;
    check(!ZrParser_ExecIr_BuildModule(&input, &module, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK &&
              diagnostic.functionToken == 77u &&
              module.functionCount == 1u && module.functions[0].id == 1u &&
              module.functions[0].functionToken == 77u,
          "failed repeat module build changed the published function table");
    input.semanticFunction = &validSemantic;
    input.functionToken = 78u;
    check(ZrParser_ExecIr_BuildModule(&input, &module, &diagnostic) &&
              module.functionCount == 2u && module.functions[1].id == 2u &&
              module.functions[1].functionToken == 78u,
          "failed module build consumed the next stable function ID");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_builder_rejects_unbacked_canonical_fact_arrays(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instruction = {0};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not prepare existing output for malformed-fact checks");
    semantic.values = input_array(NULL, 1u, sizeof(SZrSemanticIrValue));
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              output.blockCount == 1u,
          "builder read an unbacked value array or changed caller output");
    semantic.values = input_array(NULL, 0u, sizeof(SZrSemanticIrValue));
    semantic.instructions = input_array(NULL, 1u, sizeof(SZrSemanticIrInstruction));
    block.instructionCount = 1u;
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              output.blockCount == 1u,
          "builder read an unbacked instruction array or changed caller output");
    block.instructionCount = 0u;
    semantic.instructions = input_array(NULL, 0u, sizeof(SZrSemanticIrInstruction));
    semantic.valueOperands = input_array(NULL, 1u, sizeof(TZrValueId));
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              output.blockCount == 1u,
          "builder accepted an unbacked operand side pool");
    semantic.valueOperands = input_array(NULL, 0u, sizeof(TZrValueId));
    semantic.instructions = input_array(&instruction, 1u, sizeof(TZrByte));
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              output.blockCount == 1u,
          "builder accepted the wrong instruction element width");
    semantic.instructions = input_array(&instruction, 1u, sizeof(instruction));
    semantic.instructions.length = 2u; /* Advertised length exceeds storage. */
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              output.blockCount == 1u,
          "builder accepted an instruction length past its capacity");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_missing_variadic_operands(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction call = {0};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    call.id = 1u;
    call.opcode = ZR_SEMANTIC_IR_CALL_TYPED;
    call.resultValueId = ZR_VALUE_ID_INVALID;
    call.operandCount = 1u;
    semantic.instructions = input_array(&call, 1u, sizeof(call));
    block.instructionCount = 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not prepare output for missing operand test");
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
              diagnostic.instructionId == 1u && output.blockCount == 1u,
          "builder silently dropped a variadic call's missing operand");
    {
        TZrValueId operand = 1u;
        semantic.valueOperands = input_array(&operand, 1u, sizeof(operand));
        call.operandStart = UINT32_MAX;
        check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
                  diagnostic.blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY &&
                  diagnostic.instructionId == 1u && output.blockCount == 1u,
              "builder accepted a wrapped semantic operand range");
    }
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_copies_valid_variadic_operands(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue values[2] = {
        {.id = 1u, .typeId = 1u}, {.id = 2u, .typeId = 1u}
    };
    TZrValueId operands[2] = {1u, 2u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
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
    instructions[2].operandStart = 1u;
    instructions[2].operandCount = 1u;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.values = input_array(values, 2u, sizeof(*values));
    semantic.valueOperands = input_array(operands, 2u, sizeof(*operands));
    block.instructionCount = 3u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic),
          "builder rejected a backed variadic operand");
    check(output.instructions[1].operands.count == 1u &&
              output.operands[output.instructions[1].operands.start] == 1u &&
              output.instructions[2].operands.count == 1u &&
              output.operands[output.instructions[2].operands.start] == 2u,
          "builder did not preserve the valid call operand");
    output.id = 1u;
    check(ZrCore_ExecIr_VerifyFunction(&output, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                       &diagnostic),
          "valid variadic call and return did not produce structural ExecIR");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_nonterminal_tail(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instruction = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    instruction.id = 1u;
    instruction.opcode = ZR_SEMANTIC_IR_CONSTANT;
    instruction.resultValueId = 1u;
    semantic.instructions = input_array(&instruction, 1u, sizeof(instruction));
    semantic.values = input_array(&value, 1u, sizeof(value));
    block.instructionCount = 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrCore_ExecIr_FunctionAddBlock(&output, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "could not prepare output for missing-terminator test");
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 1u &&
              output.blockCount == 1u,
          "builder published a nonterminal tail as a block terminator");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_preserves_canonical_type_test_target(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue values[2] = {
        {.id = 1u, .typeId = 11u},
        {.id = 2u, .typeId = 22u},
    };
    TZrValueId operands[2] = {1u, 2u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].typeId = 11u;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_TYPE_TEST;
    instructions[1].typeId = 22u;
    instructions[1].matchTypeId = 77u;
    instructions[1].resultValueId = 2u;
    instructions[1].operandCount = 1u;
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[2].operandStart = 1u;
    instructions[2].operandCount = 1u;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.values = input_array(values, 2u, sizeof(*values));
    semantic.valueOperands = input_array(operands, 2u, sizeof(*operands));
    block.instructionCount = 3u;

    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic),
          "builder rejected a canonical type-test instruction");
    check(output.instructionCount == 3u &&
              output.instructions[1].opcode == ZR_EXEC_IR_OPCODE_TYPE_TEST &&
              output.instructions[1].typeToken == 22u &&
              output.instructions[1].matchTypeToken == 77u &&
              output.instructions[1].operands.count == 1u &&
              output.operands[output.instructions[1].operands.start] == 1u &&
              output.instructions[1].results.count == 1u &&
              output.results[output.instructions[1].results.start] == 2u,
          "builder lost the type-test result, operand, or canonical match token");
    output.id = 1u;
    check(ZrCore_ExecIr_VerifyFunction(
                  &output, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
          "verifier rejected the canonical type-test instruction");
    output.instructions[1].matchTypeToken = 0u;
    check(!ZrCore_ExecIr_VerifyFunction(
                  &output, ZR_EXEC_IR_VERIFY_ALL, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 2u,
          "verifier accepted a type test without a canonical match token");
    output.instructions[1].matchTypeToken = 77u;
    output.instructions[0].matchTypeToken = 77u;
    check(!ZrCore_ExecIr_VerifyFunction(
                  &output, ZR_EXEC_IR_VERIFY_ALL, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 1u,
          "verifier accepted hidden type-match metadata on another opcode");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_type_test_without_match_type(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue values[2] = {
        {.id = 1u, .typeId = 11u},
        {.id = 2u, .typeId = 22u},
    };
    TZrValueId operands[2] = {1u, 2u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].typeId = 11u;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_TYPE_TEST;
    instructions[1].typeId = 22u;
    instructions[1].resultValueId = 2u;
    instructions[1].operandCount = 1u;
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[2].operandStart = 1u;
    instructions[2].operandCount = 1u;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.values = input_array(values, 2u, sizeof(*values));
    semantic.valueOperands = input_array(operands, 2u, sizeof(*operands));
    block.instructionCount = 3u;

    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 2u &&
              diagnostic.expectedVersion == 1u &&
              diagnostic.actualVersion == 0u,
          "builder accepted a type test without a canonical match type");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_premature_terminator(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    TZrValueId operands[2] = {1u, 1u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[1].operandCount = 1u;
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[2].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[2].operandStart = 1u;
    instructions[2].operandCount = 1u;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    semantic.valueOperands = input_array(operands, 2u, sizeof(*operands));
    block.instructionCount = 3u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 2u &&
              output.blockCount == 0u,
          "builder accepted instructions following a block terminator");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_branch_with_two_successors(void) {
    SZrParserCfgBlock blocks[3];
    SZrSemanticIrInstruction instruction = {0};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 3u);
    instruction.id = 1u;
    instruction.opcode = ZR_SEMANTIC_IR_BRANCH;
    instruction.resultValueId = ZR_VALUE_ID_INVALID;
    semantic.instructions = input_array(&instruction, 1u, sizeof(instruction));
    blocks[0].instructionCount = 1u;
    blocks[0].successorCount = 2u;
    blocks[0].successors[0] = 1u;
    blocks[0].successors[1] = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 1u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 2u &&
              output.blockCount == 0u,
          "builder lowered a two-target branch as a one-target opcode");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_return_with_successor(void) {
    SZrParserCfgBlock blocks[2];
    SZrSemanticIrInstruction instructions[2] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    TZrValueId operand = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[1].operandCount = 1u;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    semantic.valueOperands = input_array(&operand, 1u, sizeof(operand));
    blocks[0].instructionCount = 2u;
    blocks[0].successorCount = 1u;
    blocks[0].successors[0] = 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 2u &&
              diagnostic.expectedVersion == 0u && diagnostic.actualVersion == 1u &&
              output.blockCount == 0u,
          "builder published a return instruction with a normal successor");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_switch_without_successor(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[2] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    TZrValueId selector = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_SWITCH;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[1].operandCount = 1u;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    semantic.valueOperands = input_array(&selector, 1u, sizeof(selector));
    block.instructionCount = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 2u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 0u &&
              output.blockCount == 0u,
          "builder published a switch without any CFG successor");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_accepts_switch_with_successor(void) {
    SZrParserCfgBlock blocks[2];
    SZrSemanticIrInstruction instructions[2] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    TZrValueId selector = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_SWITCH;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[1].operandCount = 1u;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    semantic.valueOperands = input_array(&selector, 1u, sizeof(selector));
    blocks[0].instructionCount = 2u;
    blocks[0].successorCount = 1u;
    blocks[0].successors[0] = 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              output.instructions[1].successorRange.count == 1u &&
              output.successors[output.instructions[1].successorRange.start] == 2u,
          "builder rejected a switch with a real CFG successor");
    output.id = 1u;
    check(ZrCore_ExecIr_VerifyFunction(&output, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                       &diagnostic),
          "valid switch output was not structurally verifiable ExecIR");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_overlapping_instruction_owners(void) {
    SZrParserCfgBlock blocks[2];
    SZrSemanticIrInstruction branch = {0};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    branch.id = 1u;
    branch.opcode = ZR_SEMANTIC_IR_BRANCH;
    branch.resultValueId = ZR_VALUE_ID_INVALID;
    semantic.instructions = input_array(&branch, 1u, sizeof(branch));
    blocks[0].instructionCount = 1u;
    blocks[0].successorCount = 1u;
    blocks[0].successors[0] = 1u;
    blocks[1].instructionCount = 1u;
    blocks[1].successorCount = 1u;
    blocks[1].successors[0] = 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == 2u && diagnostic.instructionId == 1u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 2u &&
              output.blockCount == 0u,
          "builder let two blocks claim one semantic instruction");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_rejects_unowned_semantic_instruction(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction branch = {0};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    branch.id = 1u;
    branch.opcode = ZR_SEMANTIC_IR_BRANCH;
    branch.resultValueId = ZR_VALUE_ID_INVALID;
    semantic.instructions = input_array(&branch, 1u, sizeof(branch));
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.blockId == 0u && diagnostic.instructionId == 1u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 0u &&
              output.blockCount == 0u,
          "builder silently discarded a semantic instruction outside all blocks");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_accepts_out_of_order_instruction_slices(void) {
    SZrParserCfgBlock blocks[2];
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    TZrValueId operand = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, blocks, 2u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[0].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[0].operandCount = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[1].resultValueId = 1u;
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_BRANCH;
    instructions[2].resultValueId = ZR_VALUE_ID_INVALID;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    semantic.valueOperands = input_array(&operand, 1u, sizeof(operand));
    blocks[0].firstInstructionIndex = 1u;
    blocks[0].instructionCount = 2u;
    blocks[0].successorCount = 1u;
    blocks[0].successors[0] = 1u;
    blocks[1].firstInstructionIndex = 0u;
    blocks[1].instructionCount = 1u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic),
          "builder rejected disjoint semantic slices in a different block order");
    check(output.instructionCount == 3u && output.sourceMapCount == 3u &&
              output.sourceMaps[0].sourceId == 2u &&
              output.sourceMaps[2].sourceId == 1u &&
              output.blocks[0].instructionRange.start == 0u &&
              output.blocks[1].instructionRange.start == 2u,
          "builder lost source IDs while reordering disjoint block slices");
    output.id = 1u;
    check(ZrCore_ExecIr_VerifyFunction(&output, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                       &diagnostic),
          "out-of-order source slices produced malformed ExecIR");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_reports_missing_canonical_result(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[2] = {0};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    block.instructionCount = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 1u &&
              diagnostic.instructionId == 1u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 0u &&
              output.blockCount == 0u,
          "builder lost the source block for a missing canonical result");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_builder_reports_missing_fixed_operand(void) {
    SZrParserCfgBlock block;
    SZrSemanticIrInstruction instructions[2] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_semantic_function(&semantic, &block, 1u);
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_LOAD;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    block.instructionCount = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.functionToken == 42u && diagnostic.blockId == 1u &&
              diagnostic.instructionId == 1u &&
              diagnostic.expectedVersion == 1u && diagnostic.actualVersion == 0u &&
              output.blockCount == 0u,
          "builder lost the source block for a missing fixed operand");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_diamond_preserves_every_edge_and_predecessor();
    test_rejects_out_of_range_semantic_target();
    test_rejects_edge_with_wrong_source_block();
    test_accepts_edge_with_matching_source_block();
    test_rejects_cfg_block_with_mismatched_id();
    test_rejects_unknown_dynamic_edge_kind();
    test_inline_successors_preserve_both_edges();
    test_rejects_malformed_outgoing_edge_storage();
    test_rejects_outgoing_edges_beyond_declared_capacity();
    test_rejects_excess_inline_successors();
    test_builder_preserves_instruction_ranges_and_branch_successors();
    test_builder_preserves_canonical_type_test_target();
    test_builder_rejects_type_test_without_match_type();
    test_module_builder_failure_does_not_append_partial_function();
    test_module_builder_preserves_assigned_identity();
    test_builder_rejects_unbacked_canonical_fact_arrays();
    test_builder_rejects_missing_variadic_operands();
    test_builder_copies_valid_variadic_operands();
    test_builder_rejects_nonterminal_tail();
    test_builder_rejects_premature_terminator();
    test_builder_rejects_branch_with_two_successors();
    test_builder_rejects_return_with_successor();
    test_builder_rejects_switch_without_successor();
    test_builder_accepts_switch_with_successor();
    test_builder_rejects_overlapping_instruction_owners();
    test_builder_rejects_unowned_semantic_instruction();
    test_builder_accepts_out_of_order_instruction_slices();
    test_builder_reports_missing_canonical_result();
    test_builder_reports_missing_fixed_operand();
    puts("ssa builder CFG PASS");
    return EXIT_SUCCESS;
}
