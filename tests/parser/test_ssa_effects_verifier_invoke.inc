#include "zr_vm_parser/exec_ir_builder.h"

static void append_invoke_memory_instruction(SZrExecIrFunction *function,
                                             EZrExecIrOpcode opcode,
                                             TZrExecIrValueId operand,
                                             TZrExecIrValueId result,
                                             TZrUInt32 source) {
    SZrExecIrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    instruction.sourceId = source;
    ok(ZrCore_ExecIr_FunctionAppendOperands(function, &operand, 1u,
                                             &instruction.operands),
       "invoke memory operand");
    if (result != ZR_EXEC_IR_VALUE_ID_INVALID) {
        ok(ZrCore_ExecIr_FunctionAppendResults(function, &result, 1u,
                                                &instruction.results),
           "invoke memory result");
    }
    if (opcode == ZR_EXEC_IR_OPCODE_INVOKE)
        instruction.successorRange = function->blocks[0].successorRange;
    ok(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction, NULL),
       "invoke memory instruction");
}

static SZrExecIrFunction *build_invoke_memory_successors(
        SZrExecIrModule *module) {
    TZrExecIrFunctionId id;
    SZrExecIrFunction *function = new_function(module, &id);
    TZrExecIrBlockId successors[2] = {2u, 3u};
    TZrExecIrBlockId entry = 1u;
    TZrExecIrValueId argument = ZrCore_ExecIr_FunctionAddExternalValue(
            function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    TZrExecIrValueId results[3];
    TZrUInt32 index;
    ok(argument != ZR_EXEC_IR_VALUE_ID_INVALID, "invoke memory argument");
    for (index = 0u; index < 3u; ++index) {
        results[index] = ZrCore_ExecIr_FunctionAddValue(
                function, 1u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                ZR_EXEC_IR_NULLABILITY_UNKNOWN);
        ok(results[index] != ZR_EXEC_IR_VALUE_ID_INVALID, "invoke memory values");
    }
    ok(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u,
       "invoke memory normal block");
    ok(ZrCore_ExecIr_FunctionAddBlock(
               function, ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION) == 3u,
       "invoke memory exception block");
    ok(ZrCore_ExecIr_FunctionAppendSuccessors(
               function, successors, 2u, &function->blocks[0].successorRange),
       "invoke memory successors");
    for (index = 1u; index < 3u; ++index) {
        ok(ZrCore_ExecIr_FunctionAppendPredecessors(
                   function, &entry, 1u, &function->blocks[index].predecessorRange),
           "invoke memory predecessor");
    }
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_INVOKE,
                                     argument, results[0], 1001u);
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_CALL,
                                     argument, results[1], 1002u);
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                                     results[1], 0u, 1003u);
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_CALL,
                                     argument, results[2], 1004u);
    append_invoke_memory_instruction(function, ZR_EXEC_IR_OPCODE_RETURN,
                                     results[2], 0u, 1005u);
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = 1u;
    function->blocks[0].terminatorInstructionId = 1u;
    function->blocks[1].instructions.start = 1u;
    function->blocks[1].instructions.count = 2u;
    function->blocks[1].terminatorInstructionId = 3u;
    function->blocks[2].instructions.start = 3u;
    function->blocks[2].instructions.count = 2u;
    function->blocks[2].terminatorInstructionId = 5u;
    return function;
}

static void assert_invoke_memory_diagnostic(
        const SZrExecIrDiagnostic *diagnostic,
        TZrUInt32 block, TZrUInt32 instruction, TZrUInt32 source,
        TZrUInt32 expected, TZrUInt32 actual) {
    ok(diagnostic->code == ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN &&
           diagnostic->functionToken == 1u && diagnostic->blockId == block &&
           diagnostic->instructionId == instruction && diagnostic->sourceId == source &&
           diagnostic->expectedVersion == expected && diagnostic->actualVersion == actual,
       "invoke memory diagnostic lost code, position, or versions");
}

