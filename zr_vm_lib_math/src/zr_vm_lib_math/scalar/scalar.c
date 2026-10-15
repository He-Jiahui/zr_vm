/** @file
 * ## Summary
 * 标量 native callback 实现与宏模板；注册表描述符决定脚本入口的 arity 和结果标签。 */

#include "zr_vm_lib_math/scalar.h"

/* 同构回调由 registry 按精确 arity 派发；binding 负责取数值参数，本地宏只做计算并写结果。 */
#define ZR_MATH_UNARY(NAME, EXPR) \
    TZrBool NAME(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 value = 0.0; \
        if (!ZrLib_CallContext_ReadFloat(context, 0, &value)) return ZR_FALSE; \
        ZrLib_Value_SetFloat(context->state, result, (EXPR)); return ZR_TRUE; }
#define ZR_MATH_BINARY(NAME, EXPR) \
    TZrBool NAME(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 lhs = 0.0, rhs = 0.0; \
        if (!ZrLib_CallContext_ReadFloat(context, 0, &lhs) || !ZrLib_CallContext_ReadFloat(context, 1, &rhs)) return ZR_FALSE; \
        ZrLib_Value_SetFloat(context->state, result, (EXPR)); return ZR_TRUE; }

ZR_MATH_UNARY(ZrMath_Scalar_Abs, ZrMath_AbsFloat(value))
ZR_MATH_BINARY(ZrMath_Scalar_Min, lhs < rhs ? lhs : rhs)
ZR_MATH_BINARY(ZrMath_Scalar_Max, lhs > rhs ? lhs : rhs)
ZR_MATH_UNARY(ZrMath_Scalar_Sqrt, sqrt(value))
ZR_MATH_UNARY(ZrMath_Scalar_Rsqrt, 1.0 / sqrt(value))
ZR_MATH_BINARY(ZrMath_Scalar_Pow, pow(lhs, rhs))
ZR_MATH_UNARY(ZrMath_Scalar_Exp, exp(value))
ZR_MATH_UNARY(ZrMath_Scalar_Log, log(value))
ZR_MATH_UNARY(ZrMath_Scalar_Sin, sin(value))
ZR_MATH_UNARY(ZrMath_Scalar_Cos, cos(value))
ZR_MATH_UNARY(ZrMath_Scalar_Tan, tan(value))
ZR_MATH_UNARY(ZrMath_Scalar_Asin, asin(value))
ZR_MATH_UNARY(ZrMath_Scalar_Acos, acos(value))
ZR_MATH_UNARY(ZrMath_Scalar_Atan, atan(value))
ZR_MATH_BINARY(ZrMath_Scalar_Atan2, atan2(lhs, rhs))
ZR_MATH_UNARY(ZrMath_Scalar_Floor, floor(value))
ZR_MATH_UNARY(ZrMath_Scalar_Ceil, ceil(value))
ZR_MATH_UNARY(ZrMath_Scalar_Round, round(value))
ZR_MATH_UNARY(ZrMath_Scalar_Sign, value < 0.0 ? -1.0 : (value > 0.0 ? 1.0 : 0.0))
ZR_MATH_UNARY(ZrMath_Scalar_Degrees, value * (180.0 / ZR_MATH_PI))
ZR_MATH_UNARY(ZrMath_Scalar_Radians, value * (ZR_MATH_PI / 180.0))

