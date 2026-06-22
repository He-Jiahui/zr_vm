#include "dataflow.h"

#include "zr_vm_core/memory.h"

static SZrParserCfgBlock *dataflow_cfg_block(const SZrParserCfg *cfg, TZrUInt32 blockId) {
    if (cfg == ZR_NULL || !cfg->blocks.isValid || blockId >= cfg->blocks.length) {
        return ZR_NULL;
    }
    return (SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&cfg->blocks, blockId);
}

static SZrParserDataflowBlockState *dataflow_result_block(SZrParserDataflowResult *result,
                                                          TZrUInt32 blockId) {
    if (result == ZR_NULL || !result->blockStates.isValid || blockId >= result->blockStates.length) {
        return ZR_NULL;
    }
    return (SZrParserDataflowBlockState *)ZrCore_Array_Get(&result->blockStates, blockId);
}

static const SZrParserDataflowBlockState *dataflow_result_block_const(
        const SZrParserDataflowResult *result,
        TZrUInt32 blockId) {
    if (result == ZR_NULL || !result->blockStates.isValid || blockId >= result->blockStates.length) {
        return ZR_NULL;
    }
    return (const SZrParserDataflowBlockState *)ZrCore_Array_Get((SZrArray *)&result->blockStates, blockId);
}

static TZrBool dataflow_enqueue(TZrUInt32 *queue,
                                TZrBool *queued,
                                TZrSize capacity,
                                TZrSize *tail,
                                TZrSize *count,
                                TZrUInt32 blockId) {
    if (queue == ZR_NULL || queued == ZR_NULL || tail == ZR_NULL || count == ZR_NULL ||
        blockId >= capacity) {
        return ZR_FALSE;
    }
    if (queued[blockId]) {
        return ZR_TRUE;
    }
    if (*count >= capacity) {
        return ZR_FALSE;
    }
    queue[*tail] = blockId;
    queued[blockId] = ZR_TRUE;
    *tail = (*tail + 1) % capacity;
    (*count)++;
    return ZR_TRUE;
}

