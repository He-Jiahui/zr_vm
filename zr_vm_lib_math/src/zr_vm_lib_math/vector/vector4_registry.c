//
// Vector4 descriptor registry.
//

/** @file
 * ## Summary
 * 定义 Vector4 的静态字段、命名方法、meta callback、type descriptor、空顶层函数表与语言服务提示。
 *
 * ## Constraints and lifecycle
 * 所有 descriptor、嵌套数组、字符串字面量和 callback 地址均使用静态存储；module 聚合器、native metadata
 * 和 LSP 只读借用，provider image 必须覆盖其全部使用期。方法/meta arity 由 dispatcher 在运行调用前检查，
 * 注册阶段只保存固定数量与 callback 指针。
 *
 * ## References
 * ZrVmLibMath_GetModuleDescriptor；native_binding_metadata；lsp_metadata_provider；
 * lsp_native_declaration_projection；vector4.h
 */
#include "zr_vm_lib_math/vector4_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Vector4Registry_GetType(void) {
    /* 注册表定义脚本可见的四维字段与协议绑定，供模块描述符借用。 */
    /* 字段顺序与 Construct/MakeVector4 的 x/y/z/w 写入顺序一致。 */
    static const ZrLibFieldDescriptor kFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("z", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("w", "float", ZR_NULL),
    };
    /* 固定 arity 与 callback 地址存入 descriptor；dispatcher 在每次运行调用前校验实参数量。 */
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
    /* constructor 需四个 VM 参数；meta 运算符的固定参数数同样由调用期 dispatcher 检查。 */
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR,4,4,ZrMath_Vector4_Construct,"Vector4",ZR_NULL,ZR_NULL,0},
            {ZR_META_ADD,1,1,ZrMath_Vector4_MetaAdd,"Vector4",ZR_NULL,ZR_NULL,0},
            {ZR_META_SUB,1,1,ZrMath_Vector4_MetaSub,"Vector4",ZR_NULL,ZR_NULL,0},
            {ZR_META_NEG,0,0,ZrMath_Vector4_MetaNeg,"Vector4",ZR_NULL,ZR_NULL,0},
            {ZR_META_COMPARE,1,1,ZrMath_Vector4_MetaCompare,"int",ZR_NULL,ZR_NULL,0},
            {ZR_META_TO_STRING,0,0,ZrMath_Vector4_MetaToString,"string",ZR_NULL,ZR_NULL,0},
    };
    /* 返回的总表只借用上方静态数组；字符串签名和 callback 地址也随 provider image 有效。 */
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
    /* Vector4 没有 module-level function；方法和 meta callback 已放在 GetType 的 descriptor 内。 */
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_Vector4Registry_GetHints(TZrSize *count) {
    /* 独立于运行时 type descriptor 的静态 LSP 提示；消费者仅在 provider image 有效时借用。 */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Vector4","type","struct Vector4 { x: float, y: float, z: float, w: float }","4D vector value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
