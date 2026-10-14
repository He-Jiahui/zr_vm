//
// Quaternion registry accessors.
//

#ifndef ZR_VM_LIB_MATH_QUATERNION_REGISTRY_H
#define ZR_VM_LIB_MATH_QUATERNION_REGISTRY_H

#include "zr_vm_lib_math/quaternion.h"

/** @file
 *  Quaternion 的静态 descriptor 与 type-hint 访问接口。zr.math 模块聚合器在构建模块
 *  descriptor 时读取这些借用指针，再将类型、方法和 meta 回调交给 native registry；
 *  本接口不执行注册，也不转移静态数组的所有权。
 *
 *  ## Constraints and lifecycle
 *  返回的数据及其嵌套数组位于提供者模块映像的静态存储中；注册方须保持该映像在
 *  registry 借用期间驻留，调用方只读且不释放或修改。count 输出参数可为 NULL；
 *  若有效，getter 在返回数组数量时写入它。
 *
 *  ## References
 *  zr_vm_lib_math/src/zr_vm_lib_math/module.c 中的 descriptor 聚合；
 *  zr_vm_library 的 native registry 实体化路径。
 */

/**
 * ## Summary
 *  返回 Quaternion 的静态类型描述符，供 math 模块聚合器加入 `zr.math` 类型表。
 *
 * ## Returns
 * @return 借用的提供者静态 descriptor；本 getter 不分配对象，调用方不得释放。
 *
 * ## Constraints and lifecycle
 *  descriptor 内 field、method 与 meta 数组也都是静态只读存储；模块聚合器浅拷贝
 *  descriptor 后继续借用这些数组，前提是提供者映像仍驻留。注册成功前该 getter
 *  本身不产生 VM 对象。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetFunctions；ZrMath_QuaternionRegistry_GetHints；
 *  zr_vm_lib_math/src/zr_vm_lib_math/module.c。
 */
const ZrLibTypeDescriptor *ZrMath_QuaternionRegistry_GetType(void);

/**
 * ## Summary
 *  提供 Quaternion 注册表的模块级函数列表；当前类型没有额外模块函数。
 *
 * ## Parameters
 * @param[out] count 可选数量输出；非 NULL 时写为 0。
 *
 * ## Returns
 * @return 始终返回 NULL，表示不提供模块级函数数组。
 *
 * ## Constraints and lifecycle
 *  count 可为 NULL；本 getter 不分配内存，也不触发回调注册。
 *
 * ## References
 *  zr_vm_lib_math/src/zr_vm_lib_math/module.c 中的
 *  ZR_MATH_APPEND_FUNCTION_REGISTRY 聚合宏。
 */
const ZrLibFunctionDescriptor *ZrMath_QuaternionRegistry_GetFunctions(TZrSize *count);

/**
 * ## Summary
 *  返回 Quaternion 的静态类型提示数组，供 math 模块聚合器发布编译期类型投影。
 *
 * ## Parameters
 * @param[out] count 可选数量输出；非 NULL 时写入静态提示项数。
 *
 * ## Returns
 * @return 借用的提供者静态 hint 数组；调用方不得修改或释放。
 *
 * ## Constraints and lifecycle
 *  count 可为 NULL；返回数组只描述 struct 字段形状，不创建运行时对象或验证实例。
 *
 * ## References
 *  zr_vm_lib_math/src/zr_vm_lib_math/module.c 中的
 *  ZR_MATH_APPEND_HINT_REGISTRY 聚合宏。
 */
const ZrLibTypeHintDescriptor *ZrMath_QuaternionRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_QUATERNION_REGISTRY_H
