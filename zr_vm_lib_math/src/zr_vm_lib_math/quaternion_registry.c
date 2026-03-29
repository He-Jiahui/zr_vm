//
// Quaternion descriptor registry.
//

#include "zr_vm_lib_math/quaternion_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_QuaternionRegistry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {{"x","float",ZR_NULL},{"y","float",ZR_NULL},{"z","float",ZR_NULL},{"w","float",ZR_NULL}};
    static const ZrLibMethodDescriptor kMethods[] = {
            {"length",0,0,ZrMath_Quaternion_Length,"float",ZR_NULL,ZR_FALSE},
            {"lengthSquared",0,0,ZrMath_Quaternion_LengthSquared,"float",ZR_NULL,ZR_FALSE},
            {"normalized",0,0,ZrMath_Quaternion_Normalized,"Quaternion",ZR_NULL,ZR_FALSE},
            {"conjugate",0,0,ZrMath_Quaternion_Conjugate,"Quaternion",ZR_NULL,ZR_FALSE},
            {"inverse",0,0,ZrMath_Quaternion_Inverse,"Quaternion",ZR_NULL,ZR_FALSE},
            {"dot",1,1,ZrMath_Quaternion_Dot,"float",ZR_NULL,ZR_FALSE},
            {"mul",1,1,ZrMath_Quaternion_Mul,"Quaternion",ZR_NULL,ZR_FALSE},
            {"slerp",2,2,ZrMath_Quaternion_Slerp,"Quaternion",ZR_NULL,ZR_FALSE},
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR,4,4,ZrMath_Quaternion_Construct,"Quaternion",ZR_NULL},
            {ZR_META_ADD,1,1,ZrMath_Quaternion_MetaAdd,"Quaternion",ZR_NULL},
            {ZR_META_SUB,1,1,ZrMath_Quaternion_MetaSub,"Quaternion",ZR_NULL},
            {ZR_META_MUL,1,1,ZrMath_Quaternion_MetaMul,"Quaternion",ZR_NULL},
            {ZR_META_NEG,0,0,ZrMath_Quaternion_MetaNeg,"Quaternion",ZR_NULL},
            {ZR_META_COMPARE,1,1,ZrMath_Quaternion_MetaCompare,"int",ZR_NULL},
            {ZR_META_TO_STRING,0,0,ZrMath_Quaternion_MetaToString,"string",ZR_NULL},
    };
    static const ZrLibTypeDescriptor kType = {"Quaternion", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kFields, ZR_ARRAY_COUNT(kFields),
                                              kMethods, ZR_ARRAY_COUNT(kMethods), kMeta, ZR_ARRAY_COUNT(kMeta), ZR_NULL};
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_QuaternionRegistry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_QuaternionRegistry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Quaternion","type","struct Quaternion { x: float, y: float, z: float, w: float }","Quaternion value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
