//
// Matrix4x4 descriptor registry.
//

#include "zr_vm_lib_math/matrix4x4_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Matrix4x4Registry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {
            {"m00","float",ZR_NULL},{"m01","float",ZR_NULL},{"m02","float",ZR_NULL},{"m03","float",ZR_NULL},
            {"m10","float",ZR_NULL},{"m11","float",ZR_NULL},{"m12","float",ZR_NULL},{"m13","float",ZR_NULL},
            {"m20","float",ZR_NULL},{"m21","float",ZR_NULL},{"m22","float",ZR_NULL},{"m23","float",ZR_NULL},
            {"m30","float",ZR_NULL},{"m31","float",ZR_NULL},{"m32","float",ZR_NULL},{"m33","float",ZR_NULL}
    };
    static const ZrLibMethodDescriptor kMethods[] = {
            {"translation",3,3,ZrMath_Matrix4x4_Translation,"Matrix4x4",ZR_NULL,ZR_TRUE},
            {"scale",3,3,ZrMath_Matrix4x4_Scale,"Matrix4x4",ZR_NULL,ZR_TRUE},
            {"rotationX",1,1,ZrMath_Matrix4x4_RotationX,"Matrix4x4",ZR_NULL,ZR_TRUE},
            {"rotationY",1,1,ZrMath_Matrix4x4_RotationY,"Matrix4x4",ZR_NULL,ZR_TRUE},
            {"rotationZ",1,1,ZrMath_Matrix4x4_RotationZ,"Matrix4x4",ZR_NULL,ZR_TRUE},
            {"identity",0,0,ZrMath_Matrix4x4_Identity,"Matrix4x4",ZR_NULL,ZR_TRUE},
            {"transpose",0,0,ZrMath_Matrix4x4_Transpose,"Matrix4x4",ZR_NULL,ZR_FALSE},
            {"determinant",0,0,ZrMath_Matrix4x4_Determinant,"float",ZR_NULL,ZR_FALSE},
            {"inverse",0,0,ZrMath_Matrix4x4_Inverse,"Matrix4x4",ZR_NULL,ZR_FALSE},
            {"mulVector",1,1,ZrMath_Matrix4x4_MulVector,"Vector4",ZR_NULL,ZR_FALSE},
            {"mulMatrix",1,1,ZrMath_Matrix4x4_MulMatrix,"Matrix4x4",ZR_NULL,ZR_FALSE},
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR,0,16,ZrMath_Matrix4x4_Construct,"Matrix4x4",ZR_NULL},
            {ZR_META_MUL,1,1,ZrMath_Matrix4x4_MetaMul,"object",ZR_NULL},
            {ZR_META_TO_STRING,0,0,ZrMath_Matrix4x4_MetaToString,"string",ZR_NULL},
    };
    static const ZrLibTypeDescriptor kType = {"Matrix4x4", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kFields, ZR_ARRAY_COUNT(kFields),
                                              kMethods, ZR_ARRAY_COUNT(kMethods), kMeta, ZR_ARRAY_COUNT(kMeta), ZR_NULL};
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_Matrix4x4Registry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_Matrix4x4Registry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Matrix4x4","type","struct Matrix4x4 { m00..m33 }","4x4 row-major matrix."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
