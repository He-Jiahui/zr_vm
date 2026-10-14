//
// Vector2 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_VECTOR2_REGISTRY_H
#define ZR_VM_LIB_MATH_VECTOR2_REGISTRY_H

#include "zr_vm_lib_math/vector2.h"

/** @file
 * Vector2 的静态 type、函数与 hint 描述符入口。模块聚合器借用这些表交给 native registry；
 * 返回指针由提供者持有，使用期必须短于包含这些静态数据和回调地址的 provider image。
 */

/**
 * ## Summary
 * 返回 Vector2 的静态类型描述符，供数学模块聚合器加入模块 type 表，再由 native registry 物化。
 *
 * ## Parameters
 * 无显式参数；查询静态描述符不创建 VM 实例。
 *
 * ## Returns
 * @return 指向本翻译单元静态 `ZrLibTypeDescriptor` 的借用指针；调用者不得修改或释放。
 *
 * ## Constraints and lifecycle
 * 指针及其中的字段、方法、meta 数组和回调地址只在 `zr.math` provider image 保持加载时有效。
 *
 * ## References
 * `ZrVmLibMath_GetModuleDescriptor` 聚合该结果；`ZrMath_Vector2Registry_GetFunctions` 和
 * `ZrMath_Vector2Registry_GetHints` 提供同一类型模块的其他注册面。
 */
const ZrLibTypeDescriptor *ZrMath_Vector2Registry_GetType(void);

/**
 * ## Summary
 * 查询本叶注册表提供的独立 module function 列表；Vector2 的可调用项实际嵌在 type descriptor 中。
 *
 * ## Parameters
 * @param[out] count 非空时写入列表长度；本实现总写零。该槽由调用者提供并保留所有权。
 *
 * ## Returns
 * @return 本实现返回 `NULL`，表示没有独立 module function 数组；调用者不接管任何对象。
 *
 * ## Constraints and lifecycle
 * 聚合器允许 count 为空；字段/方法/meta callback 由 `GetType` 提供，不可从空列表推断类型无方法。
 *
 * ## References
 * `ZrVmLibMath_GetModuleDescriptor` 的 `ZR_MATH_APPEND_FUNCTION_REGISTRY` 消费本 getter。
 */
const ZrLibFunctionDescriptor *ZrMath_Vector2Registry_GetFunctions(TZrSize *count);

/**
 * ## Summary
 * 返回供语言服务和类型提示使用的 Vector2 静态 hint 数组。
 *
 * ## Parameters
 * @param[out] count 非空时写入静态数组元素数；存储由调用者提供。
 *
 * ## Returns
 * @return 指向本翻译单元静态 hint 数组的借用指针；调用者不得修改或释放。
 *
 * ## Constraints and lifecycle
 * 返回表的字符串字面量和数组地址在 provider image 卸载后不可再访问；count 可为 `NULL`。
 *
 * ## References
 * `ZrVmLibMath_GetModuleDescriptor` 的 hint 聚合宏消费该数组；`GetType` 提供可执行类型描述符。
 */
const ZrLibTypeHintDescriptor *ZrMath_Vector2Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_VECTOR2_REGISTRY_H
