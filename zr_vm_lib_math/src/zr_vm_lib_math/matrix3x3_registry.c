//
// Matrix3x3 descriptor registry.
//

#include "zr_vm_lib_math/matrix3x3_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Matrix3x3Registry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m00", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m01", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m02", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m10", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m11", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m12", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m20", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m21", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m22", "float", ZR_NULL),
    };
    static const ZrLibMethodDescriptor kMethods[] = {
            ZR_LIB_METHOD_DESCRIPTOR_INIT("identity", 0, 0, ZrMath_Matrix3x3_Identity, "Matrix3x3", ZR_NULL,
                                          ZR_TRUE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("transpose", 0, 0, ZrMath_Matrix3x3_Transpose, "Matrix3x3", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("determinant", 0, 0, ZrMath_Matrix3x3_Determinant, "float", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("inverse", 0, 0, ZrMath_Matrix3x3_Inverse, "Matrix3x3", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("mulVector", 1, 1, ZrMath_Matrix3x3_MulVector, "Vector3", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("mulMatrix", 1, 1, ZrMath_Matrix3x3_MulMatrix, "Matrix3x3", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR, 0, 9, ZrMath_Matrix3x3_Construct, "Matrix3x3", ZR_NULL, ZR_NULL, 0},
            {ZR_META_MUL, 1, 1, ZrMath_Matrix3x3_MetaMul, "object", ZR_NULL, ZR_NULL, 0},
            {ZR_META_TO_STRING, 0, 0, ZrMath_Matrix3x3_MetaToString, "string", ZR_NULL, ZR_NULL, 0},
    };
    static const ZrLibTypeDescriptor kType = ZR_LIB_TYPE_DESCRIPTOR_INIT("Matrix3x3",
                                                                         ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                                                         kFields,
                                                                         ZR_ARRAY_COUNT(kFields),
                                                                         kMethods,
                                                                         ZR_ARRAY_COUNT(kMethods),
                                                                         kMeta,
                                                                         ZR_ARRAY_COUNT(kMeta),
                                                                         "3x3 row-major matrix.",
                                                                         ZR_NULL,
                                                                         ZR_NULL,
                                                                         0,
                                                                         ZR_NULL,
                                                                         0,
                                                                         ZR_NULL,
                                                                         ZR_TRUE,
                                                                         ZR_TRUE,
                                                                         "Matrix3x3(...values: float)",
                                                                         ZR_NULL,
                                                                         0);
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
