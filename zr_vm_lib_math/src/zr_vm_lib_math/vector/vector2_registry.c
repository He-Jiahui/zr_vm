//
// Vector2 descriptor registry.
//

#include "zr_vm_lib_math/vector2_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Vector2Registry_GetType(void) {
    /* 这些静态表由模块聚合器借用；type 的 methods/meta 指针必须与 provider image 同寿命。 */
    /* 字段偏移表只声明 x/y 的浮点可写字段；StructPrototype 另行物化运行时 key 映射。 */
    static const ZrLibFieldDescriptor kFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("x", "float", ZR_NULL),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("y", "float", ZR_NULL),
    };
    /* descriptor 固定 arity 在运行时 callback 前由 dispatcher 校验；binary methods 接收对象 rhs。 */
    static const ZrLibMethodDescriptor kMethods[] = {
            ZR_LIB_METHOD_DESCRIPTOR_INIT("length", 0, 0, ZrMath_Vector2_Length, "float", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("lengthSquared", 0, 0, ZrMath_Vector2_LengthSquared, "float", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("normalized", 0, 0, ZrMath_Vector2_Normalized, "Vector2", ZR_NULL,
                                          ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("dot", 1, 1, ZrMath_Vector2_Dot, "float", ZR_NULL, ZR_FALSE, ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("distance", 1, 1, ZrMath_Vector2_Distance, "float", ZR_NULL, ZR_FALSE,
                                          ZR_NULL, 0),
            ZR_LIB_METHOD_DESCRIPTOR_INIT("lerp", 2, 2, ZrMath_Vector2_Lerp, "Vector2", ZR_NULL, ZR_FALSE, ZR_NULL,
                                          0),
    };
    /* meta dispatcher 运行时按下方 descriptor 的固定 arity 预检，再派发 callback。 */
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR, 2, 2, ZrMath_Vector2_Construct, "Vector2", ZR_NULL, ZR_NULL, 0},
            {ZR_META_ADD, 1, 1, ZrMath_Vector2_MetaAdd, "Vector2", ZR_NULL, ZR_NULL, 0},
            {ZR_META_SUB, 1, 1, ZrMath_Vector2_MetaSub, "Vector2", ZR_NULL, ZR_NULL, 0},
            {ZR_META_NEG, 0, 0, ZrMath_Vector2_MetaNeg, "Vector2", ZR_NULL, ZR_NULL, 0},
            {ZR_META_COMPARE, 1, 1, ZrMath_Vector2_MetaCompare, "int", ZR_NULL, ZR_NULL, 0},
            {ZR_META_TO_STRING, 0, 0, ZrMath_Vector2_MetaToString, "string", ZR_NULL, ZR_NULL, 0},
    };
    /* 默认 descriptor 的 protocolMask 为零；该 struct 不是 REF_LIKE。fast lane 只浅复制 self 值槽，
     * 并不 pin 对象；需保活的其他 lane 按自身协议处理临时根。 */
    static const ZrLibTypeDescriptor kType = ZR_LIB_TYPE_DESCRIPTOR_INIT("Vector2",
                                                                         ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
                                                                         kFields,
                                                                         ZR_ARRAY_COUNT(kFields),
                                                                         kMethods,
                                                                         ZR_ARRAY_COUNT(kMethods),
                                                                         kMeta,
                                                                         ZR_ARRAY_COUNT(kMeta),
                                                                         "2D vector value type.",
                                                                         ZR_NULL,
                                                                         ZR_NULL,
                                                                         0,
                                                                         ZR_NULL,
                                                                         0,
                                                                         ZR_NULL,
                                                                         ZR_TRUE,
                                                                         ZR_TRUE,
                                                                         "Vector2(x: float, y: float)",
                                                                         ZR_NULL,
                                                                         0);
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_Vector2Registry_GetFunctions(TZrSize *count) {
    /* Vector2 callable surface 全部在 kType 中；保持空 module-function 列表并显式回报零项。 */
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_Vector2Registry_GetHints(TZrSize *count) {
    /* hint 表与 type 表分别借用，count 可省略但非空时必须与返回静态数组相符。 */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Vector2", "type", "struct Vector2 { x: float, y: float }", "2D vector value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
