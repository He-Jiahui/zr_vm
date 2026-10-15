//
// Scalar math callbacks.
//

#ifndef ZR_VM_LIB_MATH_SCALAR_H
#define ZR_VM_LIB_MATH_SCALAR_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 * ## Summary
 * `zr.math` 标量函数的 native callback 声明。模块注册表把函数指针、参数个数和返回类型交给
 * native binding；脚本调用时由 dispatcher 创建 `ZrLibCallContext` 并提供临时结果 slot。
 * 这些符号是 VM 回调，不是可由普通 C 调用者直接传入数值的数学函数。
 *
 * ## Constraints and lifecycle
 * 回调只在当前 VM native dispatch 中使用。`context`、其中的参数和 `result` 均为调用期借用，
 * 回调不取得其所有权，也不能保存或跨线程使用。`ReadFloat` 接受 VM 数值标签并转换为
 * `TZrFloat64`；缺参或错误标签走 VM 的 arity/type error 路径。具体每个函数的参数数目由
 * `scalar_registry.c` 描述符在派发前限制。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 将这些回调注册到 `zr.math`；native callback 的间接派发
 * 由 `ZrLib_CallValue` / native binding dispatcher 完成。
 */
/**
 * ## Summary
 * 计算单个数值的绝对值，供 `zr.math.abs` 的 native callback 使用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发提供的借用结果槽；成功时写入 `float` VM 值，不转移槽位所有权。
 *
 * ## Returns
 * @return 成功写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求恰好 1 个参数。运算委托给 `ZrMath_AbsFloat`；本 callback 不保留参数或结果槽。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `abs` 描述符；`ZrMath_AbsFloat`。
 */
