//
// Matrix3x3 descriptor registry.
//

#include "zr_vm_lib_math/matrix3x3_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

/** @file
 * ## Summary
 * 定义 Matrix3x3 的静态 descriptor 与 type hint，并实现供 math 模块聚合器调用的访问函数。
 *
 * ## Constraints and lifecycle
 * 所有表均为 `static const` 存储；getter 返回只读借用指针，指针有效期不超过包含这些表的 provider 映像加载期。
 * callback 以函数指针注册，之后由 native dispatcher 按 descriptor 动态派发。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 汇总 getter；`matrix3x3.h` 声明被注册的 callback。
 */

const ZrLibTypeDescriptor *ZrMath_Matrix3x3Registry_GetType(void) {
    /* 九个字段顺序必须与 `matrix3x3.c` 构造器名称表及 row-major 快照一致。 */
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
    /* 实例/静态标志、arity 和返回类型由 VM 元数据消费者读取；identity 是静态方法，其余为实例方法。 */
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
    /* constructor callback 实际只接受 0 或 9；descriptor 的 0..9 区间会把 1..8 也送入 callback。 */
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR, 0, 9, ZrMath_Matrix3x3_Construct, "Matrix3x3", ZR_NULL, ZR_NULL, 0},
            {ZR_META_MUL, 1, 1, ZrMath_Matrix3x3_MetaMul, "object", ZR_NULL, ZR_NULL, 0},
            {ZR_META_TO_STRING, 0, 0, ZrMath_Matrix3x3_MetaToString, "string", ZR_NULL, ZR_NULL, 0},
    };
    /* 聚合 descriptor 与其三张表均为静态只读对象，返回值不能释放或改写。 */
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
    /* 提示表与 getter 返回值均为静态借用；count 由调用方选择性接收。 */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Matrix3x3", "type", "struct Matrix3x3 { m00..m22 }", "3x3 row-major matrix."}
    };
    if (count != ZR_NULL) {
        *count = ZR_ARRAY_COUNT(kHints);
    }
    return kHints;
}
