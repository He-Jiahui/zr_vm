/* 供 zr.container 类型描述符注册的视图回调；调用者经协议角色和元方法进入。 */

#ifndef ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H
#define ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H

#include "zr_vm_lib_container/conf.h"

/** @brief Array<T>.span 回调：创建保留原数组引用的可写 Span<T>。
 *  @pre 接收者须提供非负、可表示为 int64 的 length 字段。
 */
TZrBool ZrVmLibContainer_ContiguousView_FromArray(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Span/ReadOnlySpan 的空视图构造回调；有现成接收者时就地重置其区间。 */
TZrBool ZrVmLibContainer_ContiguousView_Construct(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief 从现有视图派生共享 source 和原型的子区间；越界以运行时错误终止。 */
TZrBool ZrVmLibContainer_ContiguousView_Slice(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Span.asReadOnly 回调：保留同一 source 与区间，改用 ReadOnlySpan 类型。 */
TZrBool ZrVmLibContainer_ContiguousView_AsReadOnly(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Span/ReadOnlySpan 的索引回调；先检查相对区间，再经 source 的索引契约取值。 */
TZrBool ZrVmLibContainer_ContiguousView_GetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Span 的索引赋值回调；源对象可为数组或执行租约存活检查的 owner。 */
TZrBool ZrVmLibContainer_ContiguousView_SetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result);

#endif // ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H
