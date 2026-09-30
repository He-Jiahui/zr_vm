#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_interpreter.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct SModuleConstantFixture {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
} SModuleConstantFixture;

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void build_constant_module(SModuleConstantFixture *fixture,
                                  TZrBool hasModulePool) {
    const SZrExecIrConstant constant = {
        (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64, 0u, UINT64_C(42)};
    TZrExecIrFunctionId functionId;
    TZrExecIrValueId constantValue;
    SZrExecIrRange resultRange = {0};
    SZrExecIrRange returnOperand = {0};
    SZrExecIrInstruction instruction;
    TZrExecIrInstructionId instructionId;

    memset(fixture, 0, sizeof(*fixture));
    ZrCore_ExecIr_ModuleInit(&fixture->module);
    fixture->module.id = 1u;
    fixture->module.moduleHash = UINT64_C(0x4d4f4455);

    if (hasModulePool) {
        SZrExecIrRange constantRange = {0};
        expect_true(ZrCore_ExecIr_ModuleAppendConstant(
                            &fixture->module, &constant, 1u, &constantRange) &&
                        constantRange.start == 0u && constantRange.count == 1u,
                    "module constant fixture pool setup failed");
    }

    expect_true(ZrCore_ExecIr_ModuleAddFunction(
                        &fixture->module,
                        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 91u),
                        UINT64_C(0x535341), &functionId),
                "module constant fixture function setup failed");
    fixture->function = ZrCore_ExecIr_ModuleFunctionAt(&fixture->module,
                                                       functionId);
    expect_true(fixture->function != ZR_NULL,
                "module constant fixture function lookup failed");
    expect_true(ZrCore_ExecIr_FunctionAddBlock(
                        fixture->function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ==
                        ZR_EXEC_IR_BLOCK_ID_ENTRY,
                "module constant fixture entry setup failed");

    constantValue = ZrCore_ExecIr_FunctionAddValue(
            fixture->function, (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64,
            ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    expect_true(constantValue != ZR_EXEC_IR_VALUE_ID_INVALID &&
                    ZrCore_ExecIr_FunctionAppendResults(
                            fixture->function, &constantValue, 1u, &resultRange),
                "module constant fixture result setup failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = resultRange;
    instruction.layoutId = 0u;
    instruction.sourceId = 701u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(
                        fixture->function, &instruction, &instructionId) &&
                    instructionId == 1u,
                "module constant fixture CONSTANT append failed");

    expect_true(ZrCore_ExecIr_FunctionAppendOperands(
                        fixture->function, &constantValue, 1u, &returnOperand),
                "module constant fixture return operand setup failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperand;
    instruction.sourceId = 702u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(
                        fixture->function, &instruction, &instructionId) &&
                    instructionId == 2u,
                "module constant fixture RETURN append failed");
    fixture->function->blocks[0].instructions.start = 0u;
    fixture->function->blocks[0].instructions.count = 2u;
    fixture->function->blocks[0].terminatorInstructionId = 2u;
}

static void free_constant_module(SModuleConstantFixture *fixture) {
    ZrCore_ExecIr_FreeModule(&fixture->module);
    memset(fixture, 0, sizeof(*fixture));
}

#include "test_ssa_module_constant_pool_verifier_cases.inc"

int main(void) {
    test_module_constant_pool_accepts_valid_reference();
    test_module_constant_pool_rejects_count_boundary_index();
    test_module_constant_pool_rejects_uint32_max_index();
    test_module_constant_pool_rejects_constant_type_mismatch();
    test_module_constant_pool_rejects_instruction_type_mismatch();
    test_function_only_verifier_does_not_require_an_owned_pool();
    test_module_without_owned_pool_accepts_external_oracle_pool();
    puts("ssa module constant pool verifier PASS");
    return EXIT_SUCCESS;
}
