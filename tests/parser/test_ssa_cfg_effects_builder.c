#include "zr_vm_parser/exec_ir_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_true(TZrBool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static SZrExecIrFunction *new_function(SZrExecIrModule *module) {
    TZrExecIrFunctionId functionId;
    SZrExecIrFunction *function;

    ZrCore_ExecIr_ModuleInit(module);
    require_true(ZrCore_ExecIr_ModuleAddFunction(module, 1u, 1u, &functionId),
                 "add CFG-effects function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "query CFG-effects function");
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                         function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
                 "add CFG-effects entry block");
    return function;
}

static void append_instruction(SZrExecIrFunction *function,
                               EZrExecIrOpcode opcode) {
    SZrExecIrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    require_true(ZrCore_ExecIr_FunctionAppendInstruction(
                         function, &instruction, ZR_NULL),
                 "append CFG-effects instruction");
}

static void append_store(SZrExecIrFunction *function) {
    SZrExecIrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_STORE;
    require_true(ZrCore_ExecIr_FunctionAppendInstruction(
                         function, &instruction, ZR_NULL),
                 "append CFG-effects store");
}

static void set_block_ranges(SZrExecIrFunction *function,
                             const TZrUInt32 *starts,
                             const TZrUInt32 *counts) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        function->blocks[blockIndex].instructionRange.start = starts[blockIndex];
        function->blocks[blockIndex].instructionRange.count = counts[blockIndex];
    }
}

static void append_predecessor(SZrExecIrFunction *function,
                               TZrExecIrBlockId blockId,
                               const TZrExecIrBlockId *predecessors,
                               TZrUInt32 count) {
    require_true(ZrCore_ExecIr_FunctionAppendPredecessors(
                         function, predecessors, count,
                         &function->blocks[blockId - 1u].predecessorRange),
                 "append CFG-effects predecessor range");
}

static void test_acyclic_cfg_gets_effect_and_memory_phis(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry = 1u;
    TZrExecIrBlockId left = 2u;
    TZrExecIrBlockId right = 3u;
    TZrExecIrBlockId merge = 4u;
    TZrExecIrBlockId mergePreds[2] = {left, right};
    TZrExecIrBlockId entryPred = entry;
    TZrExecIrMemoryTokenId heapPhi;
    TZrExecIrEffectTokenId effectPhi;
    const SZrExecIrInstruction *leftStore;
    const SZrExecIrInstruction *rightStore1;
    const SZrExecIrInstruction *rightStore2;
    const SZrExecIrInstruction *mergeStore;
    const SZrExecIrPhiIncoming *incoming;

    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == left,
                 "add CFG-effects left block");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == right,
                 "add CFG-effects right block");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == merge,
                 "add CFG-effects merge block");

    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_store(function);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[4] = {0u, 1u, 2u, 4u};
        const TZrUInt32 counts[4] = {1u, 1u, 2u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, left, &entryPred, 1u);
    append_predecessor(function, right, &entryPred, 1u);
    append_predecessor(function, merge, mergePreds, 2u);

    function->phiIncomingCount = UINT32_MAX;
    function->phiIncomingCapacity = UINT32_MAX;
    require_true(!ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "CFG phi incoming overflow rejected");
    require_true(function->memoryTokenCount == 0u &&
                     function->instructions[1].memoryOut.count == 0u &&
                     function->instructions[1].effectOut == 0u &&
                     function->blocks[merge - 1u].effectPhiResult == 0u,
                 "CFG phi overflow leaves all effect facts unpublished");
    function->phiIncomingCount = 0u;
    function->phiIncomingCapacity = 0u;

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize acyclic CFG effects");
    require_true(function->memoryTokenCount == 7u,
                 "CFG producer emitted memory tokens for stores and merge call");
    require_true(function->blocks[merge - 1u].effectPhiResult != 0u,
                 "CFG producer emitted effect phi");
    require_true(function->blocks[merge - 1u].memoryPhiResults[
                         ZR_EXEC_IR_MEMORY_MANAGED_HEAP] != 0u,
                 "CFG producer emitted heap memory phi");

    leftStore = &function->instructions[1];
    rightStore1 = &function->instructions[2];
    rightStore2 = &function->instructions[3];
    mergeStore = &function->instructions[4];
    heapPhi = function->blocks[merge - 1u].memoryPhiResults[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP];
    effectPhi = function->blocks[merge - 1u].effectPhiResult;
    require_true(ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                         function->memoryTokenPool[leftStore->memoryOut.start]) == 2u,
                 "left CFG store advances past the initial heap state");
    require_true(ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                         function->memoryTokenPool[rightStore1->memoryOut.start]) == 3u &&
                     ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                         function->memoryTokenPool[rightStore2->memoryOut.start]) == 4u,
                 "right CFG stores advance heap versions");
    require_true(function->memoryTokenPool[mergeStore->memoryIn.start] == heapPhi &&
                     mergeStore->effectIn == effectPhi,
                 "merge consumers use both CFG phis");
    incoming = &function->phiIncoming[
            function->blocks[merge - 1u].memoryPhiIncomings[
                    ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start];
    require_true(incoming[0].predecessor == left &&
                     incoming[1].predecessor == right &&
                     incoming[0].value != incoming[1].value,
                 "heap memory phi preserves predecessor order");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "generated CFG effects pass verifier");

    ZrCore_ExecIr_FreeModule(&module);
}

