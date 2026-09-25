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

static void test_sibling_observable_operations_get_distinct_effects(void) {
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
                 "add sibling-effect left block");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == right,
                 "add sibling-effect right block");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == merge,
                 "add sibling-effect merge block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_store(function);
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
                 "synthesize sibling effect chains");
    mergeBlock = &function->blocks[merge - 1u];
    require_true(function->instructions[1].effectOut !=
                     function->instructions[2].effectOut &&
                     mergeBlock->effectPhiResult != 0u,
                 "sibling effects require separate versions and merge phi");
    incoming = &function->phiIncoming[mergeBlock->effectPhiIncomings.start];
    require_true(incoming[0].value == function->instructions[1].effectOut &&
                     incoming[1].value == function->instructions[2].effectOut,
                 "effect phi retains exact sibling exit versions");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "distinct sibling effect versions pass verifier");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_single_latch_loop_gets_carried_effect_phis(
        TZrBool preheaderWrites) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry = 1u;
    TZrExecIrBlockId header = 2u;
    TZrExecIrBlockId latch = 3u;
    TZrExecIrBlockId exitBlock = 4u;
    TZrExecIrBlockId headerPreds[2] = {entry, latch};
    const SZrExecIrBlock *loopHeader;
    const SZrExecIrPhiIncoming *memoryIncoming;
    const SZrExecIrPhiIncoming *effectIncoming;
    TZrExecIrMemoryTokenId heapPhi;
    TZrExecIrEffectTokenId effectPhi;

    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == header,
                 "add loop header");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == latch,
                 "add loop latch");
    require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == exitBlock,
                 "add loop exit");
    if (preheaderWrites) append_store(function);
    else append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[4] = {0u, 1u, 2u, 3u};
        const TZrUInt32 counts[4] = {1u, 1u, 1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, header, headerPreds, 2u);
    append_predecessor(function, latch, &header, 1u);
    append_predecessor(function, exitBlock, &header, 1u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize single-latch loop effects");
    loopHeader = &function->blocks[header - 1u];
    heapPhi = loopHeader->memoryPhiResults[ZR_EXEC_IR_MEMORY_MANAGED_HEAP];
    effectPhi = loopHeader->effectPhiResult;
    require_true(heapPhi != 0u && effectPhi != 0u,
                 "loop header receives memory and effect phis");
    memoryIncoming = &function->phiIncoming[loopHeader->memoryPhiIncomings[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start];
    effectIncoming = &function->phiIncoming[loopHeader->effectPhiIncomings.start];
    require_true(memoryIncoming[0].value == (preheaderWrites
                         ? function->memoryTokenPool[function->instructions[0].memoryOut.start]
                         : ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID) &&
                     memoryIncoming[1].value ==
                         function->memoryTokenPool[function->instructions[2].memoryOut.start] &&
                     effectIncoming[0].value == function->instructions[0].effectOut &&
                     effectIncoming[1].value == function->instructions[2].effectOut,
                 "loop phis record exact forward and backedge exits");
    require_true(function->instructions[2].effectIn == effectPhi &&
                     function->memoryTokenPool[function->instructions[3].memoryIn.start] ==
                             heapPhi,
                 "loop body and exit consume header state");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "single-latch loop effects pass verifier");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_two_latch_loop_tracks_both_backedges(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId entry = 1u;
    TZrExecIrBlockId header = 2u;
    TZrExecIrBlockId body = 3u;
    TZrExecIrBlockId leftLatch = 4u;
    TZrExecIrBlockId rightLatch = 5u;
    TZrExecIrBlockId exitBlock = 6u;
    TZrExecIrBlockId headerPreds[3] = {entry, leftLatch, rightLatch};
    const SZrExecIrBlock *loopHeader;
    const SZrExecIrPhiIncoming *memoryIncoming;
    const SZrExecIrPhiIncoming *effectIncoming;
    TZrUInt32 blockIndex;

    for (blockIndex = 2u; blockIndex <= 6u; ++blockIndex) {
        require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == blockIndex,
                     "add two-latch loop block");
    }
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[6] = {0u, 1u, 2u, 3u, 5u, 7u};
        const TZrUInt32 counts[6] = {1u, 1u, 1u, 2u, 2u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, header, headerPreds, 3u);
    append_predecessor(function, body, &header, 1u);
    append_predecessor(function, leftLatch, &body, 1u);
    append_predecessor(function, rightLatch, &body, 1u);
    append_predecessor(function, exitBlock, &header, 1u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize two-latch loop effects");
    loopHeader = &function->blocks[header - 1u];
    require_true(loopHeader->memoryPhiResults[ZR_EXEC_IR_MEMORY_MANAGED_HEAP] != 0u &&
                     loopHeader->effectPhiResult != 0u,
                 "two-latch header receives carried phis");
    memoryIncoming = &function->phiIncoming[loopHeader->memoryPhiIncomings[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start];
    effectIncoming = &function->phiIncoming[loopHeader->effectPhiIncomings.start];
    require_true(loopHeader->memoryPhiIncomings[
                         ZR_EXEC_IR_MEMORY_MANAGED_HEAP].count == 3u &&
                     loopHeader->effectPhiIncomings.count == 3u &&
                     memoryIncoming[0].predecessor == entry &&
                     memoryIncoming[1].predecessor == leftLatch &&
                     memoryIncoming[2].predecessor == rightLatch &&
                     effectIncoming[0].predecessor == entry &&
                     effectIncoming[1].predecessor == leftLatch &&
                     effectIncoming[2].predecessor == rightLatch &&
                     memoryIncoming[1].value == function->memoryTokenPool[
                             function->instructions[3].memoryOut.start] &&
                     memoryIncoming[2].value == function->memoryTokenPool[
                             function->instructions[5].memoryOut.start] &&
                     effectIncoming[1].value == function->instructions[3].effectOut &&
                     effectIncoming[2].value == function->instructions[5].effectOut &&
                     effectIncoming[1].value != effectIncoming[2].value,
                 "both latch exits retain distinct edge-ordered tokens");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "two-latch loop effects pass verifier");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_sequential_loops_keep_distinct_carried_phis(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId firstPreds[2] = {1u, 3u};
    TZrExecIrBlockId secondPreds[2] = {2u, 5u};
    TZrExecIrBlockId predecessor;
    TZrUInt32 index;

    for (index = 2u; index <= 6u; ++index)
        require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == index,
                     "add sequential-loop block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[6] = {0u, 1u, 2u, 3u, 4u, 5u};
        const TZrUInt32 counts[6] = {1u, 1u, 1u, 1u, 1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, 2u, firstPreds, 2u);
    predecessor = 2u;
    append_predecessor(function, 3u, &predecessor, 1u);
    append_predecessor(function, 4u, secondPreds, 2u);
    predecessor = 4u;
    append_predecessor(function, 5u, &predecessor, 1u);
    append_predecessor(function, 6u, &predecessor, 1u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize sequential loops");
    require_true(function->blocks[1].effectPhiResult != 0u &&
                     function->blocks[3].effectPhiResult != 0u &&
                     function->blocks[1].memoryPhiResults[
                             ZR_EXEC_IR_MEMORY_MANAGED_HEAP] != 0u &&
                     function->blocks[3].memoryPhiResults[
                             ZR_EXEC_IR_MEMORY_MANAGED_HEAP] != 0u,
                 "each sequential loop gets independent carried phis");
    require_true(function->phiIncoming[
                         function->blocks[1].effectPhiIncomings.start + 1u].value ==
                         function->instructions[2].effectOut &&
                     function->phiIncoming[
                         function->blocks[3].effectPhiIncomings.start + 1u].value ==
                         function->instructions[4].effectOut,
                 "sequential loop phis refer to their own latches");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "sequential loop effects pass verifier");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_nested_loops_carry_inner_writes_through_outer_header(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId outerPreds[2] = {1u, 5u};
    TZrExecIrBlockId innerPreds[2] = {2u, 4u};
    TZrExecIrBlockId predecessor;
    TZrUInt32 index;

    for (index = 2u; index <= 6u; ++index)
        require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == index,
                     "add nested-loop block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[6] = {0u, 1u, 2u, 3u, 4u, 5u};
        const TZrUInt32 counts[6] = {1u, 1u, 1u, 1u, 1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, 2u, outerPreds, 2u);
    append_predecessor(function, 3u, innerPreds, 2u);
    predecessor = 3u;
    append_predecessor(function, 4u, &predecessor, 1u);
    append_predecessor(function, 5u, &predecessor, 1u);
    predecessor = 2u;
    append_predecessor(function, 6u, &predecessor, 1u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize nested loops");
    require_true(function->blocks[1].effectPhiResult != 0u &&
                     function->blocks[2].effectPhiResult != 0u &&
                     function->blocks[1].memoryPhiResults[
                             ZR_EXEC_IR_MEMORY_MANAGED_HEAP] != 0u &&
                     function->blocks[2].memoryPhiResults[
                             ZR_EXEC_IR_MEMORY_MANAGED_HEAP] != 0u,
                 "inner write reaches both loop headers");
    require_true(function->phiIncoming[
                         function->blocks[2].effectPhiIncomings.start + 1u].value ==
                         function->instructions[3].effectOut &&
                     function->phiIncoming[
                         function->blocks[1].effectPhiIncomings.start + 1u].value ==
                         function->blocks[2].effectPhiResult,
                 "nested loop backedges carry the inner exit token");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "nested loop effects pass verifier");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_exit_only_write_does_not_create_loop_phi(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId headerPreds[2] = {1u, 4u};
    TZrExecIrBlockId predecessor;
    TZrUInt32 index;

    for (index = 2u; index <= 5u; ++index)
        require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == index,
                     "add exit-only write loop block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[5] = {0u, 1u, 2u, 3u, 4u};
        const TZrUInt32 counts[5] = {1u, 1u, 1u, 1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, 2u, headerPreds, 2u);
    predecessor = 2u;
    append_predecessor(function, 3u, &predecessor, 1u);
    append_predecessor(function, 4u, &predecessor, 1u);
    predecessor = 3u;
    append_predecessor(function, 5u, &predecessor, 1u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize loop with exit-only store");
    require_true(function->blocks[1].effectPhiResult == 0u &&
                     function->blocks[1].memoryPhiResults[
                             ZR_EXEC_IR_MEMORY_MANAGED_HEAP] == 0u &&
                     function->instructions[2].effectOut != 0u,
                 "exit-only write is not carried around loop");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "exit-only write loop effects pass verifier");
    ZrCore_ExecIr_FreeModule(&module);
}

static void test_three_latches_preserve_each_terminal_effect(void) {
    SZrExecIrModule module;
    SZrExecIrFunction *function = new_function(&module);
    SZrExecIrDiagnostic diagnostic;
    TZrExecIrBlockId headerPreds[4] = {1u, 4u, 5u, 6u};
    TZrExecIrBlockId predecessor;
    const SZrExecIrBlock *header;
    const SZrExecIrPhiIncoming *incoming;
    TZrUInt32 index;

    for (index = 2u; index <= 7u; ++index)
        require_true(ZrCore_ExecIr_FunctionAddBlock(function, 0u) == index,
                     "add three-latch loop block");
    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    for (index = 0u; index < 3u; ++index) append_store(function);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    {
        const TZrUInt32 starts[7] = {0u, 1u, 2u, 3u, 4u, 5u, 6u};
        const TZrUInt32 counts[7] = {1u, 1u, 1u, 1u, 1u, 1u, 1u};
        set_block_ranges(function, starts, counts);
    }
    append_predecessor(function, 2u, headerPreds, 4u);
    predecessor = 2u;
    append_predecessor(function, 3u, &predecessor, 1u);
    predecessor = 3u;
    for (index = 4u; index <= 6u; ++index)
        append_predecessor(function, index, &predecessor, 1u);
    predecessor = 2u;
    append_predecessor(function, 7u, &predecessor, 1u);

    require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function, &diagnostic),
                 "synthesize three-latch loop");
    header = &function->blocks[1];
    require_true(header->effectPhiIncomings.count == 4u &&
                     header->memoryPhiIncomings[
                             ZR_EXEC_IR_MEMORY_MANAGED_HEAP].count == 4u,
                 "three-latch loop carries all edges");
    incoming = &function->phiIncoming[header->effectPhiIncomings.start];
    for (index = 0u; index < 3u; ++index)
        require_true(incoming[index + 1u].predecessor == index + 4u &&
                             incoming[index + 1u].value ==
                                     function->instructions[index + 3u].effectOut,
                     "three-latch effect phi retains each latch exit");
    require_true(ZrCore_ExecIr_VerifyEffects(function, &diagnostic),
                 "three-latch effects pass verifier");
    ZrCore_ExecIr_FreeModule(&module);
}

int main(void) {
    test_acyclic_cfg_gets_effect_and_memory_phis();
    test_loop_cfg_is_left_for_loop_aware_producer();
    test_branch_read_preserves_initial_memory_state();
    test_overlapping_block_ranges_are_rejected();
    test_missing_predecessor_storage_is_rejected();
    test_untouched_branch_merges_with_written_memory();
    test_sibling_observable_operations_get_distinct_effects();
    test_single_latch_loop_gets_carried_effect_phis(ZR_TRUE);
    test_single_latch_loop_gets_carried_effect_phis(ZR_FALSE);
    test_two_latch_loop_tracks_both_backedges();
    test_sequential_loops_keep_distinct_carried_phis();
    test_nested_loops_carry_inner_writes_through_outer_header();
    test_exit_only_write_does_not_create_loop_phi();
    test_three_latches_preserve_each_terminal_effect();
    puts("ssa CFG effects builder PASS");
    return EXIT_SUCCESS;
}
