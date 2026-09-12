#include "zr_vm_core/exec_ir.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void test_empty_module_and_entry_block(void) {
    SZrExecIrModule module;
    TZrExecIrFunctionId functionId;
    TZrExecIrBlockId blockId;
    SZrExecIrFunction *function;

    ZrCore_ExecIr_ModuleInit(&module);
    expect_true(module.functionCount == 0u, "new module has functions");
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&module,
                                                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u),
                                                UINT64_C(0x1234),
                                                &functionId),
                "function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, functionId);
    expect_true(function != ZR_NULL, "function query failed");
    blockId = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    expect_true(blockId == ZR_EXEC_IR_BLOCK_ID_ENTRY, "entry block did not receive explicit ID");
    expect_true(ZrCore_ExecIr_FunctionBlockAt(function, blockId) != ZR_NULL,
                "entry block query failed");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_side_arrays_clone_without_aliasing(void) {
    SZrExecIrModule source;
    SZrExecIrModule clone;
    SZrExecIrFunction *function;
    SZrExecIrFunction *clonedFunction;
    SZrExecIrInstruction instruction;
    TZrExecIrFunctionId functionId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrValueId valueId;
    TZrExecIrValueId operandIds[2];
    TZrExecIrValueId resultIds[1];
    TZrExecIrMemoryTokenId memoryTokens[1] = {7u};
    SZrExecIrPhi phi;
    TZrExecIrBlockId entryBlock;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&source);
    ZrCore_ExecIr_ModuleInit(&clone);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&source,
                                                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 8u),
                                                UINT64_C(0xabcdef),
                                                &functionId),
                "source function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&source, functionId);
    entryBlock = ZrCore_ExecIr_FunctionAddBlock(function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    expect_true(entryBlock == ZR_EXEC_IR_BLOCK_ID_ENTRY, "source entry block append failed");
    valueId = ZrCore_ExecIr_FunctionAddValue(function,
                                              ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u),
                                              ZR_EXEC_IR_OWNERSHIP_BORROWED,
                                              ZR_EXEC_IR_NULLABILITY_NONNULL);
    expect_true(valueId != ZR_EXEC_IR_VALUE_ID_INVALID, "value append failed");
    operandIds[0] = valueId;
    operandIds[1] = valueId;
    expect_true(ZrCore_ExecIr_FunctionAppendOperands(function, operandIds, 2u, NULL),
                "operand side array append failed");
    resultIds[0] = valueId;
    expect_true(ZrCore_ExecIr_FunctionAppendResults(function, resultIds, 1u, NULL),
                "result side array append failed");
    expect_true(ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryTokens, 1u, NULL),
                "memory token side array append failed");
    memset(&phi, 0, sizeof(phi));
    phi.result = valueId;
    expect_true(ZrCore_ExecIr_FunctionAppendPhis(function, &phi, 1u, NULL),
                "phi side array append failed");
    function->blocks[0].phis.count = 1u;
    function->frameLayout = (SZrExecIrFrameLayout *)calloc(1u, sizeof(*function->frameLayout));
    expect_true(function->frameLayout != ZR_NULL, "frame layout allocation failed");
    function->frameLayout->slotCount = 1u;
    function->frameLayout->slotCapacity = 1u;
    function->frameLayout->slots = (SZrExecIrFrameSlot *)calloc(1u, sizeof(*function->frameLayout->slots));
    expect_true(function->frameLayout->slots != ZR_NULL, "frame slot allocation failed");
    function->gcMap = (SZrExecIrGcMap *)calloc(1u, sizeof(*function->gcMap));
    expect_true(function->gcMap != ZR_NULL, "gc map allocation failed");
    function->gcMap->entryCount = 1u;
    function->gcMap->entryCapacity = 1u;
    function->gcMap->entries = (SZrExecIrGcMapEntry *)calloc(1u, sizeof(*function->gcMap->entries));
    expect_true(function->gcMap->entries != ZR_NULL, "gc map entry allocation failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_ADD;
    instruction.operandRange.start = 0u;
    instruction.operandRange.count = 2u;
    instruction.resultRange.start = 0u;
    instruction.resultRange.count = 1u;
    instruction.sourceId = 41u;
    instruction.typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(function,
                                                         &instruction,
                                                         &instructionId),
                "instruction append failed");
    expect_true(instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID,
                "instruction did not receive an ID");
    expect_true(ZrCore_ExecIr_ValidateModule(&source, &diagnostic),
                "valid source module was rejected");
    expect_true(ZrCore_ExecIr_CloneModule(&source, &clone, &diagnostic),
                "module clone failed");
    clonedFunction = ZrCore_ExecIr_ModuleFunctionAt(&clone, functionId);
    expect_true(clonedFunction != function, "clone unexpectedly aliases function");
    expect_true(clonedFunction->operandCount == function->operandCount &&
                    clonedFunction->instructionCount == function->instructionCount,
                "clone lost side-array counts");
    clonedFunction->operands[0] = ZR_EXEC_IR_VALUE_ID_INVALID;
    clonedFunction->instructions[0].sourceId = 99u;
    clonedFunction->frameLayout->slots[0].slotId = 99u;
    clonedFunction->gcMap->entries[0].site = 99u;
    expect_true(function->operands[0] == valueId && function->instructions[0].sourceId == 41u,
                "clone side arrays alias source");
    expect_true(function->frameLayout->slots[0].slotId == 0u &&
                    function->gcMap->entries[0].site == 0u,
                "clone state maps alias source");

    ZrCore_ExecIr_FreeModule(&clone);
    ZrCore_ExecIr_FreeModule(&source);
}

