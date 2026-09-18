#include "zr_vm_parser/exec_ir_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void make_defined_operand(SZrExecIrFunction *function) {
    TZrExecIrValueId value, result = 0u;
    TZrExecIrInstructionId instruction = 0u;
    SZrExecIrInstruction constant;

    ZrCore_ExecIr_FunctionInit(function);
    function->functionToken = (TZrMetadataToken)77u;
    function->entryBlockId = ZrCore_ExecIr_FunctionAddBlock(function,
                                                           ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    check(function->entryBlockId == 1u, "could not create entry block");
    value = ZrCore_ExecIr_FunctionAddValue(function, (TZrMetadataToken)1u,
                                          ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                          ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(value == 1u, "could not create defined value");
    result = value;
    memset(&constant, 0, sizeof(constant));
    constant.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    check(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                              &constant.results) &&
              ZrCore_ExecIr_FunctionAppendInstruction(function, &constant,
                                                      &instruction) &&
              instruction == 1u,
          "could not define the operand");
    check(ZrCore_ExecIr_FunctionAppendOperands(function, &value, 1u, NULL),
          "could not add the operand");
}

static void test_rejects_range_past_logical_operand_count(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrInstructionId id = 0u;

    make_defined_operand(&function);
    check(function.operandCapacity > function.operandCount,
          "fixture needs spare physical operand capacity");
    function.operands[function.operandCount] = 1u;
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.start = function.operandCount;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, &id) && id == 2u,
          "could not add the operation with an invalid logical range");
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "SSA pass accepted operands beyond the logical side-pool end");
    check(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.instructionId == 2u && diagnostic.functionToken == 77u,
          "invalid SSA operand range lost the function and instruction location");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_accepts_defined_operand_in_range(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;

    make_defined_operand(&function);
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not add the valid operand operation");
    check(ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "SSA pass rejected an in-range defined operand");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_wrapped_operand_range(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;

    make_defined_operand(&function);
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.start = UINT32_MAX;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not add the wrapped operand range");
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.instructionId == 2u,
          "SSA pass accepted an overflowing operand range");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_missing_operand_storage(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId *savedOperands;

    make_defined_operand(&function);
    savedOperands = function.operands;
    function.operands = NULL;
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
              diagnostic.functionToken == 77u,
          "SSA pass accepted a missing operand backing array");
    function.operands = savedOperands;
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_unknown_opcode_with_location(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction unknown;
    SZrExecIrDiagnostic diagnostic;

    make_defined_operand(&function);
    memset(&unknown, 0, sizeof(unknown));
    unknown.opcode = ZR_EXEC_IR_OPCODE_COUNT;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &unknown, NULL),
          "could not add the unknown opcode");
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE &&
              diagnostic.functionToken == 77u && diagnostic.instructionId == 2u,
          "SSA pass lost the unknown opcode location");
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_accepts_defined_operand_in_range();
    test_rejects_range_past_logical_operand_count();
    test_rejects_wrapped_operand_range();
    test_rejects_missing_operand_storage();
    test_rejects_unknown_opcode_with_location();
    puts("ssa value validation PASS");
    return EXIT_SUCCESS;
}
