/** @file
 * ## Summary
 * 静态发布 Complex 类型、命名方法、元方法和类型形状元数据。
 *
 * ## Constraints and lifecycle
 * 描述符及嵌套表在本模块映像驻留期间借用；getter 不创建 VM 实例。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/module.c:117、zr_vm_lib_math/src/zr_vm_lib_math/module.c:129 与 zr_vm_lib_math/src/zr_vm_lib_math/module.c:142 分别聚合函数、类型与形状元数据。
 */

#include "zr_vm_lib_math/complex_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_ComplexRegistry_GetType(void) { /* 返回模块映像中的静态数据，不保留调用状态。 */
    /* 返回本模块映像内的静态描述符；其地址和嵌套表只在映像驻留期间有效。 */
    static const ZrLibFieldDescriptor kFields[] = { /* 静态字段表；顺序对应 Complex 对象的 real、imag 分量。 */
            ZR_LIB_FIELD_DESCRIPTOR_INIT("real", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("imag", "float", ZR_NULL),
    };
    static const ZrLibMethodDescriptor kMethods[] = { /* arity 只计显式实参，不含 receiver；零参回调仍读取 self 字段。 */
            ZR_LIB_METHOD_DESCRIPTOR_INIT("magnitude", 0, 0, ZrMath_Complex_Magnitude, "float", ZR_NULL, ZR_FALSE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("phase", 0, 0, ZrMath_Complex_Phase, "float", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("conjugate", 0, 0, ZrMath_Complex_Conjugate, "Complex", ZR_NULL, ZR_FALSE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("normalized", 0, 0, ZrMath_Complex_Normalized, "Complex", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = { /* arity 只计显式操作数，不含 self；标签与字段校验由回调执行。 */
            {ZR_META_CONSTRUCTOR,2,2,ZrMath_Complex_Construct,"Complex",ZR_NULL,ZR_NULL,0},
            {ZR_META_ADD,1,1,ZrMath_Complex_MetaAdd,"Complex",ZR_NULL,ZR_NULL,0},
            {ZR_META_SUB,1,1,ZrMath_Complex_MetaSub,"Complex",ZR_NULL,ZR_NULL,0},
            {ZR_META_MUL,1,1,ZrMath_Complex_MetaMul,"Complex",ZR_NULL,ZR_NULL,0},
            {ZR_META_NEG,0,0,ZrMath_Complex_MetaNeg,"Complex",ZR_NULL,ZR_NULL,0},
            {ZR_META_COMPARE,1,1,ZrMath_Complex_MetaCompare,"int",ZR_NULL,ZR_NULL,0},
            {ZR_META_TO_STRING,0,0,ZrMath_Complex_MetaToString,"string",ZR_NULL,ZR_NULL,0},
    };
    static const ZrLibTypeDescriptor kType = ZR_LIB_TYPE_DESCRIPTOR_INIT( /* 聚合静态嵌套表，不复制或拥有 VM 值。 */
            "Complex",
            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
            kFields,
            ZR_ARRAY_COUNT(kFields),
            kMethods,
            ZR_ARRAY_COUNT(kMethods),
            kMeta,
            ZR_ARRAY_COUNT(kMeta),
            "Complex number value type.",
            ZR_NULL,
            ZR_NULL,
            0,
            ZR_NULL,
            0,
            ZR_NULL,
            ZR_TRUE,
            ZR_TRUE,
            "Complex(real: float, imag: float)",
            ZR_NULL,
            0);
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_ComplexRegistry_GetFunctions(TZrSize *count) { /* 模块函数表为空。 */
    /* 实例方法随 kType 注册，不属于模块级函数表。 */
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_ComplexRegistry_GetHints(TZrSize *count) { /* 向聚合器返回静态类型形状元数据。 */
    static const ZrLibTypeHintDescriptor kHints[] = { /* 静态元数据，不是运行时字段值检查。 */
            {"Complex","type","struct Complex { real: float, imag: float }","Complex number value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
