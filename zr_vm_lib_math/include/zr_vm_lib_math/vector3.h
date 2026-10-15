//
// Vector3 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_VECTOR3_H
#define ZR_VM_LIB_MATH_VECTOR3_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 * ## Summary
 * 声明 zr.math 的 Vector3 构造、实例方法与 meta callback；registry 将函数指针交给 native binding。
 *
 * ## Constraints and lifecycle
 * dispatcher 在 callback 前检查 descriptor arity。普通 ZR_FALSE 仅在 VM thread 状态仍为 FINE 时归一化为 null；
 * 已有 VM 异常则由 dispatcher 清理并传播。新对象和字符串由 VM 管理，调用方不得手动释放。
 *
 * ## References
 * ZrMath_Vector3Registry_GetType；native_binding_auto_check_arity；ZrMath_ReadVector3Object。
 */
/**
 * ## Summary
 * 处理 init math.Vector3(x,y,z) 的构造 meta callback，把三个数值按 x/y/z 顺序写入兼容 receiver 或新实例。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver 与三个 VM 数值实参 x、y、z。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 helper 发布 receiver；无法解析或创建目标时返回 ZR_FALSE。
 * ReadFloat 的错误标签是 VM 异常，不是普通 false。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 固定 arity=3 由 dispatcher 在 callback 前校验。ReadFloat 接受有符号/无符号整数和浮点值；错误标签抛 VM TypeError。ResolveConstructTarget 复用兼容 self，否则按 construct target/owner prototype 建对象。共享 void 字段 setter 的 ignored-root 扩容失败可跳过写入而仍发布对象。
 *
 * ## References
 * registry kMeta/ZR_META_CONSTRUCTOR；ZrMath_ConstructFloatObject；ZrMath_ResolveConstructTarget；test_native_vector3_constructor_binds_all_numeric_arguments_at_runtime
 */
