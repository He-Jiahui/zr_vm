//
// Matrix3x3 registry accessors.
//

#ifndef ZR_VM_LIB_MATH_MATRIX3X3_REGISTRY_H
#define ZR_VM_LIB_MATH_MATRIX3X3_REGISTRY_H

#include "zr_vm_lib_math/matrix3x3.h"

const ZrLibTypeDescriptor *ZrMath_Matrix3x3Registry_GetType(void);
const ZrLibFunctionDescriptor *ZrMath_Matrix3x3Registry_GetFunctions(TZrSize *count);
const ZrLibTypeHintDescriptor *ZrMath_Matrix3x3Registry_GetHints(TZrSize *count);

#endif // ZR_VM_LIB_MATH_MATRIX3X3_REGISTRY_H
