//
// Quaternion native callbacks.
//

#ifndef ZR_VM_LIB_MATH_QUATERNION_H
#define ZR_VM_LIB_MATH_QUATERNION_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Quaternion 的 VM 原生回调接口。描述符在模块注册时把下列函数绑定到
 *  构造、实例方法和元方法；回调只在 native dispatch 调用期间借用 context，
 *  结果通过调用方提供的 VM 值槽交付。字段读取按 x/y/z/w 名称和数值类型进行，
 *  不以 prototype 名称验证接收者，也不统一要求单位范数。
 *
 *  ## Constraints and lifecycle
 *  context、其中的 self/参数视图及 result 均由 native dispatch 管理，回调不得
 *  保存其地址；调用结束后不得再访问借用视图，应通过 CallContext 接口读取参数。新对象由 math helper
 *  创建并交给 VM 结果槽，不由调用方释放。精确失败状态仍须遵循各函数说明。
 *  BUG: 新实例的第四个字段扩桶失败且无 VM 异常时，void setter 不反馈失败，factory
 *  可使回调报告成功却缺少 w；适用于下列构造和创建新对象的运算，完整证据见审查台账。
 *
 *  ## References
 *  ZrMath_QuaternionRegistry_GetType 发布这些回调；math API 概述见
 *  docs/wiki/03-modules/math-api.md。
 */

/**
 * ## Summary
 *  为 Quaternion 的构造描述符读取四个分量，并将其写入构造目标；目标由通用
 *  math 构造 helper 解析，因此派生类型的构造回调可以复用既有实例。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；参数 0..3 依次是 x、y、z、w，
 *                    每个值须可由 ReadFloat 转换为 double。
 * @param[out] result VM 调用结果槽；成功时接收构造目标对象。
 *
 * ## Returns
 * @return 构造 helper 报告成功并设置结果槽时为 ZR_TRUE；目标创建或 helper 的前提检查
 *         失败时为 ZR_FALSE。缺参或参数非数值会进入 VM 错误，不以普通 false 返回。
 *         BUG: 新对象第四个字段的原生扩桶失败且无 VM 异常时，helper 可报告成功但缺少 w。
 *
 * ## Constraints and lifecycle
 *  描述符固定需要四个参数；分量不做有限值或单位范数校验。context 与 result
 *  仅在本次回调期间有效；对象生命周期交由 VM 管理。common.c 构造路径经 void 字段
 *  setter 填值；新对象按 x/y/z/w 填充时，第四项扩桶失败可留下缺字段实例仍报告成功。
 *  已有兼容 self 可以复用；上述缺字段条件限定到从空 nodeMap 开始的填充路径。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 constructor 元方法；
 *  ZrMath_ConstructFloatObject；ZrLib_CallContext_ReadFloat。
 */