TZrBool ZrMath_Scalar_Clamp(ZrLibCallContext *context, SZrTypeValue *result) {
    /* TODO: 当前回调保持传入的边界顺序；核对 low > high 是否允许，以及此时
     * 应拒绝、交换还是维持现状的公开契约，再补相应边界测试。 */
    TZrFloat64 value = 0.0, minimum = 0.0, maximum = 0.0;
    if (!ZrLib_CallContext_ReadFloat(context, 0, &value) || !ZrLib_CallContext_ReadFloat(context, 1, &minimum) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &maximum)) return ZR_FALSE;
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    ZrLib_Value_SetFloat(context->state, result, value); return ZR_TRUE;
}
TZrBool ZrMath_Scalar_Lerp(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 start = 0.0, end = 0.0, factor = 0.0;
    if (!ZrLib_CallContext_ReadFloat(context, 0, &start) || !ZrLib_CallContext_ReadFloat(context, 1, &end) ||
        !ZrLib_CallContext_ReadFloat(context, 2, &factor)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, start + (end - start) * factor); return ZR_TRUE;
}
TZrBool ZrMath_Scalar_AlmostEqual(ZrLibCallContext *context, SZrTypeValue *result) {
    /* TODO: ZrMath_AlmostEqual 只比较绝对误差；核对大/小量级调用是否需要相对阈值，
     * 再明确公开比较契约及边界测试。 */
    TZrFloat64 lhs = 0.0, rhs = 0.0, epsilon = ZR_MATH_EPSILON;
    if (!ZrLib_CallContext_ReadFloat(context, 0, &lhs) || !ZrLib_CallContext_ReadFloat(context, 1, &rhs)) return ZR_FALSE;
    if (ZrLib_CallContext_ArgumentCount(context) >= 3 && !ZrLib_CallContext_ReadFloat(context, 2, &epsilon)) return ZR_FALSE;
    ZrLib_Value_SetBool(context->state, result, ZrMath_AlmostEqual(lhs, rhs, epsilon)); return ZR_TRUE;
}
TZrBool ZrMath_InvokeCallback(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *callable = ZR_NULL; TZrFloat64 value = 0.0; SZrTypeValue argument;
    if (!ZrLib_CallContext_ReadFunction(context, 0, &callable) || !ZrLib_CallContext_ReadFloat(context, 1, &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, &argument, value);
    return ZrLib_CallValue(context->state, callable, ZR_NULL, &argument, 1, result); /* BUG: 描述符写明 float，但 CallValue 会原样转交成功结果的 VM 标签。 */
}

/**
 * @def ZR_MATH_UNARY(NAME, EXPR)
 * ## Summary
 * 为一个 unary scalar operation 生成 native callback。binding 已按 registry arity 派发后，
 * 宏读取第 0 个数值并求值 `EXPR`，再将结果写入本次 callback 的结果槽。
 *
 * ## Parameters
 * @param[in] NAME 生成的 callback symbol；只能绑定本文件中的函数名。
 * @param[in] EXPR 使用局部 `value` 的计算表达式，不得保存 context 或输出槽。
 *
 * ## Returns
 * @return 有效派发中读取、写值完成返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，代码仅保留 helper 普通 false 的防御性传播。
 *
 * ## Constraints and lifecycle
 * `context`、`value` 和 `result` 仅在当前 callback 使用；缺参/错误标签由 ReadFloat 的 VM 错误路径处理。
 * registry 必须为每个展开项给出一个参数的 arity。
 *
 * ## References
 * `ZR_MATH_BINARY` 生成二元版本；公开 callback 契约位于 `scalar.h`。
 */
/**
 * @def ZR_MATH_BINARY(NAME, EXPR)
 * ## Summary
 * 为一个 binary scalar operation 生成 native callback，依次读取两个位置参数并将 `EXPR` 结果写回。
 *
 * ## Parameters
 * @param[in] NAME 生成的 callback symbol；只能绑定本文件中的函数名。
 * @param[in] EXPR 使用局部 `lhs`、`rhs` 的计算表达式，不拥有 context 参数或结果槽。
 *
 * ## Returns
 * @return 有效派发中读取、写值完成返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，代码仅保留 helper 普通 false 的防御性传播。
 *
 * ## Constraints and lifecycle
 * 两个数值快照及 callback 结果槽只在当前派发期间有效；registry 必须声明两个位置参数。
 * 宏不检查运算域，也不对 `EXPR` 的数值结果做后处理。
 *
 * ## References
 * `ZR_MATH_UNARY` 生成一元版本；展开点及 descriptor 位于 `scalar.c` 和 `scalar_registry.c`。
 */
/**
 * @fn ZrMath_Scalar_Lerp
 * ## Summary
 * 实现 `zr.math.lerp` 的位置参数读取和线性插值，并把公式结果写到 VM 结果槽。
 *
 * ## Parameters
 * @param[in] context 当前派发的借用上下文，提供数值参数 start、end、factor。
 * @param[out] result dispatcher 借出的输出槽；完成时写入 float VM 值，不取得槽位所有权。
 *
 * ## Returns
 * @return 有效数值参数读取并计算后写入 float、返回 `ZR_TRUE`；缺参或错类型走 VM 异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 公式为 `start + (end - start) * factor`，中间差值可能对有限端点溢出。
 * TODO: 是否需要稳定化该公式尚待确认；具体极端有限输入见公开声明。
 * context 与输出槽不得保存到 callback 结束之后。
 *
 * ## References
 * `scalar.h` 中 `ZrMath_Scalar_Lerp` 的公开契约和 `ZrMath_ScalarRegistry_GetFunctions` 的 descriptor。
 */
/**
 * @fn ZrMath_InvokeCallback
 * ## Summary
 * 读取 callback 和浮点实参后，将调用委托给 `ZrLib_CallValue`。
 *
 * ## Parameters
 * @param[in] context 借用上下文；第 0 项为 ReadFunction 接受的 callable，第 1 项为 ReadFloat 接受的数值标签。
 * @param[out] result dispatcher 借出的结果槽，接收嵌套调用的实际 VM 返回值，不转移槽位所有权。
 *
 * ## Returns
 * @return 返回 `ZrLib_CallValue` 的普通成功/失败状态；成功时保留被调 callable 的 VM 标签。缺参或错误标签由 ReadFunction/ReadFloat 抛 VM 异常。
 *
 * ## Constraints and lifecycle
 * BUG: registry descriptor 与 type hint 声明 float，但 `ZrLib_CallValue` 不按 returnTypeName 校验，
 * 故成功回调可交付其他 VM 类型。参数、callable 和结果槽均是各自调用期间借用；本路径不支持 yield。
 * TODO: NATIVE_POINTER 仅按标签通过 ReadFunction；由谁保证非空、FZrNativeFunction ABI 与代码映像有效期，须核对 core function.c:4273 和 C API SetNativePointer 调用方。
 *
 * ## References
 * `scalar_registry.c` 的 `invokeCallback` descriptor/hint；`ZrLib_CallContext_ReadFunction` 和 `ZrLib_CallValue`。
 */
