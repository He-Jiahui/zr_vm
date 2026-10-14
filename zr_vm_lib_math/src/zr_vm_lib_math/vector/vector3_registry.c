//
// Vector3 descriptor registry.
//

/** @file
 * ## Summary
 * 定义 Vector3 的静态 field/method/meta/type descriptor、空顶层 function 表和静态 type hints。
 *
 * ## Constraints and lifecycle
 * descriptor、嵌套表、字符串字面量与 callback 地址均由 provider image 持有；
 * module/native metadata 与语言服务只读借用，provider 卸载前必须停止这些消费者。
 *
 * ## References
 * ZrVmLibMath_GetModuleDescriptor；native_binding_metadata 的 type/method materialization；
 * lsp_metadata_provider 与 lsp_native_declaration_projection。
 */
#include "zr_vm_lib_math/vector3_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Vector3Registry_GetType(void) {
    /* 注册表把三维向量的 cross/dot 等命名方法和算符回调导出给 zr.math。 */
    /* struct prototype 的 x/y/z 字段；offset 顺序与 Construct 的字段名和 values 配对。 */
    static const ZrLibFieldDescriptor kFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("z", "float", ZR_NULL),
    };
    /* 语言服务构造签名；固定 arity 在运行时 dispatch 前检查，不在注册时执行。 */
    static const ZrLibParameterDescriptor kConstructorParameters[] = {
            {"x", "float", "The x coordinate.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
            {"y", "float", "The y coordinate.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
            {"z", "float", "The z coordinate.", ZR_LIB_PARAMETER_PASSING_MODE_VALUE},
    };
    /* 命名实例方法及固定 arity；注册保存 callback，dispatcher 在调用前校验参数数量。 */
    static const ZrLibMethodDescriptor kMethods[] = {
            ZR_LIB_METHOD_DESCRIPTOR_INIT("length", 0, 0, ZrMath_Vector3_Length, "float", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("lengthSquared", 0, 0, ZrMath_Vector3_LengthSquared, "float", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("normalized", 0, 0, ZrMath_Vector3_Normalized, "Vector3", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("dot", 1, 1, ZrMath_Vector3_Dot, "float", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("distance", 1, 1, ZrMath_Vector3_Distance, "float", ZR_NULL, ZR_FALSE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("lerp", 2, 2, ZrMath_Vector3_Lerp, "Vector3", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("cross", 1, 1, ZrMath_Vector3_Cross, "Vector3", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
    };
    /* 构造、运算符和字符串化协议映射；每项 arity 均在 callback 前由 dispatcher 校验。 */
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR,3,3,ZrMath_Vector3_Construct,"Vector3",ZR_NULL,
             kConstructorParameters,ZR_ARRAY_COUNT(kConstructorParameters)},
            {ZR_META_ADD,1,1,ZrMath_Vector3_MetaAdd,"Vector3",ZR_NULL,ZR_NULL,0},
            {ZR_META_SUB,1,1,ZrMath_Vector3_MetaSub,"Vector3",ZR_NULL,ZR_NULL,0},
            {ZR_META_NEG,0,0,ZrMath_Vector3_MetaNeg,"Vector3",ZR_NULL,ZR_NULL,0},
            {ZR_META_COMPARE,1,1,ZrMath_Vector3_MetaCompare,"int",ZR_NULL,ZR_NULL,0},
            {ZR_META_TO_STRING,0,0,ZrMath_Vector3_MetaToString,"string",ZR_NULL,ZR_NULL,0},
    };
    /* 静态聚合 descriptor 借用上方嵌套表，生命周期受 provider image 限制。 */
    static const ZrLibTypeDescriptor kType = ZR_LIB_TYPE_DESCRIPTOR_INIT(
            "Vector3",
            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
            kFields,
            ZR_ARRAY_COUNT(kFields),
            kMethods,
            ZR_ARRAY_COUNT(kMethods),
            kMeta,
            ZR_ARRAY_COUNT(kMeta),
            "3D vector value type.",
            ZR_NULL,
            ZR_NULL,
            0,
            ZR_NULL,
            0,
            ZR_NULL,
            ZR_TRUE,
            ZR_TRUE,
            "Vector3(x: float, y: float, z: float)",
            ZR_NULL,
            0);
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_Vector3Registry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_Vector3Registry_GetHints(TZrSize *count) {
    /* 与运行时字段表分离的语言服务提示；数量通过可选 count 输出。 */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Vector3","type","struct Vector3 { x: float, y: float, z: float }","3D vector value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
