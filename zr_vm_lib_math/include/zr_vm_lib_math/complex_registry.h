//
// Complex registry accessors.
//

#ifndef ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H
#define ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H

#include "zr_vm_lib_math/complex.h"

/** @file
 *  模块聚合器读取本注册表，将 Complex 的进程期静态描述符交给 native registry；
 *  返回指针由本模块持有，GetFunctions/GetHints 通过有效的 count 指针返回数量。
 */
const ZrLibTypeDescriptor *ZrMath_ComplexRegistry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_ComplexRegistry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_ComplexRegistry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_COMPLEX_REGISTRY_H
