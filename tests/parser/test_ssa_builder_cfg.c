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

int main(void) {
    test_diamond_preserves_every_edge_and_predecessor();
    test_rejects_out_of_range_semantic_target();
    test_inline_successors_preserve_both_edges();
    test_rejects_malformed_outgoing_edge_storage();
    test_rejects_excess_inline_successors();
    puts("ssa builder CFG PASS");
    return EXIT_SUCCESS;
}
