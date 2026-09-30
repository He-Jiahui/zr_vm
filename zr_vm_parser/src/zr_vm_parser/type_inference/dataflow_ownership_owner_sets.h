#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_OWNER_SETS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_OWNER_SETS_H

#include "dataflow_ownership_symbols.h"

/** 未知或不可追踪的 owner 集；OwnerSetPoolInit 固定保留 ID 0。 */
#define ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN ((TZrSize)0)
/** 已知为空的 owner 集；OwnerSetPoolInit 固定保留 ID 1，与 UNKNOWN 区分。 */
#define ZR_DATAFLOW_OWNERSHIP_OWNER_SET_EMPTY ((TZrSize)1)

/**
 * @brief 一次控制流所有权分析使用的 owner 集合驻留池。
 * @note entries 的下标就是集合 ID；初始化时 ID 0/1 分别为 UNKNOWN/EMPTY，后续 ID 仅在本池存活期间有效。
 *       非空集合保存按符号索引递增且去重的 owner 列表；释放池会释放各列表和 entries 缓冲。
 */
typedef struct SZrDataflowOwnershipOwnerSetPool {
    SZrArray entries;
} SZrDataflowOwnershipOwnerSetPool;

/** @brief 将未持有缓冲的池置为可初始化状态。
 * @pre pool 尚未持有已初始化池的缓冲；不能用 Construct 丢弃活动池。 */
void ZrParser_DataflowOwnership_OwnerSetPoolConstruct(
        SZrDataflowOwnershipOwnerSetPool *pool);

/** @brief 分配本次分析的驻留表并建立 UNKNOWN、EMPTY 两个保留集合。
 * @pre state 有效，pool 已由 PoolConstruct 或 PoolFree 置为未初始化状态。
 * @return 成功为 true；初始 entries 分配失败时返回 false，并将 pool 复位为未初始化状态。 */
TZrBool ZrParser_DataflowOwnership_OwnerSetPoolInit(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool);

/** @brief 释放池中每个 owner 列表及驻留表，并把池复位为未初始化状态。
 * @pre state 使用创建这些缓冲时的全局分配器；state 为 null 时函数不执行释放。 */
void ZrParser_DataflowOwnership_OwnerSetPoolFree(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool);

/** @brief 驻留一个单 owner 集合，已有相同集合时复用其 ID。
 * @pre state 和已初始化的 pool 有效；ownerIndex 是当前所有权符号表中的有效索引。
 * @return 集合 ID；参数无效或 owner 列表分配失败时返回 UNKNOWN。 */
TZrSize ZrParser_DataflowOwnership_OwnerSetSingleton(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize ownerIndex);

/** @brief 合并两个本池集合并驻留排序去重后的结果；UNKNOWN 传播，EMPTY 为并集恒等元。
 * @pre state 和已初始化的 pool 有效，两个 ID 来自该 pool。
 * @return 并集的集合 ID；输入未知/无效、大小溢出或结果缓冲分配失败时返回 UNKNOWN。 */
TZrSize ZrParser_DataflowOwnership_OwnerSetUnion(
        SZrState *state,
        SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize leftSetId,
        TZrSize rightSetId);

/** @brief 判断集合 ID 是否未知或无效。
 * @return pool 无效、ID 越界或集合标记为 UNKNOWN 时为 true。 */
TZrBool ZrParser_DataflowOwnership_OwnerSetIsUnknown(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId);

/** @brief 读取已知集合的 owner 数量。
 * @return 有效已知集合的元素数；未知或无效 ID 返回 0，可用 IsUnknown 区分。 */
TZrSize ZrParser_DataflowOwnership_OwnerSetCount(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId);

/** @brief 按 0 起始位置读取集合中的符号索引。
 * @return 索引位置有效且集合已知时返回 ownerIndex，否则返回 ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID。 */
TZrSize ZrParser_DataflowOwnership_OwnerSetAt(
        const SZrDataflowOwnershipOwnerSetPool *pool,
        TZrSize setId,
        TZrSize index);

#endif
