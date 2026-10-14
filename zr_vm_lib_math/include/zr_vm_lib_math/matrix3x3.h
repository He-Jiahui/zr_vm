//
// Matrix3x3 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_MATRIX3X3_H
#define ZR_VM_LIB_MATH_MATRIX3X3_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 * ## Summary
 * Matrix3x3 的同步 native callback 声明；registry 把这些函数指针交给 VM native dispatcher。
 * 数值字段按 row-major 顺序命名为 `m00` 至 `m22`，矩阵乘法按列向量解释。
 *
 * ## Constraints and lifecycle
 * 调用须由 dispatcher 提供有效的 `context`、`context->state` 与 `result`；本模块不检查这些指针是否为空。
 * `context` 及其中的 self、参数值只在本次同步 callback 内借用；不得保存或跨线程使用。`result` 是 VM
 * 管理的输出槽，调用方不释放它；普通 `ZR_FALSE` 且线程状态仍为 FINE 时 dispatcher 交付预置 null，
 * 已设置 VM 异常的路径则保留异常状态。成功标志本身不保证结果非 null，具体例外见 `MetaToString`。
 *
 * ## References
 * `ZrMath_Matrix3x3Registry_GetType` 注册回调；`zr_vm_lib_math/src/zr_vm_lib_math/module.c`
 * 聚合本模块 descriptor；`docs/wiki/03-modules/math-api.md` 记录数学接口语义。
 */
/**
 * ## Summary
 * 作为 Matrix3x3 的 constructor meta callback，在创建或复用的 receiver 上尝试初始化九个字段。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；接受零个参数，或按 `m00` 至 `m22` 顺序给出九个数值。
 * @param[out] result dispatcher 提供的结果槽；成功时接收构造对象，不由 callback 调用方释放。
 *
 * ## Returns
 * @return 构造 helper 完成时返回 `ZR_TRUE`；目标解析或完成失败且未抛出 VM 异常时返回 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 零参数表示单位矩阵；其余合法调用必须恰有九个可读数值。descriptor 当前声明范围为 0..9，
 * 因而 1..8 会进入 callback 后被拒绝并由 noreturn 的 `RaiseArityError` 抛出 VM 异常；数值标签错误由
 * `ReadFloat` 抛出 `TypeError`，都不是普通 `ZR_FALSE`。字段 setter 是 void 且可软失败；不能仅凭 `ZR_TRUE`
 * 证明每个字段都已写入。`context`、receiver 和参数只在本次同步调用中有效。
 *
 * ## References
 * `GetType.kMeta` 注册此回调；`ZrMath_ConstructFloatObject` 负责兼容 receiver 与字段写入。
 */