static void test_loop_cfg_is_left_for_loop_aware_producer(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId body;
    TZrExecIrBlockId entry = 1u;

    body = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    require_true(body == 2u, "add loop body block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    {
        const TZrUInt32 starts[2] = {0u, 1u};
        const TZrUInt32 counts[2] = {1u, 2u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, body, &entry, 1u);
    append_predecessor(function, body, &body, 1u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "loop CFG producer preserves conservative fallback");
    require_true(function->memoryTokenCount == 0u &&
                     function->instructions[1].memoryOut.count == 0u &&
                     function->instructions[1].effectIn == 0u &&
                     function->instructions[1].effectOut == 0u,
                 "loop CFG producer does not invent non-phi facts");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_branch_read_preserves_initial_memory_state(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry = 1u;
    TZrExecIrBlockId left = 2u;
    TZrExecIrBlockId right = 3u;
    TZrExecIrBlockId merge = 4u;
    TZrExecIrBlockId mergePreds[2] = {left, right};
    TZrExecIrMemoryTokenId heapPhi;
    const SZrExecIrInstruction *leftStore;
    const SZrExecIrInstruction *rightLoad;
    const SZrExecIrInstruction *mergeCall;

    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == left,
                 "add branch-read left block");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == right,
                 "add branch-read right block");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == merge,
                 "add branch-read merge block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_LOAD);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[4] = {0u, 1u, 2u, 3u};
        const TZrUInt32 counts[4] = {1u, 1u, 1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, left, &entry, 1u);
    append_predecessor(function, right, &entry, 1u);
    append_predecessor(function, merge, mergePreds, 2u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize branch-read CFG effects");
    leftStore = &function->instructions[1];
    rightLoad = &function->instructions[2];
    mergeCall = &function->instructions[3];
    require_true(function->memoryTokenCount != 0u &&
                     leftStore->memoryOut.count == 1u &&
                     rightLoad->memoryIn.count == 1u,
                 "both branch memory operations receive tokens");
    require_true(ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                         function->memoryTokenPool[rightLoad->memoryIn.start]) == 1u &&
                     ZR_EXEC_IR_MEMORY_TOKEN_VERSION(
                         function->memoryTokenPool[leftStore->memoryOut.start]) > 1u,
                 "branch read uses initial heap state, distinct from sibling write");
    heapPhi = function->blocks[merge - 1u].memoryPhiResults[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP];
    require_true(heapPhi != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                     function->memoryTokenPool[mergeCall->memoryIn.start] == heapPhi,
                 "merge call consumes the distinct branch memory states");
    if (!ZrCore_ExecIr_VerifyEffects(function, &diagnostic)) {
        fprintf(stderr, "branch-read verifier: code=%u block=%u instruction=%u expected=%u actual=%u\n",
                (unsigned)diagnostic.code, (unsigned)diagnostic.blockId,
                (unsigned)diagnostic.instructionId,
                (unsigned)diagnostic.expectedVersion,
                (unsigned)diagnostic.actualVersion);
        require_true(ZR_FALSE, "branch-read CFG effects pass verifier");
    }
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_overlapping_block_ranges_are_rejected(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;

    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u,
                 "add overlapping-range block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    {
        const TZrUInt32 starts[2] = {0u, 0u};
        const TZrUInt32 counts[2] = {1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    require_true(!ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "overlapping CFG ranges rejected");
    require_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
                     diagnostic.instructionId == 2u &&
                     function->memoryTokenCount == 0u &&
                     function->instructions[0].effectIn == 0u,
                 "overlapping CFG range diagnostic preserves function");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_missing_predecessor_storage_is_rejected(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry = 1u;
    TZrExecIrBlockId *savedPredecessors;

    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == 2u,
                 "add missing-predecessor block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_RETURN);
    {
        const TZrUInt32 starts[2] = {0u, 1u};
        const TZrUInt32 counts[2] = {1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, 2u, &entry, 1u);
    savedPredecessors = function->predecessors;
    function->predecessors = ZR_NULL;
    require_true(!ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "missing CFG predecessor storage rejected");
    require_true(diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE &&
                     function->memoryTokenCount == 0u,
                 "missing predecessor diagnostic preserves function");
    function->predecessors = savedPredecessors;
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_untouched_branch_merges_with_written_memory(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry = 1u;
    TZrExecIrBlockId left = 2u;
    TZrExecIrBlockId right = 3u;
    TZrExecIrBlockId merge = 4u;
    TZrExecIrBlockId mergePreds[2] = {left, right};
    const SZrExecIrBlock *mergeBlock;
    const SZrExecIrPhiIncoming *incoming;

    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == left,
                 "add write arm");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == right,
                 "add untouched arm");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == merge,
                 "add write/untouched merge");
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[4] = {0u, 1u, 2u, 3u};
        const TZrUInt32 counts[4] = {1u, 1u, 1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, left, &entry, 1u);
    append_predecessor(function, right, &entry, 1u);
    append_predecessor(function, merge, mergePreds, 2u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize write/untouched CFG effects");
    mergeBlock = &function->blocks[merge - 1u];
    require_true(function->memoryTokenCount != 0u &&
                     mergeBlock->memoryPhiResults[
                             ZR_EXEC_IR_MEMORY_MANAGED_HEAP] != 0u &&
                     mergeBlock->effectPhiResult != 0u,
                 "untouched arm does not suppress merge token production");
    incoming = &function->phiIncoming[mergeBlock->memoryPhiIncomings[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start];
    require_true(incoming[0].predecessor == left &&
                     incoming[1].predecessor == right &&
                     incoming[1].value == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID,
                 "memory phi records untouched predecessor state");
    incoming = &function->phiIncoming[mergeBlock->effectPhiIncomings.start];
    require_true(incoming[0].predecessor == left &&
                     incoming[1].predecessor == right &&
                     incoming[1].value == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID,
                 "effect phi records untouched predecessor state");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "write/untouched CFG effects pass verifier");
    ZrCore_ExecIr_FreeModule(&module);
}

int main(void) {
    test_acyclic_cfg_gets_effect_and_memory_phis();
    test_loop_cfg_is_left_for_loop_aware_producer();
    test_branch_read_preserves_initial_memory_state();
    test_overlapping_block_ranges_are_rejected();
    test_missing_predecessor_storage_is_rejected();
    test_untouched_branch_merges_with_written_memory();
    puts("ssa CFG effects builder PASS");
    return EXIT_SUCCESS;
}
