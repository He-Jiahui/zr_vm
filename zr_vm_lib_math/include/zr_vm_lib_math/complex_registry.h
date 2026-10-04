//
// Complex registry accessors.
//

#ifndef ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H
#define ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H

#include "zr_vm_lib_math/complex.h"

/** @file
 *  模块聚合器读取本注册表，将 Complex 的进程期静态描述符交给 native registry；
 *  返回指针由本模块持有且仅在模块仍加载时可借用；GetFunctions/GetHints 的 count 可以为空。
 */
/**
 * @brief 借出 Complex 的静态类型描述符，供模块聚合器发布同一回调契约。
 * 返回值及内嵌数组由本模块持有；仅在模块仍加载时借用，调用方不释放或修改。
 */
const ZrLibTypeDescriptor *ZrMath_ComplexRegistry_GetType(void);
/**
 * @brief 声明 Complex 没有模块级函数；实例方法仍通过 GetType 发布。
 * 始终返回 NULL；count 非 NULL 时写零，也允许传 NULL。
 */
const ZrLibFunctionDescriptor *ZrMath_ComplexRegistry_GetFunctions(TZrSize *count);
/**
 * @brief 借出一项 Complex 字段形状提示，供编译期提示聚合。
 * 返回本模块静态数组；count 非 NULL 时写一，也允许 NULL；调用方不释放或修改。
 */
const ZrLibTypeHintDescriptor *ZrMath_ComplexRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H
