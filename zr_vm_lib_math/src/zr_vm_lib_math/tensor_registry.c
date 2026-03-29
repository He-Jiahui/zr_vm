//
// Tensor descriptor registry.
//

#include "zr_vm_lib_math/tensor_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_TensorRegistry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {
            {"shape", "array", ZR_NULL},
            {"rank", "int", ZR_NULL},
            {"size", "int", ZR_NULL},
    };
    static const ZrLibMethodDescriptor kMethods[] = {
            {"clone", 0, 0, ZrMath_Tensor_Clone, "Tensor", ZR_NULL, ZR_FALSE},
            {"reshape", 1, 1, ZrMath_Tensor_Reshape, "Tensor", ZR_NULL, ZR_FALSE},
            {"fill", 1, 1, ZrMath_Tensor_Fill, "Tensor", ZR_NULL, ZR_FALSE},
            {"get", 1, 1, ZrMath_Tensor_Get, "float", ZR_NULL, ZR_FALSE},
            {"set", 2, 2, ZrMath_Tensor_Set, "Tensor", ZR_NULL, ZR_FALSE},
            {"sum", 0, 0, ZrMath_Tensor_Sum, "float", ZR_NULL, ZR_FALSE},
            {"mean", 0, 0, ZrMath_Tensor_Mean, "float", ZR_NULL, ZR_FALSE},
            {"transpose2D", 0, 0, ZrMath_Tensor_Transpose2D, "Tensor", ZR_NULL, ZR_FALSE},
            {"matmul", 1, 1, ZrMath_Tensor_Matmul, "Tensor", ZR_NULL, ZR_FALSE},
            {"add", 1, 1, ZrMath_Tensor_Add, "Tensor", ZR_NULL, ZR_FALSE},
            {"sub", 1, 1, ZrMath_Tensor_Sub, "Tensor", ZR_NULL, ZR_FALSE},
            {"mulScalar", 1, 1, ZrMath_Tensor_MulScalar, "Tensor", ZR_NULL, ZR_FALSE},
            {"toArray", 0, 0, ZrMath_Tensor_ToArray, "array", ZR_NULL, ZR_FALSE},
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR, 2, 2, ZrMath_Tensor_Construct, "Tensor", ZR_NULL},
            {ZR_META_TO_STRING, 0, 0, ZrMath_Tensor_MetaToString, "string", ZR_NULL},
    };
    static const ZrLibTypeDescriptor kType = {
            "Tensor",
            ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
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

const ZrLibFunctionDescriptor *ZrMath_TensorRegistry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) {
        *count = 0;
    }
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_TensorRegistry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Tensor", "type", "class Tensor", "Dynamic row-major tensor."}
    };
    if (count != ZR_NULL) {
        *count = ZR_ARRAY_COUNT(kHints);
    }
    return kHints;
}
