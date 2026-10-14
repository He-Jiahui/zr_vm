//
// Vector4 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_VECTOR4_H
#define ZR_VM_LIB_MATH_VECTOR4_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 * ## Summary
 * 声明 zr.math 的 Vector4 构造、实例方法与 meta callback；registry 将这些函数指针交给 native binding。
 *
 * ## Constraints and lifecycle
 * dispatcher 在 callback 前按 descriptor 固定 arity 检查参数数目，并在一次 VM 调用内同步派发。
 * context、state 与 result 只在本次调用期间借用；普通 ZR_FALSE 仅在线程仍为 FINE 时归一化为 null，
 * ReadFloat/ReadObject 标签错误则由 VM 异常路径清理。返回对象和字符串由 VM 管理，不由调用方释放。
 *
 * ## References
 * ZrMath_Vector4Registry_GetType；native_binding_auto_check_arity；native_binding_invoke_entry_callback_inline；
 * ZrMath_ReadVector4Object；docs/wiki/03-modules/math-api.md
 */
/**
 * ## Summary
 * 处理 `math.Vector4(x,y,z,w)` 的 constructor meta callback，按字段顺序初始化兼容 receiver 或目标实例。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver 与四个 VM 数值参数。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示构造 helper 已把目标对象写入 result；目标解析/创建失败时返回 ZR_FALSE。
 * 数值参数标签错误由 ReadFloat 抛 VM TypeError，不是普通 false。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=4，由 dispatcher 在 callback 前检查。ReadFloat 接受有符号/无符号整数和浮点值并转为 double；
 * `kFields` 是静态字段名，`values` 是本次调用栈上的四项快照，helper 同步消费两者。ResolveConstructTarget
 * 复用兼容 owner-prototype receiver，否则按 construct target/owner prototype 创建对象。字段 setter 为 void；
 * ignored-root 扩容或四字段 nodeMap 扩桶失败可静默漏写，helper 仍可能发布对象，见实现 BUG 块。
 *
 * ## References
 * ZrMath_Vector4Registry_GetType 的 kMeta；native_binding_auto_check_arity；ZrMath_ConstructFloatObject；
 * ZrMath_ResolveConstructTarget；compile_expression_types.c 的 hidden constructor call
 */
