#include "dataflow.h"

#include "zr_vm_core/memory.h"

/* CFG 的块编号与 blocks 数组下标一致；统一在借用入口拦截失效编号。 */
static SZrParserCfgBlock *dataflow_cfg_block(const SZrParserCfg *cfg, TZrUInt32 blockId) {
    if (cfg == ZR_NULL || !cfg->blocks.isValid || blockId >= cfg->blocks.length) {
        return ZR_NULL;
    }
    return (SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&cfg->blocks, blockId);
}

/* 入口、汇合和出口块只传递已有状态；语句与清理块才进入分析专属转移。 */
static TZrBool dataflow_block_transfers_statement(
        const SZrParserCfgBlock *block) {
    return block != ZR_NULL &&
           block->statement != ZR_NULL &&
           (block->kind == ZR_PARSER_CFG_BLOCK_STATEMENT ||
            block->kind == ZR_PARSER_CFG_BLOCK_CLEANUP);
}

/* 结果槽仍由 result 持有，返回值仅在结果未重建或释放前有效。 */
static SZrParserDataflowBlockState *dataflow_result_block(SZrParserDataflowResult *result,
                                                          TZrUInt32 blockId) {
    if (result == ZR_NULL || !result->blockStates.isValid || blockId >= result->blockStates.length) {
        return ZR_NULL;
    }
    return (SZrParserDataflowBlockState *)ZrCore_Array_Get(&result->blockStates, blockId);
}

/* 只读查询复用核心数组访问器；借出的槽不通过该转换路径修改。 */
static const SZrParserDataflowBlockState *dataflow_result_block_const(
        const SZrParserDataflowResult *result,
        TZrUInt32 blockId) {
    if (result == ZR_NULL || !result->blockStates.isValid || blockId >= result->blockStates.length) {
        return ZR_NULL;
    }
    return (const SZrParserDataflowBlockState *)ZrCore_Array_Get((SZrArray *)&result->blockStates, blockId);
}

/* 环形队列按块去重，避免尚未处理的同一状态被重复占用队列槽。 */
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

/* 队列与 queued 位图是本次 Run 的临时原生缓冲，所有退出路径按同尺寸归还。 */
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

/* 限制总出队次数；达到预算表示本次求解未完成，不等同于证明无固定点。 */
static TZrSize dataflow_iteration_budget(TZrSize blockCount) {
    TZrSize budget = blockCount * ZR_PARSER_DATAFLOW_ITERATIONS_PER_BLOCK;

    return budget < ZR_PARSER_DATAFLOW_MAX_ITERATION_COUNT
               ? budget
               : ZR_PARSER_DATAFLOW_MAX_ITERATION_COUNT;
}

/* 先标记再沿后继递归，使环边只访问一次，并为两种求解方向共享入口可达域。 */
/* TODO: 深度随最长 CFG 路径增长；下一步从 cfg.c 的 AST-to-CFG 构造追最长链并用边界图压测栈界，再决定是否改为显式栈。 */
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
        dataflow_mark_entry_reachable(
                cfg,
                ZrParser_Cfg_BlockSuccessorIdAt(block, index),
                entryReachable);
    }
}

/* 为 Run 构建入口可达位图；CFG 只借用，位图由此 helper 的调用方释放。 */
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

/* 与 compute_entry_reachability 配对，尺寸始终取同一 CFG 块数。 */
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
        /* 数组只释放槽位字节，不析构槽内指针；先逐块释放快照再释放容器。 */
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

    /* BUG: upstream allocator 可只拒绝 blockStates 缓冲请求、随后仍成功分配状态缓冲；Array_Init 仍置 valid、head 为空，
     * 首个 Push 会断言或向空地址复制，Run 不能返回 false，未入列的状态对不会进入 Result_Free 清理。 */
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

