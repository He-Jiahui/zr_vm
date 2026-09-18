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
    SZrParserCfgEdge leftEdge[1] = {{.toBlockId = 3u}};
    SZrParserCfgEdge rightEdge[1] = {{.toBlockId = 3u}};
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

int main(void) {
    test_diamond_preserves_every_edge_and_predecessor();
    test_rejects_out_of_range_semantic_target();
    test_inline_successors_preserve_both_edges();
    test_rejects_malformed_outgoing_edge_storage();
    test_rejects_excess_inline_successors();
    test_builder_preserves_instruction_ranges_and_branch_successors();
    test_module_builder_failure_does_not_append_partial_function();
    test_module_builder_preserves_assigned_identity();
    puts("ssa builder CFG PASS");
    return EXIT_SUCCESS;
}