static void dataflow_free_queue(SZrState *state,
                                TZrUInt32 *queue,
                                TZrBool *queued,
                                TZrSize blockCount) {
    if (state == ZR_NULL) {
        return;
    }
    if (queue != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      queue,
                                      blockCount * sizeof(TZrUInt32),
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (queued != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      queued,
                                      blockCount * sizeof(TZrBool),
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
}

static void dataflow_mark_entry_reachable(const SZrParserCfg *cfg,
                                          TZrUInt32 blockId,
                                          TZrBool *entryReachable) {
    SZrParserCfgBlock *block;
    TZrUInt32 index;

    if (cfg == ZR_NULL || entryReachable == ZR_NULL ||
        blockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        blockId >= cfg->blocks.length ||
        entryReachable[blockId]) {
        return;
    }

    block = dataflow_cfg_block(cfg, blockId);
    if (block == ZR_NULL) {
        return;
    }

    entryReachable[blockId] = ZR_TRUE;
    for (index = 0; index < block->successorCount; index++) {
        dataflow_mark_entry_reachable(cfg, block->successors[index], entryReachable);
    }
}

static TZrBool *dataflow_compute_entry_reachability(SZrState *state, const SZrParserCfg *cfg) {
    TZrBool *entryReachable;
    TZrSize blockCount;

    if (state == ZR_NULL || cfg == ZR_NULL || !cfg->blocks.isValid ||
        cfg->entryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_NULL;
    }

    blockCount = cfg->blocks.length;
    entryReachable = (TZrBool *)ZrCore_Memory_RawMallocWithType(state->global,
                                                               blockCount * sizeof(TZrBool),
                                                               ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (entryReachable == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(entryReachable, 0, blockCount * sizeof(TZrBool));
    dataflow_mark_entry_reachable(cfg, cfg->entryBlockId, entryReachable);
    return entryReachable;
}

static void dataflow_free_reachability(SZrState *state,
                                       TZrBool *entryReachable,
                                       TZrSize blockCount) {
    if (state != ZR_NULL && entryReachable != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      entryReachable,
                                      blockCount * sizeof(TZrBool),
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
}

void ZrParser_DataflowResult_Init(SZrParserDataflowResult *result) {
    if (result == ZR_NULL) {
        return;
    }
    ZrCore_Array_Construct(&result->blockStates);
    result->stateSize = 0;
}

void ZrParser_DataflowResult_Free(SZrState *state, SZrParserDataflowResult *result) {
    TZrSize index;

    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (result->blockStates.isValid) {
        for (index = 0; index < result->blockStates.length; index++) {
            SZrParserDataflowBlockState *blockState =
                    (SZrParserDataflowBlockState *)ZrCore_Array_Get(&result->blockStates, index);
            if (blockState != ZR_NULL && blockState->inState != ZR_NULL && result->stateSize > 0) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              blockState->inState,
                                              result->stateSize,
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
                blockState->inState = ZR_NULL;
            }
            if (blockState != ZR_NULL && blockState->outState != ZR_NULL && result->stateSize > 0) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              blockState->outState,
                                              result->stateSize,
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
                blockState->outState = ZR_NULL;
            }
        }
        ZrCore_Array_Free(state, &result->blockStates);
    }

    result->stateSize = 0;
}

const SZrParserDataflowBlockState *ZrParser_Dataflow_GetBlockState(
        const SZrParserDataflowResult *result,
        TZrUInt32 blockId) {
    return dataflow_result_block_const(result, blockId);
}

static TZrBool dataflow_prepare_result(SZrState *state,
                                       const SZrParserCfg *cfg,
                                       const SZrParserDataflowAnalysis *analysis,
                                       SZrParserDataflowResult *result) {
    TZrSize index;

    if (result->blockStates.isValid) {
        ZrParser_DataflowResult_Free(state, result);
    }

    ZrCore_Array_Init(state,
                      &result->blockStates,
                      sizeof(SZrParserDataflowBlockState),
                      cfg->blocks.length);
    result->stateSize = analysis->stateSize;

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrParserDataflowBlockState blockState;

        blockState.isReachable = ZR_FALSE;
        blockState.inState = ZrCore_Memory_RawMallocWithType(state->global,
                                                             analysis->stateSize,
                                                             ZR_MEMORY_NATIVE_TYPE_ARRAY);
        blockState.outState = ZrCore_Memory_RawMallocWithType(state->global,
                                                              analysis->stateSize,
                                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (blockState.inState == ZR_NULL || blockState.outState == ZR_NULL) {
            if (blockState.inState != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              blockState.inState,
                                              analysis->stateSize,
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            if (blockState.outState != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              blockState.outState,
                                              analysis->stateSize,
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            ZrParser_DataflowResult_Free(state, result);
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(blockState.inState, 0, analysis->stateSize);
        ZrCore_Memory_RawSet(blockState.outState, 0, analysis->stateSize);
        ZrCore_Array_Push(state, &result->blockStates, &blockState);
    }

    return ZR_TRUE;
}

static TZrBool dataflow_process_forward(SZrState *state,
                                        const SZrParserCfg *cfg,
                                        const SZrParserDataflowAnalysis *analysis,
                                        SZrParserDataflowResult *result) {
    TZrSize blockCount = cfg->blocks.length;
    TZrUInt32 *queue;
    TZrBool *queued;
    TZrBool *entryReachable;
    TZrSize head = 0;
    TZrSize tail = 0;
    TZrSize count = 0;
    SZrParserDataflowBlockState *entryState;

    entryReachable = dataflow_compute_entry_reachability(state, cfg);
    if (entryReachable == ZR_NULL) {
        return ZR_FALSE;
    }

    queue = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(state->global,
                                                         blockCount * sizeof(TZrUInt32),
                                                         ZR_MEMORY_NATIVE_TYPE_ARRAY);
    queued = (TZrBool *)ZrCore_Memory_RawMallocWithType(state->global,
                                                        blockCount * sizeof(TZrBool),
                                                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (queue == ZR_NULL || queued == ZR_NULL) {
        dataflow_free_queue(state, queue, queued, blockCount);
        dataflow_free_reachability(state, entryReachable, blockCount);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(queued, 0, blockCount * sizeof(TZrBool));

    entryState = dataflow_result_block(result, cfg->entryBlockId);
    if (entryState == ZR_NULL) {
        dataflow_free_queue(state, queue, queued, blockCount);
        dataflow_free_reachability(state, entryReachable, blockCount);
        return ZR_FALSE;
    }
    entryState->isReachable = ZR_TRUE;
    analysis->initEntry(entryState->inState, analysis->userData);
    ZrCore_Memory_RawCopy(entryState->outState, entryState->inState, analysis->stateSize);
    if (!dataflow_enqueue(queue, queued, blockCount, &tail, &count, cfg->entryBlockId)) {
        dataflow_free_queue(state, queue, queued, blockCount);
        dataflow_free_reachability(state, entryReachable, blockCount);
        return ZR_FALSE;
    }

    while (count > 0) {
        TZrUInt32 blockId = queue[head];
        SZrParserCfgBlock *block = dataflow_cfg_block(cfg, blockId);
        SZrParserDataflowBlockState *blockState = dataflow_result_block(result, blockId);
        TZrUInt32 successorIndex;

        head = (head + 1) % blockCount;
        count--;
        queued[blockId] = ZR_FALSE;
        if (block == ZR_NULL || blockState == ZR_NULL || !blockState->isReachable) {
            continue;
        }

        ZrCore_Memory_RawCopy(blockState->outState, blockState->inState, analysis->stateSize);
        if (block->kind == ZR_PARSER_CFG_BLOCK_STATEMENT &&
            block->statement != ZR_NULL &&
            analysis->transferStatement != ZR_NULL) {
            analysis->transferStatement(block->statement, blockState->outState, analysis->userData);
        }

        for (successorIndex = 0; successorIndex < block->successorCount; successorIndex++) {
            TZrUInt32 successorId = block->successors[successorIndex];
            SZrParserDataflowBlockState *successorState = dataflow_result_block(result, successorId);
            TZrBool wasReachable;
            TZrBool changed;

            if (successorState == ZR_NULL) {
                continue;
            }
            if (!entryReachable[successorId]) {
                continue;
            }

            wasReachable = successorState->isReachable;
            successorState->isReachable = ZR_TRUE;
            if (!wasReachable) {
                ZrCore_Memory_RawCopy(successorState->inState,
                                      blockState->outState,
                                      analysis->stateSize);
                changed = ZR_TRUE;
            } else {
                changed = analysis->join(successorState->inState,
                                         blockState->outState,
                                         analysis->userData);
            }
            if (changed) {
                if (!dataflow_enqueue(queue, queued, blockCount, &tail, &count, successorId)) {
                    dataflow_free_queue(state, queue, queued, blockCount);
                    dataflow_free_reachability(state, entryReachable, blockCount);
                    return ZR_FALSE;
                }
            }
        }
    }

    dataflow_free_queue(state, queue, queued, blockCount);
    dataflow_free_reachability(state, entryReachable, blockCount);
    return ZR_TRUE;
}

static TZrBool dataflow_block_has_successor(const SZrParserCfgBlock *block, TZrUInt32 successorId) {
    TZrUInt32 index;

    if (block == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < block->successorCount; index++) {
        if (block->successors[index] == successorId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool dataflow_process_backward(SZrState *state,
                                         const SZrParserCfg *cfg,
                                         const SZrParserDataflowAnalysis *analysis,
                                         SZrParserDataflowResult *result) {
    TZrSize blockCount = cfg->blocks.length;
    TZrUInt32 *queue;
    TZrBool *queued;
    TZrBool *entryReachable;
    TZrSize head = 0;
    TZrSize tail = 0;
    TZrSize count = 0;
    SZrParserDataflowBlockState *exitState;

    entryReachable = dataflow_compute_entry_reachability(state, cfg);
    if (entryReachable == ZR_NULL) {
        return ZR_FALSE;
    }

    queue = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(state->global,
                                                         blockCount * sizeof(TZrUInt32),
                                                         ZR_MEMORY_NATIVE_TYPE_ARRAY);
    queued = (TZrBool *)ZrCore_Memory_RawMallocWithType(state->global,
                                                        blockCount * sizeof(TZrBool),
                                                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (queue == ZR_NULL || queued == ZR_NULL) {
        dataflow_free_queue(state, queue, queued, blockCount);
        dataflow_free_reachability(state, entryReachable, blockCount);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(queued, 0, blockCount * sizeof(TZrBool));

    exitState = dataflow_result_block(result, cfg->exitBlockId);
    if (exitState == ZR_NULL || !entryReachable[cfg->exitBlockId]) {
        dataflow_free_queue(state, queue, queued, blockCount);
        dataflow_free_reachability(state, entryReachable, blockCount);
        return ZR_FALSE;
    }
    exitState->isReachable = ZR_TRUE;
    analysis->initEntry(exitState->outState, analysis->userData);
    ZrCore_Memory_RawCopy(exitState->inState, exitState->outState, analysis->stateSize);
    if (!dataflow_enqueue(queue, queued, blockCount, &tail, &count, cfg->exitBlockId)) {
        dataflow_free_queue(state, queue, queued, blockCount);
        dataflow_free_reachability(state, entryReachable, blockCount);
        return ZR_FALSE;
    }

    while (count > 0) {
        TZrUInt32 blockId = queue[head];
        SZrParserCfgBlock *block = dataflow_cfg_block(cfg, blockId);
        SZrParserDataflowBlockState *blockState = dataflow_result_block(result, blockId);
        TZrSize predecessorIndex;

        head = (head + 1) % blockCount;
        count--;
        queued[blockId] = ZR_FALSE;
        if (block == ZR_NULL || blockState == ZR_NULL || !blockState->isReachable) {
            continue;
        }

        ZrCore_Memory_RawCopy(blockState->inState, blockState->outState, analysis->stateSize);
        if (block->kind == ZR_PARSER_CFG_BLOCK_STATEMENT &&
            block->statement != ZR_NULL &&
            analysis->transferStatement != ZR_NULL) {
            analysis->transferStatement(block->statement, blockState->inState, analysis->userData);
        }

        for (predecessorIndex = 0; predecessorIndex < blockCount; predecessorIndex++) {
            SZrParserCfgBlock *predecessorBlock = dataflow_cfg_block(cfg, (TZrUInt32)predecessorIndex);
            SZrParserDataflowBlockState *predecessorState;
            TZrBool wasReachable;
            TZrBool changed;

            if (!dataflow_block_has_successor(predecessorBlock, blockId)) {
                continue;
            }
            if (!entryReachable[predecessorIndex]) {
                continue;
            }

            predecessorState = dataflow_result_block(result, (TZrUInt32)predecessorIndex);
            if (predecessorState == ZR_NULL) {
                continue;
            }
            wasReachable = predecessorState->isReachable;
            predecessorState->isReachable = ZR_TRUE;
            if (!wasReachable) {
                ZrCore_Memory_RawCopy(predecessorState->outState,
                                      blockState->inState,
                                      analysis->stateSize);
                changed = ZR_TRUE;
            } else {
                changed = analysis->join(predecessorState->outState,
                                         blockState->inState,
                                         analysis->userData);
            }
            if (changed) {
                if (!dataflow_enqueue(queue,
                                      queued,
                                      blockCount,
                                      &tail,
                                      &count,
                                      (TZrUInt32)predecessorIndex)) {
                    dataflow_free_queue(state, queue, queued, blockCount);
                    dataflow_free_reachability(state, entryReachable, blockCount);
                    return ZR_FALSE;
                }
            }
        }
    }

    dataflow_free_queue(state, queue, queued, blockCount);
    dataflow_free_reachability(state, entryReachable, blockCount);
    return ZR_TRUE;
}

TZrBool ZrParser_Dataflow_Run(SZrState *state,
                              const SZrParserCfg *cfg,
                              const SZrParserDataflowAnalysis *analysis,
                              SZrParserDataflowResult *result) {
    if (state == ZR_NULL || cfg == ZR_NULL || analysis == ZR_NULL || result == ZR_NULL ||
        !cfg->blocks.isValid || cfg->blocks.length == 0 ||
        analysis->stateSize == 0 || analysis->initEntry == ZR_NULL ||
        analysis->join == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!dataflow_prepare_result(state, cfg, analysis, result)) {
        return ZR_FALSE;
    }

    if (analysis->direction == ZR_PARSER_DATAFLOW_BACKWARD) {
        if (cfg->exitBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            return ZR_FALSE;
        }
        return dataflow_process_backward(state, cfg, analysis, result);
    }

    if (cfg->entryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    return dataflow_process_forward(state, cfg, analysis, result);
}
