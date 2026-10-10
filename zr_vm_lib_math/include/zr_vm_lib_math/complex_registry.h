/** @file
 * ## Summary
 * 声明 Complex 静态类型、模块函数列表和编译期提示的借用入口。数学模块聚合器
 * 在注册阶段读取这些表，使运行时方法与工具看到同一类型形状。
 *
 * ## Constraints and lifecycle
 * 表及其内嵌数组由本模块持有；模块仍加载期间可借用，不释放、不修改，也不拥有
 * VM 实例或一次调用结果。getter 只交付元数据，实际数值运算发生在后续 binding。
 * GetFunctions/GetHints 的 count 可为 NULL；调用方应区分空模块函数表与实例方法。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/module.c:117、zr_vm_lib_math/src/zr_vm_lib_math/module.c:129 和 zr_vm_lib_math/src/zr_vm_lib_math/module.c:142 分别汇入三类表。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:34 定义类型表；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:59 给出空模块函数表的数量。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:64 定义静态形状提示；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277 后续派发回调。
 */

#ifndef ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H
#define ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H

#include "zr_vm_lib_math/complex.h"

/**
 * ## Summary
 * 借出 Complex 的静态类型描述符，供模块聚合器发布字段、命名方法和元方法。
 *
 * ## Parameters
 * 无参数；不需要一次脚本调用的 context、state 或 result。
 *
 * ## Returns
 * @return 本模块持有的 const 类型描述符，包含字段、方法、元方法和签名等嵌套数据。
 *
 * ## Constraints and lifecycle
 * 只在模块保持加载期间借用；调用方不得释放或修改任何嵌套表。getter 不创建 VM
 * 实例、不调用数值回调，也不把 descriptor 当成某个 VM 的长期所有者。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/module.c:129 是类型聚合入口；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:34 定义表，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:55 返回静态地址。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:26 和 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:31 分别关联构造与比较协议。
 */
const ZrLibTypeDescriptor *ZrMath_ComplexRegistry_GetType(void);
/**
 * ## Summary
 * 向聚合器声明 Complex 没有模块级函数；实例方法仍由 GetType 的类型表发布。
 *
 * ## Parameters
 * @param[out] count 可为 NULL；非 NULL 时须指向可写数量槽，本次写入 0。
 *
 * ## Returns
 * @return 始终为 NULL，表示空模块函数段；不能据此推断 Complex 没有实例方法。
 *
 * ## Constraints and lifecycle
 * 不分配、不转移资源，不保留 count 指针。与返回值一起读取数量；无需释放空表。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/module.c:117 是函数段聚合入口；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:59 写 0，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:60 返回 NULL。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:15 另行定义非空实例方法表。
 */
const ZrLibFunctionDescriptor *ZrMath_ComplexRegistry_GetFunctions(TZrSize *count);
/**
 * ## Summary
 * 借出一项 Complex 字段形状提示，供数学模块的编译期提示聚合器使用。
 *
 * ## Parameters
 * @param[out] count 可为 NULL；非 NULL 时须指向可写数量槽，本次写入提示项数 1。
 *
 * ## Returns
 * @return 本模块持有的单项 const 静态数组；内容描述 real、imag 的数值字段形状。
 *
 * ## Constraints and lifecycle
 * 模块保持加载期间借用数组与字符串；不释放、不修改，不保留 count。提示项只描述
 * 类型，不建立实例、不验证当前 receiver 字段，也不执行运行时回调。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/module.c:142 是提示聚合入口；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:64 定义提示数组。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:67 给出数量，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:68 返回静态数据。
 */
const ZrLibTypeHintDescriptor *ZrMath_ComplexRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H
