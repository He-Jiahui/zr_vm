//
// Scalar registry accessors.
//

#ifndef ZR_VM_LIB_MATH_SCALAR_REGISTRY_H
#define ZR_VM_LIB_MATH_SCALAR_REGISTRY_H

#include "zr_vm_lib_math/scalar.h"

/** @file
 * ## Summary
 * 提供 `zr.math` 标量 native 函数和可选类型提示的注册表访问器，供 Math 模块聚合阶段读取。
 *
 * ## Constraints and lifecycle
 * 两个 getter 返回只读静态数组；返回指针由本模块映像持有，调用者不得修改或释放，且仅在
 * 包含该表的模块映像仍驻留时有效。`count` 可为 `ZR_NULL`；不共享可变计数状态。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 中的注册表追加宏消费这两个 getter；最终模块描述符
 * 再由 native binding 注册和序列化。
 */
/**
 * ## Summary
 * 返回标量模块函数描述符表，供 `zr.math` 聚合器注册 native callback。
 *
 * ## Parameters
 * @param[out] count 可选的元素数输出地址；非空时写入本次返回表的元素数，空时跳过计数写入。
 *
 * ## Returns
 * @return 指向只读 `ZrLibFunctionDescriptor` 静态数组首项的借用指针；数组不由调用者释放，
 *         其有效期受包含此表的模块映像驻留期约束。
 *
 * ## Constraints and lifecycle
 * 调用不分配或改变注册表；表中 arity、返回标签和 callback 指针须保持同步。
 * caller 可并发只读访问，但不得写入表或在模块卸载后继续使用指针。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetHints` 提供同一模块的提示表；`ZrMath_Scalar_*` 与 `ZrMath_InvokeCallback`
 * 是函数描述符中的 callback。
 */
const ZrLibFunctionDescriptor *ZrMath_ScalarRegistry_GetFunctions(TZrSize *count);
/**
 * ## Summary
 * 返回标量模块的类型提示表，供模块聚合和后续 metadata 导出使用。
 *
 * ## Parameters
 * @param[out] count 可选的元素数输出地址；非空时写入返回提示表的元素数，空时不写入。
 *
 * ## Returns
 * @return 指向只读 `ZrLibTypeHintDescriptor` 静态数组首项的借用指针；调用者不得释放或改写，
 *         仅在包含该表的模块映像仍驻留时有效。
 *
 * ## Constraints and lifecycle
 * 提示只描述注册的符号形状，不会在这里执行 callback，也不改变运行时参数校验。
 * getter 无可变共享状态，可并发读取；模块卸载后不得继续使用返回地址。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 聚合此表；native metadata serializer 消费聚合后的提示。
 */
const ZrLibTypeHintDescriptor *ZrMath_ScalarRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_SCALAR_REGISTRY_H
