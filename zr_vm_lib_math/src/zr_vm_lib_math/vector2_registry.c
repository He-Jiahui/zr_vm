//
// Vector2 descriptor registry.
//

#include "zr_vm_lib_math/vector2_registry.h"
#include "zr_vm_common/zr_meta_conf.h"

const ZrLibTypeDescriptor *ZrMath_Vector2Registry_GetType(void) {
    static const ZrLibFieldDescriptor kFields[] = {{"x", "float", ZR_NULL}, {"y", "float", ZR_NULL}};
    static const ZrLibMethodDescriptor kMethods[] = {
            {"length", 0, 0, ZrMath_Vector2_Length, "float", ZR_NULL, ZR_FALSE},
            {"lengthSquared", 0, 0, ZrMath_Vector2_LengthSquared, "float", ZR_NULL, ZR_FALSE},
            {"normalized", 0, 0, ZrMath_Vector2_Normalized, "Vector2", ZR_NULL, ZR_FALSE},
            {"dot", 1, 1, ZrMath_Vector2_Dot, "float", ZR_NULL, ZR_FALSE},
            {"distance", 1, 1, ZrMath_Vector2_Distance, "float", ZR_NULL, ZR_FALSE},
            {"lerp", 2, 2, ZrMath_Vector2_Lerp, "Vector2", ZR_NULL, ZR_FALSE},
    };
    static const ZrLibMetaMethodDescriptor kMeta[] = {
            {ZR_META_CONSTRUCTOR, 2, 2, ZrMath_Vector2_Construct, "Vector2", ZR_NULL},
            {ZR_META_ADD, 1, 1, ZrMath_Vector2_MetaAdd, "Vector2", ZR_NULL},
            {ZR_META_SUB, 1, 1, ZrMath_Vector2_MetaSub, "Vector2", ZR_NULL},
            {ZR_META_NEG, 0, 0, ZrMath_Vector2_MetaNeg, "Vector2", ZR_NULL},
            {ZR_META_COMPARE, 1, 1, ZrMath_Vector2_MetaCompare, "int", ZR_NULL},
            {ZR_META_TO_STRING, 0, 0, ZrMath_Vector2_MetaToString, "string", ZR_NULL},
    };
    static const ZrLibTypeDescriptor kType = {
            "Vector2", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
            kFields, ZR_ARRAY_COUNT(kFields),
            kMethods, ZR_ARRAY_COUNT(kMethods),
            kMeta, ZR_ARRAY_COUNT(kMeta),
            "2D vector value type.",
            ZR_NULL,
            ZR_NULL,
            0,
            ZR_NULL,
            0,
            ZR_NULL,
            ZR_TRUE,
            ZR_TRUE,
            "Vector2(x: float, y: float)"
    };
    return &kType;
}

const ZrLibFunctionDescriptor *ZrMath_Vector2Registry_GetFunctions(TZrSize *count) {
    if (count != ZR_NULL) *count = 0;
    return ZR_NULL;
}

const ZrLibTypeHintDescriptor *ZrMath_Vector2Registry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"Vector2", "type", "struct Vector2 { x: float, y: float }", "2D vector value type."}
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}
