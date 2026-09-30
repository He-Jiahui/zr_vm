#include "ssa_cfg_effects_fault_allocator.h"
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

static void append_instruction(SZrExecIrFunction *function,
                               EZrExecIrOpcode opcode) {
    SZrExecIrInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (TZrUInt16)opcode;
    require_true(ZrCore_ExecIr_FunctionAppendInstruction(
                         function, &instruction, ZR_NULL),
                 "append natural-loop effect instruction");
}

static void append_predecessor(SZrExecIrFunction *function,
                               TZrExecIrBlockId blockId,
                               const TZrExecIrBlockId *predecessors,
                               TZrUInt32 count) {
    require_true(ZrCore_ExecIr_FunctionAppendPredecessors(
                         function, predecessors, count,
                         &function->blocks[blockId - 1u].predecessorRange),
                 "append natural-loop predecessor range");
}

static void build_natural_loop(SZrExecIrModule *module,
                               SZrExecIrFunction **outFunction) {
    TZrExecIrFunctionId functionId;
    SZrExecIrFunction *function;
    TZrExecIrBlockId headerPreds[2] = {1u, 3u};
    TZrExecIrBlockId header;
    TZrExecIrBlockId latch;
    TZrExecIrBlockId exitBlock;
    TZrExecIrBlockId entry = 1u;

    ZrCore_ExecIr_ModuleInit(module);
    require_true(ZrCore_ExecIr_ModuleAddFunction(module, 1u, 1u,
                                                  &functionId),
                 "add natural-loop effect function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "query natural-loop effect function");
    require_true(ZrCore_ExecIr_FunctionAddBlock(
                         function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == entry,
                 "add natural-loop entry block");
    header = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    latch = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    exitBlock = ZrCore_ExecIr_FunctionAddBlock(function, 0u);
    require_true(header == 2u && latch == 3u && exitBlock == 4u,
                 "add natural-loop header, latch, and exit");

    append_instruction(function, ZR_EXEC_IR_OPCODE_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH);
    append_instruction(function, ZR_EXEC_IR_OPCODE_STORE);
    append_instruction(function, ZR_EXEC_IR_OPCODE_CALL);
    function->blocks[0].instructionRange.start = 0u;
    function->blocks[0].instructionRange.count = 1u;
    function->blocks[1].instructionRange.start = 1u;
    function->blocks[1].instructionRange.count = 1u;
    function->blocks[2].instructionRange.start = 2u;
    function->blocks[2].instructionRange.count = 1u;
    function->blocks[3].instructionRange.start = 3u;
    function->blocks[3].instructionRange.count = 1u;

    append_predecessor(function, header, headerPreds, 2u);
    append_predecessor(function, latch, &header, 1u);
    append_predecessor(function, exitBlock, &header, 1u);
    *outFunction = function;
}

static void assert_unpublished_after_failure(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instructionsBefore,
        const SZrExecIrBlock *blocksBefore,
        TZrExecIrMemoryTokenId *memoryTokenPoolBefore,
        TZrUInt32 memoryTokenCapacityBefore,
        TZrUInt32 memoryTokenCountBefore,
        TZrUInt32 phiIncomingCountBefore) {
    require_true(function->memoryTokenPool == memoryTokenPoolBefore &&
                     function->memoryTokenCapacity == memoryTokenCapacityBefore &&
                     function->memoryTokenCount == memoryTokenCountBefore,
                 "failed memory-token append preserves the prior token pool");
    require_true(function->phiIncomingCount == phiIncomingCountBefore,
                 "failed token append restores the prior phi scope");
    require_true(memcmp(function->instructions, instructionsBefore,
                        (size_t)function->instructionCount *
                                sizeof(*function->instructions)) == 0,
                 "failed token append leaves instruction effects unpublished");
    require_true(memcmp(function->blocks, blocksBefore,
                        (size_t)function->blockCount * sizeof(*function->blocks)) == 0,
                 "failed token append leaves block phis unpublished");
}

static void assert_recovered_natural_loop(SZrExecIrFunction *function,
                                          SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrBlock *header = &function->blocks[1];
    const SZrExecIrPhiIncoming *memoryIncoming;
    const SZrExecIrPhiIncoming *effectIncoming;
    TZrExecIrMemoryTokenId memoryPhi = header->memoryPhiResults[
            ZR_EXEC_IR_MEMORY_MANAGED_HEAP];
    TZrExecIrEffectTokenId effectPhi = header->effectPhiResult;

    require_true(memoryPhi != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                     effectPhi != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID,
                 "retry publishes both natural-loop header phis");
    memoryIncoming = &function->phiIncoming[
            header->memoryPhiIncomings[ZR_EXEC_IR_MEMORY_MANAGED_HEAP].start];
    effectIncoming = &function->phiIncoming[header->effectPhiIncomings.start];
    require_true(memoryIncoming[0].predecessor == 1u &&
                     memoryIncoming[0].value ==
                             ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID &&
                     memoryIncoming[1].predecessor == 3u &&
                     memoryIncoming[1].value == function->memoryTokenPool[
                             function->instructions[2].memoryOut.start],
                 "retry preserves the forward and latch memory token inputs");
    require_true(effectIncoming[0].predecessor == 1u &&
                     effectIncoming[0].value == function->instructions[0].effectOut &&
                     effectIncoming[1].predecessor == 3u &&
                     effectIncoming[1].value == function->instructions[2].effectOut,
                 "retry preserves the forward and latch effect inputs");
    require_true(function->memoryTokenCount != 0u &&
                     function->phiIncomingCount >= 4u,
                 "retry publishes the token and phi pools");
    require_true(ZrCore_ExecIr_VerifyEffects(function, diagnostic),
                 "recovered natural-loop effects pass the core verifier");
}

static void test_token_pool_oom_leaves_loop_retryable(void) {
    size_t ordinal;
    size_t injectedFailures = 0u;
    TZrBool sawMemoryTokenPoolFailure = ZR_FALSE;
    TZrBool completedSweep = ZR_FALSE;

    for (ordinal = 1u; ordinal <= 3u; ++ordinal) {
        SZrExecIrModule module;
        SZrExecIrFunction *function;
        SZrExecIrDiagnostic diagnostic;
        SZrExecIrInstruction instructionsBefore[4];
        SZrExecIrBlock blocksBefore[4];
        TZrExecIrMemoryTokenId *memoryTokenPoolBefore;
        TZrUInt32 memoryTokenCapacityBefore;
        TZrUInt32 memoryTokenCountBefore;
        TZrUInt32 phiIncomingCountBefore;
        TZrBool synthesized;
        TZrBool allocationFailed;

        build_natural_loop(&module, &function);
        memcpy(instructionsBefore, function->instructions,
               sizeof(instructionsBefore));
        memcpy(blocksBefore, function->blocks, sizeof(blocksBefore));
        memoryTokenPoolBefore = function->memoryTokenPool;
        memoryTokenCapacityBefore = function->memoryTokenCapacity;
        memoryTokenCountBefore = function->memoryTokenCount;
        phiIncomingCountBefore = function->phiIncomingCount;

        ssa_cfg_effects_fault_fail_reallocation(ordinal);
        synthesized = ZrParser_ExecIr_SynthesizeCfgEffects(function,
                                                           &diagnostic);
        allocationFailed = ssa_cfg_effects_fault_reallocation_failed();
        if (allocationFailed) {
            ++injectedFailures;
            require_true(!synthesized &&
                             diagnostic.code == ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                         "pool realloc failure reports OOM");
            require_true(ssa_cfg_effects_fault_reallocation_attempts() == ordinal,
                         "failure injection reached the requested pool allocation");
            assert_unpublished_after_failure(
                    function, instructionsBefore, blocksBefore,
                    memoryTokenPoolBefore, memoryTokenCapacityBefore,
                    memoryTokenCountBefore, phiIncomingCountBefore);
            if (ordinal == 2u) {
                sawMemoryTokenPoolFailure = ZR_TRUE;
                require_true(function->phiIncoming != ZR_NULL &&
                                 function->phiIncomingCapacity != 0u &&
                                 function->phiIncomingCount == phiIncomingCountBefore,
                             "token-pool OOM rolls back a prior phi append");
            }

            ssa_cfg_effects_fault_fail_reallocation(0u);
            require_true(ZrParser_ExecIr_SynthesizeCfgEffects(function,
                                                              &diagnostic),
                         "retry the same natural-loop CFG after pool OOM");
            assert_recovered_natural_loop(function, &diagnostic);
        } else {
            require_true(synthesized && ordinal == 3u &&
                             ssa_cfg_effects_fault_reallocation_attempts() == 2u,
                         "pool allocation sweep ends immediately after both sites");
            ssa_cfg_effects_fault_fail_reallocation(0u);
            assert_recovered_natural_loop(function, &diagnostic);
            completedSweep = ZR_TRUE;
        }
        ZrCore_ExecIr_FreeModule(&module);
        if (completedSweep) break;
    }

    require_true(injectedFailures == 2u && sawMemoryTokenPoolFailure,
                 "sweep injects both phi-pool and memory-token-pool OOM");
    require_true(completedSweep,
                 "sweep observes the first allocation ordinal beyond the pool sites");
}

int main(void) {
    ssa_cfg_effects_fault_fail_reallocation(0u);
    test_token_pool_oom_leaves_loop_retryable();
    ssa_cfg_effects_fault_fail_reallocation(0u);
    puts("ssa CFG effect pool OOM atomicity PASS");
    return EXIT_SUCCESS;
}