TZrBool ZrMath_Quaternion_Construct(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  返回接收者四个数值分量平方和的平方根，供 `Quaternion.length()` 使用。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 须含可读的 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收浮点长度。
 *
 * ## Returns
 * @return 字段读取成功时为 ZR_TRUE；字段缺失或非数值时为 ZR_FALSE，数值写入 result。
 *
 * ## Constraints and lifecycle
 *  计算直接平方后求和，不缩放分量；极大有限分量可在平方阶段溢出为无穷。该方法
 *  不修改接收者，也不要求其 prototype 名称或单位范数。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `length` 方法；ZrMath_ReadQuaternionObject。
 */
TZrBool ZrMath_Quaternion_Length(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  返回接收者四个分量的直接平方和，供 `Quaternion.lengthSquared()` 使用。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 须含可读的 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收浮点平方范数。
 *
 * ## Returns
 * @return 字段读取成功时为 ZR_TRUE；字段缺失或非数值时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 *  这是未缩放的浮点平方和；极大分量可能溢出。方法不修改接收者，也不验证单位范数。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `lengthSquared` 方法；ZrMath_ReadQuaternionObject。
 */
TZrBool ZrMath_Quaternion_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  按长度缩放接收者并返回新 Quaternion；长度不大于 math epsilon 时采用单位四元数
 *  `(0, 0, 0, 1)` 作为回退值。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 须含可读的 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收新对象。
 *
 * ## Returns
 * @return 字段读取和结果对象创建成功时为 ZR_TRUE；读取或对象创建失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 *  输入对象不变；返回对象由 VM 管理。平方范数直接计算，有限但足够大的输入会溢出，
 *  之后以无穷长度相除可使非零分量归零；本路径尚未改为稳定缩放算法。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `normalized` 方法；ZrMath_MakeQuaternion。
 */
TZrBool ZrMath_Quaternion_Normalized(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  返回接收者的共轭 `(-x, -y, -z, w)`，不修改输入对象。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 须含可读的 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收新 Quaternion 对象。
 *
 * ## Returns
 * @return 字段读取和结果对象创建成功时为 ZR_TRUE；否则为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 *  结果对象由 VM 管理；运算不要求单位范数，也不改变接收者。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `conjugate` 方法；ZrMath_MakeQuaternion。
 */
TZrBool ZrMath_Quaternion_Conjugate(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  返回接收者的共轭除以平方范数；近零输入采用单位四元数回退。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 须含可读的 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收新 Quaternion 对象。
 *
 * ## Returns
 * @return 字段读取和结果对象创建成功时为 ZR_TRUE；否则为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 *  输入不变，返回对象由 VM 管理。平方范数直接计算；有限但极大的分量可令平方范数
 *  溢出为无穷，从而把可表示的逆元缩成零。本回调不验证单位范数。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `inverse` 方法；ZrMath_MakeQuaternion。
 */
TZrBool ZrMath_Quaternion_Inverse(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  计算接收者与一个 Quaternion 形对象的四分量点积，供 `Quaternion.dot(other)` 使用。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 与参数 0 均须含可读的
 *                    x/y/z/w 数值字段，参数 0 须是 VM object 或 array 值。
 * @param[out] result VM 调用结果槽，成功时接收浮点点积。
 *
 * ## Returns
 * @return 两个对象的字段读取成功时为 ZR_TRUE；字段缺失或非数值时为 ZR_FALSE。
 *         缺参或参数 0 不是 object/array 会进入 VM 错误，不以普通 false 返回。
 *
 * ## Constraints and lifecycle
 *  不检查参数对象的 prototype 名称，不要求单位范数；只读取数值快照，不修改输入。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `dot` 方法；ZrLib_CallContext_ReadObject；
 *  ZrMath_ReadQuaternionObject。
 */
TZrBool ZrMath_Quaternion_Dot(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  计算左右操作数的 Hamilton 积，供命名方法 `mul` 使用。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 是左操作数，参数 0 是右操作数，
 *                    两者均须是带 x/y/z/w 数值字段的 VM object 或 array。
 * @param[out] result VM 调用结果槽，成功时接收新 Quaternion 对象。
 *
 * ## Returns
 * @return 两个对象的字段读取及结果对象创建成功时为 ZR_TRUE；字段或 factory 失败时为 ZR_FALSE。
 *         缺参或参数 0 不是 object/array 会进入 VM 错误，不以普通 false 返回。
 *
 * ## Constraints and lifecycle
 *  乘法不交换，计算保留 self 在左、参数 0 在右的顺序；不归一化输入或结果。输入不变，
 *  新对象由 VM 管理。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `mul` 方法；zr_math_quaternion_mul_value。
 */
TZrBool ZrMath_Quaternion_Mul(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  在两个 Quaternion 形对象之间做球面线性插值；负点积时翻转右端点以选择短弧，
 *  点积大于 0.9995 时改用线性分支。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 是左端点，参数 0 是右端点，
 *                    参数 1 是可转换为 double 的插值参数 t。
 * @param[out] result VM 调用结果槽，成功时接收新 Quaternion 对象。
 *
 * ## Returns
 * @return 字段读取及结果对象创建成功时为 ZR_TRUE；字段或 factory 失败时为 ZR_FALSE。
 *         缺参、参数 0 非 object/array 或 t 非数值会进入 VM 错误，不以普通 false 返回。
 *
 * ## Constraints and lifecycle
 *  math-api.md 要求调用方传入单位四元数端点；native 不验证或归一化端点。该文档未约定
 *  t 的区间，也未承诺输出单位范数；近同向线性分支不归一化结果，且 dot 不先夹紧再
 *  调用 acos。TODO: 核对调用方是否依赖单位输出，并确认 t 的适用范围。输入不变，结果对象由 VM 管理。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `slerp` 方法；docs/wiki/03-modules/math-api.md。
 */
TZrBool ZrMath_Quaternion_Slerp(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  对两个 Quaternion 形对象逐分量相加，供 VM `+` 元方法调用。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 与参数 0 均须含 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收新 Quaternion 对象。
 *
 * ## Returns
 * @return 两个对象的字段读取及结果对象创建成功时为 ZR_TRUE；字段或 factory 失败时为 ZR_FALSE。
 *         缺参或参数 0 不是 object/array 会进入 VM 错误，不以普通 false 返回。
 *
 * ## Constraints and lifecycle
 *  不检查参数 prototype，不要求单位范数；不修改输入，结果由 VM 管理。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `ZR_META_ADD` 描述符；ZrMath_MakeQuaternion。
 */
TZrBool ZrMath_Quaternion_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  对两个 Quaternion 形对象逐分量相减，供 VM `-` 元方法调用。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 是被减数，参数 0 是减数，均须
 *                    含 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收新 Quaternion 对象。
 *
 * ## Returns
 * @return 两个对象的字段读取及结果对象创建成功时为 ZR_TRUE；字段或 factory 失败时为 ZR_FALSE。
 *         缺参或参数 0 不是 object/array 会进入 VM 错误，不以普通 false 返回。
 *
 * ## Constraints and lifecycle
 *  保持 self 减参数 0 的顺序；不检查 prototype 或单位范数，不修改输入，结果由 VM 管理。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `ZR_META_SUB` 描述符；ZrMath_MakeQuaternion。
 */
TZrBool ZrMath_Quaternion_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  将 VM `*` 元方法转发到 Quaternion 的命名 Hamilton 乘法回调，避免两种入口分叉。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 为左操作数，参数 0 为右操作数。
 * @param[out] result 直接传给 ZrMath_Quaternion_Mul 的 VM 结果槽。
 *
 * ## Returns
 * @return 与 ZrMath_Quaternion_Mul 相同；成功交付新对象时为 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 *  操作数及其字段约束、结果生命周期与命名 `mul` 方法一致；本函数不另建对象或改变顺序。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `ZR_META_MUL` 描述符；ZrMath_Quaternion_Mul。
 */
TZrBool ZrMath_Quaternion_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  对接收者的四个分量逐一取负，供一元 VM `-` 元方法调用。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 须含可读的 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收新 Quaternion 对象。
 *
 * ## Returns
 * @return 字段读取和结果对象创建成功时为 ZR_TRUE；否则为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 *  不修改接收者，不验证 prototype 或单位范数；结果对象由 VM 管理。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `ZR_META_NEG` 描述符；ZrMath_MakeQuaternion。
 */
TZrBool ZrMath_Quaternion_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  按两个操作数的分量平方和执行 VM 比较协议；它比较模长键，不比较分量相等或旋转等价。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 与参数 0 均须含 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，成功时接收 -1、0 或 1 的整数。
 *
 * ## Returns
 * @return 两个对象的字段读取成功时为 ZR_TRUE；字段缺失或非数值时为 ZR_FALSE。
 *         缺参或参数 0 不是 object/array 会进入 VM 错误，不以普通 false 返回。
 *
 * ## Constraints and lifecycle
 *  等平方和会比较为 0，即使分量不同。TODO: 两边平方和都因有限大分量溢出为无穷时也
 *  比较为 0；核对 VM 对该比较键的数值范围承诺，并用合法有限输入补一条聚焦用例。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `ZR_META_COMPARE` 描述符；
 *  ZR_META_COMPARE 协议。
 */
TZrBool ZrMath_Quaternion_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 *  将接收者格式化为 `Quaternion(x, y, z, w)`，供 VM 字符串化元方法调用。
 *
 * ## Parameters
 * @param[in] context 当前 native callback context；self 须含可读的 x/y/z/w 数值字段。
 * @param[out] result VM 调用结果槽，正常创建时接收 VM 管理的字符串。
 *
 * ## Returns
 * @return 字段读取失败时为 ZR_FALSE；格式化 helper 的成功状态沿用其返回值。
 *
 * ## Constraints and lifecycle
 *  格式使用 `%g`，固定缓冲区中的文本由 VM 字符串创建接口复制。BUG: 短串表需要扩桶
 *  且原生分配失败时，底层 setter 会保留预置的 null 结果而 callback 仍报成功；实现处
 *  与 coverage 台账记录了从 descriptor 到 dispatch 的证据链。
 *
 * ## References
 *  ZrMath_QuaternionRegistry_GetType 的 `ZR_META_TO_STRING` 描述符；
 *  ZrMath_MakeStringResult；ZrLib_Value_SetString。
 */
TZrBool ZrMath_Quaternion_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_QUATERNION_H
