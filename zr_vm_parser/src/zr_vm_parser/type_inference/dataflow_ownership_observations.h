#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_OBSERVATIONS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_OBSERVATIONS_H

#include "dataflow_ownership_symbols.h"

/** @brief CFG 一次遍历按 referenceFacts 下标收集的所有权转移与违规观察。
 * @note 布尔数组和 owner 索引由本次分析拥有；violationCauses 中的 AST 指针为借用。
 *       所有数组仅在 Allocate 到 Free 之间有效，count 固定对应分配时的引用事实长度。
 */
typedef struct SZrDataflowOwnershipObservations {
    /** @brief 对应引用在 CFG 中发生 Unique 所有权转移。 */
    TZrBool *moveSeen;
    /** @brief 对应引用在 CFG 中发生区域释放。 */
    TZrBool *releaseSeen;
    /** @brief 对应引用触发所有权违规。 */
    TZrBool *violationSeen;
    /** @brief 选择出的违规原因节点，借用 AST 生命周期。 */
    SZrAstNode **violationCauses;
    /** @brief 原因对应的 owner 条目索引，无 owner 时为无效哨兵。 */
    TZrSize *violationOwnerIndices;
    /** @brief 与分配时 referenceFacts.length 相同的平行数组长度。 */
    TZrSize count;
} SZrDataflowOwnershipObservations;

/** @brief 建立一次 ownership CFG 求解使用的观察缓冲区。
 * @pre observations 必须为零初始化状态，context 的 state/referenceFacts 已有效。
 * @return 失败后仍须调用 ObservationsFree 清理已成功分配的部分。
 */
TZrBool ZrParser_DataflowOwnership_ObservationsAllocate(
        SZrSemanticContext *context,
        SZrDataflowOwnershipObservations *observations);
/** @brief 释放观察缓冲区并将结构归零；不会释放借用的 AST cause 节点。
 * @pre 使用与 Allocate 相同的 context 状态；可清理由失败分配留下的部分状态。
 */
void ZrParser_DataflowOwnership_ObservationsFree(
        SZrSemanticContext *context,
        SZrDataflowOwnershipObservations *observations);
/** @brief 把 CFG 观察结果追加到 context 的持久 ownershipFacts。
 * @pre observations、symbols 与 context 必须来自同一次未变更的 referenceFacts 快照。
 * @return false 表示输入或追加失败；已追加事实保持在 context 中，不作回滚。
 * @note 事实记录被复制，AST 节点与来源字符串仍由 AST/语义上下文持有。
 */
TZrBool ZrParser_DataflowOwnership_AppendObservedFacts(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *symbols,
        const SZrDataflowOwnershipObservations *observations);

#endif
