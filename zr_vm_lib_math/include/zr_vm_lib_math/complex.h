//
// Complex native callbacks.
//

#ifndef ZR_VM_LIB_MATH_COMPLEX_H
#define ZR_VM_LIB_MATH_COMPLEX_H

#include "zr_vm_lib_math/math_common.h"

TZrBool ZrMath_Complex_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_Magnitude(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_Phase(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_Conjugate(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Complex_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_COMPLEX_H
