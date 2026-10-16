#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_H

#include "zr_vm_parser/cfg.h"

/** @brief 限制单次分析接受的 CFG 规模，避免块状态和工作队列按无界输入扩张。 */
#define ZR_PARSER_DATAFLOW_MAX_BLOCK_COUNT ((TZrSize)1048576U)
/** @brief 以块数为基数计算单次 Run 的总出队预算。 */
#define ZR_PARSER_DATAFLOW_ITERATIONS_PER_BLOCK ((TZrSize)1024U)
/** @brief 为高块数 CFG 再设置固定的总迭代上限。 */
#define ZR_PARSER_DATAFLOW_MAX_ITERATION_COUNT ((TZrSize)16777216U)

/** @brief 指定从 CFG 入口向前传播，或从出口向入口反向传播。 */
typedef enum EZrParserDataflowDirection {
    ZR_PARSER_DATAFLOW_FORWARD = 0,
    ZR_PARSER_DATAFLOW_BACKWARD
} EZrParserDataflowDirection;

/** @brief 初始化入口或出口边界状态；state 指向 Run 期间借用的零初始化缓冲。 */
typedef void (*TZrParserDataflowInitFn)(void *state, void *userData);
/** @brief 将 src 合入 dst；返回 true 表示 dst 发生变化，供工作队列决定是否重访。 */
typedef TZrBool (*TZrParserDataflowJoinFn)(void *dst, const void *src, void *userData);
/** @brief 对语句或清理块执行状态转移；同一块可因状态传播而重复调用，不能依赖一次性副作用。 */
typedef void (*TZrParserDataflowTransferFn)(SZrAstNode *statement, void *state, void *userData);

/**
 * @brief 描述一次同步的数据流求解及其状态域操作。
 * @pre direction 为已定义枚举值，stateSize 非零且足以容纳回调状态，initEntry 与 join 有效；回调及 userData 在 Run 返回前保持有效。
 * @note 回调应对相同输入给出稳定结果，并按单调、幂等的 join 收敛；不得保留引擎借出的状态缓冲。
 */
typedef struct SZrParserDataflowAnalysis {
    EZrParserDataflowDirection direction;
    TZrSize stateSize;
    TZrParserDataflowInitFn initEntry;
    TZrParserDataflowJoinFn join;
    TZrParserDataflowTransferFn transferStatement;
    void *userData;
} SZrParserDataflowAnalysis;

/**
 * @brief 保存某 CFG 块的可达标志及两个方向相关的状态快照。
 * @note 缓冲由结果对象单独持有；正向 in/out 是合并前驱/转移后的状态，反向 out/in 是合并后继/转移后的状态。
 *       查询返回的缓冲只可读取，并在 Free、重新 Run 或结果对象失效后失效。
 */
typedef struct SZrParserDataflowBlockState {
    TZrBool isReachable;
    void *inState;
    void *outState;
} SZrParserDataflowBlockState;

/**
 * @brief 持有每个 CFG 块的原生状态快照。
 * @note Run 失败也可能留下部分分配的快照；它们不随 SZrState 自动回收，须用同一 state 显式 Free。
 */
typedef struct SZrParserDataflowResult {
    SZrArray blockStates;
    TZrSize stateSize;
} SZrParserDataflowResult;

/** @brief 将尚未持有结果缓冲区的对象置为空，以供 Run 使用。
 *  @pre result 未持有尚待释放的结果；重复 Init 不会替调用方释放旧缓冲区。 */
ZR_PARSER_API void ZrParser_DataflowResult_Init(SZrParserDataflowResult *result);
/** @brief 释放 Run 为结果及各块状态单独分配的所有原生缓冲。
 *  @pre state 与分配结果时使用同一个全局分配器；须在销毁 state/global 前调用。 */
ZR_PARSER_API void ZrParser_DataflowResult_Free(SZrState *state, SZrParserDataflowResult *result);
/** @brief 按块编号借用结果中的状态槽。
 *  @return 块编号有效时返回结果内部地址；Free、重新 Run 或结果对象失效后该地址失效。 */
ZR_PARSER_API const SZrParserDataflowBlockState *ZrParser_Dataflow_GetBlockState(
        const SZrParserDataflowResult *result,
        TZrUInt32 blockId);
/**
 * @brief 对 CFG 执行有界正向或反向工作队列求解。
 * @pre state 与 state->global 有效；result 已 Init，cfg 在调用期间有效，analysis 的方向、状态大小及回调满足其声明契约。
 * @return 成功收敛时为 true；false 可能表示校验、分配或迭代预算失败，调用方仍须 Free 结果。
 * @note state/cfg/analysis/userData 仅在调用期间借用；返回的状态快照由 result 持有。
 */
ZR_PARSER_API TZrBool ZrParser_Dataflow_Run(SZrState *state,
                                            const SZrParserCfg *cfg,
                                            const SZrParserDataflowAnalysis *analysis,
                                            SZrParserDataflowResult *result);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_H
