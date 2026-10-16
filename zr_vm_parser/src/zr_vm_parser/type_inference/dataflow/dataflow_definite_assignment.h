#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_DEFINITE_ASSIGNMENT_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_DEFINITE_ASSIGNMENT_H

#include "zr_vm_parser/conf.h"

/**
 * @brief 每个 resolved symbol 槽的三值赋值状态。
 * @note UNINIT 表示当前路径未写入，INIT 表示所有合并路径均已写入，MAYBE_INIT 表示路径间存在分歧或分析结果未知。
 */
typedef enum EZrParserDefiniteAssignmentState {
    ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT = 0,
    ZR_PARSER_DEFINITE_ASSIGNMENT_INIT,
    ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT
} EZrParserDefiniteAssignmentState;

/**
 * @brief 返回按 symbolCount 个枚举槽紧密排列的状态向量字节数。
 * @note 同一向量的 InitState、Get、Set、Join 必须使用相同 symbolCount。
 * TODO: 实现为 TZrSize 乘法，DA 的 symbolCount 来自全 context 去重映射；CFG block 上限不约束此计数。先追合法事实数组与目标 ABI 分配上限，再决定如何约束溢出风险。
 */
ZR_PARSER_API TZrSize ZrParser_DefiniteAssignment_StateSize(TZrSize symbolCount);
/** @brief 将调用方提供的完整状态向量逐槽填为 initialState；不会分配或取得 state 所有权。 */
ZR_PARSER_API void ZrParser_DefiniteAssignment_InitState(
        void *state,
        TZrSize symbolCount,
        EZrParserDefiniteAssignmentState initialState);
/** @brief 读取合法索引的状态；空指针或越界时返回 MAYBE_INIT 作为保守结果。 */
ZR_PARSER_API EZrParserDefiniteAssignmentState ZrParser_DefiniteAssignment_Get(
        const void *state,
        TZrSize symbolCount,
        TZrSize symbolIndex);
/** @brief 覆写合法索引的状态槽；空指针或越界时忽略，缓冲区大小由调用方保证。 */
ZR_PARSER_API void ZrParser_DefiniteAssignment_Set(
        void *state,
        TZrSize symbolCount,
        TZrSize symbolIndex,
        EZrParserDefiniteAssignmentState value);
/**
 * @brief 将 src 与 dst 的每槽状态做 definite-assignment join，返回 dst 是否发生变化。
 * @pre 两个缓冲至少各含 symbolCount 个槽；空指针返回 FALSE。
 * TODO: 通用 Dataflow 当前传入独立 block 快照，但导出声明未说明 src/dst 是否允许部分重叠；明确 ABI 契约或支持重叠后再扩充测试。
 */
ZR_PARSER_API TZrBool ZrParser_DefiniteAssignment_Join(
        void *dst,
        const void *src,
        TZrSize symbolCount);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_DEFINITE_ASSIGNMENT_H
