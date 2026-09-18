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
                          SZrParserCfgBlock *blocks, SZrParserCfgEdge *edge) {
    memset(semantic, 0, sizeof(*semantic));
    memset(blocks, 0, sizeof(*blocks) * 2u);
    memset(edge, 0, sizeof(*edge));
    semantic->symbolId = (TZrSymbolId)42u;
    semantic->cfg.blocks = input_array(blocks, 2u, sizeof(*blocks));
    semantic->cfg.entryBlockId = 0u;
    semantic->instructions = input_array(NULL, 0u, sizeof(SZrSemanticIrInstruction));
    blocks[0].kind = ZR_PARSER_CFG_BLOCK_ENTRY;
    blocks[1].id = 1u;
    blocks[1].kind = ZR_PARSER_CFG_BLOCK_STATEMENT;
    edge->toBlockId = 1u;
    blocks[0].outgoingEdges = input_array(edge, 1u, sizeof(*edge));
}

static void test_rejects_unrepresentable_control_edges(void) {
    const EZrParserCfgEdgeKind kinds[] = {
        ZR_PARSER_CFG_EDGE_EXCEPTION, ZR_PARSER_CFG_EDGE_CLEANUP,
        ZR_PARSER_CFG_EDGE_RETURN, ZR_PARSER_CFG_EDGE_SUSPEND,
        ZR_PARSER_CFG_EDGE_RESUME
    };
    TZrSize i;

    for (i = 0u; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        SZrParserCfgBlock blocks[2];
        SZrParserCfgEdge edge;
        SZrSemanticIrFunction semantic;
        SZrExecIrFunction output;
        SZrExecIrDiagnostic diagnostic;

        make_function(&semantic, blocks, &edge);
        edge.kind = kinds[i];
        ZrCore_ExecIr_FunctionInit(&output);
        check(ZrCore_ExecIr_FunctionAddBlock(&output,
                  ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
              "could not prepare existing output for control-edge failure");
        check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
                  diagnostic.functionToken == 42u && diagnostic.blockId == 1u &&
                  diagnostic.instructionId == 0u &&
                  diagnostic.expectedVersion == ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT &&
                  diagnostic.actualVersion == (TZrUInt32)edge.kind &&
                  output.blockCount == 1u && output.successorCount == 0u,
              "builder lowered a semantic control edge as an ordinary successor");
        ZrCore_ExecIr_FreeFunction(&output);
    }
}

static void test_exception_edge_reports_throw_source(void) {
    SZrParserCfgBlock blocks[2];
    SZrParserCfgEdge edge;
    SZrSemanticIrInstruction instructions[2] = {0};
    SZrSemanticIrValue value = {.id = 1u, .typeId = 1u};
    TZrValueId operand = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, blocks, &edge);
    edge.kind = ZR_PARSER_CFG_EDGE_EXCEPTION;
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_THROW;
    instructions[1].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[1].operandCount = 1u;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic.values = input_array(&value, 1u, sizeof(value));
    semantic.valueOperands = input_array(&operand, 1u, sizeof(operand));
    blocks[0].instructionCount = 2u;
    ZrCore_ExecIr_FunctionInit(&output);
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 2u &&
              diagnostic.sourceId == 2u && output.blockCount == 0u,
          "exception edge was published or lost its throwing instruction site");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_accepts_ordinary_dynamic_edge(void) {
    SZrParserCfgBlock blocks[2];
    SZrParserCfgEdge edge;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, blocks, &edge);
    edge.kind = ZR_PARSER_CFG_EDGE_NORMAL;
    ZrCore_ExecIr_FunctionInit(&output);
    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              output.blocks[0].successorRange.count == 1u &&
              output.successors[output.blocks[0].successorRange.start] == 2u,
          "builder rejected a representable ordinary dynamic edge");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_rejects_unrepresentable_control_edges();
    test_exception_edge_reports_throw_source();
    test_accepts_ordinary_dynamic_edge();
    puts("ssa builder control edges PASS");
    return EXIT_SUCCESS;
}
