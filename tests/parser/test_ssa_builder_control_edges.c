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

static void test_rejects_typed_control_in_inline_successors(void) {
    const EZrParserCfgTerminatorKind kinds[] = {
        ZR_PARSER_CFG_TERMINATOR_RETURN,
        ZR_PARSER_CFG_TERMINATOR_THROW,
        ZR_PARSER_CFG_TERMINATOR_SUSPEND,
        ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH,
        ZR_PARSER_CFG_TERMINATOR_EXIT
    };
    TZrSize i;

    for (i = 0u; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        SZrParserCfgBlock blocks[2];
        SZrParserCfgEdge edge;
        SZrSemanticIrFunction semantic;
        SZrExecIrFunction output;
        SZrExecIrDiagnostic diagnostic;

        make_function(&semantic, blocks, &edge);
        blocks[0].outgoingEdges.isValid = ZR_FALSE;
        blocks[0].successorCount = 1u;
        blocks[0].successors[0] = 1u;
        blocks[0].terminatorKind = kinds[i];
        ZrCore_ExecIr_FunctionInit(&output);
        check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
                  diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
                  diagnostic.functionToken == 42u && diagnostic.blockId == 1u &&
                  diagnostic.instructionId == 0u &&
                  diagnostic.actualVersion == (TZrUInt32)kinds[i] &&
                  output.blockCount == 0u,
              "builder silently published a typed control edge in an inline row");
        ZrCore_ExecIr_FreeFunction(&output);
    }
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

static void test_conditional_branch_requires_ordered_typed_edges(void) {
    SZrParserCfgBlock blocks[3];
    SZrParserCfgEdge edges[2];
    SZrSemanticIrInstruction instructions[2] = {0};
    SZrSemanticIrValue condition = {.id = 1u, .typeId = 1u};
    TZrValueId operand = 1u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, blocks, edges);
    memset(&blocks[2], 0, sizeof(blocks[2]));
    memset(&edges[1], 0, sizeof(edges[1]));
    blocks[2].id = 2u;
    blocks[2].kind = ZR_PARSER_CFG_BLOCK_STATEMENT;
    semantic.cfg.blocks = input_array(blocks, 3u, sizeof(*blocks));
    edges[0].kind = ZR_PARSER_CFG_EDGE_TRUE_BRANCH;
    edges[1].toBlockId = 2u;
    edges[1].kind = ZR_PARSER_CFG_EDGE_FALSE_BRANCH;
    blocks[0].outgoingEdges = input_array(edges, 2u, sizeof(*edges));
    blocks[0].terminatorKind = ZR_PARSER_CFG_TERMINATOR_BRANCH;
    blocks[0].instructionCount = 2u;
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_BRANCH;
    instructions[1].operandCount = 1u;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic.values = input_array(&condition, 1u, sizeof(condition));
    semantic.valueOperands = input_array(&operand, 1u, sizeof(operand));
    ZrCore_ExecIr_FunctionInit(&output);

    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              output.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
              output.instructions[1].operands.count == 1u &&
              output.operands[output.instructions[1].operands.start] == 1u &&
              output.instructions[1].successorRange.count == 2u &&
              output.successors[output.instructions[1].successorRange.start] == 2u &&
              output.successors[output.instructions[1].successorRange.start + 1u] == 3u,
          "typed true/false branch lost its condition or successor order");
    output.id = 1u;
    check(ZrCore_ExecIr_VerifyFunction(&output, ZR_EXEC_IR_VERIFY_STRUCTURE,
                                       &diagnostic),
          "conditional branch did not pass core structural verification");

    edges[0].kind = ZR_PARSER_CFG_EDGE_FALSE_BRANCH;
    edges[1].kind = ZR_PARSER_CFG_EDGE_TRUE_BRANCH;
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 2u &&
              diagnostic.expectedVersion == ZR_PARSER_CFG_EDGE_TRUE_BRANCH &&
              diagnostic.actualVersion == ZR_PARSER_CFG_EDGE_FALSE_BRANCH &&
              output.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
          "reversed true/false edges silently changed branch meaning");

    blocks[0].outgoingEdges.isValid = ZR_FALSE;
    blocks[0].successorCount = 2u;
    blocks[0].successors[0] = 1u;
    blocks[0].successors[1] = 2u;
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
              diagnostic.blockId == 1u && diagnostic.instructionId == 2u &&
              output.instructions[1].opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH,
          "untyped inline successors silently acquired true/false meaning");
    ZrCore_ExecIr_FreeFunction(&output);
}