TZrBool ZrMath_Vector3_Construct(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 的 x/y/z 并返回 sqrt(x*x+y*y+z*z)。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver；无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽写入浮点长度；字段读取失败时返回 ZR_FALSE。
 * 普通 false 仅在 thread 状态仍为 FINE 时归一化为 null。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 无显式 VM 参数，只读 receiver 数值字段；缺失或非数值字段返回普通 false。有限 (1e200,0,0) 的平方和先溢出而长度仍可表示，见 BUG 块。 BUG: struct 输入 count=3 在 scalar 及 AVX2/SSE2/NEON 尾循环读取下标2越界，见 math_common.h:134。
 *
 * ## References
 * ZrMath_ReadVector3Object；ZrMath_Dot；docs/wiki/03-modules/math-api.md
 */
TZrBool ZrMath_Vector3_Length(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 的 x/y/z 并返回平方长度，不执行平方根。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver；无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽写入平方长度；字段读取失败时返回 ZR_FALSE。
 * 普通 false 的结果归一化受 VM thread 状态约束。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 无显式 VM 参数；字段缺失或非数值时返回普通 false。平方值超出 double 范围时为 IEEE 浮点溢出结果；这与有限长度的中间溢出分开记录。 BUG: struct 输入 count=3 在 scalar 及 AVX2/SSE2/NEON 尾循环读取下标2越界，见 math_common.h:134。
 *
 * ## References
 * ZrMath_ReadVector3Object；ZrMath_Dot；ZrMath_Vector3_Length
 */
TZrBool ZrMath_Vector3_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按当前计算长度缩放分量并返回 VM 管理的新 Vector3；epsilon 范围内生成零向量。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver；无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽接收新 VM 对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * 新对象由 VM 管理；void setter 的 ignored-root pin 失败可能仍发布缺字段对象。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 无显式 VM 参数。字段读取或 MakeVector3 无法创建对象时返回普通 false。有限超大输入的平方范数会溢出为无穷，再除后返回零向量；共享工厂字段发布软失败也适用。 BUG: struct 输入 count=3 在 scalar 及 AVX2/SSE2/NEON 尾循环读取下标2越界，见 math_common.h:134。
 *
 * ## References
 * ZR_MATH_EPSILON；ZrMath_Dot；ZrMath_MakeVector3；zr_math_make_object_with_fields
 */
TZrBool ZrMath_Vector3_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 和 rhs 的 x/y/z 并返回点积。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver 与 VM 参数 0 rhs（OBJECT 或 ARRAY）。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽写入点积；字段读取失败时返回 ZR_FALSE。
 * rhs 标签错误走 VM 异常路径；字段形状错误是普通 false。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 固定 arity=1。rhs 非 OBJECT/ARRAY 时 ReadObject 抛 TypeError；只验证数值字段、不验证 nominal prototype。有限 (1e200,1e200,0) 点乘 (1e200,-1e200,0) 中间产生 +Inf/-Inf，结果 NaN 而实数点积为 0，见 BUG 块。 BUG: struct 输入 count=3 在 scalar 及 AVX2/SSE2/NEON 尾循环读取下标2越界，见 math_common.h:134。
 *
 * ## References
 * ZrMath_ReadVector3Object；ZrMath_Dot；registry kMethods.dot；native callback 间接调用
 */
TZrBool ZrMath_Vector3_Dot(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 与 rhs 的数值分量，返回两点欧几里得距离。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver 与 VM 参数 0 rhs（OBJECT 或 ARRAY）。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽写入距离；字段读取失败时返回 ZR_FALSE。
 * delta 是 callback 栈上临时数组，不保留参数地址。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 固定 arity=1；rhs 标签错误由 ReadObject 抛 TypeError。先计算局部差分，再求平方和和平方根；有限且可表示的距离可因平方差溢出而返回无穷，见 BUG 块。
 *
 * ## References
 * ZrMath_ReadVector3Object；ZrMath_Dot；registry kMethods.distance
 */
TZrBool ZrMath_Vector3_Distance(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按 lhs+(rhs-lhs)*factor 插值并返回 VM 管理的新 Vector3。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver、VM 参数 0 rhs（OBJECT 或 ARRAY）及参数 1 factor（数值）。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽接收新 VM 对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * 结果对象由 VM 管理，不转移 receiver/rhs 所有权。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 固定 arity=2 由 dispatcher 预检；参数 0 要求 OBJECT/ARRAY，参数 1 由 ReadFloat 转 double，标签错误分别抛 TypeError。factor 不夹紧。有限 x 端点 1e308/-1e308、factor=.5 的差分先溢出为 -Inf，见 BUG 块；factory 字段软失败也适用。
 *
 * ## References
 * ZrMath_ReadVector3Object；ZrMath_MakeVector3；registry kMethods.lerp；native_binding_auto_check_arity
 */
TZrBool ZrMath_Vector3_Lerp(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 计算右手叉积并按 x/y/z 顺序返回新 Vector3。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver 与 VM 参数 0 rhs（OBJECT 或 ARRAY）。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽接收新 VM 对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * 新对象由 VM 管理；callback 不保留字段快照。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 固定 arity=1；rhs 标签错误由 ReadObject 抛 TypeError，字段形状错误返回普通 false，不检查 nominal prototype。有限 (0,1e200,1e200) 与自身做叉积时 Inf-Inf 得 NaN，精确结果为零向量，见 BUG 块。factory setter 软失败也适用。
 *
 * ## References
 * ZrMath_ReadVector3Object；ZrMath_MakeVector3；registry kMethods.cross；tests/fixtures/projects/native_numeric_pipeline/src/lin_alg.zr
 */
TZrBool ZrMath_Vector3_Cross(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现加法 meta callback，逐分量相加并创建新 Vector3。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver 与 VM 参数 0 rhs（OBJECT 或 ARRAY）。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽接收新 VM 对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * 结果对象由 VM 管理；不逐字段确认发布完整性。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 固定 arity=1；rhs 标签错误抛 TypeError，字段 helper 只验证数值 x/y/z。非空 factory 对象即发布；void setter 软失败不一定反映在返回值。
 *
 * ## References
 * registry kMeta/ZR_META_ADD；ZrMath_ReadVector3Object；ZrMath_MakeVector3
 */
TZrBool ZrMath_Vector3_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现减法 meta callback，逐分量执行 receiver-rhs 并创建新 Vector3。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver 与 VM 参数 0 rhs（OBJECT 或 ARRAY）。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽接收新 VM 对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * 结果对象由 VM 管理；不转移 rhs 所有权。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 固定 arity=1；rhs 标签错误抛 TypeError，rhs 不要求 nominal Vector3 prototype，但须有数值 x/y/z。字段缺失返回普通 false；新对象 setter 软失败可能漏字段。
 *
 * ## References
 * registry kMeta/ZR_META_SUB；ZrMath_ReadVector3Object；ZrMath_MakeVector3
 */
TZrBool ZrMath_Vector3_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现取负 meta callback，对 receiver 三个分量取负并返回新对象。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver；无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽接收新 VM 对象；字段读取或对象创建失败时返回 ZR_FALSE。
 * 对象由 VM 管理；factory 字段软失败可能仍发布缺字段对象。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 无显式 VM 参数；receiver 字段缺失或非数值时普通 false。MakeVector3 对象非空后 callback 不验证每个 void 字段 setter 是否实际写入。
 *
 * ## References
 * registry kMeta/ZR_META_NEG；ZrMath_ReadVector3Object；ZrMath_MakeVector3
 */
TZrBool ZrMath_Vector3_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按平方长度严格比较：dl>dr 返回 1，dl<dr 返回 -1，否则返回 0。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver 与 VM 参数 0 rhs（OBJECT 或 ARRAY）。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示结果槽写入 -1/0/1；字段读取失败时返回 ZR_FALSE。
 * 整数结果不转移资源；另有 C11 结构体跨成员越界 BUG，不能据值比较结果推断对象相等。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 固定 arity=1；rhs 标签错误抛 TypeError，字段形状错误普通 false。NaN 或双方平方和均溢出为 Inf 时两个严格比较都失败而返回 0；这不证明相等，实际 VM compare consumer 与排序语义待核。 BUG: struct 输入 count=3 在 scalar 及 AVX2/SSE2/NEON 尾循环读取下标2越界，见 math_common.h:134。
 *
 * ## References
 * registry kMeta/ZR_META_COMPARE；docs/wiki/03-modules/math-api.md Vector3 compare；math_common.h:134 的 C11 对象边界 BUG
 */
TZrBool ZrMath_Vector3_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 格式化 receiver 为 Vector3(x, y, z) 并通过 VM 字符串表交付。
 *
 * ## Parameters
 * @param[in] context 当前 native dispatch 上下文（仅本次 callback 借用）；包含 receiver；无显式 VM 参数。
 * @param[out] result 当前 dispatch 提供的可写结果槽；回调不取得其所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示字段读取和格式化 helper 报告成功；条件性短串 rehash 失败仍可能让 dispatch 交付预置 null。
 * 写入的 VM 字符串由 VM 管理，不保留栈缓冲。
 *
 * ## Constraints and lifecycle
 * 由 native binding 同步派发；context、context->state 和 result 只在本次 callback 期间有效，不得保存到回调外或跨线程使用。
 * 无显式 VM 参数；字段读取失败返回普通 false。helper 使用 512 字节局部缓冲；未驻留短串触发 HASH_BUCKET rehash 且原生分配失败时 setter 不写 result，而 helper 仍返回 true，见本实现 BUG 块。
 *
 * ## References
 * ZrMath_MakeStringResult；ZrLib_Value_SetStringObject；ZrCore_HashSet_Rehash；native dispatch 结果初始化与成功回写
 */
TZrBool ZrMath_Vector3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_VECTOR3_H