TZrBool ZrMath_Vector4_Construct(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 的 x/y/z/w，计算平方和的平方根并将长度写入 result。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver，无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 写入浮点长度；字段缺失或非数值时返回 ZR_FALSE。
 * 有限的大分量平方溢出后仍返回成功，但长度可能错误地变成无穷。
 *
 * ## Constraints and lifecycle
 * 只读当前 receiver，不修改或保留它；字段读取经 ReadVector4Object。有限 `(1e200,0,0,0)` 的平方和溢出，
 * 虽然数学长度仍可由 double 表示，结果却是无穷，见实现 BUG。Dot 对结构体地址按连续 double 读取的布局前提另见 TODO。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_Dot；ZrMathVector4；math_common.h 的连续布局 TODO；
 * docs/wiki/03-modules/math-api.md 的 vector 方法契约
 */
TZrBool ZrMath_Vector4_Length(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 的四个分量并返回平方长度，不执行平方根。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver，无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 写入平方长度；字段缺失或非数值时返回 ZR_FALSE。
 * 平方和超出 double 可表示范围时按浮点运算结果返回无穷。
 *
 * ## Constraints and lifecycle
 * 只读 receiver 的 x/y/z/w，不保留字段指针。平方结果本身不可表示时产生无穷属于当前 IEEE double 运算；
 * 这与 Length 的“中间平方溢出但最终长度本可表示”分开。Dot 的结构体连续布局假设见 TODO。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_Dot；ZrMath_Vector4_Length；math_common.h 的连续布局 TODO
 */
TZrBool ZrMath_Vector4_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按当前计算长度缩放四个分量并返回新的 VM Vector4；epsilon 范围内返回零向量。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver，无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 接收新对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * 新对象由 VM 管理；字段 setter 的软失败可能造成不完整对象但不改变 callback 成功状态。
 *
 * ## Constraints and lifecycle
 * receiver 只读，结果不覆盖它。长度不大于 ZR_MATH_EPSILON 时造零向量，否则逐分量除以长度；
 * 有限 `(1e200,0,0,0)` 令平方范数溢出为无穷，再除后得到零而不是单位向量，见实现 BUG。
 * MakeVector4 的四字段 nodeMap 扩桶与 ignored-root pin 失败是独立的发布风险。
 *
 * ## References
 * ZR_MATH_EPSILON；ZrMath_Dot；ZrMath_MakeVector4；zr_math_make_object_with_fields；
 * docs/wiki/03-modules/math-api.md
 */
TZrBool ZrMath_Vector4_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 与 VM 参数 0 的四个数值字段并返回点积。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver 和 rhs 参数 0。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 写入点积；字段缺失或非数值时返回 ZR_FALSE。
 * rhs 标签错误由 ReadObject 抛 VM TypeError；这不是普通 false。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1，由 dispatcher 预检。rhs 可为 OBJECT 或 ARRAY；只按 x/y/z/w 读取数值字段，
 * 不要求名义 Vector4 prototype。有限 `(1e200,1e200,0,0)` 点乘 `(1e200,-1e200,0,0)` 时，
 * 乘积先成为 +Inf 和 -Inf，累加得到 NaN，而精确点积为 0，见实现 BUG。Dot 的结构体连续布局假设另见 TODO。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_Dot；ZrMath_Vector4Registry_GetType 的 kMethods.dot；
 * native_binding_dispatch.c 的 ReadObject
 */
TZrBool ZrMath_Vector4_Dot(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 与 rhs 的四个分量，返回两点欧氏距离。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver 和 rhs 参数 0。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 写入距离；字段缺失或非数值时返回 ZR_FALSE。
 * rhs 标签错误由 ReadObject 抛 VM TypeError；局部 delta 数组在返回后失效。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1。rhs 可为 OBJECT 或 ARRAY，不要求名义 Vector4 prototype；callback 先计算局部差分，
 * 再对 `d[4]` 求平方和并开根，不把 ZrMathVector4 地址当数组。有限 `(1e200,0,0,0)` 到零向量的距离
 * 可由 double 表示，但平方差溢出使结果为无穷，见实现 BUG。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_Dot；ZrMath_Vector4Registry_GetType 的 kMethods.distance
 */
TZrBool ZrMath_Vector4_Distance(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按 `lhs + (rhs - lhs) * factor` 逐分量插值并返回新的 VM Vector4。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver、rhs 参数 0 和 factor 参数 1。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 接收新对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * rhs 标签错误由 ReadObject 抛 TypeError，factor 标签错误由 ReadFloat 抛 TypeError。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=2，由 dispatcher 预检；rhs 可为 OBJECT 或 ARRAY，factor 接受整数/浮点并转 double，
 * 不夹紧到 `[0,1]`。有限 x 端点 `1e308` 与 `-1e308`、factor `0.5` 时差分先溢出为 `-Inf`，
 * 返回值不是数学中点 0，见实现 BUG。MakeVector4 新对象发布另有字段 setter 失败风险。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_MakeVector4；ZrMath_Vector4Registry_GetType 的 kMethods.lerp；
 * native_binding_auto_check_arity；ZrLib_CallContext_ReadFloat
 */
TZrBool ZrMath_Vector4_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现加法 meta callback，逐分量相加并创建新的 VM Vector4。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver 和 rhs 参数 0。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 接收新对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * rhs 标签错误抛 VM TypeError；不完整的非空 factory 对象仍可能被成功交付。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1；rhs 可为 OBJECT 或 ARRAY，只验证数值 x/y/z/w，不要求名义 Vector4 prototype。
 * 结果由 MakeVector4 创建，不修改 receiver/rhs。四字段 nodeMap 扩桶和 ignored-root pin 软失败可令某字段缺失，
 * setter 不向此 callback 报告该失败。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_MakeVector4；ZrMath_Vector4Registry_GetType 的 ZR_META_ADD；
 * ZrMath_WriteFloatField
 */
TZrBool ZrMath_Vector4_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现减法 meta callback，逐分量计算 receiver-rhs 并创建新的 VM Vector4。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver 和 rhs 参数 0。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 接收新对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * rhs 标签错误抛 VM TypeError；factory 的非空但缺字段结果仍可能被成功交付。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1；rhs 可为 OBJECT 或 ARRAY，只要求数值 x/y/z/w，不要求 nominal Vector4 prototype。
 * 结果由 MakeVector4 创建，不转移 rhs 所有权。void setter 不报告 ignored-root 或第四字段扩桶失败。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_MakeVector4；ZrMath_Vector4Registry_GetType 的 ZR_META_SUB；
 * ZrMath_WriteFloatField
 */
TZrBool ZrMath_Vector4_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现取负 meta callback，对 receiver 的四个分量取负并返回新的 VM Vector4。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver，无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 接收新对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * MakeVector4 返回非空时 callback 不会检查每个字段 setter 是否实际写入。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=0。只读 receiver；新对象由 VM 管理。MakeVector4 的四字段 nodeMap 扩桶和
 * ignored-root pin 软失败都可令结果缺字段，且不会经 void setter 传播为 callback false。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_MakeVector4；ZrMath_Vector4Registry_GetType 的 ZR_META_NEG；
 * ZrMath_WriteFloatField
 */
TZrBool ZrMath_Vector4_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按两向量的平方长度执行严格比较：dl>dr 返回 1，dl<dr 返回 -1，其余返回 0。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver 和 rhs 参数 0。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 result 写入整数 -1、0 或 1；字段读取失败时返回 ZR_FALSE。
 * 结果 0 不证明字段相等；NaN 或双方平方和都溢出为 Inf 时两个严格比较均为假。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1；rhs 可为 OBJECT 或 ARRAY，字段 helper 只校验数值形状。比较按平方长度而非分量顺序；
 * 非有限输入/平方溢出的排序策略及实际 VM compare 消费者尚待核实。此实现也把 ZrMathVector4 地址当连续 double 输入，
 * 布局前提另见 TODO。
 *
 * ## References
 * ZrMath_ReadVector4Object；ZrMath_Dot；ZrMath_Vector4Registry_GetType 的 ZR_META_COMPARE；
 * docs/wiki/03-modules/math-api.md 的 vector compare 契约；math_common.h 的布局 TODO
 */
TZrBool ZrMath_Vector4_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将 receiver 格式化为 `Vector4(x, y, z, w)`，并经 VM 字符串 helper 写入 result。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文，借用至 callback 返回；包含 receiver，无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果 slot；callback 不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示字段读取和格式化 helper 返回成功；短串 intern 失败时 result 仍可能是预置 null。
 * 写入的 VM 字符串由 VM 管理；callback 不保留局部格式缓冲。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=0。helper 用 512 字节栈缓冲区格式化；仅当结果是未驻留短串且短串表
 * HASH_BUCKET rehash 的 Memory_Allocate 失败时，SetString 不写结果但 helper 仍报告成功，dispatch 可成功回写 null，见 BUG。
 *
 * ## References
 * ZrMath_MakeStringResult；ZrLib_Value_SetString；ZrCore_String_Create；ZrCore_HashSet_Rehash；
 * native_binding_dispatch.c 的 result 初始化与成功回写
 */
TZrBool ZrMath_Vector4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_VECTOR4_H
