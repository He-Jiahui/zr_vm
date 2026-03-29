//
// Complex descriptor registry.
//

#include "zr_vm_lib_math/complex_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_ComplexRegistry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {{"real","float",ZR_NULL},{"imag","float",ZR_NULL}};
    static const ZrLibMethodDescriptor kMethods[] = {
            {"magnitude",0,0,ZrMath_Complex_Magnitude,"float",ZR_NULL,ZR_FALSE},
            {"phase",0,0,ZrMath_Complex_Phase,"float",ZR_NULL,ZR_FALSE},
            {"conjugate",0,0,ZrMath_Complex_Conjugate,"Complex",ZR_NULL,ZR_FALSE},
            {"normalized",0,0,ZrMath_Complex_Normalized,"Complex",ZR_NULL,ZR_FALSE},
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR,2,2,ZrMath_Complex_Construct,"Complex",ZR_NULL},
            {ZR_META_ADD,1,1,ZrMath_Complex_MetaAdd,"Complex",ZR_NULL},
            {ZR_META_SUB,1,1,ZrMath_Complex_MetaSub,"Complex",ZR_NULL},
            {ZR_META_MUL,1,1,ZrMath_Complex_MetaMul,"Complex",ZR_NULL},
            {ZR_META_NEG,0,0,ZrMath_Complex_MetaNeg,"Complex",ZR_NULL},
            {ZR_META_COMPARE,1,1,ZrMath_Complex_MetaCompare,"int",ZR_NULL},
            {ZR_META_TO_STRING,0,0,ZrMath_Complex_MetaToString,"string",ZR_NULL},
    };
    static const ZrLibTypeDescriptor kType = {"Complex", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kFields, ZR_ARRAY_COUNT(kFields),
                                              kMethods, ZR_ARRAY_COUNT(kMethods), kMeta, ZR_ARRAY_COUNT(kMeta), ZR_NULL};
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_ComplexRegistry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_ComplexRegistry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Complex","type","struct Complex { real: float, imag: float }","Complex number value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
