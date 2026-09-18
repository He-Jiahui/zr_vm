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

static void test_accepts_explicit_external_entry_operand(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId external;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)78u;
    external = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(external == 1u, "could not create external entry value");
    check(ZrCore_ExecIr_FunctionAppendOperands(
                  &function, &external, 1u, NULL),
          "could not add external entry operand");
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not add external entry use");
    check(ZrParser_ExecIr_BuildSsa(&function, &diagnostic),
          "SSA pass rejected an explicit external entry value");
    check(ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic),
          "SSA verifier rejected an explicit external entry value");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_core_verifier_rejects_unmarked_undefined_operand(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction drop;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId undefined;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)79u;
    undefined = ZrCore_ExecIr_FunctionAddValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    check(ZrCore_ExecIr_FunctionAppendOperands(
                  &function, &undefined, 1u, NULL),
          "could not add undefined operand");
    memset(&drop, 0, sizeof(drop));
    drop.opcode = ZR_EXEC_IR_OPCODE_DROP;
    drop.operands.count = 1u;
    check(ZrCore_ExecIr_FunctionAppendInstruction(&function, &drop, NULL),
          "could not add undefined value use");
    check(!ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 1u,
          "SSA verifier accepted an unmarked undefined operand");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_external_entry_cannot_be_instruction_result(void) {
    SZrExecIrFunction function;
    SZrExecIrInstruction constant;
    TZrExecIrValueId external;

    ZrCore_ExecIr_FunctionInit(&function);
    external = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    memset(&constant, 0, sizeof(constant));
    constant.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    check(ZrCore_ExecIr_FunctionAppendResults(
                  &function, &external, 1u, &constant.results),
          "could not add external result fixture");
    check(!ZrCore_ExecIr_FunctionAppendInstruction(
                  &function, &constant, NULL) &&
              function.instructionCount == 0u,
          "ordinary instruction redefined an external entry value");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_external_entry_cannot_be_phi_result(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrPhi phi;
    TZrExecIrValueId external;
    TZrExecIrBlockId entry;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)81u;
    external = ZrCore_ExecIr_FunctionAddExternalValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    entry = ZrCore_ExecIr_FunctionAddBlock(
            &function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    check(external == 1u && entry == ZR_EXEC_IR_BLOCK_ID_ENTRY,
          "could not create external phi-result fixture");
    memset(&phi, 0, sizeof(phi));
    phi.result = external;
    check(ZrCore_ExecIr_FunctionAppendPhis(
                  &function, &phi, 1u, &function.blocks[0].phis),
          "could not append external phi-result fixture");
    check(!ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic) &&
              diagnostic.code ==
                      ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
          "SSA verifier accepted an external entry as a phi result");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_rejects_unknown_value_flags(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrValueId value;

    ZrCore_ExecIr_FunctionInit(&function);
    function.id = 1u;
    function.functionToken = (TZrMetadataToken)80u;
    value = ZrCore_ExecIr_FunctionAddValue(
            &function, (TZrMetadataToken)1u,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    function.values[value - 1u].flags = (TZrUInt32)1u << 1u;
    check(!ZrParser_ExecIr_BuildSsa(&function, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE &&
              diagnostic.instructionId == 0u,
          "SSA pass accepted unknown flags on an unused value");
    check(!ZrCore_ExecIr_VerifyFunction(
                  &function, ZR_EXEC_IR_VERIFY_SSA, &diagnostic) &&
              diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
          "verifier accepted an unknown value flag");
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
    test_accepts_explicit_external_entry_operand();
    test_core_verifier_rejects_unmarked_undefined_operand();
    test_external_entry_cannot_be_instruction_result();
    test_external_entry_cannot_be_phi_result();
    test_rejects_unknown_value_flags();
    test_rejects_range_past_logical_operand_count();
    test_rejects_wrapped_operand_range();
    test_rejects_missing_operand_storage();
    test_rejects_unknown_opcode_with_location();
    puts("ssa value validation PASS");
    return EXIT_SUCCESS;
}
