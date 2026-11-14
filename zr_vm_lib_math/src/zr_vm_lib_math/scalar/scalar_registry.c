/** @file
 * ## Summary
 * 标量函数和提示的只读注册表；模块聚合器借用静态表并交由 native metadata 导出。 */

#include "zr_vm_lib_math/scalar_registry.h"

const ZrLibFunctionDescriptor *ZrMath_ScalarRegistry_GetFunctions(TZrSize *count) {
    /* 返回只读静态描述符；count 与该数组长度同步，调用方借用至模块映像卸载。 */
    static const ZrLibParameterDescriptor kFloatValueParameters[] = {{"value", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kFloatBinaryParameters[] = {{"lhs", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"rhs", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kClampParameters[] = {{"value", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"minimum", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"maximum", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kLerpParameters[] = {{"start", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"end", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"factor", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kAlmostEqualParameters[] = {{"lhs", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"rhs", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"epsilon", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibParameterDescriptor kInvokeCallbackParameters[] = {{"callback", "function", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}, {"value", "float", ZR_NULL, ZR_LIB_PARAMETER_PASSING_MODE_VALUE}};
    static const ZrLibFunctionDescriptor kFunctions[] = { /* 每行以名字、arity、callback、返回标签描述一个导出入口。 */
            {"abs",1,1,ZrMath_Scalar_Abs,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"min",2,2,ZrMath_Scalar_Min,"float",ZR_NULL,kFloatBinaryParameters, ZR_ARRAY_COUNT(kFloatBinaryParameters)},
            {"max",2,2,ZrMath_Scalar_Max,"float",ZR_NULL,kFloatBinaryParameters, ZR_ARRAY_COUNT(kFloatBinaryParameters)},
            {"clamp",3,3,ZrMath_Scalar_Clamp,"float",ZR_NULL,kClampParameters, ZR_ARRAY_COUNT(kClampParameters)},
            {"lerp",3,3,ZrMath_Scalar_Lerp,"float",ZR_NULL,kLerpParameters, ZR_ARRAY_COUNT(kLerpParameters)},
            {"sqrt",1,1,ZrMath_Scalar_Sqrt,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"rsqrt",1,1,ZrMath_Scalar_Rsqrt,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"pow",2,2,ZrMath_Scalar_Pow,"float",ZR_NULL,kFloatBinaryParameters, ZR_ARRAY_COUNT(kFloatBinaryParameters)},
            {"exp",1,1,ZrMath_Scalar_Exp,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"log",1,1,ZrMath_Scalar_Log,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"sin",1,1,ZrMath_Scalar_Sin,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"cos",1,1,ZrMath_Scalar_Cos,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"tan",1,1,ZrMath_Scalar_Tan,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"asin",1,1,ZrMath_Scalar_Asin,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"acos",1,1,ZrMath_Scalar_Acos,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"atan",1,1,ZrMath_Scalar_Atan,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"atan2",2,2,ZrMath_Scalar_Atan2,"float",ZR_NULL,kFloatBinaryParameters, ZR_ARRAY_COUNT(kFloatBinaryParameters)},
            {"floor",1,1,ZrMath_Scalar_Floor,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"ceil",1,1,ZrMath_Scalar_Ceil,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"round",1,1,ZrMath_Scalar_Round,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"sign",1,1,ZrMath_Scalar_Sign,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"degrees",1,1,ZrMath_Scalar_Degrees,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"radians",1,1,ZrMath_Scalar_Radians,"float",ZR_NULL,kFloatValueParameters, ZR_ARRAY_COUNT(kFloatValueParameters)},
            {"almostEqual",2,3,ZrMath_Scalar_AlmostEqual,"bool",ZR_NULL,kAlmostEqualParameters, ZR_ARRAY_COUNT(kAlmostEqualParameters)},
            {"invokeCallback",2,2,ZrMath_InvokeCallback,"float",ZR_NULL,kInvokeCallbackParameters, ZR_ARRAY_COUNT(kInvokeCallbackParameters)}, /* BUG: callback 可成功转交非 float VM 值，标签不作返回值校验。 */
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kFunctions);
    return kFunctions;
}

const ZrLibTypeHintDescriptor *ZrMath_ScalarRegistry_GetHints(TZrSize *count) {
    static const ZrLibTypeHintDescriptor kHints[] = { /* 静态只读提示供模块聚合和 metadata 导出，非运行时校验表。 */
            {"abs","function","abs(value: float): float","Absolute value."},
            {"min","function","min(lhs: float, rhs: float): float","Minimum of two values."},
            {"max","function","max(lhs: float, rhs: float): float","Maximum of two values."},
            {"almostEqual","function","almostEqual(lhs: float, rhs: float, epsilon?: float): bool","Epsilon based comparison."},
            {"invokeCallback","function","invokeCallback(callback: function, value: float): float","Invoke a zr callback from native code."} /* BUG: 返回注解为 float，但运行时原样传播被调 callback 的值。 */
    };
    if (count != ZR_NULL) *count = ZR_ARRAY_COUNT(kHints);
    return kHints;
}

/**
 * @fn ZrMath_ScalarRegistry_GetFunctions
 * ## Summary
 * 将静态函数描述符数组借给 Math 模块聚合器，并可选写回元素数。
 *
 * ## Parameters
 * @param[out] count 可空；非空时须是可写地址，写入同次返回的 `kFunctions` 元素数，仅在 getter 期间借用。
 *
 * ## Returns
 * @return `kFunctions` 的非空首地址；静态只读借用，无分配或所有权转移，调用者不得修改或释放。
 *
 * ## Constraints and lifecycle
 * 数组为函数局部 `static const` 存储，在当前模块映像驻留期间稳定；`count` 只描述同次返回的表。
 * 增删 descriptor 时需同步确认 callback、arity 与返回标签。
 *
 * ## References
 * `module.c` 的函数注册表追加宏读取本 getter，native registry 随后注册 callback。
 */
/**
 * @fn ZrMath_ScalarRegistry_GetHints
 * ## Summary
 * 将类型提示静态数组借给 Math 模块聚合器，供后续 metadata 导出。
 *
 * ## Parameters
 * @param[out] count 可空；非空时须是可写地址，写入同次返回的 `kHints` 元素数，仅在 getter 期间借用。
 *
 * ## Returns
 * @return `kHints` 的非空首地址；静态只读借用，无分配或所有权转移，调用者不得修改或释放。
 *
 * ## Constraints and lifecycle
 * 静态只读表在包含它的模块映像驻留期间有效，不执行 callback，也不替代描述符 arity/type 规则。
 *
 * ## References
 * `module.c` 的 hints 聚合宏调用本 getter；native metadata serializer 读取聚合后的提示。
 */