static void test_invoke_memory_versions_on_both_successors(void) {
    SZrExecIrModule module;
    SZrExecIrDiagnostic diagnostic;
    const SZrExecIrOpcodeInfo *call = ZrCore_ExecIr_OpcodeInfo(ZR_EXEC_IR_OPCODE_CALL);
    const SZrExecIrOpcodeInfo *invoke = ZrCore_ExecIr_OpcodeInfo(ZR_EXEC_IR_OPCODE_INVOKE);
    SZrExecIrFunction *function;
    SZrExecIrFunction invokeOnly;
    SZrExecIrBlock entryOnly;
    SZrExecIrRange savedRange;
    TZrUInt32 successor;
    TZrUInt32 regionIndex;
    const EZrExecIrMemoryClass regions[2] = {
        ZR_EXEC_IR_MEMORY_MANAGED_HEAP, ZR_EXEC_IR_MEMORY_NATIVE_FFI};

    ok(invoke != ZR_NULL && call != ZR_NULL && call->memoryReads != 0u &&
           invoke->memoryReads == call->memoryReads &&
           invoke->memoryWrites == call->memoryWrites,
       "invoke schema omitted ordinary call memory effects");
    function = build_invoke_memory_successors(&module);
    ok(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
       "invoke successor memory synthesis");
    ok(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
       "invoke successor synthesized memory rejected");
    ok(function->instructions[0].memoryIn.count == 2u &&
           function->instructions[0].memoryOut.count == 2u,
       "invoke synthesis omitted heap/native ranges");

    for (successor = 0u; successor < 2u; ++successor) {
        TZrUInt32 instructionIndex = successor == 0u ? 1u : 3u;
        for (regionIndex = 0u; regionIndex < 2u; ++regionIndex) {
            TZrExecIrMemoryTokenId before = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                    regions[regionIndex], 1u);
            TZrExecIrMemoryTokenId after = ZR_EXEC_IR_MEMORY_TOKEN_MAKE(
                    regions[regionIndex], 2u);
            TZrUInt32 inputIndex = function->instructions[instructionIndex].memoryIn.start +
                                      regionIndex;
            ok(function->memoryTokenPool[function->instructions[0].memoryIn.start +
                                             regionIndex] == before &&
                   function->memoryTokenPool[function->instructions[0].memoryOut.start +
                                             regionIndex] == after &&
                   function->memoryTokenPool[inputIndex] == after,
               "invoke successor did not inherit heap/native output");
            function->memoryTokenPool[inputIndex] = before;
            ok(!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
               "invoke successor stale heap/native memory accepted");
            assert_invoke_memory_diagnostic(&diagnostic, successor + 2u,
                                             instructionIndex + 1u,
                                             1001u + instructionIndex, after, before);
            function->memoryTokenPool[inputIndex] = after;
        }
    }
    /* Isolate required-range checks from downstream CFG mismatch diagnostics. */
    invokeOnly = *function;
    entryOnly = function->blocks[0];
    entryOnly.successorRange.count = 0u;
    invokeOnly.blocks = &entryOnly;
    invokeOnly.blockCount = 1u;
    invokeOnly.blockCapacity = 1u;
    invokeOnly.instructionCount = 1u;
    savedRange = function->instructions[0].memoryIn;
    function->instructions[0].memoryIn.count = 0u;
    ok(!ZrCore_ExecIr_VerifyEffects(&invokeOnly, &diagnostic),
       "invoke missing memory input accepted");
    assert_invoke_memory_diagnostic(&diagnostic, 1u, 1u, 1001u, 1u, 0u);
    function->instructions[0].memoryIn = savedRange;
    savedRange = function->instructions[0].memoryOut;
    function->instructions[0].memoryOut.count = 0u;
    ok(!ZrCore_ExecIr_VerifyEffects(&invokeOnly, &diagnostic),
       "invoke missing memory output accepted");
    assert_invoke_memory_diagnostic(&diagnostic, 1u, 1u, 1001u, 1u, 0u);
    function->instructions[0].memoryOut = savedRange;
    ok(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
       "invoke successor memory repair rejected");
    ZrCore_ExecIr_FreeModule(&module);
}
