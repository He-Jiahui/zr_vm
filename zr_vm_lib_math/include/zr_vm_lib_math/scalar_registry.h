//
// Scalar registry accessors.
//

#ifndef ZR_VM_LIB_MATH_SCALAR_REGISTRY_H
#define ZR_VM_LIB_MATH_SCALAR_REGISTRY_H

#include "zr_vm_lib_math/scalar.h"

/** @file
 *  模块聚合器读取标量函数及类型提示；返回指针借用静态表，调用方通过有效的
 *  count 指针取得元素数，不释放或修改描述符。
 */
const ZrLibFunctionDescriptor *ZrMath_ScalarRegistry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_ScalarRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_SCALAR_REGISTRY_H