TZrBool ZrMath_Matrix3x3_Construct(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 作为静态 `Matrix3x3.identity()` 方法创建单位矩阵并写入 callback 结果。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；本方法不读取显式参数。
 * @param[out] result dispatcher 提供的结果槽；成功时接收新 Matrix3x3 对象。
 *
 * ## Returns
 * @return 工厂返回对象时为 `ZR_TRUE`；对象创建失败时为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 单位数据在 callback 栈上构造，工厂同步复制到新对象；receiver/context 不逃逸。
 * 新对象由 VM 管理，callback 不转移或释放其所有权。共享 void 字段 setter 软失败时工厂仍可能返回
 * 不完整对象；回调的成功值不证明所有字段均已发布。
 *
 * ## References
 * `GetType.kMethods` 将 `identity` 注册为静态方法；`ZrMath_MakeMatrix3x3` 创建结果。
 */
TZrBool ZrMath_Matrix3x3_Identity(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 转置当前 Matrix3x3 receiver，并返回独立的新矩阵。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；从 self 读取九个 `m00` 至 `m22` 数值字段。
 * @param[out] result dispatcher 提供的结果槽；成功时接收转置后的新对象。
 *
 * ## Returns
 * @return 读取并创建结果成功时为 `ZR_TRUE`；字段读取或工厂失败时为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 本方法不修改 receiver；输入快照和转置快照均为 callback 栈内临时值，结果对象由 VM 管理。共享
 * 工厂的 void 字段 setter 软失败时仍可能返回字段不完整的矩阵。
 *
 * ## References
 * `GetType.kMethods` 以实例方法注册 `transpose`；`ZrMath_MakeMatrix3x3` 发布新结果。
 */
TZrBool ZrMath_Matrix3x3_Transpose(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 读取 receiver 并返回其 3×3 行列式数值。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；self 必须提供九个可读数值字段。
 * @param[out] result dispatcher 提供的结果槽；成功时写入浮点行列式。
 *
 * ## Returns
 * @return 字段读取成功时为 `ZR_TRUE`；读取失败时为 `ZR_FALSE`，不创建对象。
 *
 * ## Constraints and lifecycle
 * 公式直接使用双精度中间值，没有缩放或溢出检查。有限矩阵 `[[A,A,0],[A,A,0],[0,0,1]]`（A=1e200）
 * 的两个外层乘积分别溢出为 Inf，随后 Inf-Inf 得到 NaN，而精确行列式为 0；细节见实现 BUG 注释。
 *
 * ## References
 * `GetType.kMethods` 注册 `determinant`；`zr_math_matrix3_det` 实现 cofactor 展开。
 */
TZrBool ZrMath_Matrix3x3_Determinant(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 求 receiver 的逆矩阵，并在可逆性检查通过时返回新 Matrix3x3。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；self 必须提供九个可读数值字段。
 * @param[out] result dispatcher 提供的结果槽；成功时接收新矩阵。
 *
 * ## Returns
 * @return receiver 字段可读、行列式检查通过且工厂返回对象时为 `ZR_TRUE`；字段形状、近零阈值或工厂
 * 返回 null 时为普通 `ZR_FALSE`。字段读取失败可先设置 VM 异常，不等同于数值阈值失败。
 *
 * ## Constraints and lifecycle
 * 当前实现使用固定绝对阈值 `ZR_MATH_EPSILON`；字段形状不完整或近零行列式时返回普通 false，native
 * dispatcher 在 VM 状态仍为 FINE 时把 false 映射为 null。NaN 行列式不会满足 `fabs(det) <= epsilon`。
 * 通过阈值后，共享工厂的 void 字段 setter 仍可能软失败并发布不完整矩阵。
 * TODO: 核实 registry 声明的 `Matrix3x3` 返回类型是否应与不可逆时的 null 结果一致；下一步对照
 * `GetType.kMethods` 的 inverse 返回描述和 native false-result 契约。成功分支使用共享 void 字段 setter；
 * 其受限软失败条件也可能使工厂交付字段不完整的矩阵。
 *
 * ## References
 * `GetType.kMethods` 注册 `inverse`；`zr_math_matrix3_inverse` 执行阈值判断和伴随矩阵公式。
 */
TZrBool ZrMath_Matrix3x3_Inverse(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 计算 receiver 矩阵乘右侧三分量向量，返回新的 Vector3。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；参数 0 必须是 OBJECT/ARRAY 值，且具备数值 `x`、`y`、`z`。
 * @param[out] result dispatcher 提供的结果槽；成功时接收新 Vector3 对象。
 *
 * ## Returns
 * @return receiver/字段形状可读且结果工厂返回对象时为 `ZR_TRUE`；形状读取或工厂返回 null 时为
 * `ZR_FALSE`。参数缺失或不是 OBJECT/ARRAY 时，`ReadObject` 通过 noreturn 的 ArityError/TypeError 抛异常，
 * 不会作为普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 以 row-major 矩阵乘列向量；仅读取字段，不要求右侧对象必须具有 Vector3 原型。
 * 双精度乘加不检查有限值或中间溢出；对象结果由 VM 管理，不复用输入对象。
 *
 * ## References
 * `GetType.kMethods` 注册 `mulVector`；`GetType.kMeta` 的乘法 callback 对 Vector3 原型名转入此函数。
 */
TZrBool ZrMath_Matrix3x3_MulVector(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 计算 receiver 与右侧 3×3 矩阵的乘积，并返回新的 Matrix3x3。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；参数 0 必须是 OBJECT/ARRAY 值，且包含九个数值字段。
 * @param[out] result dispatcher 提供的结果槽；成功时接收新矩阵。
 *
 * ## Returns
 * @return 两个矩阵字段形状可读且结果工厂返回对象时为 `ZR_TRUE`；字段形状读取或工厂返回 null 时为
 * `ZR_FALSE`。参数缺失或不是 OBJECT/ARRAY 时，`ReadObject` 通过 noreturn 的 ArityError/TypeError 抛异常，
 * 不会作为普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 计算 `lhs * rhs`，索引按 row-major 且向量按列解释；不修改任一输入。
 * 本地输入/输出快照仅在 callback 内有效，乘加不检查浮点溢出。共享矩阵工厂的 void 字段 setter 软失败时，
 * callback 仍可能返回字段不完整的矩阵。
 *
 * ## References
 * `GetType.kMethods` 注册 `mulMatrix`；`GetType.kMeta` 的乘法 callback 在右侧非 Vector3 时转入此函数。
 */
TZrBool ZrMath_Matrix3x3_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现 Matrix3x3 的 `*` meta callback，并按右操作数的原型名选择向量或矩阵乘法。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；参数 0 必须是 OBJECT/ARRAY 值。
 * @param[out] result dispatcher 提供的结果槽；具体结果类型由右操作数分支决定。
 *
 * ## Returns
 * @return 右操作数读取并且所选乘法 callback 成功时为 `ZR_TRUE`；字段形状或后续工厂失败时为
 * `ZR_FALSE`。参数缺失或不是 OBJECT/ARRAY 时，`ReadObject` 通过 noreturn 的 ArityError/TypeError 抛异常，
 * 不会作为普通 false 返回。
 *
 * ## Constraints and lifecycle
 * 仅当右对象原型名精确等于 `Vector3` 时走向量路径；其他原型都按 Matrix3x3 字段形状读取，
 * 不执行一般子类型或协议分派。context 与右操作数只借用到同步 callback 返回；成功结果沿所选工厂的
 * 发布契约返回，矩阵分支的共享 void setter 软失败可留下字段不完整对象。
 *
 * ## References
 * `GetType.kMeta` 将 `ZR_META_MUL` 注册到此函数；分支分别调用 `ZrMath_Matrix3x3_MulVector`
 * 与 `ZrMath_Matrix3x3_MulMatrix`。
 */
TZrBool ZrMath_Matrix3x3_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将 receiver 的九个字段格式化为 Matrix3x3 文本并交由 VM 字符串 API 发布。
 *
 * ## Parameters
 * @param[in] context 当前 native 调用帧；从 self 读取九个数值字段。
 * @param[out] result dispatcher 提供的结果槽；成功时应包含 VM 字符串值。
 *
 * ## Returns
 * @return 字段读取及格式化 helper 报告成功时为 `ZR_TRUE`；字段读取失败时为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 格式缓冲区为 callback 栈内临时数据；结果为 VM 管理的字符串对象，短串使用驻留缓存，长串由 VM 管理对象生命周期。
 * 短字符串缓存扩桶的软分配失败可令 helper 返回成功而 result 仍为预置 null，见实现 BUG 注释。
 * TODO: 长输出也经 `ZrMath_MakeStringResult` → `ZrLib_Value_SetString` → `native_binding_create_string` →
 * `ZrCore_String_CreateTryHitCache`；该缓存以输入地址分桶，但比较宏 `ZR_CAST_STRING_TO_NATIVE` 只适用于短串。
 * 用较大有限分量使格式结果超过短串上限后，核查同地址桶中已有长串时的后续比较和结果；入口为
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:512-519`、`zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:320-324`、
 * `zr_vm_core/src/zr_vm_core/string/string.c:958-974` 与 `zr_vm_core/include/zr_vm_core/conversion.h:73-76`。
 *
 * ## References
 * `GetType.kMeta` 注册 `ZR_META_TO_STRING`；`ZrMath_MakeStringResult` 及 VM 字符串驻留路径发布文本。
 */
TZrBool ZrMath_Matrix3x3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_MATRIX3X3_H
