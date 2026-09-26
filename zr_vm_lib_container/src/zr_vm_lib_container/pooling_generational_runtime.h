#ifndef ZR_VM_LIB_CONTAINER_POOLING_GENERATIONAL_RUNTIME_H
#define ZR_VM_LIB_CONTAINER_POOLING_GENERATIONAL_RUNTIME_H

#include "zr_vm_library/native_binding.h"

/** @brief Pool<T>.deliver 回调：根据实参内联布局选槽位，并发布语言层弱句柄。 */
TZrBool ZrPooling_Generational_Deliver(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Pool<T>.isLive 回调：使用完整代数身份判定句柄是否仍有效。 */
TZrBool ZrPooling_Generational_IsLive(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Pool<T>.recycle 回调：退役实体，借用存在时延后释放。 */
TZrBool ZrPooling_Generational_Recycle(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Pool<T>.tryRead 回调：经 out 参数交付受生命周期约束的只读视图。 */
TZrBool ZrPooling_Generational_TryRead(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief Pool<T>.tryBorrow 回调：经 out 参数交付独占可写视图。 */
TZrBool ZrPooling_Generational_TryBorrow(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief PoolRef/PoolReadRef 的显式及元方法关闭回调；重复关闭安全。 */
TZrBool ZrPooling_Generational_RefClose(
        ZrLibCallContext *context,
        SZrTypeValue *result);
/** @brief guard.value 属性回调；访问权由类型描述符区分可写与只读。 */
TZrBool ZrPooling_Generational_RefValue(
        ZrLibCallContext *context,
        SZrTypeValue *result);

#endif // ZR_VM_LIB_CONTAINER_POOLING_GENERATIONAL_RUNTIME_H