static void test_typed_call_exception_edges_lower_to_invoke(void) {
    SZrParserCfgBlock blocks[3];
    SZrParserCfgEdge edges[2];
    SZrSemanticIrInstruction instructions[3] = {0};
    SZrSemanticIrValue values[2] = {{.id = 1u, .typeId = 1u},
                                    {.id = 2u, .typeId = 1u}};
    TZrValueId resultOperand = 2u;
    SZrSemanticIrFunction semantic;
    SZrExecIrFunction output;
    SZrExecIrDiagnostic diagnostic;

    make_function(&semantic, blocks, edges);
    memset(&blocks[2], 0, sizeof(blocks[2]));
    memset(&edges[1], 0, sizeof(edges[1]));
    blocks[2].id = 2u;
    blocks[2].kind = ZR_PARSER_CFG_BLOCK_STATEMENT;
    semantic.cfg.blocks = input_array(blocks, 3u, sizeof(*blocks));
    edges[0].toBlockId = 1u;
    edges[0].kind = ZR_PARSER_CFG_EDGE_NORMAL;
    edges[1].toBlockId = 2u;
    edges[1].kind = ZR_PARSER_CFG_EDGE_EXCEPTION;
    blocks[0].outgoingEdges = input_array(edges, 2u, sizeof(*edges));
    blocks[0].instructionCount = 2u;
    instructions[0].id = 1u;
    instructions[0].opcode = ZR_SEMANTIC_IR_CONSTANT;
    instructions[0].resultValueId = 1u;
    instructions[1].id = 2u;
    instructions[1].opcode = ZR_SEMANTIC_IR_CALL_TYPED;
    instructions[1].resultValueId = 2u;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    semantic.values = input_array(values, 2u, sizeof(*values));
    ZrCore_ExecIr_FunctionInit(&output);

    check(ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              output.instructions[1].opcode == ZR_EXEC_IR_OPCODE_INVOKE &&
              output.instructions[1].successorRange.count == 2u &&
              output.successors[output.instructions[1].successorRange.start] == 2u &&
              output.successors[output.instructions[1].successorRange.start + 1u] == 3u &&
              (output.blocks[2].flags & ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) != 0u,
          "typed call normal/exception edges were not preserved as INVOKE");

    /* A call result does not exist on the exceptional continuation. */
    instructions[2].id = 3u;
    instructions[2].opcode = ZR_SEMANTIC_IR_RETURN;
    instructions[2].resultValueId = ZR_VALUE_ID_INVALID;
    instructions[2].operandCount = 1u;
    blocks[2].firstInstructionIndex = 2u;
    blocks[2].instructionCount = 1u;
    semantic.instructions = input_array(instructions, 3u, sizeof(*instructions));
    semantic.valueOperands = input_array(&resultOperand, 1u, sizeof(resultOperand));
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE &&
              diagnostic.blockId == 3u && diagnostic.instructionId == 3u &&
              output.instructions[1].opcode == ZR_EXEC_IR_OPCODE_INVOKE,
          "exception handler used an INVOKE result that only exists on the normal edge");

    blocks[2].instructionCount = 0u;
    semantic.instructions = input_array(instructions, 2u, sizeof(*instructions));
    edges[0].kind = ZR_PARSER_CFG_EDGE_EXCEPTION;
    edges[1].kind = ZR_PARSER_CFG_EDGE_NORMAL;
    check(!ZrParser_ExecIr_Build(&semantic, NULL, &output, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED &&
              diagnostic.blockId == 1u && diagnostic.sourceId == 2u &&
              output.instructions[1].opcode == ZR_EXEC_IR_OPCODE_INVOKE,
          "reversed call edges silently changed the exceptional continuation");
    ZrCore_ExecIr_FreeFunction(&output);
}

int main(void) {
    test_rejects_unrepresentable_control_edges();
    test_exception_edge_reports_throw_source();
    test_rejects_typed_control_in_inline_successors();
    test_accepts_ordinary_dynamic_edge();
    test_conditional_branch_requires_ordered_typed_edges();
    test_typed_call_exception_edges_lower_to_invoke();
    puts("ssa builder control edges PASS");
    return EXIT_SUCCESS;
}
