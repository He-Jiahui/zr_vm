//
// Vector3 descriptor registry.
//

#include "zr_vm_lib_math/vector3_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Vector3Registry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {{"x","float",ZR_NULL},{"y","float",ZR_NULL},{"z","float",ZR_NULL}};
    static const ZrLibMethodDescriptor kMethods[] = {
            {"length",0,0,ZrMath_Vector3_Length,"float",ZR_NULL,ZR_FALSE},
            {"lengthSquared",0,0,ZrMath_Vector3_LengthSquared,"float",ZR_NULL,ZR_FALSE},
            {"normalized",0,0,ZrMath_Vector3_Normalized,"Vector3",ZR_NULL,ZR_FALSE},
            {"dot",1,1,ZrMath_Vector3_Dot,"float",ZR_NULL,ZR_FALSE},
            {"distance",1,1,ZrMath_Vector3_Distance,"float",ZR_NULL,ZR_FALSE},
            {"lerp",2,2,ZrMath_Vector3_Lerp,"Vector3",ZR_NULL,ZR_FALSE},
            {"cross",1,1,ZrMath_Vector3_Cross,"Vector3",ZR_NULL,ZR_FALSE},
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR,3,3,ZrMath_Vector3_Construct,"Vector3",ZR_NULL},
            {ZR_META_ADD,1,1,ZrMath_Vector3_MetaAdd,"Vector3",ZR_NULL},
            {ZR_META_SUB,1,1,ZrMath_Vector3_MetaSub,"Vector3",ZR_NULL},
            {ZR_META_NEG,0,0,ZrMath_Vector3_MetaNeg,"Vector3",ZR_NULL},
            {ZR_META_COMPARE,1,1,ZrMath_Vector3_MetaCompare,"int",ZR_NULL},
            {ZR_META_TO_STRING,0,0,ZrMath_Vector3_MetaToString,"string",ZR_NULL},
    };
    static const ZrLibTypeDescriptor kType = {"Vector3", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kFields, ZR_ARRAY_COUNT(kFields),
                                              kMethods, ZR_ARRAY_COUNT(kMethods), kMeta, ZR_ARRAY_COUNT(kMeta), ZR_NULL};
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_Vector3Registry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_Vector3Registry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Vector3","type","struct Vector3 { x: float, y: float, z: float }","3D vector value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