static void test_invalid_opcode_and_overflow_fail_before_allocation(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId functionId;
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&module);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(&module,
                                                ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 9u),
                                                UINT64_C(0x55),
                                                &functionId),
                "overflow fixture function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, functionId);
    expect_true(!ZrCore_ExecIr_FunctionReserveOperandsEx(function, SIZE_MAX, &diagnostic),
                "operand reserve overflow was accepted");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                "operand reserve overflow lost its diagnostic");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)UINT16_MAX;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(function,
                                                         &instruction,
                                                         NULL),
                "invalid opcode should be stored for verifier diagnostics");
    expect_true(!ZrCore_ExecIr_ValidateModule(&module, &diagnostic),
                "unknown opcode was accepted by structural validation");
    expect_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE &&
                    diagnostic.instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID,
                "unknown opcode diagnostic lost instruction identity");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_validation_rejects_null_operand_pool_without_dereference(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId functionId;
    SZrExecIrInstruction instruction;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(&module);
    expect_true(ZrCore_ExecIr_ModuleAddFunction(
                        &module,
                        ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 10u),
                        UINT64_C(0x66),
                        &functionId),
                "null operand fixture function append failed");
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, functionId);
    expect_true(ZrCore_ExecIr_FunctionAddBlock(function,
                                                ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ==
                        ZR_EXEC_IR_BLOCK_ID_ENTRY,
                "null operand fixture entry append failed");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_COPY;
    instruction.operandRange.count = 1u;
    /* Deliberately claim one occupied operand without providing a pool. */
    function->operandCount = 1u;
    function->operandCapacity = 1u;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
                "null operand fixture instruction append failed");
    expect_true(!ZrCore_ExecIr_ValidateModule(&module, &diagnostic),
                "null operand pool was accepted");
    expect_true(diagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                "null operand pool reported the wrong diagnostic");
    ZrCore_ExecIr_FreeModule(&module);
}

int main(void) {
    test_empty_module_and_entry_block();
    test_side_arrays_clone_without_aliasing();
    test_invalid_opcode_and_overflow_fail_before_allocation();
    test_validation_rejects_null_operand_pool_without_dereference();
    puts("ssa core model PASS");
    return EXIT_SUCCESS;
}
