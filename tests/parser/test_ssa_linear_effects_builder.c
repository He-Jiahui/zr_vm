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

static void append_instruction(SZrExecIrFunction *function,
                               EZrExecIrOpcode opcode) {
    SZrExecIrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    check(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                   ZR_NULL),
          "append linear effect instruction");
}

static void test_single_block_memory_and_effect_chain(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrMemoryTokenId heapInput;
    TZrExecIrMemoryTokenId heapOutput;

    ZrCore_ExecIr_FunctionInit(&function);
    function.functionToken = 9001u;
    check(ZrCore_ExecIr_FunctionAddBlock(&function,
                                         ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "append linear entry block");
    append_instruction(&function, ZR_EXEC_IR_OPCODE_NOP);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_LOAD);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE);
    append_instruction(&function, ZR_EXEC_IR_OPCODE_RETURN);
    function.blocks[0].instructionRange.start = 0u;
    function.blocks[0].instructionRange.count = function.instructionCount;

    check(ZrParser_ExecIr_SynthesizeLinearEffects(&function, &diagnostic),
          "synthesize single-block effect contract");
    check(function.memoryTokenCount == 2u,
          "linear producer emitted one input and one output token");
    heapInput = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u);
    heapOutput = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u);
    check(function.memoryTokenPool[0] == heapInput &&
              function.memoryTokenPool[1] == heapOutput,
          "linear producer used region-local memory versions");
    check(function.instructions[1].memoryIn.count == 1u &&
              function.instructions[1].memoryIn.start == 0u &&
              function.instructions[2].memoryOut.count == 1u &&
              function.instructions[2].memoryOut.start == 1u,
          "linear producer attached memory ranges to instructions");
    check(function.instructions[1].effectIn == 0u &&
              function.instructions[1].effectOut == 0u &&
              function.instructions[2].effectIn == 1u &&
              function.instructions[2].effectOut == 2u &&
              (function.instructions[2].flags & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u,
          "linear producer chained observable effect and schema flags");
    check(ZrCore_ExecIr_VerifyEffects(&function, &diagnostic),
          "linear producer output rejected by effect verifier");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_multi_block_function_is_left_for_cfg_producer(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_FunctionInit(&function);
    check(ZrCore_ExecIr_FunctionAddBlock(&function,
                                         ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "append first CFG block");
    check(ZrCore_ExecIr_FunctionAddBlock(&function, 0u) == 2u,
          "append second CFG block");
    append_instruction(&function, ZR_EXEC_IR_OPCODE_STORE);
    function.blocks[0].instructionRange.count = 1u;
    check(ZrParser_ExecIr_SynthesizeLinearEffects(&function, &diagnostic),
          "multi-block producer did not no-op");
    check(function.memoryTokenCount == 0u &&
              function.instructions[0].effectIn == 0u &&
              function.instructions[0].effectOut == 0u &&
              function.instructions[0].flags == 0u,
          "multi-block producer overwrote CFG-owned effect state");
    ZrCore_ExecIr_FreeFunction(&function);
}

static void test_read_write_opcode_gets_both_memory_sides(void) {
    SZrExecIrFunction function;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_FunctionInit(&function);
    check(ZrCore_ExecIr_FunctionAddBlock(&function,
                                         ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
          "append call entry block");
    append_instruction(&function, ZR_EXEC_IR_OPCODE_CALL);
    function.blocks[0].instructionRange.count = 1u;
    check(ZrParser_ExecIr_SynthesizeLinearEffects(&function, &diagnostic),
          "synthesize call memory contract");
    check(function.memoryTokenCount == 4u &&
              function.instructions[0].memoryIn.count == 2u &&
              function.instructions[0].memoryOut.count == 2u &&
              ZR_EXEC_IR_MEMORY_TOKEN_VERSION(function.memoryTokenPool[0]) == 1u &&
              ZR_EXEC_IR_MEMORY_TOKEN_VERSION(function.memoryTokenPool[1]) == 1u &&
              ZR_EXEC_IR_MEMORY_TOKEN_VERSION(function.memoryTokenPool[2]) == 2u &&
              ZR_EXEC_IR_MEMORY_TOKEN_VERSION(function.memoryTokenPool[3]) == 2u,
          "read/write opcode did not receive paired region versions");
    check(function.instructions[0].effectIn == 1u &&
              function.instructions[0].effectOut == 2u &&
              (function.instructions[0].flags &
               (ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW)) ==
                  (ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW),
          "call did not receive schema-derived effect flags");
    check(ZrCore_ExecIr_VerifyEffects(&function, &diagnostic),
          "call memory/effect contract rejected by verifier");
    ZrCore_ExecIr_FreeFunction(&function);
}

int main(void) {
    test_single_block_memory_and_effect_chain();
    test_multi_block_function_is_left_for_cfg_producer();
    test_read_write_opcode_gets_both_memory_sides();
    puts("ssa linear effects builder tests passed");
    return EXIT_SUCCESS;
}