TZrBool ZrMath_Scalar_Abs(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按当前二元条件表达式选择较小值，供 `zr.math.min` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；依次读取 `lhs`、`rhs` 两个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 成功写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 2 个参数。选择表达式为 `lhs < rhs ? lhs : rhs`，不额外规范化 NaN 或无穷值。
 * `context` 与 `result` 仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `min` 描述符；`ZR_MATH_BINARY`。
 */
TZrBool ZrMath_Scalar_Min(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按当前二元条件表达式选择较大值，供 `zr.math.max` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；依次读取 `lhs`、`rhs` 两个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 成功写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 2 个参数。选择表达式为 `lhs > rhs ? lhs : rhs`，不额外规范化 NaN 或无穷值。
 * `context` 与 `result` 仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `max` 描述符；`ZR_MATH_BINARY`。
 */
TZrBool ZrMath_Scalar_Max(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按先下界、再上界的顺序限制输入值，供 `zr.math.clamp` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；按顺序读取 `value`、`minimum`、`maximum`。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 三个数值读取并完成计算时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 3 个参数。当前实现不重排边界，依次执行 `value < minimum` 与 `value > maximum`。
 * TODO: `minimum > maximum` 时应拒绝、交换还是保留当前顺序尚无公开政策；核对 API 预期并补边界测试。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `clamp` 描述符；`scalar.c` 的 Clamp TODO。
 */
TZrBool ZrMath_Scalar_Clamp(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 用线性插值公式从起点和终点按因子计算结果，供 `zr.math.lerp` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；按顺序读取 `start`、`end`、`factor`。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 三个数值读取并计算后返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 3 个参数，计算式为 `start + (end - start) * factor`，不先检查端点或因子是否有限。
 * TODO: 有限端点 `-1e308`、`1e308` 与 `factor=0.5` 会使中间差值溢出；是否需要避免该数值路径尚待确认，核对稳定性政策及数值用例。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `lerp` 描述符；`scalar.c` 的 Lerp 实现。
 */
TZrBool ZrMath_Scalar_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 计算数值平方根，供 `zr.math.sqrt` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 完成 C 数学库运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；直接调用 C `sqrt`，不在 callback 中检查负数、NaN 或无穷值。
 * 参数和结果槽只在本次 callback 期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `sqrt` 描述符；C 数学函数 `sqrt`。
 */
TZrBool ZrMath_Scalar_Sqrt(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 计算平方根的倒数，供 `zr.math.rsqrt` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 完成 `1.0 / sqrt(value)` 并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；不预先验证输入域或除法结果，边界值沿 C 数学运算处理。
 * `context` 与 `result` 仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `rsqrt` 描述符；C 数学函数 `sqrt`。
 */
TZrBool ZrMath_Scalar_Rsqrt(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 以第一个数值为底、第二个数值为指数求幂，供 `zr.math.pow` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；依次读取底数 `lhs` 与指数 `rhs`。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 完成 C `pow(lhs, rhs)` 并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 2 个参数；不在 callback 中另行限制数学域、溢出或非有限结果。
 * 参数与结果槽仅在本次派发期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `pow` 描述符；C 数学函数 `pow`。
 */
TZrBool ZrMath_Scalar_Pow(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 计算自然指数 `exp(value)`，供 `zr.math.exp` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return C 数学运算完成并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；直接调用 C `exp`，callback 不另行检查范围或有限性。
 * `context` 与结果槽不跨本次调用保存。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `exp` 描述符；C 数学函数 `exp`。
 */
TZrBool ZrMath_Scalar_Exp(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 计算自然对数 `log(value)`，供 `zr.math.log` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return C 数学运算完成并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；直接调用 C `log`，callback 不另行检查定义域或有限性。
 * 参数和结果槽仅在本次派发期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `log` 描述符；C 数学函数 `log`。
 */
TZrBool ZrMath_Scalar_Log(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将以弧度表示的值传给 C `sin`，供 `zr.math.sin` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数，角度单位为弧度；不在 callback 中归一化输入。
 * `context` 和 `result` 仅在本次派发期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `sin` 描述符；C 数学函数 `sin`。
 */
TZrBool ZrMath_Scalar_Sin(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将以弧度表示的值传给 C `cos`，供 `zr.math.cos` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数，角度单位为弧度；不在 callback 中归一化输入。
 * `context` 和 `result` 仅在本次派发期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `cos` 描述符；C 数学函数 `cos`。
 */
TZrBool ZrMath_Scalar_Cos(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将以弧度表示的值传给 C `tan`，供 `zr.math.tan` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数，角度单位为弧度；callback 不检测渐近线或归一化输入。
 * `context` 和 `result` 仅在本次派发期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `tan` 描述符；C 数学函数 `tan`。
 */
TZrBool ZrMath_Scalar_Tan(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将无量纲数值传给 C `asin`，供 `zr.math.asin` 调用并返回弧度。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；输入范围不在 callback 中预先裁剪，返回值单位为弧度。
 * `context` 和结果槽仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `asin` 描述符；C 数学函数 `asin`。
 */
TZrBool ZrMath_Scalar_Asin(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将无量纲数值传给 C `acos`，供 `zr.math.acos` 调用并返回弧度。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；输入范围不在 callback 中预先裁剪，返回值单位为弧度。
 * `context` 和结果槽仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `acos` 描述符；C 数学函数 `acos`。
 */
TZrBool ZrMath_Scalar_Acos(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将无量纲数值传给 C `atan`，供 `zr.math.atan` 调用并返回弧度。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；返回值单位为弧度。输入和结果不跨本次 callback 保存。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `atan` 描述符；C 数学函数 `atan`。
 */
TZrBool ZrMath_Scalar_Atan(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按 C `atan2(lhs, rhs)` 顺序求角度，供 `zr.math.atan2` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；第 0 个数值作为 `lhs`，第 1 个作为 `rhs`。
 * @param[out] result 本次派发的借用结果槽；成功时写入弧度 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 2 个参数；两个无量纲分量按原顺序传给 `atan2`，不交换参数；结果为弧度。
 * 参数和结果槽仅在本次派发期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `atan2` 描述符；C 数学函数 `atan2`。
 */
TZrBool ZrMath_Scalar_Atan2(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将数值传给 C `floor` 向负无穷取整，供 `zr.math.floor` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；使用 C `floor`，结果仍以 VM float 形式交付。
 * `context` 与 `result` 仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `floor` 描述符；C 数学函数 `floor`。
 */
TZrBool ZrMath_Scalar_Floor(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将数值传给 C `ceil` 向正无穷取整，供 `zr.math.ceil` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；使用 C `ceil`，结果仍以 VM float 形式交付。
 * `context` 与 `result` 仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `ceil` 描述符；C 数学函数 `ceil`。
 */
TZrBool ZrMath_Scalar_Ceil(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将数值传给 C `round` 最近整数取整，供 `zr.math.round` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `float` VM 值。
 *
 * ## Returns
 * @return 运算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；舍入由 C `round` 定义，callback 不转换成整数 VM 标签。
 * 参数与结果槽仅在本次派发期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `round` 描述符；C 数学函数 `round`。
 */
TZrBool ZrMath_Scalar_Round(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按负、正、否则三个比较分支返回符号值，供 `zr.math.sign` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个数值参数。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `-1.0`、`0.0` 或 `1.0`。
 *
 * ## Returns
 * @return 写入符号值后返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；实现使用 `<0` 与 `>0` 两个比较，因此零及比较结果均为假时走 `0.0` 分支。
 * `context` 和 `result` 仅在本次派发期间借用。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `sign` 描述符；`ZR_MATH_UNARY`。
 */
TZrBool ZrMath_Scalar_Sign(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按 `value * (180 / pi)` 将弧度换算为角度，供 `zr.math.degrees` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个弧度数值。
 * @param[out] result 本次派发的借用结果槽；成功时写入角度 `float` VM 值。
 *
 * ## Returns
 * @return 换算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；使用 `ZR_MATH_PI` 的直接乘法公式，不作输入归一化。
 * 参数与结果槽仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `degrees` 描述符；`ZR_MATH_PI`。
 */
TZrBool ZrMath_Scalar_Degrees(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按 `value * (pi / 180)` 将角度换算为弧度，供 `zr.math.radians` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取第 0 个角度数值。
 * @param[out] result 本次派发的借用结果槽；成功时写入弧度 `float` VM 值。
 *
 * ## Returns
 * @return 换算并写入结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符要求 1 个参数；使用 `ZR_MATH_PI` 的直接乘法公式，不作输入归一化。
 * 参数与结果槽仅在本次派发期间有效。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `radians` 描述符；`ZR_MATH_PI`。
 */
TZrBool ZrMath_Scalar_Radians(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 使用绝对误差比较两个数值，供 `zr.math.almostEqual` 调用。
 *
 * ## Parameters
 * @param[in] context 本次 callback 的借用上下文；读取 `lhs`、`rhs`，第三个 `epsilon` 可选。
 * @param[out] result 本次派发的借用结果槽；成功时写入 `bool` VM 值。
 *
 * ## Returns
 * @return 完成比较并写入布尔结果时返回 `ZR_TRUE`；缺参或错类型由 VM 抛异常，不作普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 描述符接受 2 或 3 个参数；省略 `epsilon` 时使用 `ZR_MATH_EPSILON`，显式传入时直接用于
 * `ZrMath_AlmostEqual`。TODO: 当前 helper 使用绝对阈值；相对阈值是否属于公开比较契约待核，
 * 下一步需确认跨数量级预期并补边界测试。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 中的 `almostEqual` 描述符；`ZrMath_AlmostEqual`。
 */
TZrBool ZrMath_Scalar_AlmostEqual(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 在 native callback 中以一个数值参数嵌套调用 VM callable，并把该次调用结果传回当前派发。
 *
 * ## Parameters
 * @param[in] context 本次 native callback 的借用上下文；读取 callable 与一个数值参数。
 * @param[out] result 当前 native 派发的借用结果槽；成功时由嵌套调用填入 VM 返回值，不取得其所有权。
 *
 * ## Returns
 * @return `ZrLib_CallValue` 成功时返回 `ZR_TRUE` 并传播被调 callable 的实际 VM 值；嵌套调用的普通失败返回 `ZR_FALSE`，缺参或错类型由 VM 抛异常。
 *
 * ## Constraints and lifecycle
 * 描述符声明 2 个参数和 `float` 返回类型，但嵌套 `CallValue` 原样转交被调函数结果而不按该类型名校验。
 * BUG: 因此返回 `bool`、字符串或其他 VM 标签的 callable 仍可成功通过；注册元数据与实际结果类型不一致。
 * callable、参数及两个结果槽只在各自派发期间借用；该嵌套调用路径不支持 yield。 TODO: ReadFunction 也接受 NATIVE_POINTER；须核实调用者保证非空、兼容 FZrNativeFunction ABI 且代码映像在调用期间有效的入口约束，见 core function.c:4273 与 native_binding.h:630。
 *
 * ## References
 * `ZrMath_ScalarRegistry_GetFunctions` 的 `invokeCallback` 描述符；`ZrLib_CallContext_ReadFunction`；
 * `ZrLib_CallValue`；`native_binding_dispatch_lanes.h` 的 callback 派发路径。
 */
TZrBool ZrMath_InvokeCallback(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_SCALAR_H
