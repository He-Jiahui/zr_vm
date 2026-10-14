//
// Matrix3x3 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_MATRIX3X3_REGISTRY_H
#define ZR_VM_LIB_MATH_MATRIX3X3_REGISTRY_H

#include "zr_vm_lib_math/matrix3x3.h"

/** @file
 * ## Summary
 * 暴露 Matrix3x3 的类型、函数及类型提示注册表 getter，供 math 模块聚合器收集元数据。
 *
 * ## Constraints and lifecycle
 * descriptor 与 hint 都是本模块静态存储，调用者只借用只读指针，不得修改或释放；其有效期受提供这些
 * 表的 provider 映像加载期限制，并非无条件覆盖整个进程。`count` 参数可为空。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 调用本头文件声明的三个 getter 并合并其返回值。
 */
/**
 * ## Summary
 * 返回 Matrix3x3 的静态类型描述符，供 math 模块把其字段、方法和 meta callback 注册进 VM。
 *
 * ## Returns
 * @return 指向本模块静态 `ZrLibTypeDescriptor` 的只读借用指针；不会返回调用者所有的副本，且不能越过提供者映像的卸载期使用。
 *
 * ## Constraints and lifecycle
 * 指针在包含该静态 descriptor 的 provider 映像保持加载期间有效；调用方不得修改或释放 descriptor、字段表、方法表或 meta 表。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c:130` 将结果传给 `zr_math_append_type`。
 */
const ZrLibTypeDescriptor *ZrMath_Matrix3x3Registry_GetType(void);
/**
 * ## Summary
 * 提供本注册表的独立函数列表；Matrix3x3 当前没有额外的模块级函数。
 *
 * ## Parameters
 * @param[out] count 可选输出；非空时写为零。
 *
 * ## Returns
 * @return 始终返回 `ZR_NULL`，表示本注册表没有独立函数描述符。
 *
 * ## Constraints and lifecycle
 * `count` 仅在非空时写入；不分配或转移资源。模块聚合宏会把零项加入总体函数表。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c:118` 将此 getter 传给函数注册聚合宏。
 */
const ZrLibFunctionDescriptor *ZrMath_Matrix3x3Registry_GetFunctions(TZrSize *count);
/**
 * ## Summary
 * 返回供 math 模块元数据投影使用的单项 Matrix3x3 类型提示表。
 *
 * ## Parameters
 * @param[out] count 可选输出；非空时写为静态 hint 表的元素数。
 *
 * ## Returns
 * @return 指向本模块静态 `ZrLibTypeHintDescriptor` 数组的只读借用指针。
 *
 * ## Constraints and lifecycle
 * 数组在包含它的 provider 映像加载期间有效，调用者不得修改或释放；`count` 可以为空。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c:143` 将结果交给 hint 注册聚合宏。
 */
const ZrLibTypeHintDescriptor *ZrMath_Matrix3x3Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_MATRIX3X3_REGISTRY_H
