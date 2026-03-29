//
// Matrix3x3 descriptor registry.
//

#include "zr_vm_lib_math/matrix3x3_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Matrix3x3Registry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {
            {"m00", "float", ZR_NULL}, {"m01", "float", ZR_NULL}, {"m02", "float", ZR_NULL},
            {"m10", "float", ZR_NULL}, {"m11", "float", ZR_NULL}, {"m12", "float", ZR_NULL},
            {"m20", "float", ZR_NULL}, {"m21", "float", ZR_NULL}, {"m22", "float", ZR_NULL}
    };
    static const ZrLibMethodDescriptor kMethods[] = {
            {"identity", 0, 0, ZrMath_Matrix3x3_Identity, "Matrix3x3", ZR_NULL, ZR_TRUE},
            {"transpose", 0, 0, ZrMath_Matrix3x3_Transpose, "Matrix3x3", ZR_NULL, ZR_FALSE},
            {"determinant", 0, 0, ZrMath_Matrix3x3_Determinant, "float", ZR_NULL, ZR_FALSE},
            {"inverse", 0, 0, ZrMath_Matrix3x3_Inverse, "Matrix3x3", ZR_NULL, ZR_FALSE},
            {"mulVector", 1, 1, ZrMath_Matrix3x3_MulVector, "Vector3", ZR_NULL, ZR_FALSE},
            {"mulMatrix", 1, 1, ZrMath_Matrix3x3_MulMatrix, "Matrix3x3", ZR_NULL, ZR_FALSE},
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR, 0, 9, ZrMath_Matrix3x3_Construct, "Matrix3x3", ZR_NULL},
            {ZR_META_MUL, 1, 1, ZrMath_Matrix3x3_MetaMul, "object", ZR_NULL},
            {ZR_META_TO_STRING, 0, 0, ZrMath_Matrix3x3_MetaToString, "string", ZR_NULL},
    };
    static const ZrLibTypeDescriptor kType = {
            "Matrix3x3",
            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
            kFields,
            ZR_ARRAY_COUNT(kFields),
            kMethods,
            ZR_ARRAY_COUNT(kMethods),
            kMeta,
            ZR_ARRAY_COUNT(kMeta),
            ZR_NULL
    };
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_Matrix3x3Registry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) {
        *count = 0;
    }
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_Matrix3x3Registry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Matrix3x3", "type", "struct Matrix3x3 { m00..m22 }", "3x3 row-major matrix."}
    };
    if (count != ZR_NULL) {
        *count = ZR_ARRAY_COUNT(kHints);
    }
    return kHints;
}