/* 从入口边界向后继传播，只让入口可达块贡献状态；失败时保留的快照归 result。 */
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
    TZrSize iterationCount = 0;
    TZrSize iterationBudget = dataflow_iteration_budget(blockCount);
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
    /* 入口边界由分析域初始化，随后先复制成尚未转移的出口状态。 */
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

        if (iterationCount >= iterationBudget) {
            /* 临时工作缓冲立即释放；已写入 result 的快照留给调用方显式收尾。 */
            dataflow_free_queue(state, queue, queued, blockCount);
            dataflow_free_reachability(state, entryReachable, blockCount);
            return ZR_FALSE;
        }
        iterationCount++;
        head = (head + 1) % blockCount;
        count--;
        queued[blockId] = ZR_FALSE;
        if (block == ZR_NULL || blockState == ZR_NULL || !blockState->isReachable) {
            continue;
        }

        /* 每次重访先从 in 重建 out，再应用转移，避免把前一轮 out 当新输入累加。 */
        ZrCore_Memory_RawCopy(blockState->outState, blockState->inState, analysis->stateSize);
        if (dataflow_block_transfers_statement(block) &&
            analysis->transferStatement != ZR_NULL) {
            /* 该间接调用仅作用于语句/cleanup；worklist 重访时仍会再次分派。 */
            analysis->transferStatement(block->statement, blockState->outState, analysis->userData);
        }

        for (successorIndex = 0; successorIndex < block->successorCount; successorIndex++) {
            TZrUInt32 successorId =
                    ZrParser_Cfg_BlockSuccessorIdAt(block, successorIndex);
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
            /* 首个有效前驱提供初值，其余前驱通过 join 合并；只有改变时才重排后继。 */
            if (!wasReachable) {
                ZrCore_Memory_RawCopy(successorState->inState,
                                      blockState->outState,
                                      analysis->stateSize);
                changed = ZR_TRUE;
            } else {
                /* join 的 changed 返回值是重排依据，回调须反映 dst 是否真的变化。 */
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

/* 反向求解没有前驱索引，按 successor 关系确认指定块是否为该块后继。 */
static TZrBool dataflow_block_has_successor(const SZrParserCfgBlock *block, TZrUInt32 successorId) {
    TZrUInt32 index;

    if (block == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < block->successorCount; index++) {
        if (ZrParser_Cfg_BlockSuccessorIdAt(block, index) == successorId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 从可达出口设置边界，向 CFG 前驱传播反向状态；CFG 与回调数据只在本次调用中借用。 */
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
    TZrSize iterationCount = 0;
    TZrSize iterationBudget = dataflow_iteration_budget(blockCount);
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
    /* 反向分析将同一初始化回调作为出口边界，再复制到出口的入状态。 */
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

        if (iterationCount >= iterationBudget) {
            /* 预算失败不销毁部分 in/out；统一由 Run 调用方释放 result。 */
            dataflow_free_queue(state, queue, queued, blockCount);
            dataflow_free_reachability(state, entryReachable, blockCount);
            return ZR_FALSE;
        }
        iterationCount++;
        head = (head + 1) % blockCount;
        count--;
        queued[blockId] = ZR_FALSE;
        if (block == ZR_NULL || blockState == ZR_NULL || !blockState->isReachable) {
            continue;
        }

        ZrCore_Memory_RawCopy(blockState->inState, blockState->outState, analysis->stateSize);
        if (dataflow_block_transfers_statement(block) &&
            analysis->transferStatement != ZR_NULL) {
            /* 反向分派沿 CFG 的语句次序回传状态，回访仍从 out 重建输入。 */
            analysis->transferStatement(block->statement, blockState->inState, analysis->userData);
        }

        /* TODO: 每次出队遍历所有块并扫描其后继，成本为 O(V*(V+E))；下一步核对 cfg.h 的 predecessorCount 与 cfg_graph.c 的 Connect，再测语义分析最大图规模。 */
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
            /* 首个可达后继建立 out，后续后继合并到 out；状态变化才重新求该前驱。 */
            if (!wasReachable) {
                ZrCore_Memory_RawCopy(predecessorState->outState,
                                      blockState->inState,
                                      analysis->stateSize);
                changed = ZR_TRUE;
            } else {
                /* 多个有效后继在 out 域汇合；changed 决定是否重新处理前驱。 */
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

/* 先验证描述符再创建结果；边界编号在结果准备后检查，后续失败仍需调用者 Free。 */
TZrBool ZrParser_Dataflow_Run(SZrState *state,
                              const SZrParserCfg *cfg,
                              const SZrParserDataflowAnalysis *analysis,
                              SZrParserDataflowResult *result) {
    if (state == ZR_NULL || cfg == ZR_NULL || analysis == ZR_NULL || result == ZR_NULL ||
        !cfg->blocks.isValid || cfg->blocks.length == 0 ||
        cfg->blocks.length > ZR_PARSER_DATAFLOW_MAX_BLOCK_COUNT ||
        analysis->stateSize == 0 || analysis->initEntry == ZR_NULL ||
        analysis->join == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!dataflow_prepare_result(state, cfg, analysis, result)) {
        return ZR_FALSE;
    }

    /* 方向专属端点在结果准备后校验；端点失败也由调用方释放已建快照。 */
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
