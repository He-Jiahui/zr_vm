#ifndef ZR_VM_LIBRARY_BATCH_PROTOCOL_H
#define ZR_VM_LIBRARY_BATCH_PROTOCOL_H

#include "zr_vm_library/conf.h"
#include "zr_vm_core/batch_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 一元批量元素回调；output 至少提供 elementSize 字节，false 中止后续元素。 */
typedef TZrBool (*FZrBatchUnary)(const TZrByte *input, TZrByte *output,
                                TZrSize elementSize, TZrPtr userData);
/** @brief 二元批量元素回调；三方槽宽一致，false 中止后续元素。 */
typedef TZrBool (*FZrBatchBinary)(const TZrByte *left, const TZrByte *right,
                                 TZrByte *output, TZrSize elementSize,
                                 TZrPtr userData);

/** @brief 为核心批量缓冲区契约提供按索引递增的标量回调基线。
 *  @pre 回调只能访问当前元素；Map 的输出槽须能容纳按输入 elementSize 写出的结果。
 *       Map 允许输入输出完全同布局的原位映射，但拒绝部分重叠；Zip 与 MatrixMap2D
 *       要求三方元素大小一致，且输出不得与任一输入重叠。
 *  @note 回调返回 false 时立即停止，diagnostic.index 指向失败元素；此前输出已写入，不回滚。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_Batch_Map(
        const SZrBatchBuffer *input, SZrBatchBuffer *output, TZrSize count,
        FZrBatchUnary callback, TZrPtr userData,
        SZrBatchDiagnostic *diagnostic);
/** @brief 合并两个等宽输入；输出不能与任一输入重叠，失败时保留已写前缀。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Batch_Zip(
        const SZrBatchBuffer *left, const SZrBatchBuffer *right,
        SZrBatchBuffer *output, TZrSize count, FZrBatchBinary callback,
        TZrPtr userData, SZrBatchDiagnostic *diagnostic);
/** @brief 先校验二维形状的元素数，再按 Zip 的等宽和不别名约束调用回调。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Batch_MatrixMap2D(
        SZrBatchShape shape, const SZrBatchBuffer *left,
        const SZrBatchBuffer *right, SZrBatchBuffer *output,
        FZrBatchBinary callback, TZrPtr userData,
        SZrBatchDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif
