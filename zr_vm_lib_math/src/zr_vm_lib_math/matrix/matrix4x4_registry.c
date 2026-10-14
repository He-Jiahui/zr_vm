//
// Matrix4x4 descriptor registry.
//

#include "zr_vm_lib_math/matrix4x4_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

/** @file
 * ## Summary
 * 构建 Matrix4x4 的静态 field/method/meta descriptor 与语言服务 hint，并向模块聚合器提供访问函数。
 * ## Constraints and lifecycle
 * 表和描述符不可变且有静态存储期；借用指针仅在包含本实现的 provider 映像驻留期间有效。getter 不动态分配或修改表。
 * ## References
 * module.c 的 ZrMath_Matrix4x4Registry_GetType/GetFunctions/GetHints 聚合路径；native binding 用 type descriptor 建立可派发的字段和 callback 元数据。
 */
/**
 * ## Summary
 * 返回 Matrix4x4 完整静态类型描述符，供 math 模块注册字段、方法和元方法。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 借用的 const kType 地址；不得修改或释放。
 *
 * ## Constraints and lifecycle
 * kType 及其引用的表均为函数内静态只读存储；地址在 provider 映像卸载后不再有效。函数无动态初始化或可变全局状态，可供聚合阶段重复读取。
 *
 * ## References
 * module.c 的类型聚合调用；kFields、kMethods、kMeta、kType。
 */
const ZrLibTypeDescriptor *ZrMath_Matrix4x4Registry_GetType(void) {
    /* 静态工厂与实例运算共处同一类型表；callback 是否读取 self 由各方法定义。 */
    /* 16 个成员按 m00..m33 行主序排列，与 callback 的 constructor field-name 和 ReadMatrix4Object 顺序一致。 */
    static const ZrLibFieldDescriptor kFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m00", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m01", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m02", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m03", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m10", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m11", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m12", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m13", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m20", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m21", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m22", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m23", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m30", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m31", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m32", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("m33", "float", ZR_NULL),
    };
    /* 首六项是 static factories，后五项是 instance methods；每行宏记录脚本名、arity、callback、返回类型和 static 标志。 */
    static const ZrLibMethodDescriptor kMethods[] = {
            ZR_LIB_METHOD_DESCRIPTOR_INIT("translation", 3, 3, ZrMath_Matrix4x4_Translation, "Matrix4x4", ZR_NULL,
                                          ZR_TRUE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("scale", 3, 3, ZrMath_Matrix4x4_Scale, "Matrix4x4", ZR_NULL, ZR_TRUE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("rotationX", 1, 1, ZrMath_Matrix4x4_RotationX, "Matrix4x4", ZR_NULL,
                                          ZR_TRUE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("rotationY", 1, 1, ZrMath_Matrix4x4_RotationY, "Matrix4x4", ZR_NULL,
                                          ZR_TRUE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("rotationZ", 1, 1, ZrMath_Matrix4x4_RotationZ, "Matrix4x4", ZR_NULL,
                                          ZR_TRUE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("identity", 0, 0, ZrMath_Matrix4x4_Identity, "Matrix4x4", ZR_NULL,
                                          ZR_TRUE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("transpose", 0, 0, ZrMath_Matrix4x4_Transpose, "Matrix4x4", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("determinant", 0, 0, ZrMath_Matrix4x4_Determinant, "float", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("inverse", 0, 0, ZrMath_Matrix4x4_Inverse, "Matrix4x4", ZR_NULL, ZR_FALSE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("mulVector", 1, 1, ZrMath_Matrix4x4_MulVector, "Vector4", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("mulMatrix", 1, 1, ZrMath_Matrix4x4_MulMatrix, "Matrix4x4", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
    };
    /* constructor 0..16、乘法 1 参数、字符串 0 参数；meta dispatcher 按 kind/name 取 callback。 */
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR,0,16,ZrMath_Matrix4x4_Construct,"Matrix4x4",ZR_NULL,ZR_NULL,0},
            {ZR_META_MUL,1,1,ZrMath_Matrix4x4_MetaMul,"object",ZR_NULL,ZR_NULL,0},
            {ZR_META_TO_STRING,0,0,ZrMath_Matrix4x4_MetaToString,"string",ZR_NULL,ZR_NULL,0},
    };
    /* 描述符只引用本函数内静态表，聚合器借用其地址直到 provider image 卸载。 */
    static const ZrLibTypeDescriptor kType = ZR_LIB_TYPE_DESCRIPTOR_INIT("Matrix4x4",
                                                                         ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                                                         kFields,
                                                                         ZR_ARRAY_COUNT(kFields),
                                                                         kMethods,
                                                                         ZR_ARRAY_COUNT(kMethods),
                                                                         kMeta,
                                                                         ZR_ARRAY_COUNT(kMeta),
                                                                         "4x4 row-major matrix.",
                                                                         ZR_NULL,
                                                                         ZR_NULL,
                                                                         0,
                                                                         ZR_NULL,
                                                                         0,
                                                                         ZR_NULL,
                                                                         ZR_TRUE,
                                                                         ZR_TRUE,
                                                                         "Matrix4x4(...values: float)",
                                                                         ZR_NULL,
                                                                         0);
    return &kType;
}

/**
 * ## Summary
 * 告知模块聚合器本类型没有额外自由函数。
 *
 * ## Parameters
 * @param[out] count 若非空则写入 0；调用方拥有该槽。
 *
 * ## Returns
 * @return 始终返回 ZR_NULL，表示没有自由函数描述符数组。
 *
 * ## Constraints and lifecycle
 * 不分配或转移资源；不修改共享注册表。
 *
 * ## References
 * module.c 的函数注册宏调用；实例/静态方法仍由 GetType 返回的 kMethods 注册。
 */
const ZrLibFunctionDescriptor *ZrMath_Matrix4x4Registry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

/**
 * ## Summary
 * 返回供语言服务展示 Matrix4x4 结构的静态类型提示。
 *
 * ## Parameters
 * @param[out] count 若非空则写入 kHints 的条目数；调用方拥有该槽。
 *
 * ## Returns
 * @return 借用的 const kHints 数组地址；不得修改或释放。
 *
 * ## Constraints and lifecycle
 * 数组有静态存储期且仅在 provider 映像驻留期间有效；getter 不分配或变更内容。
 *
 * ## References
 * module.c 的 hint 聚合宏调用；语言服务消费模块聚合的 TypeHintDescriptor 表。
 */
const ZrLibTypeHintDescriptor *ZrMath_Matrix4x4Registry_GetHints(TZrSize *count) {
    /* 单条类型 hint 给 language service 展示结构名和 row-major 布局。 */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Matrix4x4","type","struct Matrix4x4 { m00..m33 }","4x4 row-major matrix."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
