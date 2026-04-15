//
// Vector4 descriptor registry.
//

#include "zr_vm_lib_math/vector4_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Vector4Registry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("z", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("w", "float", ZR_NULL),
    };
    static const ZrLibMethodDescriptor kMethods[] = {
            ZR_LIB_METHOD_DESCRIPTOR_INIT("length", 0, 0, ZrMath_Vector4_Length, "float", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("lengthSquared", 0, 0, ZrMath_Vector4_LengthSquared, "float", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("normalized", 0, 0, ZrMath_Vector4_Normalized, "Vector4", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("dot", 1, 1, ZrMath_Vector4_Dot, "float", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("distance", 1, 1, ZrMath_Vector4_Distance, "float", ZR_NULL, ZR_FALSE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("lerp", 2, 2, ZrMath_Vector4_Lerp, "Vector4", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR,4,4,ZrMath_Vector4_Construct,"Vector4",ZR_NULL,ZR_NULL,0},
            {ZR_META_ADD,1,1,ZrMath_Vector4_MetaAdd,"Vector4",ZR_NULL,ZR_NULL,0},
            {ZR_META_SUB,1,1,ZrMath_Vector4_MetaSub,"Vector4",ZR_NULL,ZR_NULL,0},
            {ZR_META_NEG,0,0,ZrMath_Vector4_MetaNeg,"Vector4",ZR_NULL,ZR_NULL,0},
            {ZR_META_COMPARE,1,1,ZrMath_Vector4_MetaCompare,"int",ZR_NULL,ZR_NULL,0},
            {ZR_META_TO_STRING,0,0,ZrMath_Vector4_MetaToString,"string",ZR_NULL,ZR_NULL,0},
    };
    static const ZrLibTypeDescriptor kType = ZR_LIB_TYPE_DESCRIPTOR_INIT(
            "Vector4",
            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
            kFields,
            ZR_ARRAY_COUNT(kFields),
            kMethods,
            ZR_ARRAY_COUNT(kMethods),
            kMeta,
            ZR_ARRAY_COUNT(kMeta),
            "4D vector value type.",
            ZR_NULL,
            ZR_NULL,
            0,
            ZR_NULL,
            0,
            ZR_NULL,
            ZR_TRUE,
            ZR_TRUE,
            "Vector4(x: float, y: float, z: float, w: float)",
            ZR_NULL,
            0);
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_Vector4Registry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_Vector4Registry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Vector4","type","struct Vector4 { x: float, y: float, z: float, w: float }","4D vector value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
