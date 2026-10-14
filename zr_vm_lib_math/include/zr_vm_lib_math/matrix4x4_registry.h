//
// Matrix4x4 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_MATRIX4X4_REGISTRY_H
#define ZR_VM_LIB_MATH_MATRIX4X4_REGISTRY_H

#include "zr_vm_lib_math/matrix4x4.h"

/** @file
 * ## Summary
 * 暴露 Matrix4x4 的静态类型描述符、空自由函数表和类型提示表，供 math 模块聚合器注册给 native binding 与语言服务。
 * ## Constraints and lifecycle
 * 返回的表和描述符是函数内静态只读存储；借用指针仅在包含本代码的 provider 映像保持驻留期间有效，不能由调用者释放或在映像卸载后继续使用。表只读，可由多个调用方并发读取。
 * ## References
 * ZrMath_Matrix4x4Registry_GetType、ZrMath_Matrix4x4Registry_GetFunctions、ZrMath_Matrix4x4Registry_GetHints；module.c 的类型、函数和 hint 聚合路径。
 */
/**
 * ## Summary
 * 返回含 Matrix4x4 字段、方法及元方法表的静态类型描述符。
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 借用的只读描述符指针；仅 provider 映像驻留期间有效，调用者不可释放。
 *
 * ## Constraints and lifecycle
 * 每次返回同一静态 kType，及其引用的静态字段/方法/meta 表；不分配或修改状态。
 *
 * ## References
 * module.c:131 调用并聚合本类型描述符；matrix4x4_registry.c 的 kType initializer 消费三张表。
 */
const ZrLibTypeDescriptor *ZrMath_Matrix4x4Registry_GetType(void);
/**
 * ## Summary
 * 本类型当前不导出 module-level 自由函数。

 * ## Parameters
 * @param[out] count 非空时写入 0；参数可为空。
 *
 * ## Returns
 * @return 始终返回 ZR_NULL，表示自由函数表为空；无分配或所有权转移。
 *
 * ## Constraints and lifecycle
 * 仅更新调用者提供的 count；无共享可变状态。
 *
 * ## References
 * module.c:119 的函数聚合宏调用；方法仍通过 GetType.kMethods 注册。
 */
const ZrLibFunctionDescriptor *ZrMath_Matrix4x4Registry_GetFunctions(TZrSize *count);
/**
 * ## Summary
 * 返回 Matrix4x4 的语言服务结构提示。

 * ## Parameters
 * @param[out] count 非空时写入静态 hint 记录数；参数可为空。
 *
 * ## Returns
 * @return 返回借用的只读静态 kHints 指针；仅 provider 映像驻留期间有效。
 *
 * ## Constraints and lifecycle
 * 静态 hint 不可修改或释放；调用者不得在映像卸载后继续使用指针。
 *
 * ## References
 * module.c:144 的 hint 聚合宏调用；语言服务从聚合 hint 表读取。
 */
const ZrLibTypeHintDescriptor *ZrMath_Matrix4x4Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_MATRIX4X4_REGISTRY_H
