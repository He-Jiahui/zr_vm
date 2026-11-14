/** @file
 * ## Summary
 * 定义 Tensor 的静态 native descriptor、callback 映射和类型形状提示。 */

#include "zr_vm_lib_math/tensor_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_TensorRegistry_GetType(void) {
    /* module.c 聚合此静态类型描述符；静态表只在 provider 映像驻留期间有效。
     * GetHints 只提供 metadata 形状提示；参数标签仍由 callback 读取路径判定。 */
    static const ZrLibFieldDescriptor kFields[] = { /* 仅发布 shape/rank/size；__data 是内部存储字段。 */
            ZR_LIB_FIELD_DESCRIPTOR_INIT("shape", "array", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("rank", "int", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("size", "int", ZR_NULL),
    };
    static const ZrLibParameterDescriptor kConstructorParameters[] = {{"shape", "array", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"data", "array", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kShapeParameters[] = {{"shape", "array", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kValueParameters[] = {{"value", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kIndicesParameters[] = {{"indices", "array", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kSetParameters[] = {{"indices", "array", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"value", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kOtherParameters[] = {{"other", "Tensor", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kFactorParameters[] = {{"factor", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibMethodDescriptor kMethods[] = { /* 固定顺序；arity 计显式参数，不含 receiver。 */
            ZR_LIB_METHOD_DESCRIPTOR_INIT("clone", 0, 0, ZrMath_Tensor_Clone, "Tensor", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("reshape", 1, 1, ZrMath_Tensor_Reshape, "Tensor", ZR_NULL, ZR_FALSE,
                                          kShapeParameters, ZR_ARRAY_COUNT(kShapeParameters)),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("fill", 1, 1, ZrMath_Tensor_Fill, "Tensor", ZR_NULL, ZR_FALSE, kValueParameters, ZR_ARRAY_COUNT(kValueParameters)),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("get", 1, 1, ZrMath_Tensor_Get, "float", ZR_NULL, ZR_FALSE, kIndicesParameters, ZR_ARRAY_COUNT(kIndicesParameters)),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("set", 2, 2, ZrMath_Tensor_Set, "Tensor", ZR_NULL, ZR_FALSE, kSetParameters, ZR_ARRAY_COUNT(kSetParameters)),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("sum", 0, 0, ZrMath_Tensor_Sum, "float", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("mean", 0, 0, ZrMath_Tensor_Mean, "float", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("transpose2D", 0, 0, ZrMath_Tensor_Transpose2D, "Tensor", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("matmul", 1, 1, ZrMath_Tensor_Matmul, "Tensor", ZR_NULL, ZR_FALSE, kOtherParameters, ZR_ARRAY_COUNT(kOtherParameters)),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("add", 1, 1, ZrMath_Tensor_Add, "Tensor", ZR_NULL, ZR_FALSE, kOtherParameters, ZR_ARRAY_COUNT(kOtherParameters)),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("sub", 1, 1, ZrMath_Tensor_Sub, "Tensor", ZR_NULL, ZR_FALSE, kOtherParameters, ZR_ARRAY_COUNT(kOtherParameters)),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("mulScalar", 1, 1, ZrMath_Tensor_MulScalar, "Tensor", ZR_NULL, ZR_FALSE,
                                          kFactorParameters, ZR_ARRAY_COUNT(kFactorParameters)),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("toArray", 0, 0, ZrMath_Tensor_ToArray, "array", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
    };
    /* 构造器参数精确声明 shape/data 数组，供 native metadata 与语言服务展示。 */
    static const ZrLibMetaMethodDescriptor kMeta[] = { /* constructor 与 toString 由 VM 的元方法入口派发。 */
            {ZR_META_CONSTRUCTOR, 2, 2, ZrMath_Tensor_Construct, "Tensor", ZR_NULL, kConstructorParameters, ZR_ARRAY_COUNT(kConstructorParameters)},
            {ZR_META_TO_STRING, 0, 0, ZrMath_Tensor_MetaToString, "string", ZR_NULL, ZR_NULL, 0},
    };
    /* 构造器 callback 的第二个输入是 data 数组，与上方参数表和 signature 保持一致。 */
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
                                                                         "Tensor(shape: array, data: array)",
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
    static const ZrLibTypeHintDescriptor kHints[] = { /* 一条 class Tensor 形状提示，供 module metadata 导出。 */
            {"Tensor", "type", "class Tensor", "Dynamic row-major tensor."}
    };
    if (count != ZR_NULL) {
        *count = ZR_ARRAY_COUNT(kHints);
    }
    return kHints;
}
