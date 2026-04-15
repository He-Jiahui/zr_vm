//
// Tensor descriptor registry.
//

#include "zr_vm_lib_math/tensor_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_TensorRegistry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("shape", "array", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("rank", "int", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("size", "int", ZR_NULL),
    };
    static const ZrLibMethodDescriptor kMethods[] = {
            ZR_LIB_METHOD_DESCRIPTOR_INIT("clone", 0, 0, ZrMath_Tensor_Clone, "Tensor", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("reshape", 1, 1, ZrMath_Tensor_Reshape, "Tensor", ZR_NULL, ZR_FALSE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("fill", 1, 1, ZrMath_Tensor_Fill, "Tensor", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("get", 1, 1, ZrMath_Tensor_Get, "float", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("set", 2, 2, ZrMath_Tensor_Set, "Tensor", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("sum", 0, 0, ZrMath_Tensor_Sum, "float", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("mean", 0, 0, ZrMath_Tensor_Mean, "float", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("transpose2D", 0, 0, ZrMath_Tensor_Transpose2D, "Tensor", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("matmul", 1, 1, ZrMath_Tensor_Matmul, "Tensor", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("add", 1, 1, ZrMath_Tensor_Add, "Tensor", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("sub", 1, 1, ZrMath_Tensor_Sub, "Tensor", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("mulScalar", 1, 1, ZrMath_Tensor_MulScalar, "Tensor", ZR_NULL, ZR_FALSE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("toArray", 0, 0, ZrMath_Tensor_ToArray, "array", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR, 2, 2, ZrMath_Tensor_Construct, "Tensor", ZR_NULL, ZR_NULL, 0},
            {ZR_META_TO_STRING, 0, 0, ZrMath_Tensor_MetaToString, "string", ZR_NULL, ZR_NULL, 0},
    };
    static const ZrLibTypeDescriptor kType = ZR_LIB_TYPE_DESCRIPTOR_INIT("Tensor",
                                                                         ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                                                                         kFields,
                                                                         ZR_ARRAY_COUNT(kFields),
                                                                         kMethods,
                                                                         ZR_ARRAY_COUNT(kMethods),
                                                                         kMeta,
                                                                         ZR_ARRAY_COUNT(kMeta),
                                                                         "Dynamic row-major tensor.",
                                                                         ZR_NULL,
                                                                         ZR_NULL,
                                                                         0,
                                                                         ZR_NULL,
                                                                         0,
                                                                         ZR_NULL,
                                                                         ZR_TRUE,
                                                                         ZR_TRUE,
                                                                         "Tensor(shape: array, fillValue: float)",
                                                                         ZR_NULL,
                                                                         0);
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
