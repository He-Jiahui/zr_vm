//
// Runtime callbacks for protocol-driven contiguous views.
//

#ifndef ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H
#define ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H

#include "zr_vm_lib_container/conf.h"

/** @brief Span.fromArray 回调：创建保留原数组引用及初始区间的可写视图。 */
TZrBool ZrVmLibContainer_ContiguousView_FromArray(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Span/ReadOnlySpan 的空视图构造回调，由类型描述符间接调用。 */
TZrBool ZrVmLibContainer_ContiguousView_Construct(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief 从现有视图派生共享 source 的子区间，并校验相对边界。 */
TZrBool ZrVmLibContainer_ContiguousView_Slice(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief 从可写视图派生保留同一 source 的只读视图。 */
TZrBool ZrVmLibContainer_ContiguousView_AsReadOnly(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief 视图索引回调；相对索引与当前 source 的有效性均需通过检查。 */
TZrBool ZrVmLibContainer_ContiguousView_GetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief 可写视图索引赋值回调；只由 Span 元方法暴露。 */
TZrBool ZrVmLibContainer_ContiguousView_SetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result);

#endif // ZR_VM_LIB_CONTAINER_CONTIGUOUS_VIEW_H
