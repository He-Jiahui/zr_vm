//
// Scalar descriptor registry.
//

#include "zr_vm_lib_math/scalar_registry.h"

const ZrLibFunctionDescriptor *ZrMath_ScalarRegistry_GetFunctions(TZrSize *count) {
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"abs",1,1,ZrMath_Scalar_Abs,"float",ZR_NULL,ZR_NULL,0},
            {"min",2,2,ZrMath_Scalar_Min,"float",ZR_NULL,ZR_NULL,0},
            {"max",2,2,ZrMath_Scalar_Max,"float",ZR_NULL,ZR_NULL,0},
            {"clamp",3,3,ZrMath_Scalar_Clamp,"float",ZR_NULL,ZR_NULL,0},
            {"lerp",3,3,ZrMath_Scalar_Lerp,"float",ZR_NULL,ZR_NULL,0},
            {"sqrt",1,1,ZrMath_Scalar_Sqrt,"float",ZR_NULL,ZR_NULL,0},
            {"rsqrt",1,1,ZrMath_Scalar_Rsqrt,"float",ZR_NULL,ZR_NULL,0},
            {"pow",2,2,ZrMath_Scalar_Pow,"float",ZR_NULL,ZR_NULL,0},
            {"exp",1,1,ZrMath_Scalar_Exp,"float",ZR_NULL,ZR_NULL,0},
            {"log",1,1,ZrMath_Scalar_Log,"float",ZR_NULL,ZR_NULL,0},
            {"sin",1,1,ZrMath_Scalar_Sin,"float",ZR_NULL,ZR_NULL,0},
            {"cos",1,1,ZrMath_Scalar_Cos,"float",ZR_NULL,ZR_NULL,0},
            {"tan",1,1,ZrMath_Scalar_Tan,"float",ZR_NULL,ZR_NULL,0},
            {"asin",1,1,ZrMath_Scalar_Asin,"float",ZR_NULL,ZR_NULL,0},
            {"acos",1,1,ZrMath_Scalar_Acos,"float",ZR_NULL,ZR_NULL,0},
            {"atan",1,1,ZrMath_Scalar_Atan,"float",ZR_NULL,ZR_NULL,0},
            {"atan2",2,2,ZrMath_Scalar_Atan2,"float",ZR_NULL,ZR_NULL,0},
            {"floor",1,1,ZrMath_Scalar_Floor,"float",ZR_NULL,ZR_NULL,0},
            {"ceil",1,1,ZrMath_Scalar_Ceil,"float",ZR_NULL,ZR_NULL,0},
            {"round",1,1,ZrMath_Scalar_Round,"float",ZR_NULL,ZR_NULL,0},
            {"sign",1,1,ZrMath_Scalar_Sign,"float",ZR_NULL,ZR_NULL,0},
            {"degrees",1,1,ZrMath_Scalar_Degrees,"float",ZR_NULL,ZR_NULL,0},
            {"radians",1,1,ZrMath_Scalar_Radians,"float",ZR_NULL,ZR_NULL,0},
            {"almostEqual",2,3,ZrMath_Scalar_AlmostEqual,"bool",ZR_NULL,ZR_NULL,0},
            {"invokeCallback",2,2,ZrMath_InvokeCallback,"float",ZR_NULL,ZR_NULL,0},
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kFunctions);
    return kFunctions;
}

const ZrLibTypeHintDescriptor *ZrMath_ScalarRegistry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"abs","function","abs(value: float): float","Absolute value."},
            {"min","function","min(lhs: float, rhs: float): float","Minimum of two values."},
            {"max","function","max(lhs: float, rhs: float): float","Maximum of two values."},
            {"almostEqual","function","almostEqual(lhs: float, rhs: float, epsilon?: float): bool","Epsilon based comparison."},
            {"invokeCallback","function","invokeCallback(callback: function, value: float): float","Invoke a zr callback from native code."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
